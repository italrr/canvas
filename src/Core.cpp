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
        inline bool IsItPrefixInstruction(const std::shared_ptr<CV::Instruction> &ins){
            return ins->data.size() >= 2 && ins->data[0] == CV_INS_PREFIXER_IDENFIER_INSTRUCTION;
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
        inline bool ExpectNoPrefixer(const std::string &name, const std::vector<std::shared_ptr<CV::Instruction>> &args, const CV::TokenType &token, const CV::CursorType &cursor){
            for(int i = 0; i < args.size(); ++i){
                if(args[i]->data.size() >= 2 && args[i]->data[0] == CV_INS_PREFIXER_IDENFIER_INSTRUCTION){
                    cursor->setError(CV_ERROR_MSG_ILLEGAL_PREFIXER, "Imperative '"+name+"' does not accept any prefixers: operand at "+std::to_string(i)+" is "+CV::Prefixer::name(args[i]->data[1])+"("+args[i]->token->str()+")", token);
                    return false;
                }
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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
        if(!CV::GetBooleanValue(v)){
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
    
    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
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
        if(CV::GetBooleanValue(v)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    result->v = CV::GetBooleanValue(condi);

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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor) ||
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    // Get Index
    auto indexBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(indexBranch->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    int index = std::round(std::static_pointer_cast<CV::TypeNumber>(indexBranch)->v);

    // Get target
    auto listBranch = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(listBranch->type, CV::QuantType::LIST, 0, name, token, cursor)){
        return;
    }
    auto list = std::static_pointer_cast<CV::TypeList>(listBranch);

    // Bound check
    if(index < 0){
        cursor->setError(CV_ERROR_MSG_INVALID_INDEX, "Imperative '"+name+"' expects non-negative index number at operand 0: operand is "+std::to_string(index), token);
        return;
    }
    if(index >= list->v.size()){
        cursor->setError(CV_ERROR_MSG_INVALID_INDEX, "Imperative '"+name+"' expects index number within bounds (size:"+std::to_string(list->v.size())+"): operand is "+std::to_string(index), token);
        return;
    }

    // Fetch
    auto &subject = list->v[index];

    // Fulfill promise in context
    dataCtx->memory[dataId] = subject;
}

static void __CV_CORE_LIST_LENGTH(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Get target
    auto listBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(listBranch->type, CV::QuantType::LIST, 0, name, token, cursor)){
        return;
    }
    auto list = std::static_pointer_cast<CV::TypeList>(listBranch);

    // Fulfill promise in context
    auto result = dataCtx->buildNumber(list->v.size());

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;

}

static void __CV_CORE_LIST_PUSH(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 2, name, token, cursor)){
        return;
    }
    
    // Solve subject
    auto subjectBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }

    // Solve target
    auto targetBranch = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    std::shared_ptr<CV::TypeList> target;
    if(targetBranch->type != CV::QuantType::LIST){
        target = dataCtx->buildList();
        target->v.push_back(targetBranch);
    }else{
        target = std::static_pointer_cast<CV::TypeList>(targetBranch);
    }
    target->v.push_back(subjectBranch);

    // Fulfill promise in context
    dataCtx->memory[dataId] = target;
}

static void __CV_CORE_LIST_POP(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }
    
    auto listBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(listBranch->type, CV::QuantType::LIST, 0, name, token, cursor)){
        return;
    }
    auto list = std::static_pointer_cast<CV::TypeList>(listBranch);

    if(list->v.size() == 0){
        dataCtx->memory[dataId] = list;
    }

    auto subject = list->v[list->v.size()-1];

    list->v.erase(list->v.end()-1);

    // Fulfill promise in context
    dataCtx->memory[dataId] = subject;

}


static void __CV_CORE_LIST_SPLICE(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 2, name, token, cursor)){
        return;
    }

    auto result = dataCtx->buildList();
    
    // Join
    for(int i = 0; i < args.size(); ++i){
        auto listBranch = CV::Execute(args[i], execCtx, prog, cursor);
        if(cursor->error){
            return;
        }
        if(!CV::ErrorCheck::ExpectsTypeAt(listBranch->type, CV::QuantType::LIST, 0, name, token, cursor)){
            return;
        }
        auto list = std::static_pointer_cast<CV::TypeList>(listBranch);
        for(int j = 0; j < list->v.size(); ++j){
            result->v.push_back(list->v[j]);
        }
    }

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;
}


static void __CV_CORE_LIST_RESERVE(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    auto result = dataCtx->buildList();
    
    auto listBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(listBranch->type, CV::QuantType::LIST, 0, name, token, cursor)){
        return;
    }
    auto list = std::static_pointer_cast<CV::TypeList>(listBranch);
    for(int i = 0; i < list->v.size(); ++i){
        result->v.push_back(list->v[list->v.size()-1-i]);
    }

    // Fulfill promise in context
    dataCtx->memory[dataId] = result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  MUTATORS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_MUT_PLUSPLUS(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Get subject
    auto subjectBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(subjectBranch->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    
    // Add
    ++std::static_pointer_cast<CV::TypeNumber>(subjectBranch)->v;

    // Fulfill promise in context
    dataCtx->memory[dataId] = subjectBranch;
}

static void __CV_CORE_MUT_MINUSMINUS(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Get subject
    auto subjectBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(subjectBranch->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    
    // Subtract
    --std::static_pointer_cast<CV::TypeNumber>(subjectBranch)->v;

    // Fulfill promise in context
    dataCtx->memory[dataId] = subjectBranch;
}


static void __CV_CORE_MUT_SLASHSLASH(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Get subject
    auto subjectBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(subjectBranch->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    
    // Subtract
    std::static_pointer_cast<CV::TypeNumber>(subjectBranch)->v /= static_cast<CV_DEFAULT_NUMBER_TYPE>(2.0);

    // Fulfill promise in context
    dataCtx->memory[dataId] = subjectBranch;
}

static void __CV_CORE_MUT_STARSTAR(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    // Get subject
    auto subjectBranch = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(subjectBranch->type, CV::QuantType::NUMBER, 0, name, token, cursor)){
        return;
    }
    
    // Subtract
    std::static_pointer_cast<CV::TypeNumber>(subjectBranch)->v *= std::static_pointer_cast<CV::TypeNumber>(subjectBranch)->v;

    // Fulfill promise in context
    dataCtx->memory[dataId] = subjectBranch;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  LOOPS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void __CV_CORE_LOOP_WHILE(
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

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    std::shared_ptr<CV::Quant> result = NULL;

    while(true) {
        auto deepExecCtx = prog->createContext(execCtx);

        // Conditional Branch
        auto condBranch = CV::Execute(args[0], deepExecCtx, prog, cursor);
        if(cursor->error){
            return;
        }

        // Check conditional
        if(!CV::GetBooleanValue(condBranch)){
            break;
        }
        
        // Code Branch
        if(args.size() > 1){
            result = CV::Execute(args[1], deepExecCtx, prog, cursor);
            if(cursor->error){
                return;
            }
        }

        prog->deleteContext(deepExecCtx->id);
    }

    if(result.get() == NULL){
        result = dataCtx->buildNil();
    }
    
    // Fulfill promise in context
    dataCtx->memory[dataId] = result;
}

CV::InsType GetPrefixer(const std::vector<CV::InsType> &src, const std::string &name){
    for(int i = 0; i < src.size(); ++i){
        if(CV::Test::IsItPrefixInstruction(src[i]) && src[i]->literal.size() > 0 && src[i]->literal[0] == name){
            return src[i];
        }
    }
    return CV::InsType(NULL);
}

static void __CV_CORE_LOOP_FOR(
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

    if( !CV::ErrorCheck::ExpectsOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    std::shared_ptr<CV::Quant> result = NULL;

    while(true) {
        auto deepExecCtx = prog->createContext(execCtx);

        // Conditional Branch
        auto condBranch = CV::Execute(args[0], deepExecCtx, prog, cursor);
        if(cursor->error){
            return;
        }

        // Check conditional
        if(!CV::GetBooleanValue(condBranch)){
            break;
        }
        
        // Code Branch
        if(args.size() > 1){
            result = CV::Execute(args[1], deepExecCtx, prog, cursor);
            if(cursor->error){
                return;
            }
        }

        prog->deleteContext(deepExecCtx->id);
    }

    if(result.get() == NULL){
        result = dataCtx->buildNil();
    }
    
    // Fulfill promise in context
    dataCtx->memory[dataId] = result;
}



/*


    REGISTER CORE CANVAS FUNCTIONALITY


*/
void CVInitCore(const CV::ProgramType &prog){

    /*

        ARITHMETIC OPERATORS

    */
    CV::UnwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("+", (void*)__CV_CORE_ARITHMETIC_ADDITION);
        target->rootContext->registerBinaryFuntion("*", (void*)__CV_CORE_ARITHMETIC_MULT);
        target->rootContext->registerBinaryFuntion("-", (void*)__CV_CORE_ARITHMETIC_SUB);
        target->rootContext->registerBinaryFuntion("/", (void*)__CV_CORE_ARITHMETIC_DIV);
        return true;
    }, prog);

    /*

        BOOLEAN OPERATORS

    */
    CV::UnwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("&", (void*)__CV_CORE_BOOLEAN_AND);
        target->rootContext->registerBinaryFuntion("|", (void*)__CV_CORE_BOOLEAN_OR);
        target->rootContext->registerBinaryFuntion("!", (void*)__CV_CORE_BOOLEAN_NOT);
        return true;
    }, prog);   
    
    
    /*

        CONDITIONAL OPERATORS

    */
    CV::UnwrapLibrary([](const CV::ProgramType &target){
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
    CV::UnwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("nth", (void*)__CV_CORE_LIST_NTH);
        target->rootContext->registerBinaryFuntion("len", (void*)__CV_CORE_LIST_LENGTH);
        target->rootContext->registerBinaryFuntion(">>", (void*)__CV_CORE_LIST_PUSH);
        target->rootContext->registerBinaryFuntion("<<", (void*)__CV_CORE_LIST_POP);
        target->rootContext->registerBinaryFuntion("splice", (void*)__CV_CORE_LIST_SPLICE);
        target->rootContext->registerBinaryFuntion("l-rev", (void*)__CV_CORE_LIST_RESERVE);
        return true;
    }, prog); 

    /*

        MUTATORS

    */
    CV::UnwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("++", (void*)__CV_CORE_MUT_PLUSPLUS);
        target->rootContext->registerBinaryFuntion("--", (void*)__CV_CORE_MUT_MINUSMINUS);
        target->rootContext->registerBinaryFuntion("//", (void*)__CV_CORE_MUT_SLASHSLASH);
        target->rootContext->registerBinaryFuntion("**", (void*)__CV_CORE_MUT_STARSTAR);
        return true;
    }, prog); 


    /*

        LOOPS

    */
    CV::UnwrapLibrary([](const CV::ProgramType &target){
        target->rootContext->registerBinaryFuntion("while", (void*)__CV_CORE_LOOP_WHILE);
        return true;
    }, prog); 

}

