#include <fstream>
#include <set>
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include "CV.hpp"

#define __CV_DEBUG 1
#define SHARED std::shared_ptr
#define VECTOR std::vector

#if __CV_DEBUG == 1
    #include <iostream>
#endif


static const std::string GENERIC_UNDEFINED_SYMBOL_ERROR = "Undefined constructor or name within this context or above";
static bool useColorOnText = true;

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Tools
namespace CV {
    namespace Tools {
        
        #if __CV_DEBUG == 1
        static void printList(const VECTOR<std::string> &list){
            for(unsigned i = 0; i < list.size(); ++i){
                std::cout << "'" << list[i] << "'" << std::endl;
            }
        }
        #endif

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

        namespace UnixColor {

            static std::unordered_map<int, std::string> TextColorCodes = {
                {CV::Tools::Color::BLACK,   "\e[0;30m"},
                {CV::Tools::Color::RED,     "\e[0;31m"},
                {CV::Tools::Color::GREEN,   "\e[0;32m"},
                {CV::Tools::Color::YELLOW,  "\e[0;33m"},
                {CV::Tools::Color::BLUE,    "\e[0;34m"},
                {CV::Tools::Color::MAGENTA, "\e[0;35m"},
                {CV::Tools::Color::CYAN,    "\e[0;36m"},
                {CV::Tools::Color::WHITE,   "\e[0;37m"},
                {CV::Tools::Color::RESET,   "\e[0m"}
            };

            static std::unordered_map<int, std::string> BoldTextColorCodes = {
                {CV::Tools::Color::BLACK,   "\e[1;30m"},
                {CV::Tools::Color::RED,     "\e[1;31m"},
                {CV::Tools::Color::GREEN,   "\e[1;32m"},
                {CV::Tools::Color::YELLOW,  "\e[1;33m"},
                {CV::Tools::Color::BLUE,    "\e[1;34m"},
                {CV::Tools::Color::MAGENTA, "\e[1;35m"},
                {CV::Tools::Color::CYAN,    "\e[1;36m"},
                {CV::Tools::Color::WHITE,   "\e[1;37m"},
                {CV::Tools::Color::RESET,   "\e[0m"}
            };        

            static std::unordered_map<int, std::string> BackgroundColorCodes = {
                {CV::Tools::Color::BLACK,   "\e[40m"},
                {CV::Tools::Color::RED,     "\e[41m"},
                {CV::Tools::Color::GREEN,   "\e[42m"},
                {CV::Tools::Color::YELLOW,  "\e[43m"},
                {CV::Tools::Color::BLUE,    "\e[44m"},
                {CV::Tools::Color::MAGENTA, "\e[45m"},
                {CV::Tools::Color::CYAN,    "\e[46m"},
                {CV::Tools::Color::WHITE,   "\e[47m"},
                {CV::Tools::Color::RESET,   "\e[0m"}
            };     
        }   
  

        static std::string setTextColor(int color, bool bold = false){
            return useColorOnText ? (bold ? UnixColor::BoldTextColorCodes.find(color)->second : UnixColor::TextColorCodes.find(color)->second) : "";
        }

        static std::string setBackgroundColor(int color){
            return useColorOnText ? (UnixColor::BackgroundColorCodes.find(color)->second) : "";
        }

        std::vector<std::string> splitTokenByScope(const CV::Token &token){
            auto colonp = token.first.find(":");
            if(colonp == std::string::npos){
                return {};
            }
            return {
                token.first.substr(0, colonp),
                token.first.substr(colonp+1, token.first.size())
            };
        }

