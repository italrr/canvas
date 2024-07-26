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

void __CV_REGISTER_STANDARD_BINARY_FUNCTIONS(std::shared_ptr<CV::Stack> &stack){

    stack->registerFunction("io-out", [stack](std::vector<CV::Item*> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string out = "";
        for(int i = 0; i < args.size(); ++i){
            auto item = args[i];
            if(item->type != CV::NaturalType::STRING){
                out += CV::ItemToText(stack, item);
            }else{
                out += static_cast<CV::StringType*>(item)->get();
            }
        }
        ___WRITE_STDOUT(out);
        return ctx->buildNil();
    });


    stack->registerFunction("io-err", [stack](std::vector<CV::Item*> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string out = "";
        for(int i = 0; i < args.size(); ++i){
            auto item = args[i];
            if(item->type != CV::NaturalType::STRING){
                out += CV::ItemToText(stack, item);
            }else{
                out += static_cast<CV::StringType*>(item)->get();
            }
        }
        ___WRITE_STDERR(out);
        return ctx->buildNil();
    });  

    stack->registerFunction("io-in", [stack](std::vector<CV::Item*> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        std::string input;
        ___GET_STDIN(input);
        auto data = new CV::StringType();
        data->set(input);
        ctx->store(data);
        return static_cast<CV::Item*>(data);
    });          

}