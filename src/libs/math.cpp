#include <cmath>
#include "../CV.hpp"

/*
    Here we should register all binary math functions that are part of the standard:
    - sin, cos, tan, atan
    - round, floor, ceil
    - max, min, clamp
    - mod

*/

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Stack> &stack){

    stack->registerFunction("math:sin", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::sin( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });

    stack->registerFunction("math:cos", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::cos( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });    

    stack->registerFunction("math:tan", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (angle in radians)", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::tan( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });      

    stack->registerFunction("math:atan", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (y)", token.line);
            return ctx->buildNil(stack);           
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (x)", token.line);
            return ctx->buildNil(stack);           
        }        

        return ctx->buildNumber(stack, std::atan2( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });     


    stack->registerFunction("math:round", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::round( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });         


    stack->registerFunction("math:floor", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::floor( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });    


    stack->registerFunction("math:ceil", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 1){
            cursor->setError(name, "Expects exactly 1 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER", token.line);
            return ctx->buildNil(stack);           
        }

        return ctx->buildNumber(stack, std::ceil( std::static_pointer_cast<CV::NumberType>(args[0])->get() ));
    });       


    stack->registerFunction("math:max", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil(stack);           
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil(stack);           
        }        

        return ctx->buildNumber(stack, std::max( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });       


    stack->registerFunction("math:min", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil(stack);           
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil(stack);           
        }        

        return ctx->buildNumber(stack, std::min( std::static_pointer_cast<CV::NumberType>(args[0])->get(), std::static_pointer_cast<CV::NumberType>(args[1])->get() ));
    });     

    stack->registerFunction("math:clamp", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 3){
            cursor->setError(name, "Expects exactly 3 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (n)", token.line);
            return ctx->buildNil(stack);           
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (min)", token.line);
            return ctx->buildNil(stack);           
        }        

        if(args[2]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (max)", token.line);
            return ctx->buildNil(stack);           
        }             


        auto n = std::static_pointer_cast<CV::NumberType>(args[0])->get();
        auto min = std::static_pointer_cast<CV::NumberType>(args[1])->get();
        auto max = std::static_pointer_cast<CV::NumberType>(args[2])->get();


        return ctx->buildNumber(stack, std::min(std::max(n, min), max));
    });          

    stack->registerFunction("math:mod", [stack](const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>> &args, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
        
        if(args.size() != 2){
            cursor->setError(name, "Expects exactly 2 argument", token.line);
            return ctx->buildNil(stack);
        }

        if(args[0]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 0 must be a NUMBER (x)", token.line);
            return ctx->buildNil(stack);           
        }

        if(args[1]->type != CV::NaturalType::NUMBER){
            cursor->setError(name, "argument 1 must be a NUMBER (y)", token.line);
            return ctx->buildNil(stack);           
        }        

        auto x = std::static_pointer_cast<CV::NumberType>(args[0])->get();
        auto y = std::static_pointer_cast<CV::NumberType>(args[1])->get();

        return ctx->buildNumber(stack, (int)x % (int)y);
    });         

    stack->topCtx->setName("math:PI", stack->topCtx->buildNumber(stack, 3.14159265358979323846)->id);
}