#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../Thirdparty/stb_image.h"
#include "../Thirdparty/stb_image_write.h"

#include <vector>
#include <string>

#include "../CV.hpp"

static CV::Data *bmpBuildNumber(const std::shared_ptr<CV::Program> &prog, CV_NUMBER n){
    auto out = new CV::DataNumber(n);
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static CV::Data *bmpBuildString(const std::shared_ptr<CV::Program> &prog, const std::string &s){
    auto out = new CV::DataString(s);
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static CV::Data *bmpBuildList(const std::shared_ptr<CV::Program> &prog){
    auto out = new CV::DataList();
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static CV::Data *bmpBuildStore(const std::shared_ptr<CV::Program> &prog){
    auto out = new CV::DataStore();
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static inline CV::Data *bmpGetStoreField(
    const std::shared_ptr<CV::Program> &prog,
    CV::DataStore *store,
    const std::string &key
){
    auto it = store->value.find(key);
    if(it == store->value.end()){
        return nullptr;
    }
    return prog->getData(it->second);
}

static inline void bmpStoreSet(
    CV::DataStore *store,
    const std::string &key,
    CV::Data *value
){
    if(value){
        value->incRefCount();
        store->value[key] = value->id;
    }
}

static inline void bmpListPush(
    CV::DataList *list,
    CV::Data *value
){
    if(value){
        value->incRefCount();
        list->value.push_back(value->id);
    }
}

static inline bool isValidBmpStore(
    const std::shared_ptr<CV::Program> &prog,
    CV::Data *q,
    std::string &err
){
    if(q == nullptr || q->type != CV::DataType::STORE){
        err = "Provided type is not 'STORE'";
        return false;
    }

    auto store = static_cast<CV::DataStore*>(q);

    auto width = bmpGetStoreField(prog, store, "width");
    if(width == nullptr || width->type != CV::DataType::NUMBER){
        err = "Provided STORE type has no member 'width' or this member is unexpected (Not NUMBER)";
        return false;
    }

    auto height = bmpGetStoreField(prog, store, "height");
    if(height == nullptr || height->type != CV::DataType::NUMBER){
        err = "Provided STORE type has no member 'height' or this member is unexpected (Not NUMBER)";
        return false;
    }

    auto channels = bmpGetStoreField(prog, store, "channels");
    if(channels == nullptr || channels->type != CV::DataType::NUMBER){
        err = "Provided STORE type has no member 'channels' or this member is unexpected (Not NUMBER)";
        return false;
    }

    auto data = bmpGetStoreField(prog, store, "data");
    if(data == nullptr || data->type != CV::DataType::LIST){
        err = "Provided STORE type has no member 'data' or this member is unexpected (Not LIST)";
        return false;
    }

    return true;
}

static inline bool isValidPosition(
    const std::shared_ptr<CV::Program> &prog,
    int mw,
    int mh,
    CV::Data *q,
    std::string &err
){
    if(q == nullptr || q->type != CV::DataType::LIST){
        err = "Provided type is not 'LIST'";
        return false;
    }

    auto pos = static_cast<CV::DataList*>(q);
    if(pos->value.size() != 2){
        err = "Provided position contains "+std::to_string(pos->value.size())+", expected 2 (x and y)";
        return false;
    }

    auto xData = prog->getData(pos->value[0]);
    auto yData = prog->getData(pos->value[1]);

    if(xData == nullptr || xData->type != CV::DataType::NUMBER){
        err = "Type at position 0 is not a number";
        return false;
    }

    if(yData == nullptr || yData->type != CV::DataType::NUMBER){
        err = "Type at position 1 is not a number";
        return false;
    }

    int x = static_cast<int>(static_cast<CV::DataNumber*>(xData)->value);
    int y = static_cast<int>(static_cast<CV::DataNumber*>(yData)->value);

    if(x < 0 || x >= mw){
        err = "X position is out of bounds (0-"+std::to_string(mw - 1)+")";
        return false;
    }

    if(y < 0 || y >= mh){
        err = "Y position is out of bounds (0-"+std::to_string(mh - 1)+")";
        return false;
    }

    return true;
}

static inline bool isValidPixel(
    const std::shared_ptr<CV::Program> &prog,
    int exnchan,
    CV::Data *q,
    std::string &err
){
    if(q == nullptr || q->type != CV::DataType::LIST){
        err = "Provided type is not 'LIST'";
        return false;
    }

    auto pixel = static_cast<CV::DataList*>(q);
    if(static_cast<int>(pixel->value.size()) != exnchan){
        err = "Provided pixel doesn't match the number of channels expected";
        err += " (Provided "+std::to_string(pixel->value.size())+", expected "+std::to_string(exnchan)+")";
        return false;
    }

    for(int i = 0; i < static_cast<int>(pixel->value.size()); ++i){
        auto d = prog->getData(pixel->value[i]);
        if(d == nullptr || d->type != CV::DataType::NUMBER){
            err = "Provided pixel has a non-number member at position "+std::to_string(i);
            return false;
        }
    }

    return true;
}

static CV::Data *__CV_STD_BMP_LOAD(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto failNil = [&]() -> CV::Data* {
        return prog->buildNil();
    };

    auto filenameData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "filename", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return failNil();
    }

    auto filename = static_cast<CV::DataString*>(filenameData)->value;

    if(!CV::Tools::fileExists(filename)){
        cursor->setError("File Not Found", "File '"+filename+"' does not exist", token);
        return failNil();
    }

    int width = 0;
    int height = 0;
    int nrChannels = 0;
    unsigned char *raw = stbi_load(filename.c_str(), &width, &height, &nrChannels, 0);

    if(!raw){
        cursor->setError("Bad Image Format", "File '"+filename+"' is an invalid image or not compatible with this library", token);
        return failNil();
    }

    auto handle = static_cast<CV::DataStore*>(bmpBuildStore(prog));
    auto pixels = static_cast<CV::DataList*>(bmpBuildList(prog));

    bmpStoreSet(handle, "width", bmpBuildNumber(prog, width));
    bmpStoreSet(handle, "height", bmpBuildNumber(prog, height));
    bmpStoreSet(handle, "channels", bmpBuildNumber(prog, nrChannels));
    bmpStoreSet(handle, "data", pixels);

    int total = width * height * nrChannels;
    for(int i = 0; i < total; ++i){
        bmpListPush(pixels, bmpBuildNumber(prog, static_cast<CV_NUMBER>(raw[i])));
    }

    stbi_image_free(raw);
    return static_cast<CV::Data*>(handle);
}

static CV::Data *__CV_STD_BMP_WRITE(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail0 = [&]() -> CV::Data* {
        return bmpBuildNumber(prog, 0);
    };

    auto bmpData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "bmp", args, ctx, cursor, st, token, CV::DataType::STORE
    );
    if(cursor->error){
        return fail0();
    }

    std::string bmpErr;
    if(!isValidBmpStore(prog, bmpData, bmpErr)){
        cursor->setError("Invalid BMP Store", bmpErr, token);
        return fail0();
    }

    auto store = static_cast<CV::DataStore*>(bmpData);

    auto channelsData = static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "channels"));
    auto widthData = static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "width"));
    auto heightData = static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "height"));
    auto listData = static_cast<CV::DataList*>(bmpGetStoreField(prog, store, "data"));

    int nchannel = static_cast<int>(channelsData->value);
    int width = static_cast<int>(widthData->value);
    int height = static_cast<int>(heightData->value);

    auto formatData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "format", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return fail0();
    }

    auto filenameData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "filename", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return fail0();
    }

    auto format = CV::Tools::lower(static_cast<CV::DataString*>(formatData)->value);
    auto filename = static_cast<CV::DataString*>(filenameData)->value;

    int total = width * height * nchannel;
    if(static_cast<int>(listData->value.size()) < total){
        cursor->setError("Invalid BMP Store", "BMP data length is smaller than width*height*channels", token);
        return fail0();
    }

    std::vector<unsigned char> img(total);

    for(int i = 0; i < total; ++i){
        auto d = prog->getData(listData->value[i]);
        if(d == nullptr || d->type != CV::DataType::NUMBER){
            cursor->setError("Invalid BMP Store", "BMP data contains a non-number member at index "+std::to_string(i), token);
            return fail0();
        }
        img[i] = static_cast<unsigned char>(static_cast<int>(static_cast<CV::DataNumber*>(d)->value));
    }

    int ok = 0;

    if(format == "bmp"){
        ok = stbi_write_bmp(filename.c_str(), width, height, nchannel, img.data());
    }else
    if(format == "png"){
        ok = stbi_write_png(filename.c_str(), width, height, nchannel, img.data(), width * nchannel);
    }else
    if(format == "jpg" || format == "jpeg"){
        ok = stbi_write_jpg(filename.c_str(), width, height, nchannel, img.data(), 100);
    }else{
        cursor->setError("Invalid Image Format", "Parameter 'format' ('"+format+"') is invalid or unsupported", token);
        return fail0();
    }

    return bmpBuildNumber(prog, ok ? 1 : 0);
}

