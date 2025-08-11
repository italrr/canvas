#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./thirdparty/stb_image.h"
#include "./thirdparty/stb_image_write.h"

#include "../CV.hpp"

static inline bool isValidBmpStore(const std::shared_ptr<CV::Quant> &q, std::string &err){
    if(q->type != CV::QuantType::STORE){
        err = "Provided type is not '"+CV::QuantType::name(CV::QuantType::STORE)+"'";
        return false;
    }
    auto store = std::static_pointer_cast<CV::TypeStore>(q);
    if(store->v.count("width") != 1 || store->v["width"]->type != CV::QuantType::NUMBER){
        err = "Provided STORE type has no member 'width' or this member is unexpected (Not NUMBER)";
        return false;
    }

    if(store->v.count("height") != 1 || store->v["height"]->type != CV::QuantType::NUMBER){
        err = "Provided STORE type has no member 'height' or this member is unexpected (Not NUMBER)";
        return false;
    }

    if(store->v.count("channels") != 1 || store->v["channels"]->type != CV::QuantType::NUMBER){
        err = "Provided STORE type has no member 'channels' or this member is unexpected (Not NUMBER)";
        return false;
    }

    if(store->v.count("data") != 1 || store->v["data"]->type != CV::QuantType::LIST){
        err = "Provided STORE type has no member 'data' or this member is unexpected (Not LIST)";
        return false;
    }
    return true;
}

static inline bool isValidPosition(int mw, int mh, const std::shared_ptr<CV::Quant> &q, std::string &err){
    if(q->type != CV::QuantType::LIST){
        err = "Provided type is not '"+CV::QuantType::name(CV::QuantType::LIST)+"'";
        return false;
    }
    auto pos = std::static_pointer_cast<CV::TypeList>(q);
    if(pos->v.size() != 2){
        err = "Provided position contains "+std::to_string(pos->v.size())+", expected 2 (x and y)";
        return false;
    }

    if(pos->v[0]->type != CV::QuantType::NUMBER){
        err = "Type at positon 0 is not a number";
        return false;
    }
    

    if(pos->v[1]->type != CV::QuantType::NUMBER){
        err = "Type at positon 1 is not a number";
        return false;
    }

    auto x = std::static_pointer_cast<CV::TypeNumber>(pos->v[0])->v;
    auto y = std::static_pointer_cast<CV::TypeNumber>(pos->v[1])->v;

    if(x < 0 || x >= mw){
        err = "X position is out of bounds (0-"+std::to_string(mw)+")";
        return false;
    }

    if(y < 0 || y >= mh){
        err = "Y position is out of bounds (0-"+std::to_string(mh)+")";
        return false;
    }

    return true;
}

static inline bool isValidPixel(int exnchan, const std::shared_ptr<CV::Quant> &q, std::string &err){
    if(q->type != CV::QuantType::LIST){
        err = "Provided type is not '"+CV::QuantType::name(CV::QuantType::LIST)+"'";
        return false;
    }
    auto pixel = std::static_pointer_cast<CV::TypeList>(q);
    if(pixel->v.size() != exnchan){
        err = "Provided pixel doesn't match the number of channels expected";
        err += " (Provided "+std::to_string(pixel->v.size())+", expected "+std::to_string(exnchan)+")";
        return false;
    }
    for(int i = 0; i < pixel->v.size(); ++i){
        if(pixel->v[i]->type != CV::QuantType::NUMBER){
            err = "Provided pixel has a non-number member at position "+std::to_string(i);
            err += +" as '"+CV::QuantType::name(pixel->v[i]->type)+"'";
            return false;
        }
    }
    return true;
}

static void __CV_STD_BMP_LOAD(
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
    auto &dataCtx = prog->getCtx(ctxId);
    auto &execCtx = prog->getCtx(execCtxId);

   if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 1, name, {"FILENAME"}, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNil());
        return;
    }
    auto param0Filename = CV::Execute(args[0], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param0Filename->type, CV::QuantType::STRING, 0, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNil());
        return;
    }

    auto &filename = std::static_pointer_cast<CV::TypeString>(param0Filename)->v;

    if(!CV::Tools::fileExists(filename)){
        cursor->setError("File Not Found", "File '"+filename+"' does not exist", token);
        dataCtx->set(dataId, dataCtx->buildNil());
        return;
    }

    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0); 

    if(!data){
        cursor->setError("Bad Image Format", "File '"+filename+"' is an invalid image or not compatible with this library", token);
        dataCtx->set(dataId, dataCtx->buildNil());
        return;
    }

    auto handle = dataCtx->buildStore();
    auto pixels = dataCtx->buildList();

    handle->v["width"] = dataCtx->buildNumber(width);
    handle->v["height"] = dataCtx->buildNumber(height);
    handle->v["channels"] = dataCtx->buildNumber(nrChannels);
    handle->v["data"] = pixels;

    int total = width * height * nrChannels;
    for(int i = 0; i < total; ++i){
        pixels->v.push_back(dataCtx->buildNumber(data[i]));
    }

    delete data;

    dataCtx->set(dataId,  handle);
}


