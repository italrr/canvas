#ifndef CANVAS_STD_LIBRARY_MATH_HPP
    #define CANVAS_STD_LIBRARY_MATH_HPP
    
   

    #include "../CV.hpp"

    namespace io {
        static std::string LIBNAME = "math";

        static void registerLibrary(std::shared_ptr<CV::Context> &ctx){
            auto lib = std::make_shared<CV::Context>(CV::Context());

            lib->set("sin", std::make_shared<CV::Function>(CV::Function({}, [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError("'sin': "+errormsg);
                    return std::make_shared<CV::Item>(CV::Item());
                }

                if(params.size() == 1){
                    
                }else{

                }

                return std::make_shared<CV::Item>(CV::Item());
            }, true))); 


            ctx->set("io", lib);
        }

    } 
    


#endif