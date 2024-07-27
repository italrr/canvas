#include <iostream>
#include "../CV.hpp"

/*
    Here we should register all binary functions that are part of the standard
*/


void __CV_REGISTER_STANDARD_BITMAP_FUNCTIONS(std::shared_ptr<CV::Stack> &stack){

    stack->registerFunction("bm-create", [stack](std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
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

        auto format = std::static_pointer_cast<CV::StringType>(args[0])->get();
        auto width = std::static_pointer_cast<CV::NumberType>(args[1])->get();
        auto height = std::static_pointer_cast<CV::NumberType>(args[2])->get();
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
        std::vector<std::shared_ptr<CV::Item>> numbers;
        numbers.reserve(total);

        auto list = std::make_shared<CV::ListType>();
        list->build(total);
        ctx->store(list);

        for(int i = 0; i < total; ++i){
            auto n = std::make_shared<CV::NumberType>();
            n->set(0);
            numbers.push_back(n);
            auto nId = ctx->store(n);
            list->set(i, ctx->id, nId);
        }

        return std::static_pointer_cast<CV::Item>(list);
    });

}