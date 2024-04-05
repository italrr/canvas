#ifndef CANVAS_STD_LIBRARY_IMAGE_HPP
    #define CANVAS_STD_LIBRARY_IMAGE_HPP
    
    #include <stdio.h>
    #include <fstream>
    #include <iostream>

    #include "../CV.hpp"

    namespace img {
        static std::string LIBNAME = "img";
        static std::mutex accessMutex;   

        static std::shared_ptr<CV::Context> createImageHandle(const std::string &format, __CV_NUMBER_NATIVE_TYPE w, __CV_NUMBER_NATIVE_TYPE h){
            auto ctx = std::make_shared<CV::Context>();
            ctx->set("format", std::make_shared<CV::String>(format));
            ctx->set("width", std::make_shared<CV::Number>(w));
            ctx->set("height", std::make_shared<CV::Number>(h));
            return ctx;
        }

        static void writePPM(std::shared_ptr<CV::Item> &img, const std::string &filename){
            auto ctx = std::static_pointer_cast<CV::Context>(img);
            int w = std::static_pointer_cast<CV::Number>(ctx->get("width"))->get();
            int h = std::static_pointer_cast<CV::Number>(ctx->get("height"))->get();
            auto data = std::static_pointer_cast<CV::List>(ctx->get("data"));
            std::ofstream out;
            out.open(filename, std::ios::out | std::ios::binary);
            out << "P6\n" << w << "\n" << h << "\n255\n";
            data->accessMutex.lock();
            for (int i = 0; i < w*h*3; i += 3) {
                uint8_t r = std::static_pointer_cast<CV::Number>(data->data[i+0])->n * 255.0;
                uint8_t g = std::static_pointer_cast<CV::Number>(data->data[i+1])->n * 255.0;
                uint8_t b = std::static_pointer_cast<CV::Number>(data->data[i+2])->n * 255.0;
                out.write((char*)&r, sizeof(uint8_t));
                out.write((char*)&g, sizeof(uint8_t));
                out.write((char*)&b, sizeof(uint8_t));
            }
            data->accessMutex.unlock();
            out.close();

        }
        void __writePNG(std::shared_ptr<CV::Item> &img, const std::string &filename);

        static const std::vector<std::string> IMAGE_FORMAT = {
            "PPM", "BMP", "PNG", "JPEG"
        };

        static const std::vector<std::string> PIXEL_FORMAT = {
            "RGBA", "RGB", "R"
        };

        static const std::unordered_map<std::string, __CV_NUMBER_NATIVE_TYPE> IMAGE_DETPH = { 
            {PIXEL_FORMAT[0], 4},
            {PIXEL_FORMAT[1], 3},
            {PIXEL_FORMAT[2], 1},
        };

        static void registerLibrary(std::shared_ptr<CV::Context> &ctx){
            auto lib = std::make_shared<CV::Context>(CV::Context());
            lib->copyable = false;
            lib->readOnly = true;
            lib->solid = true;



            lib->set("pixel-format", std::make_shared<CV::List>(CV::toList(PIXEL_FORMAT), false));
            lib->set("image-format", std::make_shared<CV::List>(CV::toList(IMAGE_FORMAT), false));            

               
             lib->set("write", std::make_shared<CV::Function>(std::vector<std::string>({"image", "format", "filename"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "write";
                CV::FunctionConstraints consts;
                consts.setMinParams(3);
                consts.setMaxParams(3);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = true;
                consts.setExpectedTypeAt(0, CV::ItemTypes::CONTEXT);
                consts.setExpectedTypeAt(1, CV::ItemTypes::STRING);
                consts.setExpectedTypeAt(2, CV::ItemTypes::STRING);

                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
                }
                auto img = params[0];
                auto type = std::static_pointer_cast<CV::String>(params[1]);
                auto filename = std::static_pointer_cast<CV::String>(params[2]);

                if(!consts.testAgainst(type, IMAGE_FORMAT, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
                }                
                auto format = type->get();
                // PPM
                if(format == IMAGE_FORMAT[0]){
                    writePPM(img, filename->get());
                }else
                // PNG
                if(format == IMAGE_FORMAT[2]){
                    __writePNG(img, filename->get());
                }
                
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
             }, false));       


             lib->set("set-pixel", std::make_shared<CV::Function>(std::vector<std::string>({"image", "position", "color"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "set-pixel";
                CV::FunctionConstraints consts;
                consts.setMinParams(3);
                consts.setMaxParams(3);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = true;
                consts.setExpectedTypeAt(0, CV::ItemTypes::CONTEXT);
                consts.setExpectedTypeAt(1, CV::ItemTypes::LIST);
                consts.setExpectedTypeAt(2, CV::ItemTypes::LIST);

                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
                }
                auto img = std::static_pointer_cast<CV::Context>(params[0]);
                auto position = std::static_pointer_cast<CV::List>(params[1]);
                auto color = std::static_pointer_cast<CV::List>(params[2]);

                auto format = std::static_pointer_cast<CV::String>(img->get("format"))->get();
                int channels = img::IMAGE_DETPH.find(format)->second;    

                int w = std::static_pointer_cast<CV::Number>(img->get("width"))->get();
                int h = std::static_pointer_cast<CV::Number>(img->get("height"))->get();


                if( !consts.testRange(position, 2, 2, errormsg) ||
                    !consts.testListUniformType(position, CV::ItemTypes::NUMBER, errormsg) ||
                    !consts.testRange(color, 3, channels, errormsg) ||
                    !consts.testListUniformType(color, CV::ItemTypes::NUMBER, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
                }                

                int x = std::static_pointer_cast<CV::Number>(position->get(0))->get();
                int y = std::static_pointer_cast<CV::Number>(position->get(1))->get();

                auto pixels = std::static_pointer_cast<CV::List>(img->get("data"));

                int index = w * y*channels + x*channels;
                
                if(x < 0 || x >= w || y < 0 || y >= h){
                    cursor->setError(LIBNAME+":"+name, "Out of range coordinates.");
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));                    
                }

                pixels->accessMutex.lock();
                for(int i = 0; i < color->size(); ++i){
                    pixels->data[index + i] = color->get(i);
                }

                pixels->accessMutex.unlock();
                
                
                
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
             }, false));  


            lib->set("create", std::make_shared<CV::Function>(std::vector<std::string>({"format", "width", "height"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "create";
                CV::FunctionConstraints consts;
                consts.setMinParams(3);
                consts.setMaxParams(3);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = true;
                consts.setExpectedTypeAt(0, CV::ItemTypes::STRING);
                consts.setExpectedTypeAt(1, CV::ItemTypes::NUMBER);
                consts.setExpectedTypeAt(2, CV::ItemTypes::NUMBER);

                __CV_NUMBER_NATIVE_TYPE min = 1;
                __CV_NUMBER_NATIVE_TYPE max = 1024*8;

                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }

                auto format = std::static_pointer_cast<CV::String>(params[0]);
                auto width = std::static_pointer_cast<CV::Number>(params[1]);
                auto height = std::static_pointer_cast<CV::Number>(params[2]);

                if( !consts.testAgainst(format, PIXEL_FORMAT, errormsg) ||
                    !consts.testRange(width, min, max, errormsg) ||
                    !consts.testRange(height, min, max, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }

                

                auto total = width->get() * height->get() * IMAGE_DETPH.find(format->get())->second;
                std::vector<std::shared_ptr<CV::Item>> numbers;
                numbers.reserve(total);
                auto n = CV::Tools::ticks();
                for(int i = 0; i < total; ++i){
                    numbers.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0)));
                }

                auto handle = createImageHandle(format->get(), width->get(), height->get());
                n = CV::Tools::ticks();
                handle->set("data", std::make_shared<CV::List>(numbers, true));

                return std::static_pointer_cast<CV::Item>(handle);
            }, false)); 


            ctx->set(LIBNAME, lib);
        }

    } 
    


#endif