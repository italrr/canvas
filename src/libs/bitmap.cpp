#include <iostream>
#include "../CV.hpp"

/*
    Here we should register all binary functions that are part of the standard
*/


void __CV_REGISTER_STANDARD_BITMAP_FUNCTIONS(std::shared_ptr<CV::Stack> &stack){

    stack->registerFunction("bm-create", [stack](std::vector<CV::Item*> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() < 2){
            cursor->setError("bm-create", "Expects exactly 3 arguments", 0);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::STRING){
            cursor->setError("bm-create", "argument 0 must be a STRING (Format)", 0);
            return ctx->buildNil();
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError("bm-create", "argument 1 must be a NUMBER (WIDTH)", 0);
            return ctx->buildNil();
        }        

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError("bm-create", "argument 2 must be a NUMBER (HEIGHT)", 0);
            return ctx->buildNil();
        }          

        auto format = static_cast<CV::StringType*>(args[0])->get();
        auto width = static_cast<CV::NumberType*>(args[1])->get();
        auto height = static_cast<CV::NumberType*>(args[2])->get();
        unsigned channels;
        
        if(format == "RGB"){
            channels = 3;
        }else
        if(format == "RGBA"){
            channels = 4;
        }else{
            cursor->setError("bm-create", "Invalid format '"+format+"'", 0);
            return ctx->buildNil();            
        }

        auto total = width * height * channels;
        std::vector<CV::Item*> numbers;
        numbers.reserve(total);

        auto list = new CV::ListType();
        list->build(total);

        for(int i = 0; i < total; ++i){
            auto n = new CV::NumberType();
            n->set(0);
            numbers.push_back(n);
            auto nId = ctx->store(n);
            list->set(i, ctx->id, nId);
        }

        return static_cast<CV::Item*>(list);
    });

}