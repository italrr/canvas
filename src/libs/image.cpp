#include <fstream>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "3rdparty/stb_image.h"
#include "3rdparty/stb_image_write.h"

#include "../CV.hpp"
#include "image.hpp"

namespace img {
    void __writePNG(std::shared_ptr<CV::Item> &img, const std::string &filename){
        auto ctx = std::static_pointer_cast<CV::Context>(img);
        int w = std::static_pointer_cast<CV::Number>(ctx->get("width"))->get();
        int h = std::static_pointer_cast<CV::Number>(ctx->get("height"))->get();
        auto data = std::static_pointer_cast<CV::List>(ctx->get("data"));
        auto format = std::static_pointer_cast<CV::String>(ctx->get("format"))->get();
        int channels = img::IMAGE_DETPH.find(format)->second;


        data->accessMutex.lock();

        unsigned char *bytes = new unsigned char[w * h * channels];

        for (int i = 0; i < w*h*channels; i += 1) {
            bytes[i] = std::static_pointer_cast<CV::Number>(data->data[i])->n;
        }

        stbi_write_png(filename.c_str(), w, h, channels, bytes, w * channels);

        delete bytes;

        data->accessMutex.unlock();    
    }
}