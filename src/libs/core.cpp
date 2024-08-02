#include <thread>

#include "../CV.hpp"

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    auto nsId = stack->getNamespaceId("LibCore");
    auto ns = stack->namespaces[nsId];

    stack->registerFunction(ns->id, "exit", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }        

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "Argument 0 must be a NUMBER", token.line);
            return ctx->buildNil();            
        }

        auto n = static_cast<int>( std::static_pointer_cast<CV::NumberType>(args[0])->get() );

        std::exit(n);

        return ctx->buildNil(); // shouldn't happen
    });      

    stack->registerFunction(ns->id, "sleep", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }        

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "Argument 0 must be a NUMBER", token.line);
            return ctx->buildNil();            
        }

        auto n = static_cast<uint64_t>( std::static_pointer_cast<CV::NumberType>(args[0])->get() );

        std::this_thread::sleep_for(std::chrono::milliseconds(n));

        return ctx->buildNil(); // shouldn't happen
    });      
}