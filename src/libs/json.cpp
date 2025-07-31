#include<iostream>
#include <cmath> 
#include <fstream>

#include "thirdparty/json11.hpp"
#include "../CV.hpp"

static inline bool isNotFractional(number d, number epsilon = 1e-9){
    number integral_part;
    number fractional_part = std::modf(d, &integral_part);
    return std::fabs(fractional_part) < epsilon;
}

static json11::Json buildNode(const std::string &name, const std::shared_ptr<CV::Quant> &origin, const CV::TokenType &token, const CV::CursorType &cursor){
    switch(origin->type){
        case CV::QuantType::STORE: {
            json11::Json::object obj;
            auto store = std::static_pointer_cast<CV::TypeStore>(origin);
            for(auto &it : store->v){
                obj[it.first] =  buildNode(name, it.second, token, cursor);
            }
            return obj;
        };
        case CV::QuantType::LIST: {
            json11::Json::array obj;
            auto list = std::static_pointer_cast<CV::TypeList>(origin);
            for(int i = 0; i < list->v.size(); ++i){
                obj.push_back(buildNode(name, list->v[i], token, cursor));
            }
            return obj;
        };
        case CV::QuantType::NUMBER: {
            auto v = std::static_pointer_cast<CV::TypeNumber>(origin)->v;
            return isNotFractional(v) ?  json11::Json( static_cast<int>(v) ) : json11::Json( v );
        };

        case CV::QuantType::STRING: {
            return json11::Json( std::static_pointer_cast<CV::TypeString>(origin)->v );
        };

        case CV::QuantType::NIL: {
            return json11::Json();
        };

        default: {
            cursor->setError("Unhandled Type For JSON", "Imperative '"+name+"' was provided type '"+std::to_string(origin->type)+"' is not handled by this JSON library");
            return json11::Json();
        };
    };
}

static std::shared_ptr<CV::Quant> unwrapJson(const std::string &name, json11::Json obj, const CV::TokenType &token, const CV::ContextType &ctx, const CV::CursorType &cursor){

    switch(obj.type()){
        case json11::Json::ARRAY: {
            auto arr = obj.array_items();
            auto list = ctx->buildList();
            for(int i = 0; i < arr.size(); ++i){
                list->v.push_back(unwrapJson(name, arr.at(i), token, ctx, cursor));
            }
            return list;
        };
        case json11::Json::OBJECT: {
            json11::Json::object curr { obj.object_items() };
            auto store = ctx->buildStore();
            for(auto &it : curr){
                store->v[it.first] = unwrapJson(name, it.second, token, ctx, cursor);
            }
            return store;
        };
        case json11::Json::NUMBER: {
            return ctx->buildNumber( obj.number_value() );
        };
        case json11::Json::BOOL: {
            return ctx->buildNumber( obj.bool_value() );
        };
        case json11::Json::STRING: {
            return ctx->buildString( obj.string_value() );
        };
        case json11::Json::NUL: {
            return ctx->buildNil();
        };
        default: {
            cursor->setError("Unhandled Type For JSON", "Imperative '"+name+"' was provided type '"+std::to_string(obj.type())+"' is not handled by this JSON library");
            return ctx->buildNil();
        };
    }

}

static void __CV_STD_JSON_WRITE(
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
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 2, name, {"STORE/LIST", "FILENAME"}, token, cursor)){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }
    
    // Get store param
    auto param0 = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }
    if(param0->type != CV::QuantType::LIST && param0->type != CV::QuantType::STORE){
        cursor->setError(CV_ERROR_MSG_WRONG_TYPE, "Imperative '"+name+"' expects LIST or STORE operand at first position", token);   
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }

    // Get filename param
    auto param1Filename = CV::Execute(args[1], execCtx, prog, cursor);
    if(cursor->error){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);  
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param1Filename->type, CV::QuantType::STRING, 1, name, token, cursor)){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }    

    auto &filename = std::static_pointer_cast<CV::TypeString>(param1Filename)->v;

    // Build Json hierarchy
    auto obj = buildNode(name, param0, token, cursor);
    if(cursor->error){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }

    std::ofstream outFile;
    outFile.open(filename);
    outFile << json11::Json(obj).dump();
    outFile.close();   

    dataCtx->memory[dataId] = dataCtx->buildNumber(1);
}

static void __CV_STD_JSON_PARSE_FILE(
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
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, {"FILENAME"}, token, cursor)){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }

    // Get filename param
    auto param0Filename = CV::Execute(args[0], execCtx, prog, cursor);
    if(cursor->error){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);  
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param0Filename->type, CV::QuantType::STRING, 0, name, token, cursor)){
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }    

    auto &filename = std::static_pointer_cast<CV::TypeString>(param0Filename)->v;

    if(!CV::Tools::fileExists(filename)){
        cursor->setError("File Not Found", "File '"+filename+"' does not exist", token);
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }
    auto source = CV::Tools::readFile(filename);
    std::string err;
    auto json = json11::Json::parse(source, err);
    if(err.size() > 0){
        cursor->setError("Bad JSON Input", "Imperative '"+name+"' was provided an invalid JSON input: '"+err+"'", token);
        dataCtx->memory[dataId] = dataCtx->buildNumber(0);
        return;
    }

    dataCtx->memory[dataId] = unwrapJson(name, json, token, dataCtx, cursor);
}

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    ctx->registerBinaryFuntion("json:write", (void*)__CV_STD_JSON_WRITE);
    ctx->registerBinaryFuntion("json:load", (void*)__CV_STD_JSON_PARSE_FILE);
}