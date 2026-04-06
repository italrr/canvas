#include <iostream>
#include <string>

#include "../CV.hpp"

static inline void ___WRITE_STDOUT(const std::string &v){
    std::cout << v;
}

static inline void ___WRITE_STDERR(const std::string &v){
    std::cerr << v;
}

static inline void ___GET_STDIN(std::string &v){
    std::cin >> v;
}

static std::shared_ptr<CV::Data> __cv_io_unwrap(const std::shared_ptr<CV::Data> &d){
    return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
}

static bool __cv_io_expect_at_least(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) < n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "'"+name+"' expects at least ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static bool __cv_io_expect_exactly(
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

static bool __cv_io_expect_no_named_args(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    for(int i = 0; i < static_cast<int>(args.size()); ++i){
        if(!args[i].first.empty()){
            cursor->setError(
                CV_ERROR_MSG_MISUSED_FUNCTION,
                "Function '"+name+"' does not accept named arguments",
                token
            );
            return false;
        }
    }
    return true;
}

static std::shared_ptr<CV::Data> __CV_STD_IO_OUT(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "io:out";

    if(!__cv_io_expect_at_least(name, args, 1, cursor, token)){
        return ctx->buildNil();
    }

    // Re-enable this if you want io:* to reject named args.
    // if(!__cv_io_expect_no_named_args(name, args, cursor, token)){
    //     return ctx->buildNil();
    // }

    std::string out;

    for(int i = 0; i < static_cast<int>(args.size()); ++i){
        auto quant = __cv_io_unwrap(args[i].second);

        if(!quant){
            cursor->setError(
                "Invalid Operand",
                "Function '"+name+"' received a null operand",
                token
            );
            return ctx->buildNil();
        }

        if(quant->type == CV::DataType::STRING){
            out += std::static_pointer_cast<CV::DataString>(quant)->v;
        }else{
            out += CV::DataToText(quant);
        }
    }

    ___WRITE_STDOUT(out);
    return ctx->buildNil();
}

static std::shared_ptr<CV::Data> __CV_STD_IO_ERR(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "io:err";

    if(!__cv_io_expect_at_least(name, args, 1, cursor, token)){
        return ctx->buildNil();
    }

    // Re-enable this if you want io:* to reject named args.
    // if(!__cv_io_expect_no_named_args(name, args, cursor, token)){
    //     return ctx->buildNil();
    // }

    std::string out;

    for(int i = 0; i < static_cast<int>(args.size()); ++i){
        auto quant = __cv_io_unwrap(args[i].second);

        if(!quant){
            cursor->setError(
                "Invalid Operand",
                "Function '"+name+"' received a null operand",
                token
            );
            return ctx->buildNil();
        }

        if(quant->type == CV::DataType::STRING){
            out += std::static_pointer_cast<CV::DataString>(quant)->v;
        }else{
            out += CV::DataToText(quant);
        }
    }

    ___WRITE_STDERR(out);
    return ctx->buildNil();
}

static std::shared_ptr<CV::Data> __CV_STD_IO_IN(
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    const std::string name = "io:in";

    if(!__cv_io_expect_exactly(name, args, 0, cursor, token)){
        return ctx->buildNil();
    }

    // Re-enable this if you want io:* to reject named args.
    // if(!__cv_io_expect_no_named_args(name, args, cursor, token)){
    //     return ctx->buildNil();
    // }

    std::string input;
    ___GET_STDIN(input);

    return std::static_pointer_cast<CV::Data>(
        ctx->buildString(input)
    );
}

extern "C" void _CV_REGISTER_LIBRARY(
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    (void)cursor;

    ctx->registerFunction("io:out", __CV_STD_IO_OUT);
    ctx->registerFunction("io:err", __CV_STD_IO_ERR);
    ctx->registerFunction("io:in", std::vector<std::string>{}, __CV_STD_IO_IN);
}