        static bool isScoped(const CV::Token &token){
            bool inString = false;
            bool inComment = false;
            auto &input = token.first;
            for(int i = 0; i < input.length(); ++i){
                char c = input[i];
                if(c == '\''){
                    return false;
                }else
                if(c == '#' && !inComment){
                    return false;
                }else
                if(c == '\n'){
                    return false;
                }else
                if(c == ' '){
                    return false;
                }else
                if(c == ':'){
                    return true;
                }
            }
            return false;
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

#if __CV_DEBUG == 1
    static void printTokenList(const VECTOR<CV::Token> &list){
        for(unsigned i = 0; i < list.size(); ++i){
            std::cout << "'" << list[i].first << "' " << (int)list[i].solved << std::endl;
        }
    }
#endif

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Cursor Stuff

CV::Cursor::Cursor(){
    this->error = false;
    this->used = false;
    this->shouldExit = true;
    this->autoprint = true;
}

void CV::Cursor::setError(const std::string &title, const std::string &message, unsigned line){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = line;
    this->subject = NULL;
    this->error = true;
    accessMutex.unlock();
}

void CV::Cursor::setError(const std::string &title, const std::string &message, unsigned line, CV::Item *subject){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = line;
    this->subject = subject;
    this->error = true;
    this->used = true;
    accessMutex.unlock();
}

std::string CV::Cursor::getRaised(){
    return "[Line #"+std::to_string(this->line)+"] "+this->title+": "+this->message;
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
    if(this->autoprint){
        fprintf(stderr, "%s[Line #%i]%s %s: %s%s%s.\n",
                (
                    CV::Tools::setTextColor(CV::Tools::Color::BLACK, true) +
                    CV::Tools::setBackgroundColor(CV::Tools::Color::WHITE)
                ).c_str(),
                this->line,
                CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str(),
                this->title.c_str(),
                CV::Tools::setTextColor(CV::Tools::Color::RED, false).c_str(),
                this->message.c_str(),
                CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str()
        );
    }
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

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Inner stuff
#include <iostream>
/* -------------------------------------------------------------------------------------------------------------------------------- */
// Types

// ITEM
CV::Item::Item(){
    type = CV::NaturalType::NIL;
    data = NULL;
    size = 0;
    proxy = false;
}

void CV::Item::clear(){
    if(this->proxy){
        return;
    }
    if(this->type == CV::NaturalType::NIL){
        return;
    }
    if(data){
        free(data);
    }
    this->data = NULL;
    this->size = 0;
    this->type = CV::NaturalType::NIL;
}

std::shared_ptr<CV::Item> CV::Item::copy(){
    auto item = std::make_shared<CV::Item>();
    item->type = this->type;
    item->proxy = this->proxy;
    item->size = this->size;
    item->bsize = this->bsize;
    item->id = this->id;
    if(!proxy){
        item->data = malloc(this->bsize);
        memcpy(item->data, this->data, this->bsize);
    }else{
        item->data = this->data;
    }
    return item;
}

void CV::Item::restore(std::shared_ptr<CV::Item> &item){
    this->type = item->type;
    this->size = item->size;
    this->proxy = item->proxy;
    this->bsize = item->bsize;
    this->id = item->id;
    if(!proxy){
        memcpy(this->data, item->data, item->bsize);
    }else{
        this->data = item->data;
    }
}

// NUMBER
void CV::NumberType::set(__CV_DEFAULT_NUMBER_TYPE v){
    if(this->type == CV::NaturalType::NIL){
        clear();
        this->type = CV::NaturalType::NUMBER;
        this->bsize = sizeof(__CV_DEFAULT_NUMBER_TYPE);
        this->data = malloc(sizeof(__CV_DEFAULT_NUMBER_TYPE));
        this->size = sizeof(__CV_DEFAULT_NUMBER_TYPE);
    }
    memcpy(this->data, &v, sizeof(__CV_DEFAULT_NUMBER_TYPE));
}

__CV_DEFAULT_NUMBER_TYPE CV::NumberType::get(){
    if(this->type == CV::NaturalType::NIL){
        fprintf(stderr, "Dereferenced NIL Item %i\n", this->id);
        std::exit(1);
    }
    __CV_DEFAULT_NUMBER_TYPE r;
    memcpy(&r, this->data, sizeof(__CV_DEFAULT_NUMBER_TYPE));
    return r;
}

// STRING
void CV::StringType::set(const std::string &_v){
    if(this->proxy){
        auto castStr = static_cast<std::string*>(this->data);
        *castStr = _v;
        return;
    }    
    clear();
    auto v = CV::Tools::solveEscapedCharacters(_v);
    this->type = CV::NaturalType::STRING;
    this->data = malloc(v.length());
    this->size = v.length();
    this->bsize = v.length();
    for(int i = 0; i < v.size(); ++i){
        auto *c = static_cast<char*>((void*)((size_t)(this->data) + i));
        *c = v[i]; 
    }
}

unsigned CV::StringType::getSize(){
    if(this->proxy){
        auto castStr = static_cast<std::string*>(this->data);
        return (*castStr).size();
    }else{
        return this->size;
    }
}

std::string CV::StringType::get(){
    if(this->proxy){
        auto castStr = static_cast<std::string*>(this->data);
        return *castStr;
    }       
    if(this->type == CV::NaturalType::NIL){
        fprintf(stderr, "Dereferenced NIL Item %i\n", this->id);
        std::exit(1);
    }
    std::string r;
    for(int i = 0; i < this->size; ++i){
        auto *c = static_cast<char*>((void*)((size_t)(this->data) + i));
        r += *c;
    }
    return r;
}

// LIST
void CV::ListType::build(unsigned n){
    clear();
    auto bsize = sizeof(unsigned) * 2 * n;
    this->bsize = bsize;
    this->type = CV::NaturalType::LIST;
    this->size = n;
    this->data = malloc(bsize);
    memset(this->data, '0', bsize);
}

void CV::ListType::build(const std::vector<std::shared_ptr<CV::Item>> &items){
    build(items.size());
    for(int i = 0; i < items.size(); ++i){
        this->set(i, items[i]->ctx, items[i]->id);
    }
}

void CV::ListType::set(unsigned index, unsigned ctxId, unsigned dataId){
    memcpy((void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 0), &ctxId, sizeof(unsigned));
    memcpy((void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 1), &dataId, sizeof(unsigned));
}

std::shared_ptr<CV::Item> CV::ListType::get(const CV::Stack *stack, unsigned index){
    unsigned ctxId, dataId;
    memcpy(&ctxId, (void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 0), sizeof(unsigned));
    memcpy(&dataId, (void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 1), sizeof(unsigned));    
    return stack->getContext(ctxId)->get(dataId);
}

std::shared_ptr<CV::Item> CV::ListType::get(const std::shared_ptr<CV::Stack> &stack, unsigned index){
    return this->get(stack.get(), index);
}

// FUNCTION
std::shared_ptr<CV::Item> CV::FunctionType::copy(){
    return std::shared_ptr<CV::Item>(NULL);
}

void CV::FunctionType::restore(std::shared_ptr<CV::Item> &item){
    // Does nothing (cannot be copied)
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Parser

static VECTOR<CV::Token> parseTokens(std::string input, char sep, SHARED<CV::Cursor> &cursor){
    VECTOR<CV::Token> tokens; 
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
                if(buffer != ""){
                    tokens.push_back(CV::Token(buffer, cline));
                }
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            if(buffer != ""){
                tokens.push_back(CV::Token(buffer+c, cline));
            }
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
            if(buffer != ""){
                tokens.push_back(CV::Token(buffer, cline));
            }
            buffer = "";
        }else{
            buffer += c;
        }
    }

    return tokens;
};

/* -------------------------------------------------------------------------------------------------------------------------------- */
// JIT

static CV::Instruction *compileTokens(const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor);
static CV::Instruction *interpretToken(const CV::Token &token, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, std::shared_ptr<CV::Context> &ctx, SHARED<CV::Cursor> &cursor);

static void DefConstructor(     std::shared_ptr<CV::Stack> &stack,
                                const std::string &name,
                                const std::function<CV::Instruction*(   
                                                                        const std::string &name,
                                                                        const VECTOR<CV::Token> &tokens,
                                                                        const SHARED<CV::Stack> &stack,
                                                                        SHARED<CV::Context> &ctx,
                                                                        SHARED<CV::Cursor> &cursor
                                                                    )> &method
                          ){
    stack->constructors[name] = method;
}

static bool CheckConstructor(const std::shared_ptr<CV::Stack> &stack, const std::string &name){
    return stack->constructors.find(name) != stack->constructors.end();
}

static CV::Instruction *interpretToken(const CV::Token &token, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
    if(!token.solved){
        auto tokens = parseTokens(token.first, ' ', cursor);
        // Update line number
        for(int i = 0; i < tokens.size(); ++i){
            tokens[i].line = token.line;
        }
        return compileTokens(tokens, stack, ctx, cursor);
    }

    auto buildList = [&](){
        auto nins = stack->createInstruction(CV::InstructionType::CONSTRUCT_LIST, tokens[0]);
        auto list = std::make_shared<CV::ListType>();        
        list->build(tokens.size());
        nins->data.push_back(ctx->id);
        nins->data.push_back(ctx->store(stack, list));
        nins->data.push_back(tokens.size());
        for(int i = 0; i < tokens.size(); ++i){
            auto cins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }
            
            nins->parameter.push_back(cins->id);
            nins->data.push_back(ctx->id);
        }
        return nins;      
    };

    auto &imp = token.first;

    // Infere for natural types
    if(imp == "nil" && tokens.size() == 1){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto number = std::make_shared<CV::Item>();
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(stack, number));
        return ins;
    }else    
    if(CV::Tools::isNumber(imp) && tokens.size() == 1){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto number = std::make_shared<CV::NumberType>();
        number->set(std::stod(imp));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(stack, std::static_pointer_cast<CV::Item>(number)));
        return ins;
    }else
    if(CV::Tools::isString(imp) && tokens.size() == 1){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto string = std::make_shared<CV::StringType>();
        string->set(imp.substr(1, imp.length() - 2));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(stack, std::static_pointer_cast<CV::Item>(string)));
        return ins;
    }else
    // Used to dynamically load libraries
    if(imp == "__cv_load_dynamic_library"){
        // look into ./lib/ only for now
        // TODO: add ability to load from system files
        if(tokens.size() != 2){
            cursor->setError(imp, "Expected exactly 1 argument ("+imp+" FILENAME (no .so or .dll))", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }  
        // Pre-process argument 0
        auto p0entry = compileTokens({tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }
        // Execute
        auto p0result = CV::Execute(p0entry, stack, ctx, cursor).value;
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }
        if(p0result->type != CV::NaturalType::STRING){
            cursor->setError(imp, "Argument 0 expected to be STRING", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        auto fname = std::static_pointer_cast<CV::StringType>(p0result)->get();

        std::string path = "./lib/" + fname;
        switch(CV::PLATFORM){
            case CV::SupportedPlatform::LINUX: {
                path += ".so";
            } break; 
            case CV::SupportedPlatform::WINDOWS: {
                path += ".dll";
            } break;
            case CV::SupportedPlatform::OSX: {
                // TODO
            } break;                
            default:
            case CV::SupportedPlatform::UNKNOWN: {
                fprintf(stderr, "%s: Unable to load dyanmic library for this platform (UNKNOWN/UNDEFINED).", imp.c_str());
                std::exit(1);
            } break;                        
        }

        if(!CV::Tools::fileExists(path)){
            cursor->setError(imp, "No dynamic library '"+fname+"' was found", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        // Load library (depending on the platform)
        CV::Tools::importBinaryLibrary(path, fname, stack);

        return stack->createInstruction(CV::InstructionType::NOOP, token);   
    }else
    // Import?
    if(imp == "import"){
        if(tokens.size() != 2){
            cursor->setError(imp, "Expected exactly 1 argument (import NAME)", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }   

        // Pre-process argument 0
        auto p0entry = compileTokens({tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        // Execute
        auto p0result = CV::Execute(p0entry, stack, ctx, cursor).value;
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        if(p0result->type != CV::NaturalType::STRING){
            cursor->setError(imp, "Argument 0 expected to be STRING", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        auto fname = std::static_pointer_cast<CV::StringType>(p0result)->get();

        if(!CV::Tools::fileExists(fname)){
            cursor->setError(imp, "No import named '"+fname+"' was found", token.line);
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        auto file = CV::Tools::readFile(fname);

        // Gather tokens
        auto tokens = parseTokens(file,  ' ', cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        // Compile tokens into its bytecode
        auto entry = compileTokens(tokens, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        // Load it up (execute)
        auto result = CV::Execute(entry, stack, ctx, cursor).value;
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, token);   
        }

        return stack->createInstruction(CV::InstructionType::NOOP, token);   
    }else
    // Is it a constructor?
    if(CheckConstructor(stack, imp)){
        auto cons = stack->constructors[imp];
        return cons(imp, tokens, stack, ctx, cursor);
    }else
    if(auto bf = stack->getRegisteredFunction(stack, token)){        
        auto ins = stack->createInstruction(CV::InstructionType::CF_INVOKE_BINARY_FUNCTION, token);
        auto nctx = stack->createContext(ctx.get());
        ins->data.push_back(nctx->id);
        ins->data.push_back(bf->id);
        for(int i = 1; i < tokens.size(); ++i){
            auto cins = interpretToken(tokens[i], {tokens[i]}, stack, nctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }
            ins->parameter.push_back(cins->id);
        } 
        nctx->solidify();
        return ins;
    }else{
        // Is it a name?
        if(ctx->check(stack, token)){
            auto pair = ctx->getIdByName(stack, token);
            if(pair.ctx == 0 || pair.id == 0){
                cursor->setError("Fatal Error referencing name '"+imp+"'", "Name was not found on this context or above", token.line);
                return stack->createInstruction(CV::InstructionType::NOOP, token);     
            }
            auto markedType = ctx->getMarked(pair.id);

            // Invoke a function
            if(markedType == CV::NaturalType::FUNCTION){
                auto fn = std::static_pointer_cast<CV::FunctionType>(stack->contexts[pair.ctx]->data[pair.id]);
                // Basic Error Checking
                if(!fn->variadic && tokens.size()-1 != fn->args.size()){
                    auto errmsg = "Expects "+std::to_string(fn->args.size())+" argument(s), "+std::to_string(tokens.size()-1)+" were provided";
                    cursor->setError("Function '"+imp+"'", errmsg, token.line);
                    return stack->createInstruction(CV::InstructionType::NOOP, token);     
                }
                // Create temporary context
                auto nctx = stack->createContext(ctx.get());
                // Create instruction
                auto ins = stack->createInstruction(CV::InstructionType::CF_INVOKE_FUNCTION, token);
                ins->data.push_back(pair.ctx);
                ins->data.push_back(pair.id);                
                ins->data.push_back(nctx->id);
                ins->data.push_back(fn->args.size());
                ins->data.push_back(fn->variadic);

                // Gather parameters
                if(fn->variadic){
                    auto list = std::make_shared<CV::ListType>();
                    list->build(tokens.size()-1);
                    auto argId = nctx->store(stack, list);
                    ins->data.push_back(argId);       
                    nctx->setName("@", argId);
                }else{
                    for(int i = 0; i < fn->args.size(); ++i){
                        auto argId = nctx->promise(stack);
                        ins->data.push_back(argId);       
                        auto &name = fn->args[i];
                        nctx->setName(name, argId);
                    }
                }

                // Process Body
                // auto body = interpretToken(fn->body, {fn->body}, stack, nctx, cursor);
                // if(cursor->raise()){
                //     return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
                // } 
                // ins->parameter.push_back(body->id);

                // Process
                for(int i = 1; i < tokens.size(); ++i){
                    auto cins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
                    if(cursor->raise()){
                        return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                    }
                    ins->parameter.push_back(cins->id);
                }                   

                nctx->solidify();
                return ins;
            }else
            // Is it a list?
            if(tokens.size() > 1){
                return buildList();
            }else{
            // Otherwise return type
                auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
                ins->data.push_back(pair.ctx);
                ins->data.push_back(pair.id);
                return ins;                  
            }   
        }else
        // Can it be a list?
        if(tokens.size() > 1){
            return buildList();
        // I just don't know then
        }else{
            cursor->setError("Unexpected Error on '"+imp+"'", GENERIC_UNDEFINED_SYMBOL_ERROR, token.line);
        }
    }
    
    return stack->createInstruction(CV::InstructionType::NOOP, token);
}

static CV::Instruction* compileTokens(const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
    VECTOR<CV::Instruction*> ins;
    // Single token or simple imperative token
    if(tokens.size() == 1 || tokens.size() > 1 && tokens[0].solved){
        auto nins = interpretToken(tokens[0], tokens, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins.push_back(nins);
    }else
    // Chained complex tokens
    if(tokens.size() > 1 && !tokens[0].solved){
        for(int i = 0; i < tokens.size(); ++i){
            auto nins = interpretToken(tokens[i], tokens, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }            
            if(i > 0){
                nins->previous = ins[ins.size()-1]->id;
                ins[ins.size()-1]->next = nins->id;
            }
            ins.push_back(nins);
        }
    }else{
        if(tokens.size() == 0){
            fprintf(stderr, "Invalid syntax used or malformed token\n");
            std::exit(1);
        }
        cursor->setError("Failed to parse token", "Invalid syntax used or malformed token", tokens[0].line);
        return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
    }

    return ins[0];
}

static void __addStandardConstructors(std::shared_ptr<CV::Stack> &stack){
    /*
    
        STANDARD CONSTRUCTORS

        Constructors are embedded instructions that enable a variety of fundamental features such
        as the ability to store a variable into memory (current context) using a name, mutate already existing variables,
        invoke interruptors, flow control, defining functions, and of course, native operators such as sum, sub, mult and div.

    */


    /*
        LET
    */
    DefConstructor(stack, "let", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" NAME VALUE)", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        
        // TODO: Check for valid variable names

        // Interpret data instruction
        auto dataIns = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }

        // Allocate promise in current context
        auto dataId = 0;
        unsigned type; 
        if( dataIns->type == CV::InstructionType::STATIC_PROXY ||
            dataIns->type == CV::InstructionType::PROMISE_PROXY || 
            dataIns->type == CV::InstructionType::REFERRED_PROXY ||
            dataIns->type == CV::InstructionType::STORE_PROXY){
            dataId = dataIns->data[1];
            type = 0; // direct, no need for promise
        }else{
            dataId = ctx->promise(stack);
            type = 1; // needs to be executed
        }
        ctx->setName(tokens[1].first, dataId);

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::LET, tokens[1]);
        ins->data.push_back(type);
        ins->data.push_back(ctx->id);
        ins->data.push_back(dataId);
        ins->parameter.push_back(dataIns->id);

        return ins;
    });


    /*
        MUT
    */
    DefConstructor(stack, "mut", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" NAME VALUE)", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process item to replace
        auto originIns = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }

        // Interpret replacement data instruction
        auto dataIns = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::MUT, tokens[1]);
        ins->parameter.push_back(originIns->id);
        ins->parameter.push_back(dataIns->id);

        return ins;
    }); 


    /*
        FN
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "fn", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){

        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" [args] [code])", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }

        bool variadic = false;
        std::vector<std::string> argNames;
        
        // Get find arguments
        auto argTokens = parseTokens(tokens[1].first, ' ', cursor);
        if(argTokens.size() == 1 && argTokens[0].first == ".."){
            variadic = true;
            argNames.push_back("@");            
        }else{
            for(int i = 0; i < argTokens.size(); ++i){
                argNames.push_back(argTokens[i].first);
            }
        }

        // Create Item
        auto fn = std::make_shared<CV::FunctionType>();
        fn->type = CV::NaturalType::FUNCTION;
        fn->variadic = variadic;
        fn->args = argNames;
        fn->body = tokens[2];
        auto fnId = ctx->store(stack, fn);
        
        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, tokens[0]);
        ins->data.push_back(ctx->id);
        ins->data.push_back(fnId);
        ctx->markPromise(fnId, CV::NaturalType::FUNCTION);

        return ins;   
        
    });           

    /*
        NTH
    */
    DefConstructor(stack, "nth", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process index
        auto index = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       
        // Process 
        auto list = interpretToken(tokens[2], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }              

        // Create Context
        auto ins = stack->createInstruction(CV::InstructionType::NTH_PROXY, tokens[0]);

        ins->parameter.push_back(index->id);
        ins->parameter.push_back(list->id);
        
        return ins;
    });  

    /*
        length
    */
    DefConstructor(stack, "length", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 2){
            cursor->setError(name, "Expects exactly 1 argument", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process subject
        auto subject = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       
        // Process code
        auto ins = stack->createInstruction(CV::InstructionType::OP_DESC_LENGTH, tokens[0]);
        ins->parameter.push_back(subject->id);
        return ins;
    });   


    /*
        type
    */
    DefConstructor(stack, "type", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 2){
            cursor->setError(name, "Expects exactly 1 argument", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process subject
        auto subject = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       
        // Process code
        auto ins = stack->createInstruction(CV::InstructionType::OP_DESC_TYPE, tokens[0]);
        ins->parameter.push_back(subject->id);
        return ins;
    });          


    /*
        >>
    */
    DefConstructor(stack, ">>", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
          
        auto ins = stack->createInstruction(CV::InstructionType::OP_LIST_PUSH, tokens[0]);

        // Process items
        for(int i = 1; i < tokens.size(); ++i){
            auto item = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[i]);
            }        
            ins->parameter.push_back(item->id);
        }

        return ins;
    });    



    /*
        <<
    */
    DefConstructor(stack, "<<", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
          
        auto ins = stack->createInstruction(CV::InstructionType::OP_LIST_POP, tokens[0]);

        // Process items
        for(int i = 1; i < tokens.size(); ++i){
            auto item = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[i]);
            }        
            ins->parameter.push_back(item->id);
        }

        return ins;
    });         


    /*
        list-ranged
    */
    DefConstructor(stack, "list-ranged", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        static const std::string guide = name+" START END (at 1)";
        if(tokens.size() != 3 && tokens.size() != 5){
            cursor->setError(name, "Expects at least 2 arguments ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }

        if(tokens.size() == 5 && tokens[4].first == "at"){
            cursor->setError(name, "Expected word 'at' as argument 3 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);               
        }

        // start
        auto start = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }        

        // end
        auto end = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }  

        unsigned stepIns = 0;

        // step
        if(tokens.size() == 5){
            auto step = interpretToken(tokens[4], {tokens[4]}, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
            }
            stepIns = step->id;            
        }

        auto dataId = ctx->promise(stack);


        // Create instruction
        auto consIns = stack->createInstruction(CV::InstructionType::CONSTRUCT_RANGED_LIST, tokens[0]);
        consIns->data.push_back(ctx->id);
        consIns->data.push_back(dataId);
        consIns->parameter.push_back(start->id);
        consIns->parameter.push_back(end->id);
        consIns->parameter.push_back(stepIns);

        // Create proxy instruction
        auto ins = stack->createInstruction(CV::InstructionType::PROMISE_PROXY, tokens[0]);
        ins->data.push_back(ctx->id);
        ins->data.push_back(dataId);
        ins->parameter.push_back(consIns->id);
        return ins;
    });     


    /*
        list-bulk
    */
    DefConstructor(stack, "list-bulk", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        static const std::string guide = name+" AMOUNT VALUE";
        if(tokens.size() != 3){
            cursor->setError(name, "Expects at least 2 arguments ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }

        // amount
        auto amount = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }        

        // value
        auto value = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }  

        auto dataId = ctx->promise(stack);
        // Create instruction
        auto consIns = stack->createInstruction(CV::InstructionType::CONSTRUCT_BULK_LIST, tokens[0]);
        consIns->data.push_back(ctx->id);
        consIns->data.push_back(dataId);
        consIns->parameter.push_back(amount->id);
        consIns->parameter.push_back(value->id);

        // Create proxy instruction
        auto ins = stack->createInstruction(CV::InstructionType::PROMISE_PROXY, tokens[0]);
        ins->data.push_back(ctx->id);
        ins->data.push_back(dataId);
        ins->parameter.push_back(consIns->id);
        return ins;
    });   

    /*
        do
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "do", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Create Context
        auto nctx = stack->createContext(ctx.get());

        // Process conditional branch
        auto condition = interpretToken(tokens[1], {tokens[1]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        } 

        // Process code
        auto branch = interpretToken(tokens[2], {tokens[2]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }        

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::CF_LOOP_DO, tokens[0]);
        ins->data.push_back(nctx->id);
        ins->parameter.push_back(condition->id);
        ins->parameter.push_back(branch->id);

        // Seals it
        nctx->solidify();

        return ins;
    }); 

    /*
        iter
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "iter", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        static const std::string guide = name+" VAR in SOURCE (at X) [CODE]";
        if(tokens.size() != 5 && tokens.size() != 7){
            cursor->setError(name, "Expects at least 4 arguments ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }        

        if(tokens[2].first != "in"){
            cursor->setError(name, "Expected word 'in' as argument 2 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);            
        }

        if(tokens.size() == 7 && tokens[4].first != "at"){
            cursor->setError(name, "Expected word 'at' as argument 4 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);             
        }

        auto nctx = stack->createContext(ctx.get());
        
        // TODO: verify var name
        auto stepperId = nctx->promise(stack);
        nctx->setName(tokens[1].first, stepperId);
        
        auto ins = stack->createInstruction(CV::InstructionType::CF_LOOP_ITER, tokens[0]);
        ins->data.push_back(nctx->id);
        ins->data.push_back(stepperId);

        // Process source for iter
        auto source = interpretToken(tokens[3], {tokens[3]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins->parameter.push_back(source->id);
        // Process code
        auto code = interpretToken(tokens[tokens.size()-1], {tokens[tokens.size()-1]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins->parameter.push_back(code->id);
        
        // Process counter
        if(tokens.size() == 7){
            auto stepperCountId = interpretToken(tokens[5], {tokens[5]}, stack, nctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }
            ins->parameter.push_back(stepperCountId->id);
        }else{
            ins->parameter.push_back(0);
        }

        // Seal context
        nctx->solidify();

        return ins;
    });   

    /*
        for
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "for", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        //(for i from X to Y at 2 [CODE])
        //(for i from X to Y [CODE])

        static const std::string guide = name+" VAR from NUMBER to NUMBER (at X) [CODE]";
        if(tokens.size() != 7 && tokens.size() != 9){
            cursor->setError(name, "Expects at least 4 arguments ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }        

        if(tokens[2].first != "from"){
            cursor->setError(name, "Expected word 'from' as argument 1 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);            
        }

        if(tokens[4].first != "to"){
            cursor->setError(name, "Expected word 'to' as argument 3 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);            
        }        

        if(tokens.size() == 9 && tokens[6].first != "at"){
            cursor->setError(name, "Expected word 'at' as argument 6 ("+guide+")", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);             
        }

        auto nctx = stack->createContext(ctx.get());
        
        auto ins = stack->createInstruction(CV::InstructionType::CF_LOOP_FOR, tokens[0]);

        // TODO: verify var name

        // Stepper name
        auto step = std::make_shared<CV::NumberType>();
        step->set(0);
        auto stepperId = nctx->store(stack, step);
        nctx->setName(tokens[1].first, stepperId);    
        ins->data.push_back(nctx->id);
        ins->data.push_back(stepperId);

        // from instruction
        auto fromIns = interpretToken(tokens[3], {tokens[3]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins->parameter.push_back(fromIns->id);

        // to instruction
        auto toIns = interpretToken(tokens[5], {tokens[5]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins->parameter.push_back(toIns->id);

        // Code
        auto code = interpretToken(tokens[tokens.size()-1], {tokens[tokens.size()-1]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins->parameter.push_back(code->id);        

        // Count amount
        if(tokens.size() == 9){
            auto stepperCountId = interpretToken(tokens[7], {tokens[7]}, stack, nctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }
            ins->parameter.push_back(stepperCountId->id);
        }else{
            ins->parameter.push_back(0);
        }

        // Seal context
        nctx->solidify();

        return ins;
    });           

    auto buildInterrupt = [&](const std::string &name, unsigned TYPE){
        DefConstructor(stack, name, [TYPE](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
            if(tokens.size() > 2){
                cursor->setError(name, "Expects no more than 1 argument", tokens[0].line);
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }

            CV::Instruction *payload = NULL;
            if(tokens.size() > 1){
                payload = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
                if(cursor->raise()){
                    return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                }                    
            }

            auto ins = stack->createInstruction(TYPE, tokens[0]);
            ins->data.push_back(payload != NULL);
            if(payload){
                ins->parameter.push_back(payload->id);
            }

            return ins;
        });
    }; 
    buildInterrupt("return", CV::InstructionType::CF_INT_RETURN);
    buildInterrupt("yield", CV::InstructionType::CF_INT_YIELD);
    buildInterrupt("skip", CV::InstructionType::CF_INT_SKIP);

    /*
        IF
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "if", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Create Context
        auto nctx = stack->createContext(ctx.get());

        // Process true branch
        auto condition = interpretToken(tokens[1], {tokens[1]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        } 

        // Process false branch
        auto trueBranch = interpretToken(tokens[2], {tokens[2]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }        

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::CF_COND_BINARY_BRANCH, tokens[0]);
        ins->data.push_back(nctx->id);
        ins->parameter.push_back(condition->id);
        ins->parameter.push_back(trueBranch->id);

        // Is there false branch?
        if(tokens.size() >= 4){
            auto falseBranch = interpretToken(tokens[3], {tokens[3]}, stack, nctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
            }
            ins->parameter.push_back(falseBranch->id);
        }

        // Seals it
        nctx->solidify();

        return ins;
    });     

    /*
        EMBEDDED OPERATORS (LOGIC & ARITHMETIC)
    */
   
    auto buildOp = [&](const std::string &name, unsigned TYPE, unsigned minArgs = 2){
        DefConstructor(stack, name, [TYPE, minArgs](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
            if(tokens.size() < minArgs+1){
                cursor->setError(name, "Expects at least "+std::to_string(minArgs)+" arguments", tokens[0].line);
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }

            auto result = stack->createInstruction(CV::InstructionType::REFERRED_PROXY, tokens[0]);

            auto data = std::make_shared<CV::NumberType>();
            data->set(0);
            auto dataId = ctx->store(stack, data);
            result->data.push_back(ctx->id);
            result->data.push_back(dataId);

            auto actuator = stack->createInstruction(TYPE, tokens[0]);
            actuator->data.push_back(ctx->id);
            actuator->data.push_back(dataId);
            for(int i = 1; i < tokens.size(); ++i){
                auto ins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
                if(cursor->raise()){
                    return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                }            
                actuator->parameter.push_back(ins->id);
            }

            result->parameter.push_back(actuator->id);

            return result;
        });
    };   
    buildOp("+", CV::InstructionType::OP_NUM_ADD);
    buildOp("-", CV::InstructionType::OP_NUM_SUB);
    buildOp("*", CV::InstructionType::OP_NUM_MULT);
    buildOp("/", CV::InstructionType::OP_NUM_DIV);
    buildOp("eq", CV::InstructionType::OP_COND_EQ);
    buildOp("neq", CV::InstructionType::OP_COND_NEQ);
    buildOp("lt", CV::InstructionType::OP_COND_LT);
    buildOp("lte", CV::InstructionType::OP_COND_LTE);
    buildOp("gt", CV::InstructionType::OP_COND_GT);
    buildOp("gte", CV::InstructionType::OP_COND_GTE);
    buildOp("and", CV::InstructionType::OP_LOGIC_AND);
    buildOp("or", CV::InstructionType::OP_LOGIC_OR);

    DefConstructor(stack, "not", [](const std::string &name, const VECTOR<CV::Token> &tokens, const SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 2){
            cursor->setError(name, "Expects exactly 1 argument", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }

        auto result = stack->createInstruction(CV::InstructionType::REFERRED_PROXY, tokens[0]);

        auto data = std::make_shared<CV::NumberType>();
        data->set(0);
        auto dataId = ctx->store(stack, data);
        result->data.push_back(ctx->id);
        result->data.push_back(dataId);

        // Instruction
        auto actuator = stack->createInstruction(CV::InstructionType::OP_LOGIC_NOT, tokens[0]);
        actuator->data.push_back(ctx->id);
        actuator->data.push_back(dataId);
        auto ins = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }            
        actuator->parameter.push_back(ins->id);


        result->parameter.push_back(actuator->id);

        return result;
    });    
}


