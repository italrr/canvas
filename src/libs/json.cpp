#include <cmath>
#include <fstream>

#include "../Thirdparty/json11.hpp"
#include "../CV.hpp"

static inline bool isNotFractional(CV_NUMBER d, CV_NUMBER epsilon = 1e-9){
    CV_NUMBER integral_part;
    CV_NUMBER fractional_part = std::modf(d, &integral_part);
    return std::fabs(fractional_part) < epsilon;
}

static CV::Data *buildNumber(const std::shared_ptr<CV::Program> &prog, CV_NUMBER n){
    auto out = new CV::DataNumber(n);
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static CV::Data *buildString(const std::shared_ptr<CV::Program> &prog, const std::string &s){
    auto out = new CV::DataString(s);
    prog->allocateData(out);
    return static_cast<CV::Data*>(out);
}

static json11::Json buildNode(
    const std::string &name,
    const std::shared_ptr<CV::Program> &prog,
    CV::Data *origin,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor
){
    if(origin == NULL){
        cursor->setError(
            "Unhandled Type For JSON",
            "Imperative '"+name+"' was provided a null value",
            token
        );
        return json11::Json();
    }

    switch(origin->type){
        case CV::DataType::STORE: {
            json11::Json::object obj;
            auto store = static_cast<CV::DataStore*>(origin);

            for(const auto &it : store->value){
                auto child = prog->getData(it.second);
                if(child == NULL){
                    cursor->setError(
                        "Unhandled Type For JSON",
                        "Imperative '"+name+"' store field '"+it.first+"' points to dead data id "+std::to_string(it.second),
                        token
                    );
                    return json11::Json();
                }

                obj[it.first] = buildNode(name, prog, child, token, cursor);
                if(cursor->error){
                    return json11::Json();
                }
            }

            return obj;
        }

        case CV::DataType::LIST: {
            json11::Json::array obj;
            auto list = static_cast<CV::DataList*>(origin);

            for(int i = 0; i < static_cast<int>(list->value.size()); ++i){
                auto child = prog->getData(list->value[i]);
                if(child == NULL){
                    cursor->setError(
                        "Unhandled Type For JSON",
                        "Imperative '"+name+"' list item "+std::to_string(i)+" points to dead data id "+std::to_string(list->value[i]),
                        token
                    );
                    return json11::Json();
                }

                obj.push_back(buildNode(name, prog, child, token, cursor));
                if(cursor->error){
                    return json11::Json();
                }
            }

            return obj;
        }

        case CV::DataType::NUMBER: {
            auto v = static_cast<CV::DataNumber*>(origin)->value;
            return isNotFractional(v)
                ? json11::Json(static_cast<int>(v))
                : json11::Json(v);
        }

        case CV::DataType::STRING: {
            return json11::Json(static_cast<CV::DataString*>(origin)->value);
        }

        case CV::DataType::NIL: {
            return json11::Json();
        }

        default: {
            cursor->setError(
                "Unhandled Type For JSON",
                "Imperative '"+name+"' was provided type '"+CV::DataTypeName(origin->type)+"' which is not handled by this JSON library",
                token
            );
            return json11::Json();
        }
    }
}

static CV::Data *unwrapJson(
    const std::string &name,
    const json11::Json &obj,
    const std::shared_ptr<CV::Program> &prog,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor
){
    switch(obj.type()){
        case json11::Json::ARRAY: {
            auto list = new CV::DataList();
            prog->allocateData(list);

            auto arr = obj.array_items();
            for(int i = 0; i < static_cast<int>(arr.size()); ++i){
                auto child = unwrapJson(name, arr.at(i), prog, token, cursor);
                if(cursor->error){
                    return prog->buildNil();
                }

                if(child != NULL){
                    child->incRefCount();
                    list->value.push_back(child->id);
                }
            }

            return static_cast<CV::Data*>(list);
        }

        case json11::Json::OBJECT: {
            auto store = new CV::DataStore();
            prog->allocateData(store);

            json11::Json::object curr{obj.object_items()};
            for(const auto &it : curr){
                auto child = unwrapJson(name, it.second, prog, token, cursor);
                if(cursor->error){
                    return prog->buildNil();
                }

                if(child != NULL){
                    child->incRefCount();
                    store->value[it.first] = child->id;
                }else{
                    store->value[it.first] = 0;
                }
            }

            return static_cast<CV::Data*>(store);
        }

        case json11::Json::NUMBER: {
            return buildNumber(prog, obj.number_value());
        }

        case json11::Json::BOOL: {
            return buildNumber(prog, obj.bool_value() ? 1 : 0);
        }

        case json11::Json::STRING: {
            return buildString(prog, obj.string_value());
        }

        case json11::Json::NUL: {
            return prog->buildNil();
        }

        default: {
            cursor->setError(
                "Unhandled Type For JSON",
                "Imperative '"+name+"' was provided JSON type '"+std::to_string(obj.type())+"' which is not handled by this JSON library",
                token
            );
            return prog->buildNil();
        }
    }
}

static CV::Data *__CV_STD_JSON_WRITE(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail = [&]() -> CV::Data* {
        return buildNumber(prog, 0);
    };

    auto subjectIns = CV::Tools::ErrorCheck::FetchInstruction(name, "subject", args, cursor, st, token);
    if(cursor->error){
        return fail();
    }

    auto subject = CV::Execute(subjectIns, prog, cursor, ctx, st);
    if(cursor->error){
        return fail();
    }

    if(subject->type != CV::DataType::LIST && subject->type != CV::DataType::STORE){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE",
            token
        );
        return fail();
    }

    auto filenameData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "filename", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return fail();
    }

    auto &filename = static_cast<CV::DataString*>(filenameData)->value;

    auto obj = buildNode(name, prog, subject, token, cursor);
    if(cursor->error){
        return fail();
    }

    std::ofstream outFile(filename);
    outFile << json11::Json(obj).dump();
    outFile.close();

    return buildNumber(prog, 1);
}

