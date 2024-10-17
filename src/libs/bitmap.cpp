#include <fstream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "3rdparty/stb_image.h"
#include "3rdparty/stb_image_write.h"

#include "../CV.hpp"

#include <iostream>
extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    stack->registerFunction("bm:write-png", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){

        if(args.size() != 4){
            cursor->setError(name, "Expects exactly 4 arguments: bm-write-png [PIXELS] [WIDTH HEIGHT] CHANNELS FILENAME", token.line);
            return ctx->buildNumber(stack, 0);
        }

        if(args[0]->type != CV::NaturalType::LIST){
            cursor->setError(name, "argument 0 must be a LIST: PIXELS", token.line);
            return ctx->buildNumber(stack, 0);
        }        

        if(args[1]->type != CV::NaturalType::LIST){
            cursor->setError(name, "argument 1 must be a LIST [WIDTH HEIGHT]", token.line);
            return ctx->buildNumber(stack, 0);
        }

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 2 must be a NUMBER: CHANNELS", token.line);
            return ctx->buildNumber(stack, 0);
        }

        if(args[3]->type != CV::NaturalType::STRING){
            cursor->setError(name, "argument 3 must be a STRING: FILENAME/PATH", token.line);
            return ctx->buildNumber(stack, 0);
        }        

        
        auto pixels = std::static_pointer_cast<CV::ListType>(args[0]);      
        auto size = std::static_pointer_cast<CV::ListType>(args[1]);
        auto channels = static_cast<int>(std::static_pointer_cast<CV::NumberType>(args[2])->get());
        auto filename = std::static_pointer_cast<CV::StringType>(args[3])->get();

        if(size->size != 2 || size->get(stack, 0)->type != CV::NaturalType::NUMBER || size->get(stack, 1)->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a LIST with exactly 2 numerical members [WIDTH HEIGHT]", token.line);
            return ctx->buildNumber(stack, 0);
        }

        auto width = static_cast<int>(std::static_pointer_cast<CV::NumberType>(size->get(stack, 0))->get());
        auto height = static_cast<int>(std::static_pointer_cast<CV::NumberType>(size->get(stack, 1))->get());

        unsigned char *bytes = new unsigned char[width * height * channels];

        for (int i = 0; i < width * height * channels; i += 1) {
            auto item = pixels->get(stack, i); // this might be just too slow
            bytes[i] = (*static_cast<__CV_DEFAULT_NUMBER_TYPE*>(std::static_pointer_cast<CV::NumberType>(item)->data)) * 255.0;
        }

        stbi_write_png(filename.c_str(), width, height, channels, bytes, width * channels);

        delete bytes;


        return ctx->buildNumber(stack, 1);
    });    

}