#include <cmath>
#include <fstream>

#include "../Thirdparty/json11.hpp"
#include "../CV.hpp"

static inline bool __cv_json_is_not_fractional(CV_NUMBER d, CV_NUMBER epsilon = 1e-9){
    CV_NUMBER integral_part;
    CV_NUMBER fractional_part = std::modf(d, &integral_part);
    return std::fabs(fractional_part) < epsilon;
}

static std::shared_ptr<CV::Data> __cv_json_fail_num(const CV::ContextType &ctx){
    return std::static_pointer_cast<CV::Data>(ctx->buildNumber(0));
}

static std::shared_ptr<CV::Data> __cv_json_ok_num(const CV::ContextType &ctx){
    return std::static_pointer_cast<CV::Data>(ctx->buildNumber(1));
}

static std::shared_ptr<CV::Data> __cv_json_unwrap(const std::shared_ptr<CV::Data> &d){
    return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
}

static bool __cv_json_expect_exactly(
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

static bool __cv_json_expect_type(
    const std::string &name,
    const std::shared_ptr<CV::Data> &value,
    CV::DataType expected,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    auto v = __cv_json_unwrap(value);
    if(!v || v->type != expected){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Imperative '"+name+"' expected "+CV::DataTypeName(expected)+
            ", provided "+(v ? CV::DataTypeName(v->type) : std::string("NULL")),
            token
        );
        return false;
    }
    return true;
}

static json11::Json __cv_json_build_node(
    const std::string &name,
    const std::shared_ptr<CV::Data> &origin,
    const CV::TokenType &token,
    const CV::CursorType &cursor
){
    auto value = __cv_json_unwrap(origin);

    if(!value){
        cursor->setError(
            "Unhandled Type For JSON",
            "Imperative '"+name+"' was provided a null value",
            token
        );
        return json11::Json();
    }

    switch(value->type){
        case CV::DataType::STORE: {
            json11::Json::object obj;
            auto store = std::static_pointer_cast<CV::DataStore>(value);

            for(const auto &it : store->v){
                obj[it.first] = __cv_json_build_node(name, it.second, token, cursor);
                if(cursor->error){
                    return json11::Json();
                }
            }

            return obj;
        }

        case CV::DataType::LIST: {
            json11::Json::array obj;
            auto list = std::static_pointer_cast<CV::DataList>(value);

            for(int i = 0; i < static_cast<int>(list->v.size()); ++i){
                obj.push_back(__cv_json_build_node(name, list->v[i], token, cursor));
                if(cursor->error){
                    return json11::Json();
                }
            }

            return obj;
        }

        case CV::DataType::NUMBER: {
            auto v = std::static_pointer_cast<CV::DataNumber>(value)->v;
            return __cv_json_is_not_fractional(v)
                ? json11::Json(static_cast<int>(v))
                : json11::Json(v);
        }

        case CV::DataType::STRING: {
            return json11::Json(std::static_pointer_cast<CV::DataString>(value)->v);
        }

        case CV::DataType::NIL: {
            return json11::Json();
        }

        default: {
            cursor->setError(
                "Unhandled Type For JSON",
                "Imperative '"+name+"' was provided type '"+CV::DataTypeName(value->type)+"' which is not handled by this JSON library",
                token
            );
            return json11::Json();
        }
    }
}

static std::shared_ptr<CV::Data> __cv_json_unwrap_json(
    const std::string &name,
    const json11::Json &obj,
    const CV::ContextType &ctx,
    const CV::TokenType &token,
    const CV::CursorType &cursor
){
    switch(obj.type()){
        case json11::Json::ARRAY: {
            auto list = ctx->buildList();
            auto arr = obj.array_items();

            for(int i = 0; i < static_cast<int>(arr.size()); ++i){
                auto child = __cv_json_unwrap_json(name, arr.at(i), ctx, token, cursor);
                if(cursor->error){
                    return ctx->buildNil();
                }
                list->v.push_back(child);
            }

            return std::static_pointer_cast<CV::Data>(list);
        }

        case json11::Json::OBJECT: {
            auto store = ctx->buildStore();
            auto curr = obj.object_items();

            for(const auto &it : curr){
                auto child = __cv_json_unwrap_json(name, it.second, ctx, token, cursor);
                if(cursor->error){
                    return ctx->buildNil();
                }
                store->v[it.first] = child;
            }

            return std::static_pointer_cast<CV::Data>(store);
        }

        case json11::Json::NUMBER: {
            return std::static_pointer_cast<CV::Data>(ctx->buildNumber(obj.number_value()));
        }

        case json11::Json::BOOL: {
            return std::static_pointer_cast<CV::Data>(ctx->buildNumber(obj.bool_value() ? 1 : 0));
        }

        case json11::Json::STRING: {
            return std::static_pointer_cast<CV::Data>(ctx->buildString(obj.string_value()));
        }

        case json11::Json::NUL: {
            return ctx->buildNil();
        }

        default: {
            cursor->setError(
                "Unhandled Type For JSON",
                "Imperative '"+name+"' was provided JSON type '"+std::to_string(obj.type())+"' which is not handled by this JSON library",
                token
            );
            return ctx->buildNil();
        }
    }
}

