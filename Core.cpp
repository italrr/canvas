#include "CV.hpp"

namespace CV {
    namespace Test {
        inline bool areAllNumbers(const std::vector<std::shared_ptr<CV::Quant>> &arguments){
            for(int i = 0; i < arguments.size(); ++i){
                if(arguments[i]->type != CV::QuantType::NUMBER){
                    return false;
                }
            }
            return true;
        }
    }

    namespace ErrorCheck {
        inline bool AllNumbers(const std::vector<std::shared_ptr<CV::Quant>> &args, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor){
            if(!CV::Test::areAllNumbers(args)){
                cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Function '"+name+"' expects all operands to be numbers", token->line);
                return false;
            }   
            return true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  ARITHMETIC
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_ARITHMETIC_ADDITION(
    const std::vector<std::shared_ptr<CV::Quant>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &ctx = prog->ctx[ctxId];
    auto data = ctx->memory[dataId];

    // Check for errors live
    if(!CV::ErrorCheck::AllNumbers(args, name, token, cursor)){
        ctx->memory[dataId] = ctx->buildNil();
        return;
    }

    // Build result holder
    auto result = ctx->buildNumber();

    // Fulfill promise in context
    ctx->memory[dataId] = result;

    // Do the operation
    for(int i = 0; i < args.size(); ++i){
        result->v += std::static_pointer_cast<CV::TypeNumber>(args[i])->v;
    }
}

static void __CV_CORE_ARITHMETIC_MULT(
    const std::vector<std::shared_ptr<CV::Quant>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &ctx = prog->ctx[ctxId];
    auto data = ctx->memory[dataId];

    // Check for errors live
    if(!CV::ErrorCheck::AllNumbers(args, name, token, cursor)){
        ctx->memory[dataId] = ctx->buildNil();
        return;
    }

    // Build result holder
    auto result = ctx->buildNumber();

    // Fulfill promise in context
    ctx->memory[dataId] = result;

    // Do the operation
    for(int i = 0; i < args.size(); ++i){
        result->v *= std::static_pointer_cast<CV::TypeNumber>(args[i])->v;
    }
}

/*


    REGISTER CORE CANVAS FUNCTIONALITY


*/

void CVInitCore(const CV::ProgramType &prog){

    /*

        REGISTER ARITHMETIC OPERATORS

    */
    CV::unwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("+", (void*)__CV_CORE_ARITHMETIC_ADDITION);
        target->rootContext->registerBinaryFuntion("*", (void*)__CV_CORE_ARITHMETIC_MULT);
        return true;
    }, prog);

}

