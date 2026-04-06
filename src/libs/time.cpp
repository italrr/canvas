#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_map>

#include "../CV.hpp"

static std::shared_ptr<CV::Data> __cv_tm_unwrap(const std::shared_ptr<CV::Data> &d){
    return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
}

static bool __cv_tm_expect_exactly(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) != n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "'"+name+"' expects exactly ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static bool __cv_tm_expect_type(
    const std::string &name,
    const std::shared_ptr<CV::Data> &value,
    CV::DataType expected,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    auto v = __cv_tm_unwrap(value);
    if(!v || v->type != expected){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' expected "+CV::DataTypeName(expected)+
            ", provided "+(v ? CV::DataTypeName(v->type) : std::string("NULL")),
            token
        );
        return false;
    }
    return true;
}

static std::string __cv_tm_trim(const std::string &s){
    std::size_t a = 0;
    while(a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))){
        ++a;
    }

    std::size_t b = s.size();
    while(b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))){
        --b;
    }

    return s.substr(a, b - a);
}

static std::string __cv_tm_pad2(int v){
    std::ostringstream ss;
    ss << std::setw(2) << std::setfill('0') << v;
    return ss.str();
}

static std::string __cv_tm_pad3(int v){
    std::ostringstream ss;
    ss << std::setw(3) << std::setfill('0') << v;
    return ss.str();
}

static void __cv_tm_replace_all(std::string &src, const std::string &from, const std::string &to){
    if(from.empty()){
        return;
    }

    std::size_t pos = 0;
    while((pos = src.find(from, pos)) != std::string::npos){
        src.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static bool __cv_tm_parse_timezone(
    const std::string &rawTz,
    int &offsetSeconds,
    std::string &normalized,
    const CV::CursorType &cursor,
    const CV::TokenType &token,
    const std::string &fname
){
    auto tz = __cv_tm_trim(rawTz);
    auto up = CV::Tools::upper(tz);

    if(up == "UTC" || up == "GMT" || up == "Z"){
        offsetSeconds = 0;
        normalized = "UTC";
        return true;
    }

    std::string prefix;
    if(up.rfind("UTC", 0) == 0){
        prefix = "UTC";
        up = up.substr(3);
    }else
    if(up.rfind("GMT", 0) == 0){
        prefix = "GMT";
        up = up.substr(3);
    }else{
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects timezone like 'UTC', 'GMT', 'GMT-4', or 'UTC+05:30'",
            token
        );
        return false;
    }

    if(up.empty()){
        offsetSeconds = 0;
        normalized = prefix;
        return true;
    }

    if(up[0] != '+' && up[0] != '-'){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' timezone '"+rawTz+"' is invalid",
            token
        );
        return false;
    }

    int sign = up[0] == '-' ? -1 : 1;
    std::string rest = up.substr(1);

    int hours = 0;
    int minutes = 0;

    auto colon = rest.find(':');
    try{
        if(colon == std::string::npos){
            hours = std::stoi(rest);
        }else{
            hours = std::stoi(rest.substr(0, colon));
            minutes = std::stoi(rest.substr(colon + 1));
        }
    }catch(...){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' timezone '"+rawTz+"' is invalid",
            token
        );
        return false;
    }

    if(hours < 0 || hours > 23 || minutes < 0 || minutes > 59){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' timezone '"+rawTz+"' is out of bounds",
            token
        );
        return false;
    }

    offsetSeconds = sign * (hours * 3600 + minutes * 60);

    normalized = prefix;
    normalized += (sign < 0 ? "-" : "+");
    normalized += std::to_string(hours);
    if(minutes != 0){
        normalized += ":";
        normalized += __cv_tm_pad2(minutes);
    }

    return true;
}

static bool __cv_tm_gmtime_safe(std::time_t t, std::tm &out){
#if defined(_WIN32)
    return gmtime_s(&out, &t) == 0;
#else
    return gmtime_r(&t, &out) != nullptr;
#endif
}

