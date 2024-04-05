#ifndef CANVAS_STD_LIBRARY_MATH_HPP
    #define CANVAS_STD_LIBRARY_MATH_HPP
    
    #include <cmath>
    #include <random>

    #include "../CV.hpp"

    namespace math {
        static const __CV_NUMBER_NATIVE_TYPE CANVAS_STDLIB_MATH_PI = 3.14159265358979323846;
        static const std::string LIBNAME = "math";
        static std::mt19937 rng(std::time(nullptr));
        static std::mutex accessMutex; 


        static void registerLibrary(std::shared_ptr<CV::Context> &ctx){
            auto lib = std::make_shared<CV::Context>(CV::Context());
            lib->copyable = false;
            lib->readOnly = true;
            lib->solid = true;

            lib->set("sin", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "sin";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::sin(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::sin(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true)); 


            lib->set("cos", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "cos";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::cos(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::cos(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));      


            lib->set("tan", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "tan";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::tan(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::tan(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));                     

            lib->set("atan", std::make_shared<CV::Function>(std::vector<std::string>({"y", "x"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "atan";
                CV::FunctionConstraints consts;
                consts.setMinParams(2);
                consts.setMaxParams(2);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto y = std::static_pointer_cast<CV::Number>(params[0])->get();
                auto x = std::static_pointer_cast<CV::Number>(params[1])->get();
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::atan2(y, x)));
            }, false));                     


            lib->set("round", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "round";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::round(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::round(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));    



            lib->set("floor", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "floor";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::floor(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::floor(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));    


            lib->set("ceil", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "ceil";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::ceil(std::static_pointer_cast<CV::Number>(params[0])->get());
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::ceil(std::static_pointer_cast<CV::Number>(params[i])->get());
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));      


            lib->set("deg-rads", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "deg-rads";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::static_pointer_cast<CV::Number>(params[0])->get() * 0.01745329251994329576923690768489;
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::static_pointer_cast<CV::Number>(params[i])->get() * 0.01745329251994329576923690768489;
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));          


            lib->set("rads-degs", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "rads-degs";
                CV::FunctionConstraints consts;
                consts.setMinParams(1);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                if(params.size() == 1){
                    auto n = std::static_pointer_cast<CV::Number>(params[0])->get() * 0.01745329251994329576923690768489;
                    n = (n/CANVAS_STDLIB_MATH_PI*180.0) + (n > 0.0 ? 0.0 : 360.0);
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n));
                }else{
                    std::vector<std::shared_ptr<CV::Item>> results;
                    for(int i = 0; i < params.size(); ++i){
                        auto n = std::static_pointer_cast<CV::Number>(params[0])->get() * 0.01745329251994329576923690768489;
                        n = (n/CANVAS_STDLIB_MATH_PI*180.0) + (n > 0.0 ? 0.0 : 360.0);
                        results.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(n)));
                    }
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results, false));
                }
                return std::make_shared<CV::Item>();
            }, true));          



            lib->set("max", std::make_shared<CV::Function>(std::vector<std::string>({"a", "b"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "max";
                CV::FunctionConstraints consts;
                consts.setMinParams(2);
                consts.setMaxParams(2);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto y = std::static_pointer_cast<CV::Number>(params[0])->get();
                auto x = std::static_pointer_cast<CV::Number>(params[1])->get();
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::max(y, x)));
            }, false));       


            lib->set("min", std::make_shared<CV::Function>(std::vector<std::string>({"a", "b"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "min";
                CV::FunctionConstraints consts;
                consts.setMinParams(2);
                consts.setMaxParams(2);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto y = std::static_pointer_cast<CV::Number>(params[0])->get();
                auto x = std::static_pointer_cast<CV::Number>(params[1])->get();
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::max(y, x)));
            }, false)); 


            lib->set("clamp", std::make_shared<CV::Function>(std::vector<std::string>({"n", "min", "max"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "clamp";
                CV::FunctionConstraints consts;
                consts.setMinParams(3);
                consts.setMaxParams(3);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto n = std::static_pointer_cast<CV::Number>(params[0])->get();
                auto min = std::static_pointer_cast<CV::Number>(params[1])->get();
                auto max = std::static_pointer_cast<CV::Number>(params[2])->get();
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::min(std::max(n, min), max)));
            }, false));                                                            


            lib->set("rng", std::make_shared<CV::Function>(std::vector<std::string>({"min", "max"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "rng";
                CV::FunctionConstraints consts;
                consts.setMinParams(2);
                consts.setMaxParams(2);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto x = std::static_pointer_cast<CV::Number>(params[0])->get();
                auto y = std::static_pointer_cast<CV::Number>(params[1])->get();
                std::uniform_int_distribution<int> uni(x, y);  
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(uni(rng))); 
            }, false));


            lib->set("mod", std::make_shared<CV::Function>(std::vector<std::string>({"x", "y"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
                static const std::string name = "mod";
                CV::FunctionConstraints consts;
                consts.setMinParams(2);
                consts.setMaxParams(2);
                consts.allowNil = false;
                consts.allowMisMatchingTypes = false;
                consts.setExpectType(CV::ItemTypes::NUMBER);
                std::string errormsg;
                if(!consts.test(params, errormsg)){
                    cursor->setError(LIBNAME+":"+name, errormsg);
                    return std::make_shared<CV::Item>();
                }
                auto x = (int)std::static_pointer_cast<CV::Number>(params[0])->get();
                auto y = (int)std::static_pointer_cast<CV::Number>(params[1])->get(); 
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( x % y )); 
            }, false));              


            ctx->set(LIBNAME, lib);
        }

    } 
    


#endif