static CV::Data *__CV_STD_JSON_DUMP(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail = [&]() -> CV::Data* {
        return buildNumber(prog, 0);
    };

    auto subjectIns = CV::Tools::ErrorCheck::FetchInstruction(name, "subject", args, cursor, st, token);
    if(cursor->error){
        return fail();
    }

    auto subject = CV::Execute(subjectIns, prog, cursor, ctx, st);
    if(cursor->error){
        return fail();
    }

    if(subject->type != CV::DataType::LIST && subject->type != CV::DataType::STORE){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE",
            token
        );
        return fail();
    }

    auto obj = buildNode(name, prog, subject, token, cursor);
    if(cursor->error){
        return fail();
    }

    return buildString(prog, json11::Json(obj).dump());
}

static CV::Data *__CV_STD_JSON_PARSE_FILE(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail = [&]() -> CV::Data* {
        return buildNumber(prog, 0);
    };

    auto filenameData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "filename", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return fail();
    }

    auto &filename = static_cast<CV::DataString*>(filenameData)->value;

    if(!CV::Tools::fileExists(filename)){
        cursor->setError(
            "File Not Found",
            "File '"+filename+"' does not exist",
            token
        );
        return fail();
    }

    auto source = CV::Tools::readFile(filename);

    std::string err;
    auto json = json11::Json::parse(source, err);
    if(!err.empty()){
        cursor->setError(
            "Bad JSON Input",
            "Imperative '"+name+"' was provided an invalid JSON input: '"+err+"'",
            token
        );
        return fail();
    }

    return unwrapJson(name, json, prog, token, cursor);
}

static CV::Data *__CV_STD_JSON_PARSE(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    const std::shared_ptr<CV::Token> &token,
    const std::shared_ptr<CV::Cursor> &cursor,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::ControlFlow> &st
){
    auto fail = [&]() -> CV::Data* {
        return buildNumber(prog, 0);
    };

    auto bodyData = CV::Tools::ErrorCheck::SolveInstruction(
        prog, name, "body", args, ctx, cursor, st, token, CV::DataType::STRING
    );
    if(cursor->error){
        return fail();
    }

    auto body = static_cast<CV::DataString*>(bodyData)->value;

    std::string err;
    auto json = json11::Json::parse(body, err);
    if(!err.empty()){
        cursor->setError(
            "Bad JSON Input",
            "Imperative '"+name+"' was provided an invalid JSON input: '"+err+"'",
            token
        );
        return fail();
    }

    return unwrapJson(name, json, prog, token, cursor);
}

extern "C" void _CV_REGISTER_LIBRARY(
    const std::shared_ptr<CV::Program> &prog,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::Cursor> &cursor
){
    (void)cursor;

    ctx->registerFunctionPtr(prog, "json:write", {"subject", "filename"}, __CV_STD_JSON_WRITE);
    ctx->registerFunctionPtr(prog, "json:dump", {"subject"}, __CV_STD_JSON_DUMP);
    ctx->registerFunctionPtr(prog, "json:load", {"filename"}, __CV_STD_JSON_PARSE_FILE);
    ctx->registerFunctionPtr(prog, "json:parse", {"body"}, __CV_STD_JSON_PARSE);
}