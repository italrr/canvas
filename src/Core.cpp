#include <cmath>
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
            for(int i = 0; i < args.size(); ++i){
                if(args[i]->type != CV::QuantType::NUMBER){
                    cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects all operands to be numbers: operand "+std::to_string(i)+" is "+CV::QuantType::name(args[i]->type), token);
                    return false;
                }
            }            
            return true;
        }
        inline bool ExpectsOperands(int prov, int exp, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor){
            if(prov < exp){
                cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects ("+std::to_string(exp)+") operands but provided("+std::to_string(prov)+")", token);
                return false;
            }   
            return true;
        }

        inline bool ExpectsTypeAt(int type, int exp, int at, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor){
            if(type < exp){
                auto expTName = CV::QuantType::name(exp);
                auto tName = CV::QuantType::name(type);
                auto atStr = std::to_string(at);
                cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects "+atStr+" operand to be "+expTName+": operand "+atStr+" is "+tName, token);
                return false;
            }   
            return true;
        }

        inline bool ExpectsNoMoreOperands(int prov, int max, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor){
            if(prov > max){
                cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects no more than ("+std::to_string(max)+") operands but provided("+std::to_string(prov)+")", token);
                return false;
            }   
            return true;
        }        

        inline bool ExpectsExactlyOperands(int prov, int exp, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor){
            if(prov != exp){
                cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects exactly ("+std::to_string(exp)+") operands but provided("+std::to_string(prov)+")", token);
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
#include <iostream>
static void __CV_CORE_ARITHMETIC_ADDITION(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    for(int i = 0; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }           
        if(!CV::ErrorCheck::AllNumbers({v}, name, token, cursor)){
            return;
        }
        result->v += std::static_pointer_cast<CV::TypeNumber>(v)->v;
    }
}

static void __CV_CORE_ARITHMETIC_SUB(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }    

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    auto fv = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }           
    if(!CV::ErrorCheck::AllNumbers({fv}, name, token, cursor)){
        return;
    }    
    result->v = std::static_pointer_cast<CV::TypeNumber>(fv)->v;
    // Do the operation
    for(int i = 1; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }           
        if(!CV::ErrorCheck::AllNumbers({v}, name, token, cursor)){
            return;
        }
        result->v -= std::static_pointer_cast<CV::TypeNumber>(v)->v;
    }
}

static void __CV_CORE_ARITHMETIC_MULT(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];


    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }    

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    for(int i = 0; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }           
        if(!CV::ErrorCheck::AllNumbers({v}, name, token, cursor)){
            return;
        }
        result->v *= std::static_pointer_cast<CV::TypeNumber>(v)->v;
    }
}

static void __CV_CORE_ARITHMETIC_DIV(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }    

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    auto fv = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }           
    if(!CV::ErrorCheck::AllNumbers({fv}, name, token, cursor)){
        return;
    }    
    result->v = std::static_pointer_cast<CV::TypeNumber>(fv)->v;
    // Do the operation
    for(int i = 1; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }           
        if(!CV::ErrorCheck::AllNumbers({v}, name, token, cursor)){
            return;
        }
        result->v /= std::static_pointer_cast<CV::TypeNumber>(v)->v;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  BOOLEAN
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_BOOLEAN_AND(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    result->v = 1;
    for(int i = 0; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }        
        if(!CV::getBooleanValue(v)){
            result->v = 0;
            break;
        }
    }
}

static void __CV_CORE_BOOLEAN_OR(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    result->v = 0;
    for(int i = 0; i < args.size(); ++i){
        auto v = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }        
        if(CV::getBooleanValue(v)){
            result->v = 1;
            break;
        }
    }
}

static void __CV_CORE_BOOLEAN_NOT(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto v = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }     
    if(!CV::ErrorCheck::AllNumbers({v}, name, token, cursor)){
        return;
    }    
    result->v = !std::static_pointer_cast<CV::TypeNumber>(v)->v;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  CONDITIONAL
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_CONDITIONAL_EQ(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a == b;

    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}

static void __CV_CORE_CONDITIONAL_NEQ(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a != b;

    // TRUE BRANCH
    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    // FALSE BRANCH
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}

