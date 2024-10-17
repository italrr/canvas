#include <iostream>
#include "../CV.hpp"

/*
    Here we should register all binary functions that are part of the standard:
    - stdout, stdin, stderr
    - fopen, fclose, fwrite, fread
    - exit/quit
*/

static inline void ___WRITE_STDOUT(const std::string &v){
    int n = v.size();
    std::cout << v;
}  

static inline void ___WRITE_STDERR(const std::string &v){
    int n = v.size();
    std::cerr << v;
}  

static inline void ___GET_STDIN(std::string &v){
    std::cin >> v;
}  

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){
  

    stack->registerFunction("io:out", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
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
        return ctx->buildNil(stack);
    });


    stack->registerFunction("io:err", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
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
        return ctx->buildNil(stack);
    });  

    stack->registerFunction("io:in", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string input;
        ___GET_STDIN(input);
        auto data = std::make_shared<CV::StringType>();
        data->set(input);
        ctx->store(stack, data);
        return std::static_pointer_cast<CV::Item>(data);
    });      

    stack->registerFunction("io:str-to-number", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        if(args.size() != 1 || args[0]->type != CV::NaturalType::STRING){
            cursor->setError(name, "Expects exactly 1 arguments: STRING", token.line);
            return ctx->buildNumber(stack, 0);
        }

        auto v = std::stod(std::static_pointer_cast<CV::StringType>(args[0])->get());

        return ctx->buildNumber(stack, v);
    });            

}