#include <cmath>
#include <random>
#include <ctime>
#include <mutex>

#include "../CV.hpp"

static const CV_NUMBER CANVAS_STDLIB_MATH_PI = 3.14159265358979323846;
static const std::string LIBNAME = "math";
static std::mt19937 rng(std::time(nullptr));
static std::mutex accessMutex;

static std::shared_ptr<CV::Data> __cv_math_unwrap(const std::shared_ptr<CV::Data> &d){
    return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
}

static bool __cv_math_expect_min_params(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) < n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "Function '"+LIBNAME+":"+name+"' expects at least ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static bool __cv_math_expect_exact_params(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) != n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "Function '"+LIBNAME+":"+name+"' expects exactly ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static bool __cv_math_expect_all_numbers(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    for(int i = 0; i < static_cast<int>(args.size()); ++i){
        auto v = __cv_math_unwrap(args[i].second);
        if(!v || v->type != CV::DataType::NUMBER){
            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function '"+LIBNAME+":"+name+"' expects NUMBER operands only",
                token
            );
            return false;
        }
    }
    return true;
}

static std::shared_ptr<CV::Data> __cv_math_variadic_number_map(
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor,
    const CV::TokenType &token,
    const std::function<CV_NUMBER(CV_NUMBER)> &fn
){
    if(!__cv_math_expect_min_params(name, args, 1, cursor, token)){
        return ctx->buildNil();
    }

    if(!__cv_math_expect_all_numbers(name, args, cursor, token)){
        return ctx->buildNil();
    }

    if(args.size() == 1){
        auto n = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v;
        return std::static_pointer_cast<CV::Data>(ctx->buildNumber(fn(n)));
    }

    auto out = ctx->buildList();
    for(int i = 0; i < static_cast<int>(args.size()); ++i){
        auto n = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[i].second))->v;
        out->v.push_back(std::static_pointer_cast<CV::Data>(ctx->buildNumber(fn(n))));
    }
    return std::static_pointer_cast<CV::Data>(out);
}

extern "C" void _CV_REGISTER_LIBRARY(
    const CV::ContextType &lib,
    const CV::CursorType &cursor
){
    (void)cursor;

    lib->registerFunction(LIBNAME+":sin",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("sin", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::sin(n); });
        }
    );

    lib->registerFunction(LIBNAME+":cos",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("cos", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::cos(n); });
        }
    );

    lib->registerFunction(LIBNAME+":tan",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("tan", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::tan(n); });
        }
    );

    lib->registerFunction(LIBNAME+":atan", {"y", "x"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("atan", args, 2, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("atan", args, cursor, token)){
                return fctx->buildNil();
            }

            auto y = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v;
            auto x = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v;
            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(std::atan2(y, x)));
        }
    );

    lib->registerFunction(LIBNAME+":round",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("round", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::round(n); });
        }
    );

    lib->registerFunction(LIBNAME+":floor",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("floor", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::floor(n); });
        }
    );

    lib->registerFunction(LIBNAME+":ceil",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("ceil", args, fctx, cursor, token,
                [](CV_NUMBER n){ return std::ceil(n); });
        }
    );

    lib->registerFunction(LIBNAME+":deg-rads",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("deg-rads", args, fctx, cursor, token,
                [](CV_NUMBER n){ return n * (CANVAS_STDLIB_MATH_PI / 180.0); });
        }
    );

    lib->registerFunction(LIBNAME+":rads-degs",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            return __cv_math_variadic_number_map("rads-degs", args, fctx, cursor, token,
                [](CV_NUMBER n){
                    CV_NUMBER deg = n * (180.0 / CANVAS_STDLIB_MATH_PI);
                    return deg >= 0.0 ? deg : deg + 360.0;
                });
        }
    );

    lib->registerFunction(LIBNAME+":max", {"a", "b"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("max", args, 2, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("max", args, cursor, token)){
                return fctx->buildNil();
            }

            auto a = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v;
            auto b = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v;
            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(std::max(a, b)));
        }
    );

    lib->registerFunction(LIBNAME+":min", {"a", "b"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("min", args, 2, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("min", args, cursor, token)){
                return fctx->buildNil();
            }

            auto a = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v;
            auto b = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v;
            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(std::min(a, b)));
        }
    );

    lib->registerFunction(LIBNAME+":clamp", {"n", "min", "max"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("clamp", args, 3, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("clamp", args, cursor, token)){
                return fctx->buildNil();
            }

            auto n   = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v;
            auto min = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v;
            auto max = std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[2].second))->v;
            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(std::min(std::max(n, min), max)));
        }
    );

    lib->registerFunction(LIBNAME+":rng", {"min", "max"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("rng", args, 2, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("rng", args, cursor, token)){
                return fctx->buildNil();
            }

            auto a = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v);
            auto b = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v);

            if(a > b){
                std::swap(a, b);
            }

            std::lock_guard<std::mutex> lock(accessMutex);
            std::uniform_int_distribution<int> uni(a, b);
            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(uni(rng)));
        }
    );

    lib->registerFunction(LIBNAME+":mod", {"x", "y"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const CV::ContextType &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            if(!__cv_math_expect_exact_params("mod", args, 2, cursor, token)){
                return fctx->buildNil();
            }
            if(!__cv_math_expect_all_numbers("mod", args, cursor, token)){
                return fctx->buildNil();
            }

            auto x = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[0].second))->v);
            auto y = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(__cv_math_unwrap(args[1].second))->v);

            if(y == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "Function '"+LIBNAME+":mod' division by zero",
                    token
                );
                return fctx->buildNil();
            }

            return std::static_pointer_cast<CV::Data>(fctx->buildNumber(x % y));
        }
    );

    lib->data[LIBNAME+":pi"] = std::static_pointer_cast<CV::Data>(
        lib->buildNumber(CANVAS_STDLIB_MATH_PI)
    );
}
