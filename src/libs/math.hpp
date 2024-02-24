#ifndef CANVAS_STDLIB_MATH_HPP
    #define CANVAS_STDLIB_MATH_HPP

    #include "../cv.hpp"

    /*
        STANDARD LIBRARY `math`
        
        math includes buffer manipulation such as files, stdout, stdin, and stderr
    
    */

    #include <stdio.h>
    #include <iostream>
    #include <cmath>
    #include <random>

    #define CANVAS_STDLIB_MATH_PI 3.14159265358979323846

    namespace math {

        static std::string LIBNAME = "math";
        static std::mt19937 rng(std::time(nullptr));

        static void registerLibrary(std::shared_ptr<CV::Item> &ctx){
            auto lib = std::make_shared<CV::ProtoType>(CV::ProtoType());

            lib->registerProperty("sin", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":sin': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":sin': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":sin': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::sin(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));	


            lib->registerProperty("cos", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":cos': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":cos': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":cos': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::cos(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));	   


            lib->registerProperty("tan", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":tan': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":tan': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":tan': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::tan(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));	    



            lib->registerProperty("atan", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 2){
                        cursor->setError("operator '"+LIBNAME+":atan': expects at least 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 2){
                        cursor->setError("operator '"+LIBNAME+":atan': expects no more than 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":atan': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":atan': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                    

                    return CV::create(std::atan2(*static_cast<double*>(operands[0]->data), *static_cast<double*>(operands[1]->data)));	   
                }, {}
            )));	                     



            lib->registerProperty("round", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":round': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":round': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":round': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::round(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));


            lib->registerProperty("floor", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":floor': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":floor': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":floor': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::floor(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));            


            lib->registerProperty("ceil", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":ceil': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":ceil': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":ceil': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::ceil(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));     



            lib->registerProperty("abs", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":abs': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":abs': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":abs': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::abs(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));                                    


            lib->registerProperty("sqrt", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":sqrt': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":sqrt': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":sqrt': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(std::sqrt(*static_cast<double*>(operands[0]->data)));	   
                }, {}
            )));   

            lib->registerProperty("deg-rads", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    return CV::create(*static_cast<double*>(operands[0]->data) * 0.01745329251994329576923690768489);	   
                }, {}
            )));   


            lib->registerProperty("rads-degs", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 1){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': expects at least 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 1){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': expects no more than 1 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":deg-rads': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    

                    double rads = *static_cast<double*>(operands[0]->data);
                    return CV::create( (rads/CANVAS_STDLIB_MATH_PI*180.0) + (rads > 0.0 ? 0.0 : 360.0) );	   
                }, {}
            )));              


            lib->registerProperty("max", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 2){
                        cursor->setError("operator '"+LIBNAME+":max': expects at least 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 2){
                        cursor->setError("operator '"+LIBNAME+":max': expects no more than 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":max': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":max': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                    

                    return CV::create(std::max(*static_cast<double*>(operands[0]->data), *static_cast<double*>(operands[1]->data)));	   
                }, {}
            )));	   


            lib->registerProperty("pow", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 2){
                        cursor->setError("operator '"+LIBNAME+":pow': expects at least 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 2){
                        cursor->setError("operator '"+LIBNAME+":pow': expects no more than 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":pow': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":pow': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                    

                    return CV::create(std::pow(*static_cast<double*>(operands[0]->data), *static_cast<double*>(operands[1]->data)));	   
                }, {}
            )));                              

            lib->registerProperty("min", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 2){
                        cursor->setError("operator '"+LIBNAME+":min': expects at least 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 2){
                        cursor->setError("operator '"+LIBNAME+":min': expects no more than 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":min': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":min': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                    

                    return CV::create(std::min(*static_cast<double*>(operands[0]->data), *static_cast<double*>(operands[1]->data)));	   
                }, {}
            )));	                     

            lib->registerProperty("clamp", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 3){
                        cursor->setError("operator '"+LIBNAME+":clamp': expects at least 3 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 3){
                        cursor->setError("operator '"+LIBNAME+":clamp': expects no more than 3 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":clamp': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":clamp': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }    
                    if(operands[2]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":clamp': argument(2) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                                      

                    return CV::create(std::min(std::max(*static_cast<double*>(operands[0]->data), *static_cast<double*>(operands[1]->data)), *static_cast<double*>(operands[2]->data)));	   
                }, {}
            )));




            lib->registerProperty("rand-int", std::make_shared<CV::FunctionType>(CV::FunctionType([](const std::vector<std::shared_ptr<CV::Item>> &operands, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
                    if(operands.size() < 2){
                        cursor->setError("operator '"+LIBNAME+":max': expects at least 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands.size() > 2){
                        cursor->setError("operator '"+LIBNAME+":max': expects no more than 2 operand");
                        return std::make_shared<CV::Item>(CV::Item());						
                    }
                    if(operands[0]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":max': argument(0) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }
                    if(operands[1]->type != CV::ItemTypes::NUMBER){
                        cursor->setError("operator '"+LIBNAME+":max': argument(1) must be a NUMBER");
                        return std::make_shared<CV::Item>(CV::Item());	                        
                    }                    


                    auto min = *static_cast<double*>(operands[0]->data);
                    auto max = *static_cast<double*>(operands[1]->data);

                    std::uniform_int_distribution<int> uni(min,max);        
                    return CV::create(uni(rng));	   
                }, {}
            )));            	


            lib->registerProperty("pi", CV::create(CANVAS_STDLIB_MATH_PI));


            ctx->registerProperty(LIBNAME, std::static_pointer_cast<CV::Item>(lib));
        }   

    }


#endif