static bool __cv_tm_extract_date_store(
    const std::string &fname,
    const std::shared_ptr<CV::Data> &subject,
    CV_NUMBER &epochOut,
    std::string &tzOut,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    auto v = __cv_tm_unwrap(subject);
    if(!v || v->type != CV::DataType::STORE){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects a STORE created by tm:date",
            token
        );
        return false;
    }

    auto store = std::static_pointer_cast<CV::DataStore>(v);

    if(store->v.count("epoch") == 0){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects store field 'epoch'",
            token
        );
        return false;
    }

    if(store->v.count("tz") == 0){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects store field 'tz'",
            token
        );
        return false;
    }

    auto epochData = __cv_tm_unwrap(store->v["epoch"]);
    auto tzData = __cv_tm_unwrap(store->v["tz"]);

    if(!epochData || epochData->type != CV::DataType::NUMBER){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects store field 'epoch' to be NUMBER",
            token
        );
        return false;
    }

    if(!tzData || tzData->type != CV::DataType::STRING){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expects store field 'tz' to be STRING",
            token
        );
        return false;
    }

    epochOut = std::static_pointer_cast<CV::DataNumber>(epochData)->v;
    tzOut = std::static_pointer_cast<CV::DataString>(tzData)->v;
    return true;
}

static std::shared_ptr<CV::Data> __CV_STD_TM_TICKS(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "tm:ticks";

    if(!__cv_tm_expect_exactly(name, args, 0, cursor, token)){
        return ctx->buildNil();
    }

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(static_cast<CV_NUMBER>(ms))
    );
}

static std::shared_ptr<CV::Data> __CV_STD_TM_EPOCH(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "tm:epoch";

    if(!__cv_tm_expect_exactly(name, args, 0, cursor, token)){
        return ctx->buildNil();
    }

    auto now = std::chrono::system_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()
    ).count();

    return std::static_pointer_cast<CV::Data>(
        ctx->buildNumber(static_cast<CV_NUMBER>(sec))
    );
}

static std::shared_ptr<CV::Data> __CV_STD_TM_DATE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "tm:date";

    if(!__cv_tm_expect_exactly(name, args, 2, cursor, token)){
        return ctx->buildNil();
    }

    auto epochData = __cv_tm_unwrap(args[0].second);
    auto tzData = __cv_tm_unwrap(args[1].second);

    if(!__cv_tm_expect_type(name, epochData, CV::DataType::NUMBER, cursor, token) ||
       !__cv_tm_expect_type(name, tzData, CV::DataType::STRING, cursor, token)){
        return ctx->buildNil();
    }

    auto epoch = std::static_pointer_cast<CV::DataNumber>(epochData)->v;
    auto tz = std::static_pointer_cast<CV::DataString>(tzData)->v;

    int offsetSeconds = 0;
    std::string normalized;
    if(!__cv_tm_parse_timezone(tz, offsetSeconds, normalized, cursor, token, name)){
        return ctx->buildNil();
    }

    auto out = ctx->buildStore();
    out->v["epoch"] = std::static_pointer_cast<CV::Data>(ctx->buildNumber(epoch));
    out->v["tz"] = std::static_pointer_cast<CV::Data>(ctx->buildString(normalized));
    return std::static_pointer_cast<CV::Data>(out);
}

static std::shared_ptr<CV::Data> __CV_STD_TM_FLIP_TZ(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "tm:flip-tz";

    if(!__cv_tm_expect_exactly(name, args, 2, cursor, token)){
        return ctx->buildNil();
    }

    CV_NUMBER epoch = 0;
    std::string oldTz;
    if(!__cv_tm_extract_date_store(name, args[0].second, epoch, oldTz, cursor, token)){
        return ctx->buildNil();
    }

    auto tzData = __cv_tm_unwrap(args[1].second);
    if(!__cv_tm_expect_type(name, tzData, CV::DataType::STRING, cursor, token)){
        return ctx->buildNil();
    }

    auto newTz = std::static_pointer_cast<CV::DataString>(tzData)->v;

    int offsetSeconds = 0;
    std::string normalized;
    if(!__cv_tm_parse_timezone(newTz, offsetSeconds, normalized, cursor, token, name)){
        return ctx->buildNil();
    }

    auto out = ctx->buildStore();
    out->v["epoch"] = std::static_pointer_cast<CV::Data>(ctx->buildNumber(epoch));
    out->v["tz"] = std::static_pointer_cast<CV::Data>(ctx->buildString(normalized));
    return std::static_pointer_cast<CV::Data>(out);
}