/* -------------------------------------------------------------------------------------------------------------------------------- */
// Context

CV::Context::Context(){
    top = NULL;
    solid = false;
}

void CV::Context::deleteData(unsigned id){
    // TODO
}

unsigned CV::Context::store(CV::Stack *stack, const std::shared_ptr<CV::Item> &item){
    if(solid){
        // fprintf(stderr, "Context %i: Tried storing new Item %i in a solid context\n", this->id, item->id);
        // std::exit(1);           
    }
    item->id = stack->generateId();
    item->ctx = this->id;
    this->data[item->id] = item;
    return item->id;
}

unsigned CV::Context::store(const std::shared_ptr<CV::Stack> &stack, const std::shared_ptr<CV::Item> &item){
    return store(stack.get(), item);
}

unsigned CV::Context::promise(CV::Stack *stack){
    auto id = stack->generateId();
    this->data[id] = NULL;
    return id;
}

unsigned CV::Context::promise(const std::shared_ptr<CV::Stack> &stack){
    return promise(stack.get());
}


void CV::Context::markPromise(unsigned id, unsigned type){
    this->markedItems[id] = type;
}

unsigned CV::Context::getMarked(unsigned id){
    auto it = this->markedItems.find(id);
    if(it == this->markedItems.end()){
        return this->top ? this->top->getMarked(id) : CV::NaturalType::UNDEFINED;
    }
    return it->second;
}

