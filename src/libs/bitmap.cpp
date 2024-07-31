#include <fstream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "3rdparty/stb_image.h"
#include "3rdparty/stb_image_write.h"

#include "../CV.hpp"

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    auto ns = stack->createNamespace("LibBitmap", "bm");

    stack->registerFunction(ns->id, "create", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 3){
            cursor->setError(name, "Expects exactly 3 arguments", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (Channels)", token.line);
            return ctx->buildNil();
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (WIDTH)", token.line);
            return ctx->buildNil();
        }        

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 2 must be a NUMBER (HEIGHT)", token.line);
            return ctx->buildNil();
        }          

        auto channels = std::static_pointer_cast<CV::NumberType>(args[0])->get();
        auto width = std::static_pointer_cast<CV::NumberType>(args[1])->get();
        auto height = std::static_pointer_cast<CV::NumberType>(args[2])->get();

        if(channels != 3 && channels != 4){
            cursor->setError(name, "Invalid number of channels ('"+std::to_string(channels)+")", token.line);
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


    stack->registerFunction(ns->id, "write-png", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        // bm-write array format fileformat filename

        if(args.size() != 5){
            cursor->setError(name, "Expects exactly 5 arguments", token.line);
            return ctx->buildNumber(0);
        }

        if(args[0]->type != CV::NaturalType::LIST){
            cursor->setError(name, "argument 0 must be a LIST (pixels)", token.line);
            return ctx->buildNumber(0);
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (channels)", token.line);
            return ctx->buildNumber(0);
        }    

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 2 must be a NUMBER (width)", token.line);
            return ctx->buildNumber(0);
        }  

        if(args[3]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 3 must be a NUMBER (height)", token.line);
            return ctx->buildNumber(0);
        }          

        if(args[4]->type != CV::NaturalType::STRING){
            cursor->setError(name, "argument 4 must be a STRING (filename)", token.line);
            return ctx->buildNumber(0);
        }               

        auto list = std::static_pointer_cast<CV::ListType>(args[0]);
        unsigned channels = std::static_pointer_cast<CV::NumberType>(args[1])->get();
        unsigned width = std::static_pointer_cast<CV::NumberType>(args[2])->get();
        unsigned height = std::static_pointer_cast<CV::NumberType>(args[3])->get();
        auto filename = std::static_pointer_cast<CV::StringType>(args[4])->get();

        if(list->size % channels != 0){
            cursor->setError(name, "Invalid number of channels ("+std::to_string(channels)+")", token.line);
            return ctx->buildNumber(0);        
        }


        unsigned char *bytes = new unsigned char[width * height * channels];

        for (int i = 0; i < width * height * channels; i += 1) {
            auto item = list->get(stack, i); // this might be just too slow
            bytes[i] = (*static_cast<__CV_DEFAULT_NUMBER_TYPE*>(std::static_pointer_cast<CV::NumberType>(item)->data)) * 255.0;
        }

        stbi_write_png(filename.c_str(), width, height, channels, bytes, width * channels);

        delete bytes;


        return ctx->buildNumber(1);
    });    

}