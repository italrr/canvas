// SYSTEM INCLUDES
#include <iostream>
#include <stdarg.h>
#include <functional>
#include <sys/stat.h>
#include <fstream>

// LOCAL INCLUDES
#include "CV.hpp"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TOOLS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace CV {
    namespace Tools {

        std::string format(const std::string &_str, ...){
            va_list arglist;
            char buff[1024];
            va_start(arglist, _str);
            vsnprintf(buff, sizeof(buff), _str.c_str(), arglist);
            va_end( arglist );
            return std::string(buff);
        }

        static bool isNumber(const std::string &s){
            return (s.find_first_not_of( "-.0123456789") == std::string::npos) && (s != "-" && s != "--" && s != "+" && s != "++");
        }      

        static bool isString(const std::string &s){
            return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
        }

        static bool isList(const std::string &s){
            if(s.length() < 3){
                return false;
            }
            bool str = false;
            for(unsigned i = 0; i < s.length(); ++i){
                char c = s[i];
                if(c == '\''){
                    str = !str;
                }     
                if(!str && s[i] == ' '){
                    return true;
                }
            }
            return false;
        }     

        static bool isValidVarName(const std::string &name){
            if(name.size() == 0){
                return false;
            }
            return !isNumber(std::string("")+name[0]);
        }

        static bool isReservedWord(const std::string &name){
            static const std::vector<std::string> reserved {
                "let", "bring", "~"
            };
            for(int i = 0; i < reserved.size(); ++i){
                if(reserved[i] == name){
                    return true;
                }
            }
            return false;
        }

        static std::string removeTrailingZeros(double d){
            size_t len = std::snprintf(0, 0, "%.8f", d);
            std::string s(len+1, 0);
            std::snprintf(&s[0], len+1, "%.8f", d);
            s.pop_back();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if(s.back() == '.') {
                s.pop_back();
            }
            return s;
        }

        static std::string compileList(const std::vector<std::string> &strings){
            std::string out;
            for(int i = 0; i < strings.size(); ++i){
                out += strings[i];
                if(i < strings.size()-1){
                    out += " ";
                }
            }
            return out;
        }

        static std::string removeOutterBrackets(const std::string &input){
            if(input.size() < 3){
                return input;
            }
            auto r = input;
            if(input[0] == '['){
                r = std::string(r.begin() + 1, r.end());
            }
            if(input[input.size()-1] == ']'){
                r = std::string(r.begin(), r.end() - 1);
            }
            return r;
        }

        std::string solveEscapedCharacters(const std::string &v){
            std::string result;
            for(int i = 0; i < v.size(); ++i){
                auto c = v[i];
                // process escaped quotation
                if(i < v.size()-1 && v[i] == '\\' && v[i+1] == '\''){
                    result += '\'';
                    ++i;
                    continue;
                }  
                result += c;              
            }
            return result;
        }

        bool fileExists(const std::string &path){
			struct stat tt;
			stat(path.c_str(), &tt);
			return S_ISREG(tt.st_mode);	            
        }

        std::string readFile(const std::string &path){
            std::string buffer;
            std::ifstream file(path);
            for(std::string line; std::getline(file, line);){
                if(line.size() > 0){
                    buffer += line+"\n";
                }
            }    
            return buffer;
        } 

    }
}