static void __CV_STD_BMP_WRITE(
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
    auto &dataCtx = prog->getCtx(ctxId);
    auto &execCtx = prog->getCtx(execCtxId);

   if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 3, name, {"BMP", "FORMAT", "FILENAME"}, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    // Get BMP Store
    auto param0Store = CV::Execute(args[0], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param0Store->type, CV::QuantType::STORE, 0, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    std::string p0err;
    if(!isValidBmpStore(param0Store, p0err)){
        cursor->setError("Invalid BMP Store", p0err, token);
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    auto store = std::static_pointer_cast<CV::TypeStore>(param0Store);
    auto &nchannel = std::static_pointer_cast<CV::TypeNumber>(store->v["channels"])->v;
    auto &width = std::static_pointer_cast<CV::TypeNumber>(store->v["width"])->v;
    auto &height = std::static_pointer_cast<CV::TypeNumber>(store->v["height"])->v;
    auto &data = std::static_pointer_cast<CV::TypeList>(store->v["data"])->v;

    // Get Format
    auto param1Format = CV::Execute(args[1], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param1Format->type, CV::QuantType::STRING, 1, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    auto &format = std::static_pointer_cast<CV::TypeString>(param1Format)->v;
    
    // Get Filename
    auto param2Filename = CV::Execute(args[2], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param2Filename->type, CV::QuantType::STRING, 2, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    auto &filename = std::static_pointer_cast<CV::TypeString>(param2Filename)->v;


    // Build image
    auto total = static_cast<int>(width) * static_cast<int>(height) * static_cast<int>(nchannel);
    char *img = (char*)malloc(total);

    for(int i = 0; i < total; ++i){
        img[i] = static_cast<char>(std::static_pointer_cast<CV::TypeNumber>(data[i])->v);
    }

    // Write
    if(CV::Tools::lower(format) == "bmp"){
        stbi_write_bmp(filename.c_str(), width, height, nchannel, img);
    }else
    if(CV::Tools::lower(format) == "png"){
        stbi_write_png(filename.c_str(), width, height, nchannel, img, width * nchannel);
    }else
    if(CV::Tools::lower(format) == "jpg" || CV::Tools::lower(format) == "jpeg"){
        stbi_write_jpg(filename.c_str(), width, height, nchannel, img, 100);  
    }else{
        cursor->setError("Invalid Image Format", "Parameter 1 Image Format ('"+format+"') is invalid or unsupported", token);
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        delete img; 
        return;
    }

    delete img;

    dataCtx->set(dataId,  dataCtx->buildNumber(1));
}



static void __CV_STD_BMP_SET_PIXEL_AT(
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
    auto &dataCtx = prog->getCtx(ctxId);
    auto &execCtx = prog->getCtx(execCtxId);

   if( !CV::ErrorCheck::ExpectNoPrefixer(name, args, token, cursor) ||
        !CV::ErrorCheck::ExpectsExactlyOperands(args.size(), 3, name, {"BMP", "POS", "PIXEL"}, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    // Get BMP Store
    auto param0Store = CV::Execute(args[0], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param0Store->type, CV::QuantType::STORE, 0, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    std::string p0err;
    if(!isValidBmpStore(param0Store, p0err)){
        cursor->setError("Invalid BMP Store", p0err, token);
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    auto store = std::static_pointer_cast<CV::TypeStore>(param0Store);
    auto &nchannel = std::static_pointer_cast<CV::TypeNumber>(store->v["channels"])->v;
    auto &width = std::static_pointer_cast<CV::TypeNumber>(store->v["width"])->v;
    auto &height = std::static_pointer_cast<CV::TypeNumber>(store->v["height"])->v;
    auto &data = std::static_pointer_cast<CV::TypeList>(store->v["data"])->v;


    // Get Pixel Position
    auto param1Pos = CV::Execute(args[1], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param1Pos->type, CV::QuantType::LIST, 1, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    std::string p1err;
    if(!isValidPosition(width, height, param1Pos, p1err)){
        cursor->setError("Invalid Position", p1err, token);
        dataCtx->set(dataId, dataCtx->buildNumber(0));
    }
    auto pos = std::static_pointer_cast<CV::TypeList>(param1Pos);

    // Get Pixel
    auto param2Pos = CV::Execute(args[2], execCtx, prog, cursor, st);
    if(cursor->error){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    if(!CV::ErrorCheck::ExpectsTypeAt(param2Pos->type, CV::QuantType::LIST, 2, name, token, cursor)){
        dataCtx->set(dataId, dataCtx->buildNumber(0));
        return;
    }
    std::string p2err;
    if(!isValidPixel(nchannel, param2Pos, p2err)){
        cursor->setError("Invalid Pixel", p2err, token);
        dataCtx->set(dataId, dataCtx->buildNumber(0));
    }
    auto pixel = std::static_pointer_cast<CV::TypeList>(param2Pos);

    auto x = std::static_pointer_cast<CV::TypeNumber>(pos->v[0])->v;
    auto y = std::static_pointer_cast<CV::TypeNumber>(pos->v[1])->v;
    
    

    auto index = (x + y * width) * nchannel;

    for(int i = 0; i < nchannel; ++i){
        data[index + i] = std::static_pointer_cast<CV::TypeNumber>(pixel->v[i]);
    }

    dataCtx->set(dataId, dataCtx->buildNumber(1));
}

extern "C" void _CV_REGISTER_LIBRARY(const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    ctx->registerBinaryFuntion("bmp:load", (void*)__CV_STD_BMP_LOAD);
    ctx->registerBinaryFuntion("bmp:write", (void*)__CV_STD_BMP_WRITE);
    ctx->registerBinaryFuntion("bmp:setp", (void*)__CV_STD_BMP_SET_PIXEL_AT);
}