void CV::Context::setPromise(unsigned id, std::shared_ptr<CV::Item> &item){
    if(this->data.count(id) == 0){
        fprintf(stderr, "Context %i: Closing Promise using non-existing ID %i\n", this->id, id);
        std::exit(1);        
    }
    this->data[id] = item;
}

CV::ContextDataPair CV::Context::getIdByName(const std::shared_ptr<CV::Stack> &stack, const CV::Token &token){
    auto it = this->dataIds.find(token.first);
    if(it == this->dataIds.end()){
        return this->top ? this->top->getIdByName(stack, token) : CV::ContextDataPair();
    }
    return CV::ContextDataPair(this->id, it->second);
}

bool CV::Context::check(const std::shared_ptr<CV::Stack> &stack, const CV::Token &token){
    auto dv = this->dataIds.count(token.first);
    auto bfv = stack->bfIds.count(token.first);      
    if(dv == 0 && bfv == 0){
        return this->top && this->top->check(stack, token);
    }
    return true;
}

void CV::Context::setName(const std::string &name, unsigned id){
    auto it = this->data.find(id);
    if(it == this->data.end()){
        fprintf(stderr, "Context %i: Naming non-existing Item %i\n", this->id, id);
        std::exit(1);
    }
    auto item = it->second;
    this->dataIds[name] = id;
}

void CV::Context::deleteName(unsigned id){
    for(auto &it : this->dataIds){
        if(it.second == id){
            this->dataIds.erase(it.first);
            return;
        }
    }
}

std::shared_ptr<CV::Item> CV::Context::getByName(const std::string &name){
    auto it = this->dataIds.find(name);
    if(it == this->dataIds.end()){
        return this->top ? this->top->getByName(name) : NULL;
    }
    return this->data[it->second];
}

std::shared_ptr<CV::Item> CV::Context::get(unsigned id){
    auto it = this->data.find(id);
    if(it == this->data.end()){
        return this->top ? this->top->get(id) : NULL;
    }
    return it->second;
}