static int GEN_ID(){
    static std::mutex access;
    static int v = 0;
    access.lock();
    int r = ++v;
    access.unlock();
    return r;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TYPES
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// BASE
CV::Quant::Quant(){
    this->id = GEN_ID();
    this->type = CV::QuantType::NIL;
}

// NUMBER
CV::TypeNumber::TypeNumber() : CV::Quant() {
    this->type = CV::QuantType::NUMBER;
}

bool CV::TypeNumber::clear(){
    v = 0;
    return true;
}

// STRING
CV::TypeString::TypeString() : CV::Quant() {
    this->type = CV::QuantType::STRING;
}

bool CV::TypeString::clear(){
    v = "";
    return true;
}

// LIST
CV::TypeList::TypeList() : CV::Quant() {
    this->type = CV::QuantType::LIST;
}
bool CV::TypeList::clear(){
    v.clear();
    return true;
}
bool CV::TypeList::isInit(){
    for(int i = 0; i < this->v.size(); ++i){
        if(v[i].get() == NULL){
            return false;
        }
    }
    return true;
}

// STORE
CV::TypeStore::TypeStore(){
    this->type = CV::QuantType::STORE;
}

bool CV::TypeStore::clear(){
    v.clear();
    return true;
}

bool CV::TypeStore::isInit(){
    for(auto &it : this->v){
        if(it.second.get() == NULL){
            return false;
        }
    }
    return true;
}

// FUNCTION
CV::TypeFunction::TypeFunction() : CV::Quant(){
    this->type = CV::QuantType::FUNCTION;
}

bool CV::TypeFunction::clear(){
    return true;
}

// BINARY FUNCTION
CV::TypeFunctionBinary::TypeFunctionBinary(){
    this->type = CV::QuantType::BINARY_FUNCTION;
}

bool CV::TypeFunctionBinary::clear(){
    return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  CURSOR
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CV::Cursor::Cursor(){
    this->error = false;
    this->used = false;
    this->shouldExit = true;
    this->autoprint = true;
}

void CV::Cursor::setError(const std::string &title, const std::string &message, const std::shared_ptr<CV::Token> &subject){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = subject->line;
    this->subject = subject;
    this->error = true;
    accessMutex.unlock();
}

void CV::Cursor::setError(const std::string &title, const std::string &message, int line){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = line;
    this->subject = subject;
    this->error = true;
    accessMutex.unlock();
}

std::string CV::Cursor::getRaised(){
    std::string in = "";
    if(this->subject.get()){
        in = " in '"+this->subject->str()+"'";
        this->line = this->subject->line;
    }
    return "[Line #"+std::to_string(this->line)+"] "+this->title+": "+this->message+in;
}

bool CV::Cursor::raise(){
    if(used){
        return true;
    }
    accessMutex.lock();
    if(!this->error){
        accessMutex.unlock();
        return false;
    }
    // if(this->autoprint){
    //     fprintf(stderr, "%s[Line #%i]%s %s: %s%s%s.\n",
    //             (
    //                 CV::Tools::setTextColor(CV::Tools::Color::BLACK, true) +
    //                 CV::Tools::setBackgroundColor(CV::Tools::Color::WHITE)
    //             ).c_str(),
    //             this->line,
    //             CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str(),
    //             this->title.c_str(),
    //             CV::Tools::setTextColor(CV::Tools::Color::RED, false).c_str(),
    //             this->message.c_str(),
    //             CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str()
    //     );
    // }
    if(this->shouldExit){
        std::exit(1);
    }
    used = true;
    accessMutex.unlock();
    return true;
}

void  CV::Cursor::reset(){
    used = false;
    error = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TYPE TO STRING
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string CV::QuantToText(const std::shared_ptr<CV::Quant> &t){
    switch(t->type){
        
        default:
        case CV::QuantType::NIL: {
            // return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
            return "nil";
        };

        case CV::QuantType::NUMBER: {
            // return  CV::Tools::setTextColor(Tools::Color::CYAN) +
            //         CV::Tools::removeTrailingZeros(static_cast<CV::NumberType*>(item)->get())+
            //         CV::Tools::setTextColor(Tools::Color::RESET);
            return CV::Tools::removeTrailingZeros(std::static_pointer_cast<CV::TypeNumber>(t)->v);
        };

        case CV::QuantType::STRING: {
            // return Tools::setTextColor(Tools::Color::GREEN)+
            //         "'"+static_cast<CV::StringType*>(item)->get()+"'"+
            //         Tools::setTextColor(Tools::Color::RESET);
            // return 

            return "'"+std::static_pointer_cast<CV::TypeString>(t)->v+"'";
        };     

        // case CV::NaturalType::FUNCTION: {
        //     auto fn = static_cast<CV::FunctionType*>(item);      

        //     std::string start = Tools::setTextColor(Tools::Color::RED, true)+"["+Tools::setTextColor(Tools::Color::RESET);
        //     std::string end = Tools::setTextColor(Tools::Color::RED, true)+"]"+Tools::setTextColor(Tools::Color::RESET);
        //     std::string name = Tools::setTextColor(Tools::Color::RED, true)+"fn"+Tools::setTextColor(Tools::Color::RESET);
        //     std::string binary = Tools::setTextColor(Tools::Color::BLUE, true)+"BINARY"+Tools::setTextColor(Tools::Color::RESET);
        //     std::string body = Tools::setTextColor(Tools::Color::BLUE, true)+fn->body.first+Tools::setTextColor(Tools::Color::RESET);

        //     return start+name+" "+start+(fn->variadic ? "args" : Tools::compileList(fn->args))+end+" "+( body )+end;
        // };

        case CV::QuantType::LIST: {
            // std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string output = "[";

            auto list = std::static_pointer_cast<CV::TypeList>(t);

            int total = list->v.size();
            int limit = total > 30 ? 10 : total;
            for(int i = 0; i < limit; ++i){
                auto &q = list->v[i];
                output += CV::QuantToText(q);
                if(i < list->v.size()-1){
                    output += " ";
                }
            }
            if(limit != total){
                // output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);

                output += "...("+std::to_string(total-limit)+" hidden)";

            }
                        
            // return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
            return output + "]";
        };

        case CV::QuantType::STORE: {
            // std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string output = "[";

            auto store = std::static_pointer_cast<CV::TypeStore>(t);

            int total = store->v.size();
            int limit = total > 30 ? 10 : total;
            int i = 0;
            for(auto &it : store->v){
                auto &q = it.second;
                output += CV::QuantToText(q)+":"+it.first;
                if(i < store->v.size()-1){
                    output += " ";
                }
                ++i;
            }
            if(limit != total){
                // output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);

                output += "...("+std::to_string(total-limit)+" hidden)";

            }
                        
            // return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
            return output + "]";
        };        

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PARSING
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::vector<CV::TokenType> parseTokens(std::string input, char sep, std::shared_ptr<CV::Cursor> &cursor){
    std::vector<std::shared_ptr<CV::Token>> tokens; 
    std::string buffer;
    int open = 0;
    bool onString = false;
    bool onComment = false;
    int leftBrackets = 0;
    int rightBrackets = 0;
    int cline = 0;
    int quotes = 0;
    // Count outer brackets
    for(int i = 0; i < input.size(); ++i){
        char c = input[i];
        if(c == '\n'){
            ++cline;
        }else
        if(c == '\\' && i < input.size()-1 && input[i+1] == '\''){
            i += 1;
        }else
        if(c == '\''){
            onString = !onString;
            if(onString){
                ++quotes;
            }else{
                --quotes;
            }
        }else
        if(c == '[' && !onString){
            if(open == 0) ++leftBrackets;
            ++open;
        }else
        if(c == ']' && !onString){
            if(open == 1) ++rightBrackets;
            --open;
        }
    }
    onString = false;
    open = 0;
    // Throw mismatching brackets
    if(leftBrackets != rightBrackets){
        cursor->setError("Syntax Error", "Mismatching brackets", cline+1);
        return {};
    }
    // Throw mismatching quotes
    if(quotes != 0){
        cursor->setError("Syntax Error", "Mismatching quotes", cline);
        return {};
    }
    cline = 1;
    // Add missing brackets for simple statements
    if(((leftBrackets+rightBrackets)/2 > 1) || (leftBrackets+rightBrackets) == 0 || input[0] != '[' || input[input.length()-1] != ']'){
        input = "[" + input + "]";
    }
    // For debugging:
    // std::cout << "To Parse '" << input << "' " << leftBrackets+rightBrackets << std::endl;
    // Parse
    
    for(int i = 0; i < input.size(); ++i){
        char c = input[i];
        if(i < input.size()-1 && c == '\\' && input[i+1] == '\'' && onString){
            buffer += '\\';
            buffer += '\'';
            i += 1;
            continue;
        }else      
        if(c == '\'' && !onComment){
            onString = !onString;
            buffer += c;
        }else        
        if(c == '#' && !onString){
            onComment = !onComment;
        }else
        if(i < input.size()-1 && c == '\\' && input[i+1] == 'n' && onString){
            buffer += '\n';
            ++i;
            continue;
        }else            
        if(c == '\t'){
            buffer +=  ' ';
        }else
        if(c == '\n' && !onString){
            ++cline;
            onComment = false;
            continue;
        }else
        if(onComment){
            continue;
        }else
        if(c == '[' && !onString){
            if(open > 0){
                buffer += c;
            }
            ++open;
        }else
        if(c == ']' && !onString){
            if(open > 1){
                buffer += c;
            }            
            --open;
            if(open == 1 || i == input.size()-1 && buffer.length() > 0){
                // std::cout << "[1] \"" << buffer << "\"" << std::endl;
                if(buffer != ""){
                    tokens.push_back(std::make_shared<CV::Token>(buffer, cline));
                }
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            // std::cout << "[2] " << buffer+c << std::endl;
            if(buffer != ""){
                tokens.push_back(std::make_shared<CV::Token>(buffer+c, cline));
            }
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
            // std::cout << "[3] \"" << buffer << "\"" << std::endl;
            if(buffer != ""){
                tokens.push_back(std::make_shared<CV::Token>(buffer, cline));
            }
            buffer = "";
        }else{
            buffer += c;
        }
    }

    return tokens;
};

static std::vector<CV::TokenType> rebuildTokenHierarchy(const std::vector<CV::TokenType> &input, char sep, CV::CursorType &cursor){
    
    if(input.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, 1);
        return {};
    }

    std::function<CV::TokenType(const CV::TokenType &token)> unwrap = [&](const CV::TokenType &token){
        auto innerTokens = parseTokens(token->first, sep, cursor);
        if(cursor->error){
            return CV::TokenType(NULL);
        }
        auto target = innerTokens[0];

        if(!target->solved){
            target->inner.push_back(unwrap(target));
            target->first = "";
            if(cursor->error){
                return CV::TokenType(NULL);
            }
        }
        for(int i = 1; i < innerTokens.size(); ++i){
            target->inner.push_back(unwrap(innerTokens[i]));
        }
        target->refresh();
        return target;
    };

    std::vector<CV::TokenType> result;
    
    for(int i = 0; i < input.size(); ++i){
        auto built = unwrap(input[i]);
        if(cursor->error){
            return {};
        }
        result.push_back(built);
    }

    return result;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  JIT
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


CV::InsType CV::Translate(const CV::TokenType &token, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    
    if(token->first.size() == 0 && token->inner.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, token);
        return prog->createInstruction(CV::InstructionType::NOOP, token);
    }

    auto bListConstruct = [prog, cursor](const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_LIST, origin);
        auto list = ctx->buildList();
        ins->data.push_back(ctx->id);
        ins->data.push_back(list->id);
        for(int i = 0; i < tokens.size(); ++i){
            auto &inc = tokens[i];
            auto fetched = CV::Translate(inc, prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = origin;
                return prog->createInstruction(CV::InstructionType::NOOP, origin);
            }
            ins->params.push_back(fetched->id);
            list->v.push_back(std::shared_ptr<CV::Quant>(NULL));
        }
        return ins;
    };

    auto bListConstructFromToken = [bListConstruct, prog, cursor](const CV::TokenType &origin, const CV::ContextType &ctx){
        auto cpy = origin->emptyCopy();
        std::vector<CV::TokenType> inners { cpy };
        for(int i = 0; i < origin->inner.size(); ++i){
            inners.push_back(origin->inner[i]);
        }
        return bListConstruct(origin, inners, ctx);
    };

    auto isName = [](const CV::TokenType &token, const CV::ContextType &ctx){
        return ctx->getName(token->first).size() > 0;
    };
    
    auto isNameFunction = [prog](const CV::TokenType &token, const CV::ContextType &ctx){
        if(!token->solved){
            return false;
        }
        if(CV::Tools::isReservedWord(token->first)){
            return true;
        }
        auto nameRef = ctx->getName(token->first);
        if(nameRef.size() == 0){
            return false;
        }
        auto &cctx = prog->ctx[nameRef[0]];
        auto &quant = cctx->memory[nameRef[1]];  
        return quant->type == CV::QuantType::FUNCTION || quant->type == CV::QuantType::BINARY_FUNCTION;
    };    

    if(token->first.size() == 0 && token->inner.size() > 0){
        return bListConstruct(token, token->inner, ctx);
    }else{
        /*
            LET
        */
        if(token->first == "let"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens (let NAME VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto &nameToken = token->inner[0];
            auto &insToken = token->inner[1];

            if(nameToken->inner.size() != 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects NAME as second operand", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }
            auto &name = nameToken->first;
            if(!CV::Tools::isValidVarName(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' second operand '"+name+"' is an invalid name", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                   
            }
            if(CV::Tools::isReservedWord(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' second operand '"+name+"' is a name of native constructor which cannot be overriden", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }            

            auto target = CV::Translate(insToken, prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }         

            auto targetData = ctx->buildNil();

            auto ins = prog->createInstruction(CV::InstructionType::LET, token);
            ins->literal.push_back(name);
            ins->data.push_back(ctx->id);
            ins->data.push_back(targetData->id);
            ins->params.push_back(target->id);

            ctx->names[name] = targetData->id;  

            return ins;
        }else
        /*
            NUMBER
        */
        if(CV::Tools::isNumber(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                auto ins = prog->createInstruction(CV::InstructionType::STATIC_PROXY, token);
                auto data = ctx->buildNumber(std::stod(token->first));
                ins->data.push_back(ctx->id);
                ins->data.push_back(data->id);
                return ins;
            }
        }else
        /*
            STRING
        */
        if(CV::Tools::isString(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{            
                auto ins = prog->createInstruction(CV::InstructionType::STATIC_PROXY, token);
                auto string = ctx->buildString("");
                string->v = token->first.substr(1, token->first.length() - 2);
                ins->data.push_back(ctx->id);
                ins->data.push_back(string->id);
                return ins;
            }
        }else{
            /*
                IS NAME?        
            */            
            auto nameRef = ctx->getName(token->first);
            if(nameRef.size() > 0){
                // TODO: Check for PROMISE names
                auto ctx = prog->ctx[nameRef[0]];
                auto &dataId = nameRef[1];
                auto &quant = ctx->memory[dataId];     

                switch(quant->type){
                    case CV::QuantType::BINARY_FUNCTION: {
                        // Build invokation
                        auto ins = prog->createInstruction(CV::InstructionType::CF_INVOKE_BINARY_FUNCTION, token);
                        auto tempCtx = prog->createContext(ctx); // build context where we store temporary parameters
                        // Where the function is stored
                        ins->data.push_back(ctx->id); // where this function is stored
                        ins->data.push_back(quant->id);
                        // Where the params to the function are stored 
                        ins->data.push_back(tempCtx->id); // With the store parameters to the function
                        ins->data.push_back(token->inner.size());
                        for(int i = 0; i < token->inner.size(); ++i){
                            auto &inc = token->inner[i];
                            auto fetched = CV::Translate(inc, prog, tempCtx, cursor);
                            if(cursor->error){
                                cursor->subject = token;
                                return prog->createInstruction(CV::InstructionType::NOOP, token);
                            }                        
                            ins->params.push_back(fetched->id);
                        }
                        // Build Promise Proxy to store result value 
                        auto childIns = prog->createInstruction(CV::InstructionType::PROMISE_PROXY, token);
                        auto targetData = ctx->buildNil();
                        childIns->data.push_back(ctx->id);
                        childIns->data.push_back(targetData->id);
                        childIns->params.push_back(ins->id);
                        // Tell invoke binary function who they're going to fulfill
                        ins->data.push_back(ctx->id);
                        ins->data.push_back(targetData->id);
                        return childIns;
                    };                 
                    default: {
                        if(token->inner.size() > 0){
                            return bListConstructFromToken(token, ctx);
                        }else{  
                            auto ins = prog->createInstruction(CV::InstructionType::STATIC_PROXY, token);
                            ins->data.push_back(ctx->id);
                            ins->data.push_back(dataId);
                            return ins;
                        }
                    };
                }
            }else{
                cursor->setError(CV_ERROR_MSG_UNDEFINED_IMPERATIVE, "Name '"+token->first+"'", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

        }



    }
    

    return prog->createInstruction(CV::InstructionType::NOOP, token);
}

void CV::Compile(const std::string &input, const CV::ProgramType &prog, CV::CursorType &cursor){


    // Fix outter brackets (Input must always be accompained by brackets)
    auto fixedInput = input;
    if(fixedInput[0] != '[' || fixedInput[fixedInput.size()-1] != ']'){
        fixedInput = "["+fixedInput+"]";
    }
    fixedInput = "["+fixedInput+"]";


    // Split basic tokens
    auto tokens = parseTokens(fixedInput, ' ', cursor);
    if(cursor->error){
        return;
    }

    std::vector<CV::TokenType> root;
    // Build Token hierarchy
    for(int i = 0; i < tokens.size(); ++i){
        auto built = rebuildTokenHierarchy({tokens[i]}, ' ', cursor);
        if(cursor->error){
            return;
        }
        for(int j = 0; j < built.size(); ++j){
            root.push_back(built[j]);
        }
    }
    
    
    // std::cout << "total " << root.size() << std::endl;
    // for(int i = 0; i < root.size(); ++i){
    //     std::cout << "\"" << root[i]->str() << "\" " << root[i]->inner.size() << std::endl;
    // }
    // std::cout << root[0]->inner[0]->inner[0]->str() << std::endl;
    // std::exit(1);

    // Convert tokens into instructions
    std::vector<CV::InsType> instructions;
    for(int i = 0 ; i < root.size(); ++i){
        auto ins = CV::Translate(root[i], prog, prog->rootContext, cursor);
        if(cursor->error){
            return;
        }
        instructions.push_back(ins);
    }
 
    // Order instructions
    for(int i = 1; i < instructions.size(); ++i){
        auto &current = instructions[i];
        auto &previous = instructions[i-1];
        previous->next = current->id;
        current->prev = previous->id;
    }

    // Set entry point (should be first instruction in the list)
    prog->entrypointIns = instructions[0]->id;

}


static std::shared_ptr<CV::Quant> __flow(  const CV::InsType &ins,
                                            const CV::ContextType &ctx,
                                            const CV::ProgramType &prog,
                                            const CV::CursorType &cursor,
                                            const CV::CFType &st
                                        ){
    // Update control flow cursor
    
    st->prev = ins->prev;
    st->current = ins->id;
    st->next = ins->next;

    switch(ins->type){
        /*
            PROXIES
        */
        case CV::InstructionType::STATIC_PROXY: {
            auto ctxId = ins->data[0];
            auto dataId = ins->data[1];
            return prog->ctx[ctxId]->memory[dataId];
        };
        case CV::InstructionType::PROMISE_PROXY: {
            auto cctx = prog->ctx[ins->data[0]];
            auto targetDataId = ins->data[1];
            // Execute (The instruction needs to fulfill the promise on its side)
            auto &entrypoint = prog->instructions[ins->params[0]];
            auto quant = CV::Execute(entrypoint, cctx, prog, cursor);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return cctx->memory[targetDataId];
            }            
            return cctx->memory[targetDataId];
        };        
        case CV::InstructionType::DYNAMIC_PROXY: { // Slow as hell
            auto cctx = prog->ctx[ins->data[0]];
            auto v = cctx->memory[ins->data[1]];
            auto target = v;

            // Optional ins
            if(ins->params.size() > 0 && (v.get() == NULL || !v->isInit())){
                auto quant = CV::Execute(prog->instructions[ins->params[0]], cctx, prog, cursor);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return cctx->buildNil();
                }  
                v = cctx->memory[ins->data[1]];
                target = v;                                  
            }

            for(int i = 0; i < ins->literal.size(); ++i){
                auto &c = ins->literal[i];
                switch(target->type){
                    case CV::QuantType::LIST:{
                        auto list = std::static_pointer_cast<CV::TypeList>(target);
                        if(!CV::Tools::isNumber(c)){
                            st->state = CV::ControlFlowState::CRASH;
                            cursor->setError(CV_ERROR_MSG_INVALID_ACCESOR, "Accessing LIST using non-numerical specifier '"+c+"'", ins->token);
                            return ctx->buildNil();
                        }
                        auto index = std::stoi(c);
                        if(index < 0){
                            st->state = CV::ControlFlowState::CRASH;
                            cursor->setError(CV_ERROR_MSG_INVALID_ACCESOR, "Accessing LIST using negative specifier '"+c+"'", ins->token);
                            return ctx->buildNil();                            
                        }
                        if(index >= list->v.size()){
                            st->state = CV::ControlFlowState::CRASH;
                            cursor->setError(CV_ERROR_MSG_INVALID_ACCESOR, "Accessing LIST using out-bounds specifier '"+c+"' while LIST size is "+std::to_string(list->v.size()), ins->token);
                            return ctx->buildNil();                              
                        }
                        target = list->v[index];
                    };
                    case CV::QuantType::STORE:{
                        auto store = std::static_pointer_cast<CV::TypeStore>(target);
                        if(store->v.count(c) == 0){
                            st->state = CV::ControlFlowState::CRASH;
                            cursor->setError(CV_ERROR_MSG_INVALID_ACCESOR, "Accessing STORE using a non-existing member name '"+c+"'", ins->token);
                            return ctx->buildNil();     
                        }
                        target = store->v[c];
                    };
                }
            }
            return target;
        };            


        /*
            CONSTRUCTORS        
        */
        case CV::InstructionType::CONSTRUCT_LIST: {
            auto &cctx = prog->ctx[ins->data[0]];
            auto &qlist = cctx->memory[ins->data[1]];
            auto list = std::static_pointer_cast<CV::TypeList>(qlist);
            for(int i = 0; i < ins->params.size(); ++i){
                auto quant = CV::Execute(prog->instructions[ins->params[i]], cctx, prog, cursor);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return ctx->buildNil();
                }
                list->v[i] = quant;
            }   
            return std::static_pointer_cast<CV::Quant>(list);
        };
        case CV::InstructionType::CONSTRUCT_STORE: {
            auto &cctx = prog->ctx[ins->data[0]];
            auto &qstore = cctx->memory[ins->data[1]];
            auto store = std::static_pointer_cast<CV::TypeStore>(qstore);
            for(int i = 0; i < ins->params.size(); ++i){
                auto quant = CV::Execute(prog->instructions[ins->params[i]], cctx, prog, cursor);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return ctx->buildNil();
                }
                store->v[ins->literal[i]] = quant;
            }   
            return std::static_pointer_cast<CV::Quant>(store);
        };


        case CV::InstructionType::LET: {
            auto &cctx = prog->ctx[ins->data[0]];
            auto targetDataID = ins->data[1];
            auto &name = ins->literal[0];
            auto &entrypoint = prog->instructions[ins->params[0]];

            auto v = CV::Execute(entrypoint, cctx, prog, cursor);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return v;
            }
            cctx->memory[targetDataID] = v;
            return v;
        };
        
        /*
            CONTROL FLOW        
        */
        case CV::InstructionType::CF_INVOKE_BINARY_FUNCTION: {
            auto &fnData = prog->ctx[ins->data[0]]->memory[ins->data[1]];
            auto fn = std::static_pointer_cast<CV::TypeFunctionBinary>(fnData);

            auto &paramCtx = prog->ctx[ins->data[2]];

            int targetCtxId = ins->data[4];
            int targetDataId = ins->data[5];

            // Compile operands
            int nArgs = ins->data[3];
            std::vector<std::shared_ptr<CV::Instruction>> arguments;
            for(int i = 0; i < nArgs; ++i){
                arguments.push_back(prog->instructions[ins->params[i]]);
            }

            // Cast ref into pointer function
            void (*ref)(CV_BINARY_FN_PARAMS) = (void (*)(CV_BINARY_FN_PARAMS))fn->ref;

            // Invoke it in a temporary context
            auto tempCtx = prog->createContext(paramCtx);
            ref(arguments, fn->name, ins->token, cursor, tempCtx->id, targetCtxId, targetDataId, prog);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
            }             
            // Delete context afterwards. The target value must be within the level above
            prog->deleteContext(tempCtx->id);

            return std::shared_ptr<CV::Quant>(NULL);
        };

        /*
            NOOP
        */
        case CV::InstructionType::NOOP: {
            st->state = CV::ControlFlowState::CRASH;
            return ctx->buildNil();            
        } break;
        /*
            INVALID
        */
        default: {
            cursor->setError(CV_ERROR_MSG_INVALID_INSTRUCTION, "Type '"+std::to_string(ins->type)+"' from token '"+ins->token->str()+"' is undefined/invalid", ins->token);
            st->state = CV::ControlFlowState::CRASH;
            return ctx->buildNil();
        };
    }
}

std::shared_ptr<CV::Quant> CV::Execute(const CV::InsType &entry, const CV::ContextType &ctx, const CV::ProgramType &prog, const CV::CursorType &cursor){

    CV::InsType ins = entry;
    std::shared_ptr<CV::Quant> result;
    auto st = std::make_shared<CV::ControlFlow>();

    while(true){
        result = __flow(ins, ctx, prog, cursor, st);
        if( 
            cursor->error ||
            st->state == CV::ControlFlowState::CRASH ||
            st->state == CV::ControlFlowState::RETURN
        ){
            return result;
        }
        if(ins->next != CV::InstructionType::INVALID){
            ins = prog->instructions[ins->next];
            continue;
        }
        break;
    }
    
    if(cursor->error){
        return ctx->buildNil();
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  CONTEXT
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CV::Context::Context(){
    this->id = GEN_ID();
}

CV::Context::Context(const std::shared_ptr<CV::Context> &head) : Context(){
    this->head = head;
}

std::shared_ptr<CV::TypeNumber> CV::Context::buildNumber(number n){
    auto t = std::make_shared<CV::TypeNumber>();
    this->memory[t->id] = t;
    t->v = n;
    return t;
}

std::shared_ptr<CV::Quant> CV::Context::buildNil(){
    auto t = std::make_shared<CV::Quant>();
    this->memory[t->id] = t;
    return t;
}

std::shared_ptr<CV::TypeString> CV::Context::buildString(const std::string &s){
    auto t = std::make_shared<CV::TypeString>();
    this->memory[t->id] = t;
    t->v = s;
    return t;
}

std::shared_ptr<CV::TypeList> CV::Context::buildList(const std::vector<std::shared_ptr<CV::Quant>> &list){
    auto t = std::make_shared<TypeList>();
    this->memory[t->id] = t;
    for(int i = 0; i < list.size(); ++i){
        t->v.push_back(list[i]);
    }
    return t;
}

std::shared_ptr<CV::TypeStore> CV::Context::buildStore(const std::unordered_map<std::string, std::shared_ptr<CV::Quant>> &store){
    auto t = std::make_shared<TypeStore>();
    this->memory[t->id] = t;
    for(auto &it : store){
        t->v[it.first] = it.second;
    }
    return t;
}

std::shared_ptr<CV::Quant> CV::Context::get(int id){
    return std::shared_ptr<CV::Quant>(NULL);
}

std::shared_ptr<CV::TypeFunctionBinary> CV::Context::registerBinaryFuntion(const std::string &name, void *ref){
    auto fn = std::make_shared<CV::TypeFunctionBinary>();
    fn->ref = ref;
    fn->ctxId = this->id;
    fn->name = name;
    this->memory[fn->id] = fn;
    this->names[name] = fn->id;
    return fn;
}

// Name's context Id and data Id upwards
std::vector<int> CV::Context::getName(const std::string &name){
    return this->names.count(name) > 0 ? std::vector<int>({this->id, this->memory[this->names[name]]->id}) : (this->head.get() ? this->head->getName(name) : std::vector<int>({}));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PROGRAM
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


CV::InsType CV::Program::createInstruction(unsigned type, const std::shared_ptr<CV::Token> &token){
    auto ins = std::make_shared<CV::Instruction>();
    ins->id = GEN_ID();
    ins->type = type;
    ins->token = token;
    this->instructions[ins->id] = ins;
    return ins;
}

std::shared_ptr<CV::Context> CV::Program::createContext(const std::shared_ptr<CV::Context> &head){
    auto nctx = std::make_shared<CV::Context>(head);
    this->ctx[nctx->id] = nctx;
    return nctx;
}

bool CV::Program::deleteContext(int id){
    if(this->ctx.count(id) > 0){
        this->ctx.erase(id); // Careful!! TODO: Check
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  API
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CV::unwrapLibrary(const std::function<bool(const CV::ProgramType &target)> &fn, const CV::ProgramType &target){
    return fn(target);
}

bool CV::getBooleanValue(const std::shared_ptr<CV::Quant> &data){
    switch(data->type){
        case CV::QuantType::NUMBER: {
            auto v = std::static_pointer_cast<CV::TypeNumber>(data);
            return v->v != 0;
        };
        default:
        case CV::QuantType::NIL: {
            return false;
        };
    }
}

void CVInitCore(const CV::ProgramType &prog);

int main(){

    auto cursor = std::make_shared<CV::Cursor>();
    auto program = std::make_shared<CV::Program>();
    program->rootContext = program->createContext();
    
    CVInitCore(program);

    CV::Compile("[[+ 1 2 4 5] 1222 10 [+ 1 1 1 1]]", program, cursor);
    if(cursor->error){
        std::cout << cursor->getRaised() << std::endl;
        std::exit(1);
    }
    auto &entrypoint = program->instructions[program->entrypointIns];
    auto result = CV::Execute(entrypoint, program->rootContext, program, cursor);
    if(cursor->error){
        std::cout << cursor->getRaised() << std::endl;
        std::exit(1);
    }

    std::cout << CV::QuantToText(result) << std::endl;

    std::cout << "exit" << std::endl;

    return 0;

}