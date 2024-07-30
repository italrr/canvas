#include <iostream>
#include <cmath>
#include "../CV.hpp"

/*
    Here we should register all binary math functions that are part of the standard:
    - sin, cos, tan, atan
    - round, floor, ceil
    - max, min, clamp
    - mod

*/

void __CV_REGISTER_MATH_BINARY_FUNCTIONS(std::shared_ptr<CV::Stack> &stack){

    auto ns = stack->createNamespace("Standard Math Library", "math");

    stack->registerFunction(ns->id, "sin", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::sin( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });

    stack->registerFunction(ns->id, "cos", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::cos( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });    

    stack->registerFunction(ns->id, "tan", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::tan( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });      

    stack->registerFunction(ns->id, "atan", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (y)", token.line);
            return ctx->buildNil();            
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (x)", token.line);
            return ctx->buildNil();            
        }        

        return ctx->buildNumber(std::atan2( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });     


    stack->registerFunction(ns->id, "round", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::round( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });         


    stack->registerFunction(ns->id, "floor", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::floor( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });    


    stack->registerFunction(ns->id, "ceil", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil();            
        }

        return ctx->buildNumber(std::ceil( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });       


    stack->registerFunction(ns->id, "max", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil();            
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil();            
        }        

        return ctx->buildNumber(std::max( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });       


    stack->registerFunction(ns->id, "min", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil();            
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil();            
        }        

        return ctx->buildNumber(std::min( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });     

    stack->registerFunction(ns->id, "clamp", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 3){
            cursor->setError(name, "Expects exactly 3 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (n)", token.line);
            return ctx->buildNil();            
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (min)", token.line);
            return ctx->buildNil();            
        }        

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (max)", token.line);
            return ctx->buildNil();            
        }             


        auto n = std::static_pointer_cast<CV::NumberType>(args[0])->get();
        auto min = std::static_pointer_cast<CV::NumberType>(args[1])->get();
        auto max = std::static_pointer_cast<CV::NumberType>(args[2])->get();


        return ctx->buildNumber(std::min(std::max(n, min), max));
    });          



    stack->registerFunction(ns->id, "mod", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil();
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil();            
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil();            
        }        

        auto x = std::static_pointer_cast<CV::NumberType>(args[0])->get();
        auto y = std::static_pointer_cast<CV::NumberType>(args[1])->get();

        return ctx->buildNumber((int)x % (int)y);
    });         

}