void CV::Context::clear(){
    std::set<std::shared_ptr<CV::Item>> uniqueItems;
    for(auto &it : this->data){
        if(it.second == NULL){
            continue;
        }
        uniqueItems.insert(it.second);
        auto item = it.second;
        item->clear();
    }
    this->data.clear();
    this->dataIds.clear();
    // Clear originalData
    for(int i = 0; i < this->originalData.size(); ++i){
        if(uniqueItems.count(this->originalData[i]) > 0){
            continue;
        }
        auto item = this->originalData[i];
        item->clear();
    }
    uniqueItems.clear();
    this->originalData.clear();
}

std::shared_ptr<CV::Item> CV::Context::buildNil(const std::shared_ptr<CV::Stack> &stack){
    auto nil = std::make_shared<CV::Item>();
    this->store(stack, nil);
    return nil;
}

std::shared_ptr<CV::Item> CV::Context::buildString(const std::shared_ptr<CV::Stack> &stack, const std::string &v){
    auto str = std::make_shared<CV::StringType>();
    str->set(v);
    this->store(stack, str);
    return str;
}

std::shared_ptr<CV::Item> CV::Context::buildNumber(const std::shared_ptr<CV::Stack> &stack, __CV_DEFAULT_NUMBER_TYPE n){
    auto number = std::make_shared<CV::NumberType>();
    number->set(n);
    this->store(stack, number);
    return number;
}

std::shared_ptr<CV::Item> CV::Context::buildProxyNumber(const std::shared_ptr<CV::Stack> &stack, void *ref){
    auto number = std::make_shared<CV::NumberType>();
    number->type = CV::NaturalType::NUMBER;
    number->proxy = true;
    number->size = sizeof(__CV_DEFAULT_NUMBER_TYPE);
    number->bsize = sizeof(__CV_DEFAULT_NUMBER_TYPE);
    number->data = ref;
    this->store(stack, number);
    return number;
}

std::shared_ptr<CV::Item> CV::Context::buildProxyString(const std::shared_ptr<CV::Stack> &stack, void *ref){
    auto str = std::make_shared<CV::StringType>();
    str->type = CV::NaturalType::STRING;
    str->proxy = true;
    str->data = ref;
    this->store(stack, str);
    return str;
}

void CV::Context::transferFrom(CV::Stack *stack, std::shared_ptr<CV::Item> &item){
    // if it's here already, then we skip this
    if(this->data.count(item->id) == 1){ 
        return;
    }
    // Find and store it here
    auto ctx = stack->contexts[item->ctx];
    item->ctx = this->id;
    this->data[item->id] = item;
    // Remove from other
    ctx->data.erase(id);
    deleteName(item->id);
}

void CV::Context::transferFrom(std::shared_ptr<CV::Stack> &stack, std::shared_ptr<CV::Item> &item){
    transferFrom(stack.get(), item);
}

void CV::Context::solidify(){
    if(this->solid){
        return;
    }
    for(auto &it : this->data){ 
        if(it.second == NULL){
            continue;
        }else{
            originalData.push_back(it.second->copy());
        }
    }
    solid = true;
}

