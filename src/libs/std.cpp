#include <iostream>
#include "../CV.hpp"

/*
    Here we should register all binary functions that are part of the standard:
    - stdout, stdin, stderr
    - fopen, fclose, fwrite, fread
    - exit/quit
*/

static void ___WRITE_STDOUT(const std::string &v){
    int n = v.size();
    std::cout << v;
}  

static void ___WRITE_STDERR(const std::string &v){
    int n = v.size();
    std::cerr << v;
}  

static void ___GET_STDIN(std::string &v){
    std::cin >> v;
}  

void __CV_REGISTER_STANDARD_BINARY_FUNCTIONS(std::shared_ptr<CV::Context> &topCtx, std::shared_ptr<CV::Stack> &stack){

    auto ns = stack->createNamespace(topCtx, "Standard IO Library", "io");

    stack->registerFunction(ns->id, "out", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string out = "";
        for(int i = 0; i < args.size(); ++i){
            auto item = args[i];
            if(item->type != CV::NaturalType::STRING){
                out += CV::ItemToText(stack, item.get());
            }else{
                out += std::static_pointer_cast<CV::StringType>(item)->get();
            }
        }
        ___WRITE_STDOUT(out);
        return ctx->buildNil();
    });


    stack->registerFunction(ns->id, "err", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string out = "";
        for(int i = 0; i < args.size(); ++i){
            auto item = args[i];
            if(item->type != CV::NaturalType::STRING){
                out += CV::ItemToText(stack, item.get());
            }else{
                out += std::static_pointer_cast<CV::StringType>(item)->get();
            }
        }
        ___WRITE_STDERR(out);
        return ctx->buildNil();
    });  

    stack->registerFunction(ns->id, "in", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string input;
        ___GET_STDIN(input);
        auto data = std::make_shared<CV::StringType>();
        data->set(input);
        ctx->store(data);
        return std::static_pointer_cast<CV::Item>(data);
    });          

}