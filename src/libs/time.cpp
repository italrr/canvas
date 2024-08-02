#include <chrono>
#include "../CV.hpp"

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    auto nsId = stack->getNamespaceId("LibTime");
    auto ns = stack->namespaces[nsId];

    stack->registerFunction(ns->id, "epoch", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        if(args.size() != 0){
            cursor->setError(name, "Expects no arguments", token.line);
            return ctx->buildNil();
        }
        double t = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return ctx->buildNumber(t);
    });    

    stack->registerFunction(ns->id, "epoch-secs", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        if(args.size() != 0){
            cursor->setError(name, "Expects no arguments", token.line);
            return ctx->buildNil();
        }

        auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return ctx->buildNumber(t);
    });    
}