void CV::Context::revert(){
    for(int i = 0; i < this->originalData.size(); ++i){
        auto backup = this->originalData[i];
        auto local = this->data[backup->id];
        local->restore(backup);
    }
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Stack Management

CV::Stack::Stack(){
    this->lastGenId = 100;
}

unsigned CV::Stack::generateId(){
    idGeneratorMutex.lock();
    auto id = lastGenId;
    ++lastGenId;
    idGeneratorMutex.unlock();
    return id;
}

CV::Instruction *CV::Stack::createInstruction(unsigned type, const CV::Token &token){
    auto *ins = new CV::Instruction();
    ins->id = this->generateId();
    ins->type = type;
    ins->token = token;
    this->instructions[ins->id] = ins;
    return ins;
}

std::shared_ptr<CV::Context> CV::Stack::createContext(CV::Context *top){
    auto ctx = std::make_shared<CV::Context>();
    ctx->id = this->generateId();
    ctx->top = top;
    this->contexts[ctx->id] = ctx;
    return ctx;
}

std::shared_ptr<CV::Context> CV::Stack::getContext(unsigned id) const {
    auto it = this->contexts.find(id);
    return it->second;
}

CV::BinaryFunction *CV::Stack::getRegisteredFunction(const std::shared_ptr<CV::Stack> &stack, const CV::Token &token){
    auto it = this->bfIds.find(token.first);
    if(it == this->bfIds.end()){
        return NULL;
    }
    return this->binaryFunctions[it->second].get();
}

void CV::Stack::registerFunction(const std::string &name, const std::function<std::shared_ptr<CV::Item>(const std::string &name, const CV::Token &token, std::vector<std::shared_ptr<CV::Item>>&, std::shared_ptr<CV::Context>&, std::shared_ptr<CV::Cursor> &cursor)> &fn){
    auto bf = std::make_shared<CV::BinaryFunction>();
    bf->id = this->generateId();
    bf->fn = fn;
    bf->name = name;
    this->binaryFunctions[bf->id] = bf;
    this->bfIds[name] = bf->id;

}

void CV::Stack::setTopContext(std::shared_ptr<CV::Context> &ctx){
    this->topCtx = ctx;
}

void CV::Stack::deleteContext(unsigned id){
    auto it = this->contexts.find(id);
    if(it == this->contexts.end()){
        fprintf(stderr, "Context %i: Failed to find such context\n", id);
        std::exit(1);
    }
    auto ctx = it->second;
    ctx->clear();
    this->contexts.erase(it);
}

void CV::Stack::garbageCollect(){
    int count = 0;
    for(auto &itctx : this->contexts){
        auto &ctx = itctx.second;
        for(auto &itdata : ctx->data){
            auto &data = itdata.second;
            if(data.unique()){
                ctx->data.erase(data->id);
                ++count;
            }
        }
    }
}

static CV::ControlFlow __execute(const std::shared_ptr<CV::Stack> &stack, CV::Instruction *ins, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    switch(ins->type){
        default:
        case CV::InstructionType::INVALID: {
            fprintf(stderr, "Instruction ID %i of invalid type (%i)\n", ins->id, ins->type);
            std::exit(1);
        } break;
        case CV::InstructionType::NOOP: {
            return CV::ControlFlow(ctx->buildNil(stack));
        };

        /*
            CONSTRUCTRORS 
        */
        case CV::InstructionType::LET: {
            auto &type = ins->data[0];
            auto &ctxId = ins->data[1];
            auto &dataId = ins->data[2];
            
            auto v = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            
            if(type != 0){
                stack->contexts[ctxId]->setPromise(dataId, v.value);
            }

            return v;
        };
        case CV::InstructionType::MUT: {

            auto target = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            auto v = CV::Execute(stack->instructions[ins->parameter[1]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            if(target->type != v->type){
                cursor->setError("Illegal Mutation '"+ins->token.first+"'", "Can only mutate variables of the same type", ins->token.line);
                return ctx->buildNil(stack);
            }else
            if(target->type != CV::NaturalType::NUMBER && target->type != CV::NaturalType::STRING){
                cursor->setError("Illegal Mutation '"+ins->token.first+"'", "Only natural types 'STRING' and 'NUMBER' may be mutated", ins->token.line);
                return ctx->buildNil(stack);                
            }

            switch(target->type){
                case CV::NaturalType::NUMBER: {
                    auto original = std::static_pointer_cast<CV::NumberType>(target);
                    auto newValue = std::static_pointer_cast<CV::NumberType>(v);
                    original->set(newValue->get());
                } break;
                case CV::NaturalType::STRING: {
                    auto original = std::static_pointer_cast<CV::StringType>(target);
                    auto newValue = std::static_pointer_cast<CV::StringType>(v);                    
                    original->set(newValue->get());
                } break;                
            }

            return target;
        };
        case CV::InstructionType::CONSTRUCT_LIST: {
            auto &lsCtxId = ins->data[0];
            auto &lsDataId = ins->data[1];            
            auto list = std::static_pointer_cast<CV::ListType>(stack->contexts[lsCtxId]->data[lsDataId]);

            unsigned nelements = ins->data[2];

            unsigned c = 0;
            
            for(int i = 0; i < nelements; ++i){
                auto elementV = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                } 
                unsigned elCtxId = ins->data[3 +  i]; // Bringing context might be unneccesary
                list->set(i, elementV->ctx, elementV->id);
            }

            return std::static_pointer_cast<CV::Item>(list);
        };        

        case CV::InstructionType::CONSTRUCT_RANGED_LIST: {
            __CV_DEFAULT_NUMBER_TYPE start = 0;
            __CV_DEFAULT_NUMBER_TYPE end = 0;
            __CV_DEFAULT_NUMBER_TYPE step = 1;

            // complete promise
            auto list = std::make_shared<CV::ListType>();
            auto cctx = stack->contexts[ins->data[0]];
            auto lItem = std::static_pointer_cast<CV::Item>(list);
            cctx->setPromise(ins->data[1], lItem);

            // Process start
            auto startV = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }  
            if(startV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "START must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                
            }
            start = std::static_pointer_cast<CV::NumberType>(startV)->get();

            // Process end
            auto endV = CV::Execute(stack->instructions[ins->parameter[1]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }  
            if(endV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "END must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                
            }
            end = std::static_pointer_cast<CV::NumberType>(endV)->get();   

            // Process step if any
            if(ins->parameter[2] != 0){
                auto stepV = CV::Execute(stack->instructions[ins->parameter[2]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }  
                if(stepV->type != CV::NaturalType::NUMBER){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "STEP must be a NUMBER", ins->token.line);
                    return ctx->buildNil(stack);                
                }
                step = std::static_pointer_cast<CV::NumberType>(stepV)->get();                   
            }

            // Count how many items
            __CV_DEFAULT_NUMBER_TYPE s = start;
            __CV_DEFAULT_NUMBER_TYPE e = end;
            bool lessthan = e > s;
            std::vector<std::shared_ptr<CV::Item>> elements;
            while((lessthan && s <= e) || (!lessthan && s >= e)){
                elements.push_back(cctx->buildNumber(stack, s));
                s += step;
            }
            list->build(elements);
            return lItem;
        };


        case CV::InstructionType::CONSTRUCT_BULK_LIST: {
            __CV_DEFAULT_NUMBER_TYPE amount = 0;
            __CV_DEFAULT_NUMBER_TYPE value = 0;

            // complete promise
            auto list = std::make_shared<CV::ListType>();
            auto cctx = stack->contexts[ins->data[0]];
            auto lItem = std::static_pointer_cast<CV::Item>(list);
            cctx->setPromise(ins->data[1], lItem);

            // Process amount
            auto amountV = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }  
            if(amountV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "END must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                
            }
            amount = std::static_pointer_cast<CV::NumberType>(amountV)->get();  
            if(amount <= 0){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "AMOUNT must be a non-zero positive NUMBER", ins->token.line);
                return ctx->buildNil(stack);  
            }
            // Process value
            auto valueV = CV::Execute(stack->instructions[ins->parameter[1]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }  
            if(valueV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "VALUE must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                
            }
            value = std::static_pointer_cast<CV::NumberType>(valueV)->get();               

            std::vector<std::shared_ptr<CV::Item>> elements;
            for(int i = 0; i < amount; ++i){
                elements.push_back(cctx->buildNumber(stack, value));
            }
            list->build(elements);            

            return lItem;
        };        

        case CV::InstructionType::CONSTRUCT_NAMESPACE: {

            for(int i = 0; i < ins->parameter.size(); ++i){
                auto type = ins->data[2 + i*3 + 0];
                auto cctx = ins->data[2 + i*3 + 1];
                auto memId = ins->data[2 + i*3 + 2];
                
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }    
                if(type == 1){
                    ctx->setPromise(memId, v);
                }
            }

            return ctx->buildNil(stack);
        };

        /*
            CONTROL FLOW
        */
        case CV::InstructionType::CF_COND_BINARY_BRANCH: { // CC

            auto nctx = stack->contexts[ins->data[0]];

            auto cond = CV::Execute(stack->instructions[ins->parameter[0]], stack, nctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }       

            bool isTrue = cond->type != CV::NaturalType::NIL && (cond->type == CV::NaturalType::NUMBER ? std::static_pointer_cast<CV::NumberType>(cond)->get() != 0 : true);

            if(isTrue){
                auto trueBranch = CV::Execute(stack->instructions[ins->parameter[1]], stack, nctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }          
                ctx->transferFrom(stack.get(), trueBranch.value);
                return trueBranch;
            }else
            if(ins->parameter.size() > 2){
                auto falseBranch = CV::Execute(stack->instructions[ins->parameter[2]], stack, nctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                    
                ctx->transferFrom(stack.get(), falseBranch.value);   
                return falseBranch;                
            }

            nctx->revert();

            return ctx->buildNil(stack);
        };
        case CV::InstructionType::CF_INVOKE_BINARY_FUNCTION: {
            auto nctx = stack->contexts[ins->data[0]];
            auto bf = stack->binaryFunctions[ins->data[1]];
            std::vector<std::shared_ptr<CV::Item>> arguments;
            nctx->revert();
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto cins = stack->instructions[ins->parameter[i]];
                auto v = CV::Execute(cins, stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                } 
                arguments.push_back(v);
            }
            auto result = bf->fn(bf->name, ins->token, arguments, nctx, cursor);
            nctx->store(stack, result);




            return result;
        };
        case CV::InstructionType::CF_INVOKE_FUNCTION: { // CC
            auto fnCtxId = ins->data[0];
            auto fnId = ins->data[1];
            auto fn = std::static_pointer_cast<CV::FunctionType>(stack->contexts[fnCtxId]->data[fnId]);

            auto nctx = stack->contexts[ins->data[2]];
            auto nargs = ins->data[3];
            bool variadic = ins->data[4];
            nctx->revert();
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto cins = stack->instructions[ins->parameter[i]];
                auto v = CV::Execute(cins, stack, nctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                } 
                if(variadic){
                    auto argument = ins->data[5];
                    auto varList = std::static_pointer_cast<CV::ListType>(nctx->data[argument]);
                    varList->set(i, ctx->id, v->id);
                }else{
                    auto argument = ins->data[i+5];
                    nctx->setPromise(argument, v);
                }
            }

            auto bodyIns = interpretToken(fn->body, {fn->body}, stack, nctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            auto r = CV::Execute(bodyIns, stack, nctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            return CV::ControlFlow(r.value, CV::ControlFlowType::CONTINUE);

        }
        case CV::InstructionType::CF_LOOP_DO: { // CC

            auto nctx = stack->contexts[ins->data[0]];

            std::shared_ptr<CV::Item> cond(NULL);
            std::shared_ptr<CV::Item> v(NULL);

            auto isTrue = [](std::shared_ptr<CV::Item> &cond){
                return cond->type != CV::NaturalType::NIL && (cond->type == CV::NaturalType::NUMBER ? std::static_pointer_cast<CV::NumberType>(cond)->get() != 0 : true);
            };

            unsigned ncf = CV::ControlFlowType::CONTINUE;

            do {
                // Check condition
                nctx->revert();
                cond = CV::Execute(stack->instructions[ins->parameter[0]], stack, nctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }

                if(!isTrue(cond)){
                    break;
                }

                // Execute
                auto cfv = CV::Execute(stack->instructions[ins->parameter[1]], stack, nctx, cursor);
                if(cursor->raise()){
                    break;
                }

                v = cfv.value;
                if(cfv.type != CV::ControlFlowType::CONTINUE){
                    if(cfv.type == CV::ControlFlowType::SKIP){
                        continue;
                    }else
                    if(cfv.type == CV::ControlFlowType::YIELD){
                        break;
                    }else{
                        ncf = cfv.type;
                        break;
                    }
                }


            } while(isTrue(cond));

            if(v){
                ctx->transferFrom(stack.get(), v);
            }

            stack->deleteContext(nctx->id);

            return CV::ControlFlow(v ? v : ctx->buildNil(stack), ncf);
        };      


        case CV::InstructionType::CF_LOOP_ITER: { // CC
            auto nctx = stack->contexts[ins->data[0]];
            auto stepperCountId = ins->parameter[2];
            auto counter = 1;
            std::shared_ptr<CV::Item> v(NULL);
            unsigned ncf = CV::ControlFlowType::CONTINUE;

            // Figure out counter
            if(stepperCountId != 0){
                auto v = CV::Execute(stack->instructions[stepperCountId], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                if(v->type != CV::NaturalType::NUMBER){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "Step increment must be a NUMBER", ins->token.line);
                    return ctx->buildNil(stack);                     
                }
                counter = std::static_pointer_cast<CV::NumberType>(v)->get();
            }

            // Figure out source
            auto source = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }          
            if(source->type != CV::NaturalType::LIST){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "iterator must be a LIST", ins->token.line);
                return ctx->buildNil(stack);                 
            }
            auto list = std::static_pointer_cast<CV::ListType>(source);
            if(list->getSize() == 0){
                return source;
            }
            if(list->getSize() % (int)counter != 0){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Step increment must a number divisible by the amount of the LIST", ins->token.line);
                return ctx->buildNil(stack);                 
            }

            // Figure out stepper
            std::shared_ptr<CV::Item> stepper(NULL);
            if(counter > 1){
                stepper = std::make_shared<CV::ListType>();
                std::static_pointer_cast<CV::ListType>(stepper)->build(counter);
                nctx->setPromise(ins->data[1], stepper);
            }
            auto updateStepper = [&](int index){
                if(counter == 1){
                    auto citem = list->get(stack, index);
                    nctx->setPromise(ins->data[1], citem);
                }else{
                    for(int i = 0; i < counter; ++i){
                        auto citem = list->get(stack, i + index);
                        std::static_pointer_cast<CV::ListType>(stepper)->set(i, citem->ctx, citem->id);
                    }
                }
            };

            for(int i = 0; i < list->getSize(); i += counter){
                nctx->revert();
                updateStepper(i);
                auto cfv = CV::Execute(stack->instructions[ins->parameter[1]], stack, nctx, cursor);
                if(cursor->raise()){
                    break;
                }

                v = cfv.value;
                if(cfv.type != CV::ControlFlowType::CONTINUE){
                    if(cfv.type == CV::ControlFlowType::SKIP){
                        continue;
                    }else
                    if(cfv.type == CV::ControlFlowType::YIELD){
                        break;
                    }else{
                        ncf = cfv.type;
                        break;
                    }
                }

            }

            if(v){
                ctx->transferFrom(stack.get(), v);
            }            

            stack->deleteContext(nctx->id);

            return CV::ControlFlow(v ? v : ctx->buildNil(stack), ncf);
        }; 

        case CV::InstructionType::CF_LOOP_FOR: { // CC

            auto nctx = stack->contexts[ins->data[0]];
            auto stepper = nctx->data[ins->data[1]];
            auto stepperCountId = ins->parameter[3];
            std::shared_ptr<CV::Item> v(NULL);
            unsigned ncf = CV::ControlFlowType::CONTINUE;
            __CV_DEFAULT_NUMBER_TYPE counter = 1;
            // Figure out counter
            if(stepperCountId != 0){
                auto v = CV::Execute(stack->instructions[stepperCountId], stack, nctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                if(v->type != CV::NaturalType::NUMBER){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "Step increment must be a NUMBER", ins->token.line);
                    return ctx->buildNil(stack);                     
                }
                counter = std::static_pointer_cast<CV::NumberType>(v)->get();
            }
            // Figure from
            auto fromV = CV::Execute(stack->instructions[ins->parameter[0]], stack, nctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            } 
            if(fromV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "from value must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                     
            }
            // Figure to
            auto toV = CV::Execute(stack->instructions[ins->parameter[1]], stack, nctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }   
            if(toV->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "to value must be a NUMBER", ins->token.line);
                return ctx->buildNil(stack);                     
            }                      

            auto start = std::static_pointer_cast<CV::NumberType>(fromV);
            auto end = std::static_pointer_cast<CV::NumberType>(toV);
            nctx->revert();

            for(__CV_DEFAULT_NUMBER_TYPE i = start->get(); i <= end->get(); i += counter){
                std::static_pointer_cast<CV::NumberType>(stepper)->set(i);                
                auto cfv = CV::Execute(stack->instructions[ins->parameter[2]], stack, nctx, cursor);
                if(cursor->raise()){
                    break;
                }
                v = cfv.value;
                if(cfv.type != CV::ControlFlowType::CONTINUE){
                    if(cfv.type == CV::ControlFlowType::SKIP){
                        continue;
                    }else
                    if(cfv.type == CV::ControlFlowType::YIELD){
                        break;
                    }else{
                        ncf = cfv.type;
                        break;
                    }
                }
                nctx->revert();
                // Process next from/to
                fromV = CV::Execute(stack->instructions[ins->parameter[0]], stack, nctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                } 
                if(fromV->type != CV::NaturalType::NUMBER){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "from value must be a NUMBER", ins->token.line);
                    return ctx->buildNil(stack);                     
                }
                // Figure to
                toV = CV::Execute(stack->instructions[ins->parameter[1]], stack, nctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }   
                if(toV->type != CV::NaturalType::NUMBER){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "to value must be a NUMBER", ins->token.line);
                    return ctx->buildNil(stack);                     
                } 
                start = std::static_pointer_cast<CV::NumberType>(fromV);
                end = std::static_pointer_cast<CV::NumberType>(toV);                                 
            }

            if(v){
                ctx->transferFrom(stack.get(), v);
            }

            stack->deleteContext(nctx->id);

            return CV::ControlFlow(v ? v : ctx->buildNil(stack), ncf);
        }; 

        case CV::InstructionType::CF_INT_RETURN:
        case CV::InstructionType::CF_INT_YIELD:
        case CV::InstructionType::CF_INT_SKIP: {
            auto hasPayload = ins->data[0];
            if(hasPayload){
                auto v = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                return CV::ControlFlow(v, ins->type);
            }else{
                return CV::ControlFlow(ctx->buildNil(stack), CV::ControlFlowType::type(ins->type));
            }
        };              

        /*
            EMBEDDED OPERATORS (LOGIC & ARITHMETIC)
        */        
        case CV::InstructionType::OP_NUM_ADD: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) += std::static_pointer_cast<CV::NumberType>(v)->get();
            }
            return item;
        };
        case CV::InstructionType::OP_NUM_SUB: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = std::static_pointer_cast<CV::NumberType>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) -= std::static_pointer_cast<CV::NumberType>(v)->get();
            }
            return item;
        };
        case CV::InstructionType::OP_NUM_MULT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = std::static_pointer_cast<CV::NumberType>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) *= std::static_pointer_cast<CV::NumberType>(v)->get();
            }
            return item;
        };
        case CV::InstructionType::OP_NUM_DIV: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = std::static_pointer_cast<CV::NumberType>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto cins = stack->instructions[ins->parameter[i]];
                auto v = CV::Execute(cins, stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                auto operand = std::static_pointer_cast<CV::NumberType>(v)->get();
                if(operand == 0){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "Division by zero", ins->token.line);
                    return ctx->buildNil(stack);
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) /= operand;
            }
            return item;
        };
        case CV::InstructionType::OP_COND_EQ: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                if(std::static_pointer_cast<CV::NumberType>(v)->get() != std::static_pointer_cast<CV::NumberType>(firstv)->get()){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return ctx->buildNil(stack);
                }
            }
            return item;
        };      
        case CV::InstructionType::OP_COND_NEQ: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<std::shared_ptr<CV::Item>> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                operands.push_back(v);
            }
            for(int i = 0; i < operands.size(); ++i){
                auto &v1 = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
                for(int j = 0; j < operands.size(); ++j){
                    if(j == i){
                        continue;
                    }
                    auto &v2 = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[j]->data);
                    if(v1 == v2){
                        *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                        return ctx->buildNil(stack);
                    }
                }
            }
            return item;
        };   
        case CV::InstructionType::OP_COND_LT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<std::shared_ptr<CV::Item>> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last >= *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return ctx->buildNil(stack);
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return item;
        };       
        case CV::InstructionType::OP_COND_LTE: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<std::shared_ptr<CV::Item>> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last > *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return ctx->buildNil(stack);
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return item;
        };      
        case CV::InstructionType::OP_COND_GT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<std::shared_ptr<CV::Item>> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last <= *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return ctx->buildNil(stack);
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return item;
        };       
        case CV::InstructionType::OP_COND_GTE: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<std::shared_ptr<CV::Item>> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last < *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return ctx->buildNil(stack);
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return item;
        };    

        case CV::InstructionType::OP_LOGIC_AND: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];

            auto item = stack->contexts[ctxId]->data[dataId];

            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;

            
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                if(     v->type == CV::NaturalType::NIL ||
                        (v->type == CV::NaturalType::NUMBER && std::static_pointer_cast<CV::NumberType>(v)->get() == 0)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    break;
                }
            }

            return item;
        };  

        case CV::InstructionType::OP_LOGIC_OR: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];

            auto item = stack->contexts[ctxId]->data[dataId];

            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;

            
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }                
                if(     (v->type == CV::NaturalType::NUMBER && std::static_pointer_cast<CV::NumberType>(v)->get() != 0) ||
                        (v->type != CV::NaturalType::NUMBER && v->type != CV::NaturalType::NIL)
                        ){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
                    break;
                }
            }

            return item;
        };        

        case CV::InstructionType::OP_LOGIC_NOT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];

            auto item = stack->contexts[ctxId]->data[dataId];

            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;

            auto v = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            if(v->type == CV::NaturalType::NUMBER){
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(v->data) == 0;
            }else
            if(v->type == CV::NaturalType::NIL){
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            }


            return item;
        };              

        case CV::InstructionType::OP_DESC_LENGTH: {
            auto &subject = ins->parameter[0];

            auto v = CV::Execute(stack->instructions[subject], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            return ctx->buildNumber(stack, v->getSize());
        };     

        case CV::InstructionType::OP_DESC_TYPE: {
            auto &subject = ins->parameter[0];

            auto v = CV::Execute(stack->instructions[subject], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }

            return ctx->buildString(stack, CV::NaturalType::name(v->type));
        };                                           


        case CV::InstructionType::OP_LIST_POP:
        case CV::InstructionType::OP_LIST_PUSH: {
            auto target = std::make_shared<CV::ListType>();
            auto targetId = ctx->store(stack, target);

            auto subjectIns = ins->type == CV::InstructionType::OP_LIST_PUSH ? ins->parameter.size()-1  : 0 ;

            unsigned start = ins->type == CV::InstructionType::OP_LIST_PUSH ? 0 : 1;
            unsigned end = ins->type == CV::InstructionType::OP_LIST_PUSH ? ins->parameter.size()-1 : ins->parameter.size();

            std::vector<std::shared_ptr<CV::Item>> items;
            auto subject = CV::Execute(stack->instructions[ins->parameter[subjectIns]], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            // Subject
            if(subject->type == CV::NaturalType::LIST){
                auto list = std::static_pointer_cast<CV::ListType>(subject); 
                for(int i = 0; i < list->getSize(); ++i){
                    items.push_back(list->get(stack, i));
                }
            }else{
                items.push_back(subject);
            }            
            // Remainer
            for(int i = start; i < end; ++i){
                auto v = CV::Execute(stack->instructions[ins->parameter[i]], stack, ctx, cursor).value;
                if(cursor->raise()){
                    return ctx->buildNil(stack);
                }
                items.push_back(v);
            }

            target->build(items);

            return std::static_pointer_cast<CV::Item>(target);
        };   


        /*
            PROXIES
        */
        case CV::InstructionType::NTH_PROXY: {
            // fetch
            auto &indexId = ins->parameter[0];
            auto &listId = ins->parameter[1];

            // run
            auto index = CV::Execute(stack->instructions[indexId], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            auto list = CV::Execute(stack->instructions[listId], stack, ctx, cursor).value;
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            // Basic checks
            if(index->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Expected a NUMBER at position 0 as index, "+CV::NaturalType::name(index->type)+" was provided", ins->token.line);
                return ctx->buildNil(stack);                
            }

            if(list->type != CV::NaturalType::LIST && list->type != CV::NaturalType::STRING){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Expected a LIST or STRING at position 1 as container, "+CV::NaturalType::name(list->type)+" was provided", ins->token.line);
                return ctx->buildNil(stack);                
            }
            // fetch and return
            int n = std::static_pointer_cast<CV::NumberType>(index)->get();
            switch(list->type){
                case CV::NaturalType::LIST: {
                    auto ls = std::static_pointer_cast<CV::ListType>(list);
                    if(n < 0 || n >= ls->getSize()){
                        cursor->setError("Logic Error At '"+ins->token.first+"'", "Provided out-of-range index("+std::to_string(n)+"), expected from 0 to "+std::to_string(ls->getSize()-1), ins->token.line);
                        return ctx->buildNil(stack);                 
                    }

                    return ls->get(stack, n);
                };
                case CV::NaturalType::STRING: {
                    auto str = std::static_pointer_cast<CV::StringType>(list);
                    if(n < 0 || n >= str->get().size()){
                        cursor->setError("Logic Error At '"+ins->token.first+"'", "Provided out-of-range index("+std::to_string(n)+"), expected from 0 to "+std::to_string(str->get().size()-1), ins->token.line);
                        return ctx->buildNil(stack);                 
                    }  
                    return ctx->buildString(stack, std::string()+str->get()[n]);
                };                            
            }
            return ctx->buildNil(stack);                
        } break;
        case CV::InstructionType::STATIC_PROXY: {
            // fetch and return
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            return stack->contexts[ctxId]->data[dataId];
        };      
        case CV::InstructionType::PROMISE_PROXY: {
            // run instruction
            auto v = CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }
            // complete promise
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto cctx = stack->contexts[ctxId];
            cctx->setPromise(dataId, v.value);
            return v;            
        };
        case CV::InstructionType::REFERRED_PROXY: {
            // pre-fetch the item
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            // run instruction
            CV::Execute(stack->instructions[ins->parameter[0]], stack, ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil(stack);
            }      
            return item;
        };          
    }

    return ctx->buildNil(stack);
}