static void __CV_CORE_CONDITIONAL_IF(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 3, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto condi = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }

    result->v = CV::getBooleanValue(condi);

    // TRUE BRANCH
    if(args.size() > 1 && result->v){
        auto trueBranch = CV::Execute(args[1], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() == 2 && !result->v){
        dataCtx->memory[dataId] = dataCtx->buildNil();
    }else
    // FALSE BRANCH
    if(args.size() > 2 && !result->v){
        auto falseBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}



static void __CV_CORE_CONDITIONAL_MORE_THAN(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a > b;

    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}

static void __CV_CORE_CONDITIONAL_MORE_OR_EQ(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a >= b;

    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}


static void __CV_CORE_CONDITIONAL_LESS_THAN(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a < b;

    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}

static void __CV_CORE_CONDITIONAL_LESS_OR_EQUAL(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
        !CV::ErrorCheck::ExpectsNoMoreOperands(args.size(), 4, name, token, cursor)){
        return;
    }

    // Build result holder
    auto result = dataCtx->buildNumber();

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

    // Do the operation
    auto ai = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }  

    auto bi = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }      

    if(!CV::ErrorCheck::AllNumbers({ai, bi}, name, token, cursor)){
        return;
    }    

    auto a = std::static_pointer_cast<CV::TypeNumber>(ai)->v;
    auto b = std::static_pointer_cast<CV::TypeNumber>(bi)->v;

    result->v = a <= b;

    if(args.size() > 2 && result->v){
        auto trueBranch = CV::Execute(args[2], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = trueBranch;
    }else
    if(args.size() > 3 && !result->v){
        auto falseBranch = CV::Execute(args[3], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        dataCtx->memory[dataId] = falseBranch;
    }    
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  LIST TOOLS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_LIST_NTH(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if(!CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    // Get Index
    if(!CV::ErrorCheck::ExpectsTypeAt(args[0]->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    int index = std::round(std::static_pointer_cast<CV::TypeNumber>(args[0])->v);

    // Get target
    if(!CV::ErrorCheck::ExpectsTypeAt(args[0]->type, CV::QuantType::LIST, 0, name, token, cursor)){
        return;
    }
    auto list = std::static_pointer_cast<CV::TypeList>(args[1]);

    if(index < 0){
        cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects non-negative index number at operand 0: operand is"+std::to_string(index), token);
        return;
    }
    if(index >= list->v.size()){
        cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects index number within bounds (size:"+std::to_string(list->v.size())+"): operand is "+std::to_string(index), token);
        return;
    }

    auto &subject = list->v[index];

    // Fulfill promise in context
    dataCtx->memory[dataId] = subject;
}


/*


    REGISTER CORE CANVAS FUNCTIONALITY


*/
void CVInitCore(const CV::ProgramType &prog){

    /*

        ARITHMETIC OPERATORS

    */
    CV::unwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("+", (void*)__CV_CORE_ARITHMETIC_ADDITION);
        target->rootContext->registerBinaryFuntion("*", (void*)__CV_CORE_ARITHMETIC_MULT);
        target->rootContext->registerBinaryFuntion("-", (void*)__CV_CORE_ARITHMETIC_SUB);
        target->rootContext->registerBinaryFuntion("/", (void*)__CV_CORE_ARITHMETIC_DIV);
        return true;
    }, prog);

    /*

        BOOLEAN OPERATORS

    */
    CV::unwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("&", (void*)__CV_CORE_BOOLEAN_AND);
        target->rootContext->registerBinaryFuntion("|", (void*)__CV_CORE_BOOLEAN_OR);
        target->rootContext->registerBinaryFuntion("!", (void*)__CV_CORE_BOOLEAN_NOT);
        return true;
    }, prog);   
    
    
    /*

        CONDITIONAL OPERATORS

    */
    CV::unwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("=", (void*)__CV_CORE_CONDITIONAL_EQ);
        target->rootContext->registerBinaryFuntion("!=", (void*)__CV_CORE_CONDITIONAL_NEQ);
        target->rootContext->registerBinaryFuntion("if", (void*)__CV_CORE_CONDITIONAL_IF);
        target->rootContext->registerBinaryFuntion(">", (void*)__CV_CORE_CONDITIONAL_MORE_THAN);
        target->rootContext->registerBinaryFuntion(">=", (void*)__CV_CORE_CONDITIONAL_MORE_OR_EQ);
        target->rootContext->registerBinaryFuntion("<", (void*)__CV_CORE_CONDITIONAL_LESS_THAN);
        target->rootContext->registerBinaryFuntion("<=", (void*)__CV_CORE_CONDITIONAL_LESS_OR_EQUAL);
        return true;
    }, prog);    

    /*

        LIST TOOLS

    */
    CV::unwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("nth", (void*)__CV_CORE_LIST_NTH);
        return true;
    }, prog); 

}