static CV::Data *__CV_STD_BMP_SET_PIXEL_AT(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail0 = [&]() -> CV::Data* {
        return bmpBuildNumber(prog, 0);
    };

    auto bmpData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "bmp", args, ctx, cursor, st, token, CV::DataType::STORE
    );
    if(cursor->error){
        return fail0();
    }

    std::string bmpErr;
    if(!isValidBmpStore(prog, bmpData, bmpErr)){
        cursor->setError("Invalid BMP Store", bmpErr, token);
        return fail0();
    }

    auto store = static_cast<CV::DataStore*>(bmpData);

    int nchannel = static_cast<int>(static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "channels"))->value);
    int width = static_cast<int>(static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "width"))->value);
    int height = static_cast<int>(static_cast<CV::DataNumber*>(bmpGetStoreField(prog, store, "height"))->value);
    auto data = static_cast<CV::DataList*>(bmpGetStoreField(prog, store, "data"));

    auto posData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "pos", args, ctx, cursor, st, token, CV::DataType::LIST
    );
    if(cursor->error){
        return fail0();
    }

    std::string posErr;
    if(!isValidPosition(prog, width, height, posData, posErr)){
        cursor->setError("Invalid Position", posErr, token);
        return fail0();
    }

    auto pixelData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "pixel", args, ctx, cursor, st, token, CV::DataType::LIST
    );
    if(cursor->error){
        return fail0();
    }

    std::string pxErr;
    if(!isValidPixel(prog, nchannel, pixelData, pxErr)){
        cursor->setError("Invalid Pixel", pxErr, token);
        return fail0();
    }

    auto pos = static_cast<CV::DataList*>(posData);
    auto pixel = static_cast<CV::DataList*>(pixelData);

    int x = static_cast<int>(static_cast<CV::DataNumber*>(prog->getData(pos->value[0]))->value);
    int y = static_cast<int>(static_cast<CV::DataNumber*>(prog->getData(pos->value[1]))->value);

    int index = (x + y * width) * nchannel;

    for(int i = 0; i < nchannel; ++i){
        auto dst = prog->getData(data->value[index + i]);
        auto src = prog->getData(pixel->value[i]);

        if(dst == nullptr || dst->type != CV::DataType::NUMBER || src == nullptr || src->type != CV::DataType::NUMBER){
            cursor->setError("Invalid BMP Store", "Pixel write target contains invalid number data", token);
            return fail0();
        }

        static_cast<CV::DataNumber*>(dst)->value = static_cast<CV::DataNumber*>(src)->value;
    }

    return bmpBuildNumber(prog, 1);
}

