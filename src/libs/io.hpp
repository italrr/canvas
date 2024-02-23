#ifndef CANVAS_STDLIB_IO_HPP
    #define CANVAS_STDLIB_IO_HPP

    #include "../cv.hpp"

    /*
        STANDARD LIBRARY `IO`
        
        IO includes buffer manipulation such as files, stdout, stdin, and stderr
    
    */

    #include <stdio.h>
    #include <iostream>

    static void ___WRITE_STDOUT(const std::string &v){
        int n = v.size();
        for(int i = 0; i < v.size(); ++i){
            putchar(v[i]);
        }
    }

    namespace io {

        static void registerLibrary(std::shared_ptr<CV::Item> &ctx){
            auto lib = std::make_shared<CV::ProtoType>(CV::ProtoType());

            lib->registerProperty("out", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator 'io:out': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }

                    for(int i = 0; i < operands.size(); ++i){
                        ___WRITE_STDOUT(operands[i]->str());
                    }

                    return CV::create(1);
                }, {}
            )));	

            ctx->registerProperty("io", std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif