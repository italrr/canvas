#include <fstream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "3rdparty/stb_image.h"
#include "3rdparty/stb_image_write.h"

#include "../CV.hpp"

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    auto nsId = stack->getNamespaceId("LibBitmap");
    auto ns = stack->namespaces[nsId];

    stack->registerFunction(ns->id, "write-png", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){

        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 arguments", token.line);
            return ctx->buildNumber(0);
        }

        if(args[0]->type != CV::NaturalType::STORE){
            cursor->setError(name, "argument 0 must be a STORE (bm object)", token.line);
            return ctx->buildNumber(0);
        }

        if(args[1]->type != CV::NaturalType::STRING){
            cursor->setError(name, "argument 1 must be a STORE (filename)", token.line);
            return ctx->buildNumber(0);
        }        

        auto store = std::static_pointer_cast<CV::StoreType>(args[0]);
        auto filename = std::static_pointer_cast<CV::StringType>(args[1])->get();


        auto widthV = store->getId("width");
        auto width = static_cast<unsigned>(std::static_pointer_cast<CV::NumberType>(stack->contexts[widthV.ctx]->data[widthV.id])->get());

        auto heightV = store->getId("height");
        auto height = static_cast<unsigned>(std::static_pointer_cast<CV::NumberType>(stack->contexts[heightV.ctx]->data[heightV.id])->get());

        auto channelsV = store->getId("channels");
        auto channels = static_cast<unsigned>(std::static_pointer_cast<CV::NumberType>(stack->contexts[channelsV.ctx]->data[channelsV.id])->get());


        auto pixelsV = store->getId("pixels");
        auto pixels = std::static_pointer_cast<CV::ListType>(stack->contexts[pixelsV.ctx]->data[pixelsV.id]);          

        unsigned char *bytes = new unsigned char[width * height * channels];

        for (int i = 0; i < width * height * channels; i += 1) {
            auto item = pixels->get(stack, i); // this might be just too slow
            bytes[i] = (*static_cast<__CV_DEFAULT_NUMBER_TYPE*>(std::static_pointer_cast<CV::NumberType>(item)->data)) * 255.0;
        }

        stbi_write_png(filename.c_str(), width, height, channels, bytes, width * channels);

        delete bytes;


        return ctx->buildNumber(1);
    });    

}