static std::shared_ptr<CV::Data> __CV_STD_JSON_WRITE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "json:write";

    if(!__cv_json_expect_exactly(name, args, 2, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto subject = __cv_json_unwrap(args[0].second);
    if(!subject || (subject->type != CV::DataType::LIST && subject->type != CV::DataType::STORE)){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE",
            token
        );
        return __cv_json_fail_num(ctx);
    }

    auto filenameData = __cv_json_unwrap(args[1].second);
    if(!__cv_json_expect_type(name, filenameData, CV::DataType::STRING, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto filename = std::static_pointer_cast<CV::DataString>(filenameData)->v;

    auto obj = __cv_json_build_node(name, subject, token, cursor);
    if(cursor->error){
        return __cv_json_fail_num(ctx);
    }

    std::ofstream outFile(filename);
    outFile << json11::Json(obj).dump();
    outFile.close();

    return __cv_json_ok_num(ctx);
}

static std::shared_ptr<CV::Data> __CV_STD_JSON_DUMP(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "json:dump";

    if(!__cv_json_expect_exactly(name, args, 1, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto subject = __cv_json_unwrap(args[0].second);
    if(!subject || (subject->type != CV::DataType::LIST && subject->type != CV::DataType::STORE)){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE",
            token
        );
        return __cv_json_fail_num(ctx);
    }

    auto obj = __cv_json_build_node(name, subject, token, cursor);
    if(cursor->error){
        return __cv_json_fail_num(ctx);
    }

    return std::static_pointer_cast<CV::Data>(
        ctx->buildString(json11::Json(obj).dump())
    );
}

static std::shared_ptr<CV::Data> __CV_STD_JSON_PARSE_FILE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "json:load";

    if(!__cv_json_expect_exactly(name, args, 1, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto filenameData = __cv_json_unwrap(args[0].second);
    if(!__cv_json_expect_type(name, filenameData, CV::DataType::STRING, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto filename = std::static_pointer_cast<CV::DataString>(filenameData)->v;

    if(!CV::Tools::fileExists(filename)){
        cursor->setError(
            "File Not Found",
            "File '"+filename+"' does not exist",
            token
        );
        return __cv_json_fail_num(ctx);
    }

    auto source = CV::Tools::readFile(filename);

    std::string err;
    auto json = json11::Json::parse(source, err);
    if(!err.empty()){
        cursor->setError(
            "Bad JSON Input",
            "Imperative '"+name+"' was provided an invalid JSON input: '"+err+"'",
            token
        );
        return __cv_json_fail_num(ctx);
    }

    return __cv_json_unwrap_json(name, json, ctx, token, cursor);
}

static std::shared_ptr<CV::Data> __CV_STD_JSON_PARSE(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "json:parse";

    if(!__cv_json_expect_exactly(name, args, 1, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto bodyData = __cv_json_unwrap(args[0].second);
    if(!__cv_json_expect_type(name, bodyData, CV::DataType::STRING, cursor, token)){
        return __cv_json_fail_num(ctx);
    }

    auto body = std::static_pointer_cast<CV::DataString>(bodyData)->v;

    std::string err;
    auto json = json11::Json::parse(body, err);
    if(!err.empty()){
        cursor->setError(
            "Bad JSON Input",
            "Imperative '"+name+"' was provided an invalid JSON input: '"+err+"'",
            token
        );
        return __cv_json_fail_num(ctx);
    }

    return __cv_json_unwrap_json(name, json, ctx, token, cursor);
}

extern "C" void _CV_REGISTER_LIBRARY(
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    (void)cursor;

    ctx->registerFunction("json:write", {"subject", "filename"}, __CV_STD_JSON_WRITE);
    ctx->registerFunction("json:dump",  {"subject"}, __CV_STD_JSON_DUMP);
    ctx->registerFunction("json:load",  {"filename"}, __CV_STD_JSON_PARSE_FILE);
    ctx->registerFunction("json:parse", {"body"}, __CV_STD_JSON_PARSE);
}