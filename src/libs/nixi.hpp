#ifndef CANVAS_STDLIB_NIXI_HPP
    #define CANVAS_STDLIB_NIXI_HPP

    #include "../cv.hpp"


    /*
        STANDARD LIBRARY `NIXI`

        NIXI is a simple 2D drawing library that uses SDL2 and OpenGL 1.1 (or 1.0?) for
        rendering.
    
    */

   namespace io {

        static void registerLibrary(std::shared_ptr<CV::Item> &ctx){
            auto lib = std::make_shared<CV::ProtoType>(CV::ProtoType());

            lib->registerProperty("window", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));

            lib->registerProperty("close", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));          

            lib->registerProperty("pixel", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));

           lib->registerProperty("fillrect", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));

           lib->registerProperty("rect", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));      

           lib->registerProperty("set-color", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    return CV::create(1);
                }, {}
            )));                                             


            ctx->registerProperty("nixi", std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif