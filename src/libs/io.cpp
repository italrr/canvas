#include <iostream>
#include "../CV.hpp"

static inline void ___WRITE_STDOUT(const std::string &v){
    int n = v.size();
    std::cout << v;
}  

static inline void ___WRITE_STDERR(const std::string &v){
    int n = v.size();
    std::cerr << v;
}  

static inline void ___GET_STDIN(std::string &v){
    std::cin >> v;
}  

static void __CV_STD_IO_OUT(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog,
    const CV::CFType &st
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 1, name, token, cursor)){
        return;
    }
    
    std::string out = "";
    for(int i = 0; i < args.size(); ++i){
        auto quant = CV::Execute(args[i], execCtx, prog, cursor, st);
        if(cursor->error){
            return;
        }
        if(quant->type != CV::QuantType::STRING){
            out += CV::QuantToText(quant);
        }else{
            out += std::static_pointer_cast<CV::TypeString>(quant)->v;
        }
    }
    ___WRITE_STDOUT(out);


    dataCtx->memory[dataId] = dataCtx->buildNil();
}

static void __CV_STD_IO_ERR(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog,
    const CV::CFType &st
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsOperands(args.size(), 1, name, token, cursor)){
        return;
    }

    std::string out = "";
    for(int i = 0; i < args.size(); ++i){
        auto quant = CV::Execute(args[i], execCtx, prog, cursor, st);
        if(cursor->error){
            return;
        }
        if(quant->type != CV::QuantType::STRING){
            out += CV::QuantToText(quant);
        }else{
            out += std::static_pointer_cast<CV::TypeString>(quant)->v;
        }
    }
    ___WRITE_STDERR(out);


    dataCtx->memory[dataId] = dataCtx->buildNil();
}

static void __CV_STD_IO_IN(
    const std::vector<std::shared_ptr<CV::Instruction>> &args,
    const std::string &name,
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    int execCtxId,
    int ctxId,
    int dataId,
    const std::shared_ptr<CV::Program> &prog,
    const CV::CFType &st
){
    // Fetch context & data target
    auto &dataCtx = prog->ctx[ctxId];
    auto &execCtx = prog->ctx[execCtxId];

    if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ){
        return;
    }

    std::string input;
    ___GET_STDIN(input);

    dataCtx->memory[dataId] = dataCtx->buildString(input);
}

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    ctx->registerBinaryFuntion("io:out", (void*)__CV_STD_IO_OUT);
    ctx->registerBinaryFuntion("io:err", (void*)__CV_STD_IO_ERR);
    ctx->registerBinaryFuntion("io:in", (void*)__CV_STD_IO_IN);
}