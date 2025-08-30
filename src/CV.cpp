// SYSTEM INCLUDES
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <sys/stat.h>
#include <fstream>

// LOCAL INCLUDES
#include "CV.hpp"

// DYNAMIC LIBRARY STUFF
#if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX)
    #include <dlfcn.h> 
#elif  (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
    // should prob move the entire DLL loading routines to its own file
    #include <Libloaderapi.h>
#endif

static bool UseColorOnText = false;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TOOLS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace CV {
    namespace Test {
        bool areAllNumbers(const std::vector<std::shared_ptr<CV::Quant>> &arguments){
            for(int i = 0; i < arguments.size(); ++i){
                if(arguments[i]->type != CV::QuantType::NUMBER){
                    return false;
                }
            }
            return true;
        }
        bool IsItPrefixInstruction(const std::shared_ptr<CV::Instruction> &ins){
            return ins->data.size() >= 2 && ins->data[0] == CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION;
        }
    }
    namespace Tools {

        namespace ColorTable {

            static std::unordered_map<int, std::string> TextColorCodes = {
                {CV::Tools::Color::BLACK,   "\e[0;30m"},
                {CV::Tools::Color::RED,     "\e[0;31m"},
                {CV::Tools::Color::GREEN,   "\e[0;32m"},
                {CV::Tools::Color::YELLOW,  "\e[0;33m"},
                {CV::Tools::Color::BLUE,    "\e[0;34m"},
                {CV::Tools::Color::MAGENTA,  "\e[0;35m"},
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
  
        std::string setTextColor(int color, bool bold){
            if(!UseColorOnText){
                return "";
            }
            return (bold ? ColorTable::BoldTextColorCodes.find(color)->second : ColorTable::TextColorCodes.find(color)->second);
        }

        std::string setBackgroundColor(int color){
            if(!UseColorOnText){
                return "";
            }

            return ColorTable::BackgroundColorCodes.find(color)->second;
        }

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

        std::string lower(const std::string &str){
            std::string out;
            for(int i = 0; i < str.size(); i++){
                out += tolower(str.at(i));
            }
            return out;
        }
        
        std::string upper(const std::string &str){
            std::string out;
            for(int i = 0; i < str.size(); i++){
                out += toupper(str.at(i));
            }
            return out;
        }

        bool isValidVarName(const std::string &name){
            if(name.size() == 0){
                return false;
            }
            return !isNumber(std::string("")+name[0]) && name[0] != '.' && name[0] != '~';
        }

        bool isReservedWord(const std::string &name){
            static const std::vector<std::string> reserved {
                "let", "bring", "bring:dynamic-library", "~", ".", "|", "`", "cc", "mut", "fn",
                "return", "yield", "skip", "b:list", "b:store", "untether", "async"
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
        
        std::string cleanSpecialCharacters(const std::string &v){
            std::string result;
            for(int i = 0; i < v.size(); ++i){
                auto c = v[i];
                if(c == '\n'){
                    result += "\\n";
                    continue;
                }else
                if(c == '\t'){
                    result += "\\t";
                    continue;
                }else
                if(c == '\r'){
                    result += "\\r";
                    continue;
                }

                result += c;              
            }
            return result;
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
                if(c == '\n'){
                    result += "\\n";
                    ++i;
                    continue;
                }  
                result += c;              
            }
            return result;
        }

        std::string cleanTokenInput(const std::string &str){
            std::string out;
            for(int i = 0; i < str.size(); ++i){
                if(str[i] == ' ' || str[i] == '\n' || str[i] == '\t' || str[i] == '\r'){
                    continue;
                }
                out += str[i];
            }
            return out;
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
                }else{
                    buffer += "\n";
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

std::shared_ptr<CV::Quant> CV::Quant::copy(){
    return std::make_shared<CV::Quant>();
}

// NUMBER
CV::TypeNumber::TypeNumber() : CV::Quant() {
    this->type = CV::QuantType::NUMBER;
}

bool CV::TypeNumber::clear(){
    v = 0;
    return true;
}

std::shared_ptr<CV::Quant> CV::TypeNumber::copy(){
    auto cpy = std::make_shared<CV::TypeNumber>();
    cpy->v = this->v;
    return cpy;
}

// STRING
CV::TypeString::TypeString() : CV::Quant() {
    this->type = CV::QuantType::STRING;
}

bool CV::TypeString::clear(){
    v = "";
    return true;
}

std::shared_ptr<CV::Quant> CV::TypeString::copy(){
    auto cpy = std::make_shared<CV::TypeString>();
    cpy->v = this->v;
    return cpy;
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

std::shared_ptr<CV::Quant> CV::TypeList::copy(){
    auto cpy = std::make_shared<CV::TypeList>();
    return cpy;
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

std::shared_ptr<CV::Quant> CV::TypeStore::copy(){
    auto cpy = std::make_shared<CV::TypeStore>();
    return cpy;
}

// FUNCTION
CV::TypeFunction::TypeFunction() : CV::Quant(){
    this->type = CV::QuantType::FUNCTION;
}

bool CV::TypeFunction::clear(){
    return true;
}

std::shared_ptr<CV::Quant> CV::TypeFunction::copy(){
    auto cpy = std::make_shared<CV::TypeFunction>();
    cpy->name = this->name;
    cpy->entrypoint = this->entrypoint;
    cpy->ctxId = this->ctxId;
    cpy->body = this->body->copy();
    return cpy;
}

// BINARY FUNCTION
CV::TypeFunctionBinary::TypeFunctionBinary(){
    this->type = CV::QuantType::BINARY_FUNCTION;
}

bool CV::TypeFunctionBinary::clear(){
    return true;
}

std::shared_ptr<CV::Quant> CV::TypeFunctionBinary::copy(){
    auto cpy = std::make_shared<CV::TypeFunctionBinary>();
    cpy->name = this->name;
    cpy->entrypoint = this->entrypoint;
    cpy->ctxId = this->ctxId;
    cpy->ref = this->ref;
    cpy->body = this->body->copy();
    return cpy;
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

void CV::Cursor::clear(){
    this->error = false;
    this->used = false;
    this->used = false;
    this->subject = CV::TokenType(NULL);
    this->message = "";
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
    return      CV::Tools::setTextColor(CV::Tools::Color::RED, false) +
                CV::Tools::cleanSpecialCharacters("[Line #"+std::to_string(this->line)+"] "+this->title+": "+this->message+in)+
                CV::Tools::setTextColor(CV::Tools::Color::RESET, false);
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
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::QuantType::NUMBER: {
            return      CV::Tools::setTextColor(Tools::Color::CYAN)+
                        CV::Tools::removeTrailingZeros(std::static_pointer_cast<CV::TypeNumber>(t)->v)+
                        CV::Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::QuantType::STRING: {
            auto out = std::static_pointer_cast<CV::TypeString>(t)->v;

            return  Tools::setTextColor(Tools::Color::GREEN)+
                    "'"+out+
                    "'"+Tools::setTextColor(Tools::Color::RESET);
        };     

        case CV::QuantType::FUNCTION: {
            auto fn = std::static_pointer_cast<CV::TypeFunction>(t);      

            std::string start = Tools::setTextColor(Tools::Color::RED, true)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string end = Tools::setTextColor(Tools::Color::RED, true)+"]"+Tools::setTextColor(Tools::Color::RESET);
            std::string name = Tools::setTextColor(Tools::Color::RED, true)+"fn"+Tools::setTextColor(Tools::Color::RESET);
            std::string binary = Tools::setTextColor(Tools::Color::BLUE, true)+"BINARY"+Tools::setTextColor(Tools::Color::RESET);
            std::string body = Tools::setTextColor(Tools::Color::BLUE, true)+fn->body->str()+Tools::setTextColor(Tools::Color::RESET);

            return start+name+" "+start+CV::Tools::compileList(fn->params)+end+" "+( body )+end;
        };

        case CV::QuantType::LIST: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);

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
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);


            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::QuantType::STORE: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);

            auto store = std::static_pointer_cast<CV::TypeStore>(t);

            int total = store->v.size();
            int limit = total > 30 ? 10 : total;
            int i = 0;
            for(auto &it : store->v){
                auto &q = it.second;
                output +=   Tools::setTextColor(Tools::Color::YELLOW)+"["+Tools::setTextColor(Tools::Color::RESET)+
                            "~"+it.first+" "+CV::QuantToText(q)+
                            Tools::setTextColor(Tools::Color::YELLOW)+"]"+Tools::setTextColor(Tools::Color::RESET);
                if(i < store->v.size()-1){
                    output += " ";
                }
                ++i;
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);

            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };        

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PARSING
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::vector<CV::TokenType> parseTokens(std::string input, char sep, const std::shared_ptr<CV::Cursor> &cursor, int startLine){
    std::vector<std::shared_ptr<CV::Token>> tokens; 
    std::string buffer;
    int open = 0;
    bool onString = false;
    bool onComment = false;
    int leftBrackets = 0;
    int rightBrackets = 0;
    int cline = startLine;
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
    cline = startLine;
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

static std::vector<CV::TokenType> rebuildTokenHierarchy(const std::vector<CV::TokenType> &input, char sep, const CV::CursorType &cursor){
    
    if(input.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, 1);
        return {};
    }

    std::function<CV::TokenType(const CV::TokenType &token)> unwrap = [&](const CV::TokenType &token){
        auto innerTokens = parseTokens(token->first, sep, cursor, token->line);
        if(cursor->error){
            return CV::TokenType(NULL);
        }

        if(innerTokens.size() == 0){
            auto empty = token->copy();
            empty->first = "";
            empty->inner.clear();
            empty->refresh();
            return empty;
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
            if(fetched->type == CV::InstructionType::PROXY_EXPANDER){
                auto targetIns = prog->getIns(fetched->params[0]);
                if( targetIns->type == CV::InstructionType::CONSTRUCT_LIST ||
                    targetIns->type == CV::InstructionType::CONSTRUCT_STORE){
                    for(int j = 0; j < targetIns->params.size(); ++j){
                        ins->params.push_back(targetIns->params[j]);
                        list->v.push_back(std::shared_ptr<CV::Quant>(NULL));
                    }
                }else{
                    ins->params.push_back(fetched->id);
                    list->v.push_back(std::shared_ptr<CV::Quant>(NULL));                    
                }
            }else{
                ins->params.push_back(fetched->id);
                list->v.push_back(std::shared_ptr<CV::Quant>(NULL));
            }
        }
        return ins;
    };

    auto bStoreConstruct = [prog, cursor](const std::string &name, const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_STORE, origin);
        auto store = ctx->buildStore();
        ins->data.push_back(ctx->id);
        ins->data.push_back(store->id);
        for(int i = 0; i < tokens.size(); ++i){
            auto &inc = tokens[i];
            auto fetched = CV::Translate(inc, prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = origin;
                return prog->createInstruction(CV::InstructionType::NOOP, origin);
            }
            if(fetched->type != CV::InstructionType::PROXY_NAMER){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' may only be able to construct named types using NAMER prefixer", origin);
                return prog->createInstruction(CV::InstructionType::NOOP, origin);
            }
            // Deny expander usage in Stores
            if(fetched->type != CV::InstructionType::PROXY_EXPANDER){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "Expander Prefixer cannot be used while constructing Stores", inc);
                return prog->createInstruction(CV::InstructionType::NOOP, origin);
            }
            ins->params.push_back(fetched->id);
            auto vname = fetched->literal[0];
            if(!CV::Tools::isValidVarName(vname)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' is attempting to construct store with invalidly named type '"+vname+"'", origin);
                return prog->createInstruction(CV::InstructionType::NOOP, origin);                   
            }
            if(CV::Tools::isReservedWord(vname)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' is attempting to construct store with reserved name named type '"+vname+"'", origin);
                return prog->createInstruction(CV::InstructionType::NOOP, origin);                
            }
            ins->literal.push_back(vname);
            store->v[vname] = std::shared_ptr<CV::Quant>(NULL);
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
        auto &cctx = prog->getCtx(nameRef[0]);
        auto &quant = cctx->get(nameRef[1]);  
        return  quant->type == CV::QuantType::FUNCTION ||
                quant->type == CV::QuantType::BINARY_FUNCTION ||
                quant->type == CV::QuantType::STORE;
    };
    
    auto areAllFunctions = [isNameFunction](const CV::TokenType &token, const CV::ContextType &ctx){
        if(token->inner.size() < 1){
            return false;
        } 
        
        for(int i = 0; i < token->inner.size(); ++i){
            if(token->inner[i]->first.size() == 0 || !isNameFunction(token->inner[i], ctx)){
                return false;
            } 
        }
        return true;
    };

    auto areAllNames = [isNameFunction](const CV::TokenType &token, const CV::ContextType &ctx){
        if(token->inner.size() < 1){
            return false;
        } 
        
        for(int i = 0; i < token->inner.size(); ++i){
            if(token->inner[i]->first.size() < 2 || token->inner[i]->first[0] != '~'){
                return false;
            } 
        }
        return true;
    };

    if(token->first.size() == 0){
        // Instruction list
        if(areAllFunctions(token, ctx) && token->inner.size() >= 2){
            return CV::Compile(token, prog, ctx, cursor);
        }else
        if(areAllNames(token, ctx)){
            return bStoreConstruct(token->first, token, token->inner, ctx);
        }else{
        // Or list?
            return bListConstruct(token, token->inner, ctx);
        }
    }else{
        /*
            s:store
        */
        if(token->first == "b:store"){
            return bStoreConstruct(token->first, token, token->inner, ctx);
        }else
        /*
            l:list
        */
        if(token->first == "b:list"){
            return bListConstruct(token, token->inner, ctx);
        }else
        /*
            SKIP / RETURN / YIELD
        */
        if(token->first == "skip" || token->first == "return" || token->first == "yield"){
            if(token->inner.size() > 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects no more than 1 operand ("+token->first+" VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::CF_INTERRUPT, token);
            
            int type = CV::InterruptType::UNDEFINED;
            if(token->first == "skip"){
                type = CV::InterruptType::SKIP;
            }else
            if(token->first == "yield"){
                type = CV::InterruptType::YIELD;
            }else
            if(token->first == "return"){
                type = CV::InterruptType::RETURN;
            }
            ins->data.push_back(type);

            if(token->inner.size() > 0){
                ins->data.push_back(ctx->id);
                auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
                if(cursor->error){
                    cursor->subject = token;
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }
                ins->params.push_back(target->id);
            }

            return ins;
        }else
        /*
            BRING
        */
        if(token->first== "bring" || token->first == "bring:dynamic-library"){
            bool isdotcv = token->first == "bring";
            if(token->inner.size() != 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            
            if(!CV::ErrorCheck::ExpectNoPrefixer(token->first, {target}, token, cursor)){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto st = std::make_shared<CV::ControlFlow>();
            auto fnamev = CV::Execute(target, ctx, prog, cursor, st);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            if(!CV::ErrorCheck::ExpectsTypeAt(fnamev->type, CV::QuantType::STRING, 0, token->first, token, cursor)){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Get symbolic or literal
            auto fname = std::static_pointer_cast<CV::TypeString>(fnamev)->v;

            // TODO: Solve name (Could be a local or system library, probably using CV_LIB)

            // Load .cv
            if(isdotcv){
                if(!CV::Tools::fileExists(fname)){
                    cursor->setError(CV_ERROR_MSG_LIBRARY_NOT_VALID, "No dynamic library '"+fname+"' was found", token);
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }
                int id = CV::Import(fname, prog, ctx, cursor);
            }else{
            // Load .so/.dll
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
                        fprintf(stderr, "%s: Unable to load dyanmic library for this platform (UNKNOWN/UNDEFINED).", token->first.c_str());
                        std::exit(1);
                    } break;                        
                }

                if(!CV::Tools::fileExists(path)){
                    cursor->setError(CV_ERROR_MSG_LIBRARY_NOT_VALID, "No dynamic library '"+fname+"' was found", token);
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }

                int id = CV::ImportDynamicLibrary(path, fname, prog, ctx, cursor);

            }
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::LIBRAY_IMPORT, token);
            ins->data.push_back(ctx->id);
            ins->data.push_back(0); // library id, TODO
            ins->data.push_back(isdotcv);
            ins->literal.push_back(fname);
            return ins;
        }else
        /*
            FN
        */
        if(token->first == "fn"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens (fn [arguments][code])", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto fn = std::make_shared<CV::TypeFunction>();
            ctx->set(fn->id, fn);
            auto fctx = prog->createContext(ctx);
            fn->ctxId = fctx->id;
            fn->body = token->inner[1];

            auto paramNameList = token->inner[0]->inner;
            paramNameList.insert(paramNameList.begin(), token->inner[0]);
            for(int i = 0; i < paramNameList.size(); ++i){
                auto &name = paramNameList[i]->first;
                if(!CV::Tools::isValidVarName(name)){
                    cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' parameter name  '"+name+"' is an invalid", token);
                    return prog->createInstruction(CV::InstructionType::NOOP, token);                   
                }
                if(CV::Tools::isReservedWord(name)){
                    cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' parameter name '"+name+"' is a name of native constructor which cannot be overriden", token);
                    return prog->createInstruction(CV::InstructionType::NOOP, token);                
                }  
                fn->params.push_back(name);
                auto v = fctx->buildNil();
                fctx->setName(name, v->id);
            }

            auto target = CV::Translate(token->inner[1], prog, fctx, cursor);

            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            fn->entrypoint = target->id;
            auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
            ins->data.push_back(ctx->id); // Where function is stored
            ins->data.push_back(fn->id);
            return ins;
            
        }else
        /*
            CC
        */
        if(token->first == "cc"){
            if(token->inner.size() != 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 2 tokens (cc VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }         
            auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto ins = prog->createInstruction(CV::InstructionType::CARBON_COPY, token);
            auto nv = ctx->buildNil();
            ins->data.push_back(ctx->id);
            ins->data.push_back(nv->id);
            ins->params.push_back(target->id);
            return ins;
        }else
        /*
            MUT
        */
        if(token->first == "mut"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens (mut NAME VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto nameRef = ctx->getName(token->inner[0]->first);
            if(nameRef.size() == 0){
                cursor->setError(CV_ERROR_MSG_UNDEFINED_IMPERATIVE, "Name '"+token->first+"'", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token); 
            }
            auto target = CV::Translate(token->inner[1], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            } 
            auto ins = prog->createInstruction(CV::InstructionType::MUT, token);
            ins->data.push_back(nameRef[0]);
            ins->data.push_back(nameRef[1]);
            ins->data.push_back(ctx->id);
            ins->params.push_back(target->id);
            return ins;
        }else
        /*
            LET
        */
        if(token->first == "let"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' expects exactly 3 tokens (let NAME VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto &nameToken = token->inner[0];
            auto &insToken = token->inner[1];

            if(nameToken->inner.size() != 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' expects NAME as second operand", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }
            auto &name = nameToken->first;
            if(!CV::Tools::isValidVarName(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' second operand '"+name+"' is an invalid name", token);
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

            
            if(target->type == CV::InstructionType::PROXY_STATIC){
                auto &targetData = prog->getCtx(target->data[0])->get(target->data[1]);
                ctx->set(targetData->id, targetData);
                ctx->setName(name, targetData->id);  
                return target;
            }else{
                auto targetData = ctx->buildNil();

                auto ins = prog->createInstruction(CV::InstructionType::LET, token);
                ins->literal.push_back(name);
                ins->data.push_back(ctx->id);
                ins->data.push_back(targetData->id);
                ins->params.push_back(target->id);

                ctx->setName(name, targetData->id);  

                return ins;
            }

        }else
        /*
            NIL
        */
        if(token->first == "nil"){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                auto data = ctx->buildNil();
                ins->data.push_back(ctx->id);
                ins->data.push_back(data->id);
                return ins;
            } 
        }else
        /*
            NUMBER
        */
        if(CV::Tools::isNumber(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
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
                auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                auto string = ctx->buildString("");
                string->v = token->first.substr(1, token->first.length() - 2);
                ins->data.push_back(ctx->id);
                ins->data.push_back(string->id);
                return ins;
            }
        }else
        // EXPANDER
        if(token->first[0] == '^'){
            if(token->first.length() > 1){
                std::string subpart(token->first.begin() + 1, token->first.end());
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Expander Prefix '"+token->first+"' may not be appended with '"+subpart+"'", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                            
            }
            if(token->inner.size() != 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Expander Prefix '"+token->first+"' expects exactly one value", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                                            
            }

            auto ins = prog->createInstruction(CV::InstructionType::PROXY_EXPANDER, token);
            ins->data.push_back(CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION);
            ins->data.push_back(CV::Prefixer::NAMER);
            std::shared_ptr<CV::Quant> ghostData;

            auto &inc = token->inner[0];
            auto fetched = CV::Translate(inc, prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            ins->params.push_back(fetched->id);

            return ins;
        }else
        // NAMER
        if(token->first[0] == '~'){
            auto name = std::string(token->first.begin()+1, token->first.end());

            // Test if the first field (name) is a complex token
            auto testSplit = parseTokens(name, ' ', cursor, token->line);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);                   
            }
            if(testSplit.size() > 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Namer Prefix '"+token->first+"' cannot take any complex token", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);            
            }
            // Is it a valid name?
            if(!CV::Tools::isValidVarName(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "'"+token->first+"' prefixer '"+name+"' is an invalid name", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                   
            }
            // Is it a reserved word?
            if(CV::Tools::isReservedWord(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "'"+token->first+"' prefixer '"+name+"' is a name of native constructor which cannot be overriden", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }
            // Only expects a single value
            if(token->inner.size() > 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Namer Prefix '"+token->first+"' no more than 1 value. Provided "+std::to_string(token->inner.size()), token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }
            // Build position proxy instruction
            auto ins = prog->createInstruction(CV::InstructionType::PROXY_NAMER, token);
            ins->literal.push_back(name);
            ins->data.push_back(CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION);
            ins->data.push_back(CV::Prefixer::NAMER);
            std::shared_ptr<CV::Quant> ghostData;
            // Build referred instruction
            if(token->inner.size() > 0){
                auto &inc = token->inner[0];
                auto fetched = CV::Translate(inc, prog, ctx, cursor);
                if(cursor->error){
                    cursor->subject = token;
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }                        
                // Pre-set ghost name 
                if(fetched->type == CV::InstructionType::PROXY_STATIC){
                    ghostData = prog->getCtx(fetched->data[0])->get(fetched->data[1]);
                }else{
                    ghostData = ctx->buildNil();
                }
                if(ctx->getName(name, true).size() == 0){
                    ctx->setName(name, ghostData->id);
                }
                ins->params.push_back(fetched->id);
            }else{
                ghostData = ctx->buildNil();
                if(ctx->getName(name, true).size() == 0){
                    ctx->setName(name, ghostData->id);
                }
            }
            // Pass rest of data to ins
            ins->data.push_back(ctx->id);
            ins->data.push_back(ghostData->id);
            return ins;
        }else{
            /*
                IS NAME?        
            */   
            auto hctx = ctx;
            std::string qname = token->first;
            auto nameRef = ctx->getName(token->first);
            if(nameRef.size() > 0){
                // TODO: Check for PROMISE names
                auto fctx = prog->getCtx(nameRef[0]);
                auto &dataId = nameRef[1];
                auto &quant = fctx->get(dataId);     

                switch(quant->type){
                    case CV::QuantType::FUNCTION: {
                        auto fn = std::static_pointer_cast<CV::TypeFunction>(quant);
                        auto &fnCtx = prog->getCtx(fn->ctxId);
                        if(fn->params.size() != token->inner.size()){
                            cursor->setError(CV_ERROR_MSG_MISUSED_FUNCTION, "'"+token->first+"' expects exactly "+std::to_string(fn->params.size())+". Provided "+std::to_string(token->inner.size()), token);
                            return prog->createInstruction(CV::InstructionType::NOOP, token);                
                        }
                        auto ins = prog->createInstruction(CV::InstructionType::CF_INVOKE_FUNCTION, token);
                        ins->data.push_back(nameRef[0]); // Context where the function is contained
                        ins->data.push_back(nameRef[1]); // Data ID to the function
                        ins->data.push_back(ctx->id); // Current context where the function is being invoked on
                        // Fetch params
                        for(int i = 0; i < token->inner.size(); ++i){
                            auto &inc = token->inner[i];
                            auto fetched = CV::Translate(inc, prog, ctx, cursor);
                            if(cursor->error){
                                cursor->subject = token;
                                return prog->createInstruction(CV::InstructionType::NOOP, token);
                            }                        
                            if(fetched->type == CV::InstructionType::PROXY_EXPANDER){
                                auto targetIns = prog->getIns(fetched->params[0]);
                                if( targetIns->type == CV::InstructionType::CONSTRUCT_LIST ||
                                    targetIns->type == CV::InstructionType::CONSTRUCT_STORE){
                                    for(int j = 0; j < targetIns->params.size(); ++j){
                                        ins->params.push_back(targetIns->params[j]);
                                    }
                                }else{
                                    ins->params.push_back(fetched->id);
                                }
                            }else{
                                ins->params.push_back(fetched->id);
                            }
                        }

                        auto childIns = prog->createInstruction(CV::InstructionType::PROXY_PROMISE, token);
                        auto targetData = hctx->buildNil();
                        childIns->data.push_back(hctx->id);
                        childIns->data.push_back(targetData->id);
                        childIns->params.push_back(ins->id);
                        // Tell invoke binary function who they're going to fulfill
                        ins->data.push_back(hctx->id);
                        ins->data.push_back(targetData->id);

                        return childIns;
                    };
                    case CV::QuantType::BINARY_FUNCTION: {
                        // Build invokation
                        auto ins = prog->createInstruction(CV::InstructionType::CF_INVOKE_BINARY_FUNCTION, token);
                        auto tempCtx = prog->createContext(hctx); // build context where we store temporary parameters
                        // Where the function is stored
                        ins->data.push_back(fctx->id); // where this function is stored
                        ins->data.push_back(quant->id);
                        // Where the params to the function are stored 
                        ins->data.push_back(tempCtx->id); // With the store parameters to the function
                        for(int i = 0; i < token->inner.size(); ++i){
                            auto &inc = token->inner[i];
                            auto fetched = CV::Translate(inc, prog, tempCtx, cursor);
                            if(cursor->error){
                                cursor->subject = token;
                                return prog->createInstruction(CV::InstructionType::NOOP, token);
                            }
                            if(fetched->type == CV::InstructionType::PROXY_EXPANDER){
                                auto targetIns = prog->getIns(fetched->params[0]);

                                if( targetIns->type == CV::InstructionType::CONSTRUCT_LIST ||
                                    targetIns->type == CV::InstructionType::CONSTRUCT_STORE){
                                    for(int j = 0; j < targetIns->params.size(); ++j){
                                        ins->params.push_back(targetIns->params[j]);
                                    }
                                }else{
                                    ins->params.push_back(fetched->id);
                                }
                            }else{
                                ins->params.push_back(fetched->id);
                            }
                        }
                        ins->data.push_back(ins->params.size());
                        // Build Promise Proxy to store result value 
                        auto childIns = prog->createInstruction(CV::InstructionType::PROXY_PROMISE, token);
                        auto targetData = hctx->buildNil();
                        childIns->data.push_back(hctx->id);
                        childIns->data.push_back(targetData->id);
                        childIns->params.push_back(ins->id);
                        // Tell invoke binary function who they're going to fulfill
                        ins->data.push_back(hctx->id);
                        ins->data.push_back(targetData->id);
                        return childIns;
                    };
                    case CV::QuantType::STORE: {
                        auto store = std::static_pointer_cast<CV::TypeStore>(quant);
                        // Does this named proxy come with parameters?
                        if(token->inner.size() > 0){
                            // Are they named?
                            if(areAllNames(token, ctx)){
                                std::vector<std::string> names;
                                std::vector<int> ids;
                                // Gather names
                                for(int i = 0; i < token->inner.size(); ++i){                               
                                    auto fetched = CV::Translate(token->inner[i], prog, ctx, cursor);
                                    if(cursor->error){
                                        cursor->subject = token;
                                        return prog->createInstruction(CV::InstructionType::NOOP, token);
                                    }          
                                    names.push_back(fetched->literal[0]);
                                }
                                // Check if they're indeed part of this
                                for(int i = 0; i < names.size(); ++i){
                                    if(store->v.count(names[i]) == 0){
                                        cursor->setError(CV_ERROR_MSG_STORE_UNDEFINED_MEMBER, "store '"+qname+"' has no member named '"+names[i]+"'", token);
                                        return prog->createInstruction(CV::InstructionType::NOOP, token);                
                                    }
                                }
                                // Build Access Proxy
                                auto buildAccessProxy = [token](int ctxId, int dataId, const std::string &mname, const CV::ProgramType &prog){
                                    auto ins = prog->createInstruction(CV::InstructionType::PROXY_ACCESS, token);
                                    ins->data.push_back(ctxId);
                                    ins->data.push_back(dataId);
                                    ins->literal.push_back(mname);
                                    return ins;                                     
                                };
                                // One name returns the value itself
                                if(names.size() == 1){
                                    return buildAccessProxy(nameRef[0], nameRef[1], names[0], prog);
                                }else{
                                // Several names returns a list of values
                                    auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_LIST, token);
                                    auto list = hctx->buildList();
                                    ins->data.push_back(hctx->id);
                                    ins->data.push_back(list->id);
                                    for(int i = 0; i < names.size(); ++i){
                                        auto fetched = buildAccessProxy(nameRef[0], nameRef[1], names[i], prog);
                                        ins->params.push_back(fetched->id);
                                        list->v.push_back(std::shared_ptr<CV::Quant>(NULL));
                                    }
                                    return ins;
                                }
                            }else{
                            // Otherwise this must be a list
                                return bListConstructFromToken(token, ctx);
                            }
                        }else{
                        // Return its static proxy
                            auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                            ins->data.push_back(nameRef[0]);
                            ins->data.push_back(nameRef[1]);
                            return ins;                            
                        }
                    };  
                    default: {
                        if(token->inner.size() > 0){
                            return bListConstructFromToken(token, ctx);
                        }else{  
                            auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                            ins->data.push_back(nameRef[0]);
                            ins->data.push_back(nameRef[1]);
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

CV::InsType CV::Compile(const CV::TokenType &input, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){

    std::vector<CV::TokenType> list = {};

    for(int i = 0; i < input->inner.size(); ++i){
        list.push_back(input->inner[i]);
    }

    std::vector<CV::InsType> instructions;
    for(int i = 0 ; i < list.size(); ++i){
        auto ins = CV::Translate(list[i], prog, ctx, cursor);
        if(cursor->error){
            return ins;
        }
        instructions.push_back(ins);
    }
    
    for(int i = 1; i < instructions.size(); ++i){
        auto &current = instructions[i];
        auto &previous = instructions[i-1];
        previous->next = current->id;
        current->prev = previous->id;
    }

    return instructions[0];
}

CV::InsType CV::Compile(const std::string &input, const CV::ProgramType &prog, const CV::CursorType &cursor){   

    // Fix outter brackets (Input must always be accompained by brackets)
    auto cleanInput = CV::Tools::cleanTokenInput(input);
    auto fixedInput = input;
    if(cleanInput[0] != '[' || cleanInput[cleanInput.size()-1] != ']'){
        fixedInput = "["+fixedInput;
        if(fixedInput[fixedInput.size()-1] == '\n'){
            fixedInput[fixedInput.size()-1] = ']';
        }else{
            fixedInput = fixedInput+"]";
        }
    }
    // Double brackets (Statement must respect [ [] -> STATEMENT ] -> PROGRAM hierarchy)
    cleanInput = CV::Tools::cleanTokenInput(fixedInput);
    if( cleanInput[0] == '[' && cleanInput[1] != '[' &&
        cleanInput[cleanInput.size()-1] == ']' && cleanInput[cleanInput.size()-2] != ']'){
        fixedInput = "["+fixedInput+"]";
    }

    // Split basic tokens
    auto tokens = parseTokens(fixedInput, ' ', cursor, 1);
    if(cursor->error){
        return prog->createInstruction(CV::InstructionType::NOOP, tokens[0]);
    }

    std::vector<CV::TokenType> root;
    // Build Token hierarchy
    for(int i = 0; i < tokens.size(); ++i){
        auto built = rebuildTokenHierarchy({tokens[i]}, ' ', cursor);
        if(cursor->error){
            return prog->createInstruction(CV::InstructionType::NOOP, built[0]);
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
            return prog->createInstruction(CV::InstructionType::NOOP, root[i]);
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
    return instructions[0];

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
        case CV::InstructionType::PROXY_ACCESS: {
            auto ctxId = ins->data[0];
            auto dataId = ins->data[1];
            std::string mname = ins->literal[0];
            auto &quant = prog->getCtx(ctxId)->get(dataId);
            auto store = std::static_pointer_cast<CV::TypeStore>(quant);
            // TODO: Perhaps some error checking?
            return store->v[mname];
        };
        case CV::InstructionType::PROXY_EXPANDER: {
            if(ctx->isPrefetched(ins->id)){
                auto dir = ctx->getPrefetch(ins->id);
                return prog->getCtx(dir[0])->get(dir[1]);
            }
            std::shared_ptr<CV::Quant> quant;
            auto &entrypoint = prog->getIns(ins->params[0]);
            quant = CV::Execute(entrypoint, ctx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return ctx->buildNil();
            }
            ctx->setPrefetch(ins->id, {(unsigned)ctx->id, (unsigned)quant->id});
            return quant;            
        };
        case CV::InstructionType::PROXY_NAMER: {
            if(ctx->isPrefetched(ins->id)){
                auto dir = ctx->getPrefetch(ins->id);
                return prog->getCtx(dir[0])->get(dir[1]);
            }
            std::shared_ptr<CV::Quant> quant;
            if(ins->params.size() > 0){
                auto &entrypoint = prog->getIns(ins->params[0]);
                quant = CV::Execute(entrypoint, ctx, prog, cursor, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return ctx->buildNil();
                }
                // Fulfill ghost name
                if(ins->data.size() > 2){
                    auto ghostDataId = ins->data[3];
                    auto ghostDataCtxId = ins->data[2];
                    auto &cctx = prog->getCtx(ghostDataCtxId);
                    cctx->set(ghostDataId, quant); 
                }  
            }else{
                quant = prog->getCtx(ins->data[2])->get(ins->data[3]);
            }
            ctx->setPrefetch(ins->id, {(unsigned)ctx->id, (unsigned)quant->id});
            return quant;
        };
        case CV::InstructionType::PROXY_STATIC: {
            auto ctxId = ins->data[0];
            auto dataId = ins->data[1];
            return prog->getCtx(ctxId)->get(dataId);
        };
        case CV::InstructionType::PROXY_PROMISE: {
            // if(ctx->prefetched.count(ins->id) > 0){
            //     auto dir = ctx->prefetched[ins->id];
            //     return prog->ctx[dir[0]]->memory[dir[1]];
            // }
            auto cctx = prog->getCtx(ins->data[0]);
            auto targetDataId = ins->data[1];
            // Execute (The instruction needs to fulfill the promise on its side)
            auto &entrypoint = prog->getIns(ins->params[0]);
            auto quant = CV::Execute(entrypoint, cctx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return cctx->get(targetDataId);
            }            
            // ctx->prefetched[ins->id] = {(unsigned)cctx->id, (unsigned)targetDataId};
            return cctx->get(targetDataId);
        };                    


        /*
            CONSTRUCTORS        
        */
        case CV::InstructionType::CONSTRUCT_LIST: {
            auto &cctx = prog->getCtx(ins->data[0]);
            auto &qlist = cctx->get(ins->data[1]);
            auto list = std::static_pointer_cast<CV::TypeList>(qlist);
            for(int i = 0; i < ins->params.size(); ++i){
                auto cins = prog->getIns(ins->params[i]);
                bool isExpander = cins->type == CV::InstructionType::PROXY_EXPANDER;
                auto quant = CV::Execute(cins, cctx, prog, cursor, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return ctx->buildNil();
                }
                if(isExpander && quant->type == CV::QuantType::LIST){
                    auto subject = std::static_pointer_cast<CV::TypeList>(quant);
                    for(int j = 0; j < subject->v.size(); ++j){
                        list->v.push_back(subject->v[j]);
                    }
                }else{
                    list->v[i] = quant;
                }
            }   
            return std::static_pointer_cast<CV::Quant>(list);
        };
        case CV::InstructionType::CONSTRUCT_STORE: {
            auto &cctx = prog->getCtx(ins->data[0]);
            auto &qstore = cctx->get(ins->data[1]);
            auto store = std::static_pointer_cast<CV::TypeStore>(qstore);
            for(int i = 0; i < ins->params.size(); ++i){
                auto quant = CV::Execute(prog->getIns(ins->params[i]), cctx, prog, cursor, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return ctx->buildNil();
                }
                store->v[ins->literal[i]] = quant;
            }   
            return std::static_pointer_cast<CV::Quant>(store);
        };

        case CV::InstructionType::MUT: {
            
            if(ctx->isPrefetched(ins->id)){
                auto dir = ctx->getPrefetch(ins->id);
                return prog->getCtx(dir[0])->get(dir[1]);
            }
            
            auto &targetDataCtxId = ins->data[0];
            auto &targetDataId = ins->data[1];
            auto &execCtxId = ins->data[2];
            auto &execCtx = prog->getCtx(execCtxId);
            auto &data = prog->getCtx(targetDataCtxId)->get(targetDataId);
            auto &entrypoint = prog->getIns(ins->params[0]);

            auto v = CV::Execute(entrypoint, execCtx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }
            
            if(data->type != CV::QuantType::NUMBER && data->type != CV::QuantType::STRING){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: Only numbers and string types might be mutated. Target is '"+CV::QuantType::name(data->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }
            if(v->type != CV::QuantType::NUMBER && v->type != CV::QuantType::STRING){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: Only numbers and string types might be mutated. New value is '"+CV::QuantType::name(v->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }

            if(v->type != data->type){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: both target and new value must match in type. Target is '"+CV::QuantType::name(data->type)+"' and new value is '"+CV::QuantType::name(v->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }

            switch(data->type){
                case CV::QuantType::STRING: {
                    std::static_pointer_cast<CV::TypeString>(data)->v = std::static_pointer_cast<CV::TypeString>(v)->v;
                } break;
                case CV::QuantType::NUMBER: {
                    std::static_pointer_cast<CV::TypeNumber>(data)->v = std::static_pointer_cast<CV::TypeNumber>(v)->v;
                } break;
            }

            ctx->setPrefetch(ins->id, {(unsigned)targetDataCtxId, (unsigned)targetDataId});


            return data;
        };

        case CV::InstructionType::LET: {

            if(ctx->isPrefetched(ins->id)){
                auto dir = ctx->getPrefetch(ins->id);
                return prog->getCtx(dir[0])->get(dir[1]);
            }

            auto &cctx = prog->getCtx(ins->data[0]);
            auto targetDataID = ins->data[1];
            auto &name = ins->literal[0];
            auto &entrypoint = prog->getIns(ins->params[0]);

            auto v = CV::Execute(entrypoint, cctx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return v;
            }
            cctx->set(targetDataID, v);

            ctx->setPrefetch(ins->id, {(unsigned)cctx->id, (unsigned)targetDataID});

            return v;
        };

        case CV::InstructionType::CARBON_COPY: {
            if(ctx->isPrefetched(ins->id)){
                auto dir = ctx->getPrefetch(ins->id);
                return prog->getCtx(dir[0])->get(dir[1]);
            }

            auto &cctx = prog->getCtx(ins->data[0]);
            auto &nv = cctx->get(ins->data[1]);

            auto &entrypoint = prog->getIns(ins->params[0]);

            auto v = CV::Execute(entrypoint, cctx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return v;
            }
            auto copied = cctx->buildCopy(v);
            cctx->set(ins->data[1], copied);

            ctx->setPrefetch(ins->id, {(unsigned)cctx->id, (unsigned)copied->id});
        
            return copied;
        };
        
        /*
            CONTROL FLOW        
        */
        case CV::InstructionType::CF_INTERRUPT: {
            switch(ins->data[0]){
                case CV::InterruptType::YIELD: {
                    st->state = CV::ControlFlowState::YIELD;
                } break;
                case CV::InterruptType::SKIP: {
                    st->state = CV::ControlFlowState::SKIP;
                } break;
                case CV::InterruptType::RETURN: {
                    st->state = CV::ControlFlowState::RETURN;
                } break;
                default: {
                    st->state = CV::ControlFlowState::CRASH;
                } break;
            }
            if(ins->params.size() > 0){
                auto &payloadCtx = prog->getCtx(ins->data[1]);
                auto v = CV::Execute(prog->getIns(ins->params[0]), payloadCtx, prog, cursor, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return v;
                }   
                st->payload = v;
                return v;
            }
            return ctx->buildNil();
        };
        case CV::InstructionType::CF_INVOKE_FUNCTION: {
            auto fnCtxId = ins->data[0];
            auto fnId = ins->data[1];
            auto paramCtxId = ins->data[2];

            auto fn = std::static_pointer_cast<CV::TypeFunction>(prog->getCtx(fnCtxId)->get(fnId));
            auto &topExecCtx = prog->getCtx(fn->ctxId);
            auto &paramCtx = prog->getCtx(paramCtxId);

            int targetCtxId = ins->data[3];
            int targetDataId = ins->data[4];

            // topExecCtx->memory.clear();
            topExecCtx->clearPrefetch();

            // Prepare Context
            int namei = 0;
            for(int i = 0; i < ins->params.size(); ++i){
                auto &cins = prog->getIns(ins->params[i]);
                auto v = CV::Execute(cins, paramCtx, prog, cursor, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return v;
                }   
                // Is it prefix?
                if(CV::Test::IsItPrefixInstruction(cins) && cins->type == CV::InstructionType::PROXY_NAMER){
                    topExecCtx->set(topExecCtx->getName(cins->literal[0], true)[1], v);
                }else
                if( CV::Test::IsItPrefixInstruction(cins) &&
                    cins->type == CV::InstructionType::PROXY_EXPANDER &&
                    v->type == CV::QuantType::LIST){
                    auto inner = std::static_pointer_cast<CV::TypeList>(v);
                    for(int j = 0; j < inner->v.size(); ++j){
                        int fin = namei + j;
                        if(fin >= fn->params.size()){
                            cursor->setError(CV_ERROR_MSG_WRONG_OPERANDS, "Provided wrong number of parameters("+std::to_string(fin)+") through the Expander", ins->token);
                            return ctx->buildNil();
                        }
                        topExecCtx->set(topExecCtx->getName(fn->params[fin], true)[1], inner->v[j]);
                    }
                    namei += inner->v.size();
                }else{
                    topExecCtx->set(topExecCtx->getName(fn->params[namei], true)[1], v);
                    ++namei;
                }
            }
            // Execute
            auto execCtx = prog->createContext(topExecCtx);
            auto v = CV::Execute(prog->getIns(fn->entrypoint), execCtx, prog, cursor, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
            }else  
            if(st->state == CV::ControlFlowState::RETURN){
                st->state = CV::ControlFlowState::CONTINUE;
                v = st->payload;
            }

            // Fulfill target
            prog->getCtx(targetCtxId)->set(targetDataId, v);

            prog->deleteContext(execCtx->id);
            
            return std::shared_ptr<CV::Quant>(NULL);
        } break;
        case CV::InstructionType::CF_INVOKE_BINARY_FUNCTION: {
            auto &fnData = prog->getCtx(ins->data[0])->get(ins->data[1]);
            auto fn = std::static_pointer_cast<CV::TypeFunctionBinary>(fnData);

            auto &paramCtx = prog->getCtx(ins->data[2]);

            int targetCtxId = ins->data[4];
            int targetDataId = ins->data[5];

            // Compile operands
            int nArgs = ins->data[3];
            std::vector<std::shared_ptr<CV::Instruction>> arguments;
            for(int i = 0; i < nArgs; ++i){
                auto cins = prog->getIns(ins->params[i]);
                if(cins->type == CV::InstructionType::PROXY_EXPANDER){
                    auto innerIns = prog->getIns(cins->params[0]);
                    if(innerIns->type == CV::InstructionType::CONSTRUCT_LIST){
                        for(int j = 0; j < innerIns->params.size(); ++j){
                            arguments.push_back( prog->getIns(innerIns->params[j]) );
                        }
                    }else{
                        arguments.push_back(cins);
                    }
                }else{
                    arguments.push_back(cins);
                }
            }


            // Cast ref into pointer function
            void (*ref)(CV_BINARY_FN_PARAMS) = (void (*)(CV_BINARY_FN_PARAMS))fn->ref;

            // Invoke it in a temporary context
            auto tempCtx = prog->createContext(paramCtx);
            ref(arguments, fn->name, ins->token, cursor, tempCtx->id, targetCtxId, targetDataId, prog, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
            }             
            // Delete context afterwards. The target value must be within the level above
            prog->deleteContext(tempCtx->id);

            return std::shared_ptr<CV::Quant>(NULL);
        };
        /*
            LIBRARY IMPORT
        */
        case CV::InstructionType::LIBRAY_IMPORT: {
            return ctx->buildNumber(ins->data[1]); // returns library id handle            
        } break;
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
            cursor->setError(CV_ERROR_MSG_INVALID_INSTRUCTION, "Instruction Type '"+std::to_string(ins->type)+"' is undefined/invalid", ins->token);
            st->state = CV::ControlFlowState::CRASH;
            return ctx->buildNil();
        };
    }
}

std::shared_ptr<CV::Quant> CV::Execute(const CV::InsType &entry, const CV::ContextType &ctx, const CV::ProgramType &prog, const CV::CursorType &cursor, CFType cf){

    CV::InsType ins = entry;
    std::shared_ptr<CV::Quant> result;
    auto st = cf.get() == NULL ? std::make_shared<CV::ControlFlow>() : cf;

    if( 
        cursor->error ||
        st->state == CV::ControlFlowState::YIELD ||
        st->state == CV::ControlFlowState::SKIP ||
        st->state == CV::ControlFlowState::CRASH ||
        st->state == CV::ControlFlowState::RETURN
    ){
        return ctx->buildNil();
    }

    while(true){
        result = __flow(ins, ctx, prog, cursor, st);
        if( 
            cursor->error ||
            st->state == CV::ControlFlowState::YIELD ||
            st->state == CV::ControlFlowState::SKIP ||
            st->state == CV::ControlFlowState::CRASH ||
            st->state == CV::ControlFlowState::RETURN
        ){
            return result;
        }
        if(ins->next != CV::InstructionType::INVALID){
            ins = prog->getIns(ins->next);
            continue;
        }
        break;
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

std::shared_ptr<CV::Quant> CV::Context::buildCopy(const std::shared_ptr<CV::Quant> &subject){
    std::shared_ptr<CV::Quant> c = subject->copy();
    switch(subject->type){
        case CV::QuantType::LIST: {
            auto orig = std::static_pointer_cast<CV::TypeList>(subject);
            auto target = std::static_pointer_cast<CV::TypeList>(c);
            for(int i = 0; i < orig->v.size(); ++i){
                target->v.push_back(this->buildCopy(orig->v[i]));
            }
        };
        case CV::QuantType::STORE: {
            auto orig = std::static_pointer_cast<CV::TypeStore>(subject);
            auto target = std::static_pointer_cast<CV::TypeStore>(c);
            for(auto &it : orig->v){
                target->v[it.first] = this->buildCopy(it.second);
            }
        };
    }
    this->memoryMutex.lock();
    this->memory[c->id] = c;
    this->memoryMutex.unlock();
    return c;
}

std::shared_ptr<CV::TypeNumber> CV::Context::buildNumber(number n){
    auto t = std::make_shared<CV::TypeNumber>();
    this->memoryMutex.lock();
    this->memory[t->id] = t;
    this->memoryMutex.unlock();
    t->v = n;
    return t;
}

std::shared_ptr<CV::Quant> CV::Context::buildNil(){
    auto t = std::make_shared<CV::Quant>();
    this->memoryMutex.lock();
    this->memory[t->id] = t;
    this->memoryMutex.unlock();
    return t;
}

std::shared_ptr<CV::Quant> CV::Context::buildType(int type){
    switch(type){
        case CV::QuantType::NUMBER: {
            return this->buildNumber();
        };
        case CV::QuantType::STRING: {
            return this->buildString();
        };
        case CV::QuantType::LIST: {
            return this->buildList();
        };
        case CV::QuantType::STORE: {
            return this->buildStore();
        };
        default:
        case CV::QuantType::NIL: {
            return this->buildNil();
        };
    }
}

std::shared_ptr<CV::TypeString> CV::Context::buildString(const std::string &s){
    auto t = std::make_shared<CV::TypeString>();
    this->memoryMutex.lock();
    this->memory[t->id] = t;
    this->memoryMutex.unlock();
    t->v = s;
    return t;
}

std::shared_ptr<CV::TypeList> CV::Context::buildList(const std::vector<std::shared_ptr<CV::Quant>> &list){
    auto t = std::make_shared<TypeList>();
    this->memoryMutex.lock();
    this->memory[t->id] = t;
    this->memoryMutex.unlock();
    for(int i = 0; i < list.size(); ++i){
        t->v.push_back(list[i]);
    }
    return t;
}

std::shared_ptr<CV::TypeStore> CV::Context::buildStore(const std::unordered_map<std::string, std::shared_ptr<CV::Quant>> &store){
    auto t = std::make_shared<TypeStore>();
    this->memoryMutex.lock();
    this->memory[t->id] = t;
    this->memoryMutex.unlock();
    for(auto &it : store){
        t->v[it.first] = it.second;
    }
    return t;
}

std::shared_ptr<CV::Quant> &CV::Context::get(int id){
    memoryMutex.lock();
    auto &v = this->memory[id];
    memoryMutex.unlock();
    return v;
}

void CV::Context::set(int id, const std::shared_ptr<CV::Quant> &q){
    memoryMutex.lock();
    this->memory[id] = q;
    memoryMutex.unlock();
}

void CV::Context::setName(const std::string &name, unsigned id){
    nameMutex.lock();
    this->names[name] = id;
    nameMutex.unlock();
}

void CV::Context::setPrefetch(unsigned id, const std::vector<unsigned> &v){
    prefetchMutex.lock();
    this->prefetched[id] = v;
    prefetchMutex.unlock();
}

std::vector<unsigned> CV::Context::getPrefetch(unsigned id){
    prefetchMutex.lock();
    std::vector<unsigned> vs = this->prefetched[id];
    prefetchMutex.unlock();
    return vs;
}  

bool CV::Context::isPrefetched(unsigned id){
    prefetchMutex.lock();
    auto v = this->prefetched.count(id);
    prefetchMutex.unlock();
    return v > 0;
}

void CV::Context::clearPrefetch(){
    prefetchMutex.lock();
    this->prefetched.clear();
    prefetchMutex.unlock();
}

std::shared_ptr<CV::TypeFunctionBinary> CV::Context::registerBinaryFuntion(const std::string &name, void *ref){
    auto fn = std::make_shared<CV::TypeFunctionBinary>();
    fn->ref = ref;
    fn->ctxId = this->id;
    fn->name = name;
    this->memoryMutex.lock();
    this->memory[fn->id] = fn;
    this->memoryMutex.unlock();
    this->names[name] = fn->id;
    return fn;
}

// Name's context Id and data Id upwards
std::vector<int> CV::Context::getName(const std::string &name, bool local){
    this->nameMutex.lock();
    if(this->names.count(name) > 0){
        std::vector<int> ids;
        ids.push_back(this->id);
        ids.push_back(this->get(this->names[name])->id);
        this->nameMutex.unlock();
        return ids;
    }
    this->nameMutex.unlock();
    return (this->head.get() && !local ? this->head->getName(name) : std::vector<int>({}));
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
    insMutex.lock();
    this->instructions[ins->id] = ins;
    insMutex.unlock();
    return ins;
}

std::shared_ptr<CV::Context> CV::Program::createContext(const std::shared_ptr<CV::Context> &head){
    auto nctx = std::make_shared<CV::Context>(head);
    ctxMutex.lock();
    this->ctx[nctx->id] = nctx;
    ctxMutex.unlock();
    return nctx;
}

bool CV::Program::deleteContext(int id){
    ctxMutex.lock();
    if(this->ctx.count(id) > 0){
        this->ctx.erase(id); // Careful!! TODO: Check
        ctxMutex.unlock();
        return true;
    }
    ctxMutex.unlock();
    return false;
}

std::shared_ptr<CV::Context> &CV::Program::getCtx(int id){
    ctxMutex.lock();
    auto &c = this->ctx[id];
    ctxMutex.unlock();
    return c;
}

std::shared_ptr<CV::Instruction> &CV::Program::getIns(int id){
    this->insMutex.lock();
    auto &i = this->instructions[id];
    this->insMutex.unlock();
    return i;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  API
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CV::GetBooleanValue(const std::shared_ptr<CV::Quant> &data){
    if(data.get() == NULL){
        return false;
    }
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


int CV::Import(const std::string &fname, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    if(!CV::Tools::fileExists(fname)){
        cursor->setError(CV_ERROR_MSG_LIBRARY_NOT_VALID, "'"+fname+"' does not exist");
        return 0;
    }
    auto literal = CV::Tools::readFile(fname);
    auto entrypoint = CV::Compile(literal, prog, cursor);
    if(cursor->error){
        std::cout << cursor->getRaised() << std::endl;
        std::exit(1);
    }
    auto st = std::make_shared<CV::ControlFlow>();
    auto result = CV::Execute(entrypoint, ctx, prog, cursor, st);
    if(cursor->error){
        std::cout << cursor->getRaised() << std::endl;
        std::exit(1);
    }
    return 1;
}

bool CV::Unimport(int id){
    return true;
}

int CV::ImportDynamicLibrary(const std::string &path, const std::string &fname, const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    #if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX)
        void (*registerlibrary )(const std::shared_ptr<CV::Stack> &stack);
        void* handle = NULL;
        const char* error = NULL;
        handle = dlopen(path.c_str(), RTLD_LAZY);
        if(!handle){
            fprintf( stderr, "Failed to load dynamic library %s: %s\n", fname.c_str(), dlerror());
            std::exit(1);
        }
        dlerror();
        registerlibrary = (void (*)(const std::shared_ptr<CV::Stack> &stack)) dlsym( handle, "_CV_REGISTER_LIBRARY" );
        error = dlerror();
        if(error){
            fprintf( stderr, "Failed to load dynamic library '_CV_REGISTER_LIBRARY' from '%s': %s\n", fname.c_str(), error);
            dlclose(handle);
            std::exit(1);
        }
        // Try to load
        (*registerlibrary)(stack);
        loadedLibraries.push_back(handle);
        return true;
    #elif  (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
        typedef void (*rlib)(const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);

        rlib entry;
        HMODULE hdll;
        // Load lib
        hdll = LoadLibraryA(path.c_str());
        if(hdll == NULL){
            fprintf( stderr, "Failed to load dynamic library %s\n", fname.c_str());
            std::exit(1);                
        }
        // Fetch register's address
        entry = (rlib)GetProcAddress(hdll, "_CV_REGISTER_LIBRARY");
        if(entry == NULL){
            fprintf( stderr, "Failed to load dynamic library '_CV_REGISTER_LIBRARY' from '%s'\n", fname.c_str());
            std::exit(1);      
        }
        entry(prog, ctx, cursor);
        auto id = GEN_ID();
        prog->loadedDynamicLibs[id] = hdll;
        return id;
    #endif
}

void CV::SetUseColor(bool v){
    UseColorOnText = v;
}

std::string CV::GetLogo(){
    std::string start = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "[" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string cv = CV::Tools::setTextColor(Tools::Color::CYAN) + "~" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string end = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "]" + CV::Tools::setTextColor(Tools::Color::RESET);
    return start+cv+end;
}

// int main(){

//     auto cursor = std::make_shared<CV::Cursor>();
//     auto program = std::make_shared<CV::Program>();
//     program->rootContext = program->createContext();
    
//     CV::InitializeCore(program);

//     auto entrypoint = CV::Compile("print 'hello world'", program, cursor);
//     if(cursor->error){
//         std::cout << cursor->getRaised() << std::endl;
//         std::exit(1);
//     }
//     program->entrypointIns = entrypoint->id;
//     auto result = CV::Execute(entrypoint, program->rootContext, program, cursor);
//     if(cursor->error){
//         std::cout << cursor->getRaised() << std::endl;
//         std::exit(1);
//     }

//     std::cout << CV::QuantToText(result) << std::endl;

//     std::cout << "exit" << std::endl;

//     return 0;

// }