CV::ControlFlow CV::Execute(CV::Instruction *ins, const std::shared_ptr<CV::Stack> &stack, std::shared_ptr<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
    CV::ControlFlow result;
    bool valid = false;
    do {
        result = __execute(stack, ins, ctx, cursor);
        if(cursor->raise()){
            return CV::ControlFlow(ctx->buildNil(stack));
        }        
        if(result.type != CV::ControlFlowType::CONTINUE){
            return result;
        }
        valid = ins->next != CV::InstructionType::INVALID;    
        ins = valid ? stack->instructions[ins->next] : NULL;
    } while(valid);

    return result;
}

void CV::Stack::clear(){
    // Clear Instructions
    for(auto &it : this->instructions){
        delete it.second;
    }
    this->instructions.clear();
    // Clear contexts
    for(auto &it : this->contexts){
        it.second->clear();
    }
    this->contexts.clear();
}


/* -------------------------------------------------------------------------------------------------------------------------------- */

void CV::SetUseColor(bool v){
    useColorOnText = v;
}

std::string CV::ItemToText(const std::shared_ptr<CV::Stack> &stack, CV::Item *item){
    switch(item->type){
        
        default:
        case CV::NaturalType::NIL: {
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::NaturalType::NUMBER: {
            return  CV::Tools::setTextColor(Tools::Color::CYAN) +
                    CV::Tools::removeTrailingZeros(static_cast<CV::NumberType*>(item)->get())+
                    CV::Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::NaturalType::STRING: {
            return Tools::setTextColor(Tools::Color::GREEN)+
                    "'"+static_cast<CV::StringType*>(item)->get()+"'"+
                    Tools::setTextColor(Tools::Color::RESET);
        };     

        case CV::NaturalType::FUNCTION: {
            auto fn = static_cast<CV::FunctionType*>(item);      

            std::string start = Tools::setTextColor(Tools::Color::RED, true)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string end = Tools::setTextColor(Tools::Color::RED, true)+"]"+Tools::setTextColor(Tools::Color::RESET);
            std::string name = Tools::setTextColor(Tools::Color::RED, true)+"fn"+Tools::setTextColor(Tools::Color::RESET);
            std::string binary = Tools::setTextColor(Tools::Color::BLUE, true)+"BINARY"+Tools::setTextColor(Tools::Color::RESET);
            std::string body = Tools::setTextColor(Tools::Color::BLUE, true)+fn->body.first+Tools::setTextColor(Tools::Color::RESET);

            return start+name+" "+start+(fn->variadic ? "args" : Tools::compileList(fn->args))+end+" "+( body )+end;
        };

        case CV::NaturalType::LIST: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);
            auto list = static_cast<CV::ListType*>(item);

            int total = list->getSize();
            int limit = total > 30 ? 10 : total;
            for(int i = 0; i < limit; ++i){
                auto item = list->get(stack, i);
                output += CV::ItemToText(stack, item.get());
                if(i < list->getSize()-1){
                    output += " ";
                }
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);
            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };

    }
}