static CV::Data *__CV_STD_BMP_CREATE(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto failNil = [&]() -> CV::Data* {
        return prog->buildNil();
    };

    auto widthData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "width", args, ctx, cursor, st, token, CV::DataType::NUMBER
    );
    if(cursor->error){
        return failNil();
    }

    auto heightData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "height", args, ctx, cursor, st, token, CV::DataType::NUMBER
    );
    if(cursor->error){
        return failNil();
    }

    auto formatData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "format", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return failNil();
    }

    int width = static_cast<int>(static_cast<CV::DataNumber*>(widthData)->value);
    int height = static_cast<int>(static_cast<CV::DataNumber*>(heightData)->value);
    std::string format = static_cast<CV::DataString*>(formatData)->value;

    int nrChannels = 3;

    if(format == "RGB"){
        nrChannels = 3;
    }else
    if(format == "RGBA"){
        nrChannels = 4;
    }else
    if(format == "RG"){
        nrChannels = 2;
    }else{
        cursor->setError("Invalid Bitmap Format", "'"+format+"' is an undefined format", token);
        return failNil();
    }

    auto handle = static_cast<CV::DataStore*>(bmpBuildStore(prog));
    auto pixels = static_cast<CV::DataList*>(bmpBuildList(prog));

    bmpStoreSet(handle, "width", bmpBuildNumber(prog, width));
    bmpStoreSet(handle, "height", bmpBuildNumber(prog, height));
    bmpStoreSet(handle, "channels", bmpBuildNumber(prog, nrChannels));
    bmpStoreSet(handle, "data", pixels);

    int total = width * height * nrChannels;
    for(int i = 0; i < total; ++i){
        bmpListPush(pixels, bmpBuildNumber(prog, 1));
    }

    return static_cast<CV::Data*>(handle);
}

extern "C" void _CV_REGISTER_LIBRARY(
    const std::shared_ptr<CV::Program> &prog,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::Cursor> &cursor
){
    (void)cursor;

    ctx->registerFunctionPtr(prog, "bmp:create", {"width", "height", "format"}, __CV_STD_BMP_CREATE);
    ctx->registerFunctionPtr(prog, "bmp:load", {"filename"}, __CV_STD_BMP_LOAD);
    ctx->registerFunctionPtr(prog, "bmp:write", {"bmp", "format", "filename"}, __CV_STD_BMP_WRITE);
    ctx->registerFunctionPtr(prog, "bmp:setp", {"bmp", "pos", "pixel"}, __CV_STD_BMP_SET_PIXEL_AT);
}