static std::shared_ptr<CV::Data> __CV_STD_TM_FORMAT(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "tm:format";

    if(!__cv_tm_expect_exactly(name, args, 2, cursor, token)){
        return ctx->buildNil();
    }

    CV_NUMBER epoch = 0;
    std::string tz;
    if(!__cv_tm_extract_date_store(name, args[0].second, epoch, tz, cursor, token)){
        return ctx->buildNil();
    }

    auto formatData = __cv_tm_unwrap(args[1].second);
    if(!__cv_tm_expect_type(name, formatData, CV::DataType::STRING, cursor, token)){
        return ctx->buildNil();
    }

    auto fmt = std::static_pointer_cast<CV::DataString>(formatData)->v;

    int offsetSeconds = 0;
    std::string normalized;
    if(!__cv_tm_parse_timezone(tz, offsetSeconds, normalized, cursor, token, name)){
        return ctx->buildNil();
    }

    CV_NUMBER wholePart = 0;
    CV_NUMBER frac = std::modf(epoch, &wholePart);
    if(frac < 0){
        frac = -frac;
    }

    int milliseconds = static_cast<int>(std::round(frac * 1000.0));
    if(milliseconds >= 1000){
        milliseconds = 0;
        wholePart += 1;
    }

    std::time_t adjusted = static_cast<std::time_t>(wholePart) + offsetSeconds;
    std::tm tmv{};
    if(!__cv_tm_gmtime_safe(adjusted, tmv)){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+name+"' failed to convert epoch into date",
            token
        );
        return ctx->buildNil();
    }

    int yearFull = tmv.tm_year + 1900;
    int yearShort = yearFull % 100;
    int month = tmv.tm_mon + 1;
    int day = tmv.tm_mday;
    int hour = tmv.tm_hour;
    int minute = tmv.tm_min;
    int second = tmv.tm_sec;

    __cv_tm_replace_all(fmt, "%year", std::to_string(yearFull));
    __cv_tm_replace_all(fmt, "%tz", normalized);
    __cv_tm_replace_all(fmt, "%ms", __cv_tm_pad3(milliseconds));
    __cv_tm_replace_all(fmt, "%y", __cv_tm_pad2(yearShort));
    __cv_tm_replace_all(fmt, "%m", __cv_tm_pad2(month));
    __cv_tm_replace_all(fmt, "%d", __cv_tm_pad2(day));
    __cv_tm_replace_all(fmt, "%h", __cv_tm_pad2(hour));
    __cv_tm_replace_all(fmt, "%M", __cv_tm_pad2(minute));
    __cv_tm_replace_all(fmt, "%s", __cv_tm_pad2(second));

    return std::static_pointer_cast<CV::Data>(
        ctx->buildString(fmt)
    );
}

extern "C" void _CV_REGISTER_LIBRARY(
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    (void)cursor;

    ctx->registerFunction("tm:ticks", __CV_STD_TM_TICKS);
    ctx->registerFunction("tm:epoch", __CV_STD_TM_EPOCH);
    ctx->registerFunction("tm:date", {"epoch", "tz"}, __CV_STD_TM_DATE);
    ctx->registerFunction("tm:flip-tz", {"subject", "tz"}, __CV_STD_TM_FLIP_TZ);
    ctx->registerFunction("tm:format", {"subject", "format"}, __CV_STD_TM_FORMAT);
}