std::string CV::QuickInterpret(const std::string &input, const std::shared_ptr<CV::Stack> &stack, std::shared_ptr<Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool shouldClear){
    // Get text tokens
    auto tokens = parseTokens(input,  ' ', cursor);
    if(cursor->raise()){
        return "";
    }

    // Compile tokens into its bytecode
    auto entry = compileTokens(tokens, stack, ctx, cursor);
    if(cursor->raise()){
        return "";
    }
    // Execute
    auto result = CV::Execute(entry, stack, ctx, cursor).value;
    if(cursor->raise()){
        return CV::ItemToText(stack, result.get());
    }

    auto text = CV::ItemToText(stack, result.get());

    if(shouldClear){
        stack->clear();
    }

    return text;
}

void CV::SetColorTableText(const std::unordered_map<int, std::string> &ct){
    CV::Tools::UnixColor::TextColorCodes = ct;
    CV::Tools::UnixColor::BoldTextColorCodes = ct;
}

void CV::SetColorTableBackground(const std::unordered_map<int, std::string> &ct){
    CV::Tools::UnixColor::BackgroundColorCodes = ct;
}

std::string CV::GetPrompt(){
    std::string start = Tools::setTextColor(Tools::Color::MAGENTA) + "[" + Tools::setTextColor(Tools::Color::RESET);
    std::string cv = Tools::setTextColor(Tools::Color::CYAN) + "~" + Tools::setTextColor(Tools::Color::RESET);
    std::string end = Tools::setTextColor(Tools::Color::MAGENTA) + "]" + Tools::setTextColor(Tools::Color::RESET);
    return start+cv+end+"> ";
}

void CV::AddStandardConstructors(std::shared_ptr<CV::Stack> &stack){
    __addStandardConstructors(stack);
}

/* -------------------------------------------------------------------------------------------------------------------------------- */

CV::ErrorCheck::ConstraintArgNumber::ConstraintArgNumber(){
    type = ConstraintType::ARG_NUMBER;
}

CV::ErrorCheck::ConstraintArgNumber::ConstraintArgNumber(unsigned minNumber, unsigned maxNumber){
    type = ConstraintType::ARG_NUMBER;
    this->minNumber = minNumber;
    this->maxNumber = maxNumber;
}

bool CV::ErrorCheck::ConstraintArgNumber::check(const std::shared_ptr<CV::Stack> &stack, std::string &msg, const std::vector<std::shared_ptr<CV::Item>> &args){
    if(minNumber == 0 && maxNumber == 0 && args.size() != 0){
        msg = CV::Tools::format("Expected no arguments, %i were provided", args.size());
        return true;
    }
    if(minNumber != maxNumber && args.size() < minNumber){
        msg = CV::Tools::format("Expected at least %i arguments, %i were provided", minNumber, args.size());
        return true;
    }
    if(minNumber != maxNumber && args.size() > maxNumber){
        msg = CV::Tools::format("Expected no more than %i arguments, %i were provided", maxNumber, args.size());
        return true;
    }                
    if(minNumber == maxNumber && args.size() != maxNumber){
        msg = CV::Tools::format("Expected exactly %i arguments, %i were provided", maxNumber, args.size());
        return true;                    
    }
    return false;
}

CV::ErrorCheck::ConstraintArgType::ConstraintArgType(){
    this->type = ConstraintType::ARG_TYPE;
}

CV::ErrorCheck::ConstraintArgType::ConstraintArgType(unsigned argPos, unsigned argType){
    this->type = ConstraintType::ARG_TYPE;
    this->argPos = argPos;
    this->argType = argType;
}

bool CV::ErrorCheck::ConstraintArgType::check(const std::shared_ptr<CV::Stack> &stack, std::string &msg, const std::vector<std::shared_ptr<CV::Item>> &args){
    if(args.size() < (this->argPos+1)){
        msg = CV::Tools::format("Provided mismatching number of arguments, %i were provided, %i expected at least", args.size(), argPos+1);
        return true;
    }
    if(args[argPos]->type != this->argType){
        msg = CV::Tools::format(
            "Provided argument type '%s' at position %i: Expected '%s'",
            CV::NaturalType::name(args[argPos]->type).c_str(),
            argPos,
            CV::NaturalType::name(this->argType).c_str()
        );
        return true;                    
    }
    return false;
}

CV::ErrorCheck::ConstraintArgList::ConstraintArgList(){
    this->type = ConstraintType::ARG_LIST;
}

CV::ErrorCheck::ConstraintArgList::ConstraintArgList(unsigned argType, unsigned argPos, unsigned argSize){
    this->type = ConstraintType::ARG_LIST;
    this->argType = argType;
    this->argPos = argPos;
    this->argSize = argSize;
}

bool CV::ErrorCheck::ConstraintArgList::check(const std::shared_ptr<CV::Stack> &stack, std::string &msg, const std::vector<std::shared_ptr<CV::Item>> &args){
    if(args.size() < (this->argPos+1)){
        msg = CV::Tools::format("Provided mismatching number of arguments, %i were provided, %i expected at least", args.size(), argPos+1);
        return true;
    }

    if(args[argPos]->type != CV::NaturalType::LIST){
        msg = CV::Tools::format("Expected argument type 'LIST' at position %i: Provided %s",
            argPos,
            CV::NaturalType::name(args[argPos]->type)
        );
        return true;                    
    }

    auto lst = std::static_pointer_cast<CV::ListType>(args[argPos]);

    if(lst->getSize() != this->argSize){
        msg = CV::Tools::format("Expected argument type 'LIST' of exactly %i elements, provided %i",
            argSize,
            lst->getSize()
        );
        return true;                      
    }

    for(int i = 0; i < lst->getSize(); ++i){
        auto item = lst->get(stack, i);
        if(item->type != argType){
            msg = CV::Tools::format("Expected argument type 'LIST' of '%s' elements, provided '%s' at position %i",
                CV::NaturalType::name(argType).c_str(),
                CV::NaturalType::name(item->type).c_str(),
                i
            );
            return true; 
        }
    }

    return false;
}

bool CV::ErrorCheck::TestArguments(
                const std::vector<std::shared_ptr<Constraint>> &constraints,
                const std::string &name,
                const CV::Token &token,
                const std::shared_ptr<CV::Stack> &stack,
                std::vector<std::shared_ptr<CV::Item>> &args,
                std::shared_ptr<CV::Cursor> &cursor){
    for(int i = 0; i < constraints.size(); ++i){
        auto &cons = constraints[i];
        std::string error;
        if(cons->check(stack, error, args)){
            cursor->setError(name, error, token.line);
            return true;
        }
    }
    return false;
}