#ifndef CANVAS_STD_LIBRARY_IO_HPP
    #define CANVAS_STD_LIBRARY_IO_HPP
    
    #include <stdio.h>

    #include "../CV.hpp"

    namespace io {
        static std::string LIBNAME = "io";
        static std::mutex accessMutex; 

        static void ___WRITE_STDOUT(const std::string &v){
            int n = v.size();
            accessMutex.lock();
            for(int i = 0; i < v.size(); ++i){
                putchar(v[i]);
            }
            accessMutex.unlock();
    }        

        static void registerLibrary(std::shared_ptr<CV::Context> &ctx){
            auto lib = std::make_shared<CV::Context>(CV::Context());
            lib->copyable = false;
            lib->readOnly = true;

            lib->set("out", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = true;

                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError("'"+LIBNAME+":out': "+errormsg);
                    return std::make_shared<CV::Item>(CV::Item());
                }

                for(int i = 0; i < params.size(); ++i){
                    if(params[i]->type == CV::ItemTypes::STRING){
                        auto str = std::static_pointer_cast<CV::String>(params[i]);
                        ___WRITE_STDOUT(str->get());
                    }else{
                        ___WRITE_STDOUT(CV::ItemToText(params[i].get()));
                    }
                }

                return std::make_shared<CV::Item>(CV::Item());
            }, true)); 


            ctx->set("io", lib);
        }

    } 
    


#endif