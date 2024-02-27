#ifndef CANVAS_STDLIB_TIME_HPP
    #define CANVAS_STDLIB_TIME_HPP

    #include <chrono>
    #include <stdio.h>
    #include <iostream>

    #include "../cv.hpp"

    /*
        STANDARD LIBRARY `time`
        
        time includes time features such as epoch and ticks
    
    */

    namespace timelib {

        static std::string LIBNAME = "tm";

        static void registerLibrary(std::shared_ptr<CV::Item> &ctx){
            auto lib = std::make_shared<CV::ProtoType>(CV::ProtoType());

            lib->registerProperty("epoch", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    auto t = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    return CV::create((double)t);
                }, {}
            )));	

            ctx->registerProperty(LIBNAME, std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif