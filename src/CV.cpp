#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <sys/stat.h>
#include <fstream>

#include "CV.hpp"


static bool UseColorOnText = true;
static std::string CV_LIB_HOME = "./";

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
//  TOOLS
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace CV {
    namespace Test {
        bool areAllNumbers(const std::vector<CV::Data*> &arguments){
            for(int i = 0; i < arguments.size(); ++i){
                if(arguments[i]->type != CV::DataType::NUMBER){
                    return false;
                }
            }
            return true;
        }
        // bool IsItPrefixInstruction(const std::shared_ptr<CV::Instruction> &ins){
        //     return ins->data.size() >= 2 && ins->data[0] == CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION;
        // }
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
  
        void sleep(uint64_t t){
            std::this_thread::sleep_for(std::chrono::milliseconds(t));
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
        
		std::string fileExtension(const std::string &filename){
			int k = filename.rfind(".");
			return k != -1 ? filename.substr(k+1, filename.length()-1) : "";
		}

        bool isInList(const std::string &v, const std::vector<std::string> &list){
            for(int i = 0; i < list.size(); ++i){
                if(list[i] == v){
                    return true;
                }
            }
            return false;
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

        namespace ErrorCheck {

            bool AreAllType(
                const std::string &name,
                const std::vector<CV::Data*> &params,
                const std::shared_ptr<CV::Cursor> &cursor,
                const CV::TokenType &token,
                int expType
            ){
                for(int i = 0; i < params.size(); ++i){
                    auto data = params[i];
                    if(data->type != expType){
                        cursor->setError(
                            CV_ERROR_MSG_WRONG_OPERANDS,
                            "Imperative '"+name+"' expects all operands to be "+CV::DataTypeName(expType)+": operand "+std::to_string(i)+" is "+CV::DataTypeName(data->type),
                            token
                        );
                        return false;
                    }
                }            
                return true;            
            }    
        }        
        
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  TYPES
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CV::Data::Data(){
    this->id = GEN_ID();
    this->ref = 0;
    this->type = CV::DataType::NIL;
}

int CV::Data::getRefCount(){
    int n = 0;
    mutexRefCount.lock();
    n = this->ref;
    mutexRefCount.unlock();
    return n;
}

void CV::Data::incRefCount(){
    mutexRefCount.lock();
    ++this->ref;
    mutexRefCount.unlock();
}

void CV::Data::decRefCount(){
    mutexRefCount.lock();
    --this->ref;
    mutexRefCount.unlock();
}   

void CV::Data::clear(const std::shared_ptr<CV::Program> &prog){  

}

void CV::Data::build(const std::shared_ptr<CV::Program> &prog){

}

/* -------------------------------------------------------------------------------------------------------------------------------- */

//
//  NUMBER
//
CV::DataNumber::DataNumber(CV_NUMBER n){
    this->value = n;
    this->type = CV::DataType::NUMBER;
}

//
//  STRING
//
CV::DataString::DataString(const std::string &value){
    this->value = value;
    this->type = CV::DataType::STRING;
}

//
//  LIST
//
CV::DataList::DataList(){
    this->type = CV::DataType::LIST;
}

void CV::DataList::clear(const std::shared_ptr<CV::Program> &prog){
    for(int i = 0; i < this->value.size(); ++i){
        auto &dataId = this->value[i];
        auto subject = prog->getData(dataId);
        if(subject == NULL){
            continue;
        }
        subject->decRefCount();
    }
    this->value.clear();
    this->value.~vector();
}

//
//  STORE
//
CV::DataStore::DataStore(){
    this->type = CV::DataType::STORE;
}

void CV::DataStore::clear(const std::shared_ptr<CV::Program> &prog){
    for(auto it : this->value){
        auto &dataId = it.second;
        auto subject = prog->getData(dataId);
        if(subject == NULL){
            continue;
        }
        subject->decRefCount();
    }
    this->value.clear();
    this->value.~unordered_map();
}

//
//  FUNCTION
//
CV::DataFunction::DataFunction(){
    this->type = CV::DataType::FUNCTION;
    this->isBinary = false;
    this->body = NULL;
    this->isBinary = false;
    this->isVariadic = false;
    this->lambda = [](
        const std::shared_ptr<CV::Program> &prog,
        const std::string &name,
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &params,
        const std::shared_ptr<CV::Token> &token,
        const std::shared_ptr<CV::Cursor> &cursor,
        const std::shared_ptr<CV::Context> &ctx,
        const std::shared_ptr<CV::ControlFlow> &st      
    ) -> CV::Data*{

        return NULL;
    };
}

void CV::DataFunction::clear(const std::shared_ptr<CV::Program> &prog){
    this->params.clear();
    this->params.~vector();
    this->lambda.~function();
    this->body = NULL;
    this->body.~shared_ptr();
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
    accessMutex.lock();
    this->error = false;
    this->used = false;
    this->shouldExit = true;
    this->autoprint = true;
    this->subject = CV::TokenType(NULL);
    this->message = "";
    accessMutex.unlock();
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

std::string CV::DataToText(const std::shared_ptr<CV::Program> &prog, CV::Data *t){
    switch(t->type){
        
        default:
        case CV::DataType::NIL: {
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::DataType::NUMBER: {
            return      CV::Tools::setTextColor(Tools::Color::CYAN)+
                        CV::Tools::removeTrailingZeros(static_cast<CV::DataNumber*>(t)->value)+
                        CV::Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::DataType::STRING: {
            auto out = static_cast<CV::DataString*>(t)->value;

            return  Tools::setTextColor(Tools::Color::GREEN)+
                    "'"+out+
                    "'"+Tools::setTextColor(Tools::Color::RESET);
        };     

        case CV::DataType::FUNCTION: {
            auto fn = static_cast<CV::DataFunction*>(t);      

            std::string start = Tools::setTextColor(Tools::Color::RED, true)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string end = Tools::setTextColor(Tools::Color::RED, true)+"]"+Tools::setTextColor(Tools::Color::RESET);
            std::string name = Tools::setTextColor(Tools::Color::RED, true)+"fn"+Tools::setTextColor(Tools::Color::RESET);
            std::string binary = Tools::setTextColor(Tools::Color::BLUE, true)+"BINARY"+Tools::setTextColor(Tools::Color::RESET);
            std::string body = Tools::setTextColor(Tools::Color::BLUE, true)+(fn->body.get() ? fn->body->str() : "")+Tools::setTextColor(Tools::Color::RESET);
            std::string variadic = Tools::setTextColor(Tools::Color::YELLOW, true)+"@"+Tools::setTextColor(Tools::Color::RESET);

            return start+name+" "+start+(fn->isVariadic ? variadic : CV::Tools::compileList(fn->params))+end+" "+( fn->isBinary ? binary : body )+end;
        };

        case CV::DataType::LIST: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);

            auto list = static_cast<CV::DataList*>(t);

            int total = list->value.size();
            int limit = total > 30 ? 10 : total;
            for(int i = 0; i < limit; ++i){
                auto &id = list->value[i];
                auto q = prog->getData(id);
                output += q ? CV::DataToText(prog, q) : "[NULL]";
                if(i < list->value.size()-1){
                    output += " ";
                }
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);


            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::DataType::STORE: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);

            auto store = static_cast<CV::DataStore*>(t);

            int total = store->value.size();
            int limit = total > 30 ? 10 : total;
            int i = 0;
            for(auto &it : store->value){
                auto &id = it.second;
                auto q = prog->getData(id);
                output +=   Tools::setTextColor(Tools::Color::YELLOW)+"["+Tools::setTextColor(Tools::Color::RESET)+
                            "~"+it.first+" "+(q ? CV::DataToText(prog, q): "[NULL]")+
                            Tools::setTextColor(Tools::Color::YELLOW)+"]"+Tools::setTextColor(Tools::Color::RESET);
                if(i < store->value.size()-1){
                    output += " ";
                }
                ++i;
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);

            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };        

        // case CV::DataType::THREAD: {
        //     auto thread = std::static_pointer_cast<CV::TypeThread>(t);

        //     return Tools::setTextColor(Tools::Color::YELLOW)
        //                         +"[THREAD "+std::to_string(thread->id)+" "
        //                         +Tools::setTextColor(Tools::Color::GREEN)
        //                         +"'"+CV::ThreadState::name(thread->state)
        //                         +"'"
        //                         +Tools::setTextColor(Tools::Color::YELLOW)+"]"
        //                         +Tools::setTextColor(Tools::Color::RESET);
        // };

    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PARSING
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CV::Token::Token(){
    solved = false;
    complex = false;
}

CV::Token::Token(const std::string &first, unsigned line){
    this->first = first;
    this->line = line;
    refresh();
}    

std::shared_ptr<CV::Token> CV::Token::emptyCopy(){
    auto c = std::make_shared<CV::Token>();
    c->first = this->first;
    c->line = this->line;
    c->refresh();
    return c;
}
std::shared_ptr<CV::Token> CV::Token::copy(){
    auto c = std::make_shared<CV::Token>();
    c->first = this->first;
    c->line = this->line;
    c->inner = this->inner;
    c->refresh();
    return c;
}

std::string CV::Token::str() const {
    auto f = this->first;
    std::string out = f + (this->first.size() > 0 && this->inner.size() > 0  ? " " : "");
    for(int i = 0; i < this->inner.size(); ++i){
        out += inner[i]->inner.size() == 0 ? inner[i]->first : inner[i]->str();
        if(i < this->inner.size()-1){
            out += " ";
        }
    }
    return this->inner.size() > 0 ? "[" + out + "]" : out;
}

void CV::Token::refresh(){
    complex = this->inner.size() > 0;
    solved = !(first.length() >= 3 && first[0] == '[' && first[first.size()-1] == ']');
}

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

CV::InsType CV::Compile(const CV::TokenType &input, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx){

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

CV::InsType CV::Compile(const std::string &input, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx){   

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
    // Double brackets (Statement must respect the "[ [] -> STATEMENT ] -> PROGRAM" hierarchy)
    cleanInput = CV::Tools::cleanTokenInput(fixedInput);
    if( cleanInput[0] == '[' && cleanInput[1] != '[' &&
        cleanInput[cleanInput.size()-1] == ']' && cleanInput[cleanInput.size()-2] != ']'){
        fixedInput = "["+fixedInput+"]";
    }

    // Split basic tokens
    auto tokens = parseTokens(fixedInput, ' ', cursor, 1);
    if(cursor->error){
        return prog->createInstruction(
            CV::InstructionType::NOOP,
            tokens.size() == 0 ? std::make_shared<CV::Token>(fixedInput, cursor->line) : tokens[0]
        );
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

    // Convert tokens into instructions
    std::vector<CV::InsType> instructions;
    for(int i = 0 ; i < root.size(); ++i){
        auto ins = CV::Translate(root[i], prog, ctx ? ctx : prog->root, cursor);
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

CV::InsType CV::Translate(const CV::TokenType &token, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor){
    
    if(token->first.size() == 0 && token->inner.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, token);
        return prog->createInstruction(CV::InstructionType::NOOP, token);
    }

    auto bListConstruct = [prog, cursor](const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_LIST, origin);
        auto list = new CV::DataList();
        prog->allocateData(list);
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
                        list->value.push_back(-1);
                    }
                }else{
                    ins->params.push_back(fetched->id);
                    list->value.push_back(-1);                    
                }
            }else{
                ins->params.push_back(fetched->id);
                list->value.push_back(-1);
            }
        }
        return ins;
    };


    auto bStoreConstruct = [prog, cursor](const std::string &name, const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_STORE, origin);
        auto store = new CV::DataStore();
        prog->allocateData(store);
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
            // We eat up the instruction itself as we don't really execute it for this situation
            if(fetched->params.size() == 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' expects Namer prefixed tokens to have an appended body so it defines a value", origin);
                return prog->createInstruction(CV::InstructionType::NOOP, origin);                
            }
            ins->params.push_back(fetched->params[0]);
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
            store->value[vname] = -1;
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

    auto isNameFunction = [prog](const CV::TokenType &token, const CV::ContextType &ctx){
        if(!token->solved){
            return false;
        }
        if(CV::Tools::isReservedWord(token->first)){
            return true;
        }
        auto dataId = ctx->getNamed(prog, token->first).second;
        if(dataId == -1){
            return false;
        }
        auto quant = prog->getData(dataId);  
        return  quant->type == CV::DataType::FUNCTION ||
                quant->type == CV::DataType::STORE;
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

    if(token->first.size() == 0){
        // Instruction list
        if(areAllFunctions(token, ctx) && token->inner.size() >= 2){
            return CV::Compile(token, prog, cursor, ctx);
        }else
        if(areAllNames(token, ctx)){
            return bStoreConstruct(token->first, token, token->inner, ctx);
        }else{
        // Or list?
            return bListConstruct(token, token->inner, ctx);
        }
    }else{
        /*
            b:store
        */
        if(token->first == "b:store"){
            return bStoreConstruct(token->first, token, token->inner, ctx);
        }else
        /*
            b:list
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
            NIL
        */
        if(token->first == "nil"){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                auto data = prog->buildNil();
                prog->allocateData(data);
                ins->data.push_back(data->id);
                return ins;
            } 
        }else
        /*
            BRING
        */
        // if(token->first== "bring"){
        //     if(token->inner.size() != 1){
        //         cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
        //     if(cursor->error){
        //         cursor->subject = token;
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }
            
        //     if(!CV::ErrorCheck::ExpectNoPrefixer(token->first, {target}, token, cursor)){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }
        //     auto st = std::make_shared<CV::ControlFlow>();
        //     auto fnamev = CV::Execute(target, ctx, prog, cursor, st);
        //     if(cursor->error){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     if(!CV::ErrorCheck::ExpectsTypeAt(fnamev->type, CV::QuantType::STRING, 0, token->first, token, cursor)){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     // Get symbolic or literal
        //     auto fname = std::static_pointer_cast<CV::TypeString>(fnamev)->v;

        //     // Does it include .cv?
        //     if(CV::Tools::fileExtension(fname) == ""){
        //         fname += ".cv";
        //     }

        //     // Is it a local file?
        //     if(CV::Tools::fileExists("./"+fname)){
        //         fname = "./"+fname;
        //     }else{
        //     // Then it must be a 'lib/' library
        //         fname = CV_LIB_HOME+"/"+fname;
        //     }

        //     // Does it exist at all?
        //     if(!CV::Tools::fileExists(fname)){
        //         cursor->setError(CV_ERROR_MSG_LIBRARY_NOT_VALID, "No canvas library '"+fname+"' was found", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     // Load it
        //     int id = CV::Import(fname, prog, ctx, cursor);
        //     if(cursor->error){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     auto ins = prog->createInstruction(CV::InstructionType::LIBRAY_IMPORT, token);
        //     ins->data.push_back(ctx->id);
        //     ins->data.push_back(id);
        //     ins->data.push_back(true);
        //     ins->literal.push_back(fname);
        //     return ins;
        // }else
        /*
            BRING "dynamic-library"
        */
        // if(token->first == "bring:dynamic-library"){
        //     if(token->inner.size() != 1){
        //         cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
        //     if(cursor->error){
        //         cursor->subject = token;
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }
            
        //     if(!CV::ErrorCheck::ExpectNoPrefixer(token->first, {target}, token, cursor)){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }
        //     auto st = std::make_shared<CV::ControlFlow>();
        //     auto fnamev = CV::Execute(target, ctx, prog, cursor, st);
        //     if(cursor->error){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     if(!CV::ErrorCheck::ExpectsTypeAt(fnamev->type, CV::QuantType::STRING, 0, token->first, token, cursor)){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     // Get symbolic or literal
        //     auto fname = std::static_pointer_cast<CV::TypeString>(fnamev)->v;

        //     // Does it include .cv?
        //     if(CV::Tools::fileExtension(fname) == ""){
        //         switch(CV::PLATFORM){
        //             case CV::SupportedPlatform::LINUX: {
        //                 fname += ".so";
        //             } break; 
        //             case CV::SupportedPlatform::WINDOWS: {
        //                 fname += ".dll";
        //             } break;
        //             case CV::SupportedPlatform::OSX: {
        //                 // TODO
        //             } break;                
        //             default:
        //             case CV::SupportedPlatform::UNKNOWN: {
        //                 fprintf(stderr, "Failed to infere dynamic library platform in '%s'", token->first.c_str());
        //                 std::exit(1);
        //             } break;                        
        //         }
        //     }

        //     // Is it a local file?
        //     if(CV::Tools::fileExists("./"+fname)){
        //         fname = "./"+fname;
        //     }else{
        //     // Then it must be a 'lib/' library
        //         fname = CV_LIB_HOME+"/"+fname;
        //     }

        //     // Does it exist at all?
        //     if(!CV::Tools::fileExists(fname)){
        //         cursor->setError(CV_ERROR_MSG_LIBRARY_NOT_VALID, "No dynamic library '"+fname+"' was found", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     // Load it
        //     int id = CV::ImportDynamicLibrary(fname, fname, prog, ctx, cursor);
        //     if(cursor->error){
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     auto ins = prog->createInstruction(CV::InstructionType::LIBRAY_IMPORT, token);
        //     ins->data.push_back(ctx->id);
        //     ins->data.push_back(id); 
        //     ins->data.push_back(false);
        //     ins->literal.push_back(fname);
        //     return ins;
        // }else
        /*
            FN
        */
        if(token->first == "fn"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" [arguments][code])", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto fn = new CV::DataFunction();
            prog->allocateData(fn);

            fn->isBinary = false;
            fn->body = token->inner[1];

            auto paramNameList = token->inner[0]->inner;
            paramNameList.insert(paramNameList.begin(), token->inner[0]);

            if(paramNameList.size() == 1 && paramNameList[0]->first == "@"){
                fn->isVariadic = true;
            }else{
                for(int i = 0; i < paramNameList.size(); ++i){
                    auto &name = paramNameList[i]->first;
                    if(!CV::Tools::isValidVarName(name)){
                        cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' argument name '"+name+"' is an invalid", token);
                        return prog->createInstruction(CV::InstructionType::NOOP, token);                   
                    }
                    if(CV::Tools::isReservedWord(name)){
                        cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' argument name '"+name+"' is a name of native constructor which cannot be overriden", token);
                        return prog->createInstruction(CV::InstructionType::NOOP, token);                
                    }  
                    if(name == "@"){
                        cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' argument name '"+name+"' is illegal in definitions with more than one argument", token);
                        return prog->createInstruction(CV::InstructionType::NOOP, token);                          
                    }
                    fn->params.push_back(name);
                }
            }

            fn->body = token->inner[1];
            auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
            ins->data.push_back(fn->id);
            return ins;
            
        }else
        /*
            CC
        */
        // if(token->first == "cc"){
        //     if(token->inner.size() != 1){
        //         cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 2 tokens (cc VALUE)", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }         
        //     auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
        //     if(cursor->error){
        //         cursor->subject = token;
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }
        //     auto ins = prog->createInstruction(CV::InstructionType::CARBON_COPY, token);
        //     auto nv = CV::Build::Nil();
        //     ctx->set(nv->id, nv);
        //     ins->data.push_back(ctx->id);
        //     ins->data.push_back(nv->id);
        //     ins->params.push_back(target->id);
        //     return ins;
        // }else
        /*
            MUT
        */
        if(token->first == "mut"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens (mut NAME VALUE)", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto nameRef = ctx->getNamed(prog, token->inner[0]->first);
            if(nameRef.second == -1){
                cursor->setError(CV_ERROR_MSG_UNDEFINED_IMPERATIVE, "Name '"+token->first+"'", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token); 
            }
            auto target = CV::Translate(token->inner[1], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            } 
            auto ghostData = prog->buildNil();
            auto ins = prog->createInstruction(CV::InstructionType::MUT, token);
            ins->data.push_back(nameRef.second);
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
            
            // if(target->type == CV::InstructionType::PROXY_PARALELER){

            //     auto &targetData = prog->getCtx(target->data[2])->get(target->data[3]);
            //     ctx->set(targetData->id, targetData);
            //     ctx->setName(name, targetData);  
            //     return target;

            // }else

            auto ghostData = prog->buildNil();
            ctx->setData(prog, ghostData->id);
            ctx->setName(name, ghostData->id);

            auto ins = prog->createInstruction(CV::InstructionType::LET, token);
            ins->literal.push_back(name);
            ins->data.push_back(ghostData->id);
            ins->params.push_back(target->id);

            return ins;

        }else        
        /*
            NUMBER
        */
        if(CV::Tools::isNumber(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
                auto data = new CV::DataNumber(std::stod(token->first));
                prog->allocateData(data);
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
                auto string = new CV::DataString(token->first.substr(1, token->first.length() - 2));
                prog->allocateData(string);
                ins->data.push_back(string->id);
                return ins;
            }
        }else
        // PARALLELER
        // if(token->first[0] == '|'){

        //     if(token->inner.size() != 0){
        //         cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Paralleler Prefix '"+token->first+"' expects no values", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);                                            
        //     }

        //     if(token->first.size() == 1){
        //         cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Paralleler Prefix '"+token->first+"' expects an apppended body", token);
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);                
        //     }

        //     std::string statement = std::string(token->first.begin()+1, token->first.end());

        //     auto ins = prog->createInstruction(CV::InstructionType::PROXY_PARALELER, token);
        //     ins->data.push_back(CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION);
        //     ins->data.push_back(CV::Prefixer::PARALELLER);
        //     std::shared_ptr<CV::Quant> ghostData;

        //     auto tctx = prog->createContext(ctx);

        //     auto entrypoint = CV::Compile(statement, prog, cursor, tctx);
        //     if(cursor->error){
        //         cursor->subject = token;
        //         return prog->createInstruction(CV::InstructionType::NOOP, token);
        //     }

        //     auto data = CV::Build::Thread();
        //     tctx->set(data->id, data);

        //     data->ctxId = tctx->id;
        //     data->entrypoint = entrypoint->id;

        //     ins->data.push_back(tctx->id);
        //     ins->data.push_back(data->id);
        //     ins->params.push_back(entrypoint->id);

        //     return ins;

        // }else
        // EXPANDER
        if(token->first[0] == '^'){
            if(token->inner.size() != 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Expander Prefix '"+token->first+"' expects no values", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                                            
            }

            if(token->first.size() == 1){
                cursor->setError(CV_ERROR_MSG_MISUSED_PREFIX, "Expander Prefix '"+token->first+"' expects an apppended body", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);                
            }

            std::string statement = std::string(token->first.begin()+1, token->first.end());

            auto ins = prog->createInstruction(CV::InstructionType::PROXY_EXPANDER, token);
            ins->data.push_back(CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION);
            ins->data.push_back(CV::Prefixer::EXPANDER);

            auto entrypoint = CV::Compile(statement, prog, cursor, ctx);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            ins->params.push_back(entrypoint->id);

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
            
            auto ghostData = prog->buildNil();
            ins->data.push_back(ghostData->id);

            // Namer as a definitor is passive as avoid violating immutability
            if(!ctx->isName(name)){
                ctx->setName(ins->literal[0], ghostData->id);         
            }

            // Build referred instruction
            if(token->inner.size() > 0){
                auto &inc = token->inner[0];
                auto fetched = CV::Translate(inc, prog, ctx, cursor);
                if(cursor->error){
                    cursor->subject = token;
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }                        
                ins->params.push_back(fetched->id);
            }
            
            return ins;
        }else{
            /*
                IS NAME?        
            */   
            auto hctx = ctx;
            std::string qname = token->first;
            auto nameRef = ctx->getNamed(prog, token->first);
            if(nameRef.second != -1){
                // TODO: Check for PROMISE names
                auto fctx = prog->getContext(nameRef.first);
                auto &dataId = nameRef.second;
                auto data = prog->getData(dataId);     

                switch(data->type){
                    case CV::DataType::FUNCTION: {
                        auto ins = prog->createInstruction(CV::InstructionType::CF_INVOKE_FUNCTION, token);
                        
                        auto fn = static_cast<CV::DataFunction*>(data);

                        ins->data.push_back(data->id);
                        ins->literal.push_back(qname);
                        int c = 0;
                        std::vector<std::string> names;

                        auto mismatchnArg = [&](int c){
                            std::string comp = "provided "+std::to_string(c)+", expected "+std::to_string(fn->params.size());
                            std::string err = "was provided with an invalid number of arguments (with or without named positional arguments): "+err;
                            cursor->setError(CV_ERROR_MSG_MISUSED_FUNCTION, "Function '"+token->first+"'"+err, token);
                            return prog->createInstruction(CV::InstructionType::NOOP, token);                              
                        };

                        auto pushOrderArg = [&](){
                            while(CV::Tools::isInList(fn->params[c], names)){
                                ++c;
                            }
                            if(c >= fn->params.size()){
                                return mismatchnArg(c);                                        
                            }
                            names.push_back(fn->params[c]);
                            ++c;      
                            return std::shared_ptr<CV::Instruction>(NULL);                                      
                        };

                        for(int i = 0; i < token->inner.size(); ++i){
                            auto &inc = token->inner[i];
                            
                            auto fetched = CV::Translate(inc, prog, ctx, cursor);
                            if(cursor->error){
                                cursor->subject = token;
                                return prog->createInstruction(CV::InstructionType::NOOP, token);
                            }

                            if(fetched->type == CV::InstructionType::PROXY_EXPANDER){
                                auto targetIns = prog->getIns(fetched->params[0]);
                                if(targetIns->type == CV::InstructionType::CONSTRUCT_STORE){
                                    for(int j = 0; j < targetIns->params.size(); ++j){
                                        names.push_back(targetIns->literal[j]);
                                        ins->params.push_back(targetIns->params[j]);
                                    }
                                }else
                                if(targetIns->type == CV::InstructionType::CONSTRUCT_LIST){
                                    for(int j = 0; j < targetIns->params.size(); ++j){
                                        if(fn->isVariadic){
                                            ins->params.push_back(targetIns->params[j]);
                                            names.push_back("p"+std::to_string(i));
                                        }else{
                                            auto r = pushOrderArg();
                                            if(r){
                                                return r;
                                            }
                                            ins->params.push_back(targetIns->params[j]);
                                        }                                         
                                    }                                    
                                }else{
                                    ins->params.push_back(fetched->id);
                                }
                            }else{
                                if(fn->isVariadic){
                                    names.push_back("p"+std::to_string(i));
                                }else
                                if(fn->params.size() > 0){
                                    if(fetched->type == CV::InstructionType::PROXY_NAMER){
                                        auto &vname = fetched->literal[0];
                                        names.push_back(vname);
                                    }else{
                                        auto r = pushOrderArg();
                                        if(r){
                                            return r;
                                        }
                                    }
                                }
                                ins->params.push_back(fetched->id);
                            }
                        }
                        
                        if(!fn->isVariadic && fn->params.size() != names.size()){
                            return mismatchnArg(names.size());  
                        }

                        for(int i = 0; i < names.size(); ++i){
                            ins->literal.push_back(names[i]);
                        }

                        return ins;
                    };
                    case CV::DataType::STORE: {
                        auto store = static_cast<CV::DataStore*>(data);
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
                                    if(store->value.count(names[i]) == 0){
                                        cursor->setError(CV_ERROR_MSG_STORE_UNDEFINED_MEMBER, "Store '"+qname+"' has no member named '"+names[i]+"'", token);
                                        return prog->createInstruction(CV::InstructionType::NOOP, token);                
                                    }
                                }
                                // Build Access Proxy
                                auto buildAccessProxy = [token](int dataId, const std::string &mname, const CV::ProgramType &prog){
                                    auto ins = prog->createInstruction(CV::InstructionType::PROXY_ACCESS, token);
                                    ins->data.push_back(dataId);
                                    ins->literal.push_back(mname);
                                    return ins;                                     
                                };
                                // One name returns the value itself
                                if(names.size() == 1){
                                    return buildAccessProxy(dataId, names[0], prog);
                                }else{
                                // Several names returns a list of values
                                    auto ins = prog->createInstruction(CV::InstructionType::CONSTRUCT_LIST, token);
                                    auto list = new CV::DataList();
                                    prog->allocateData(list);
                                    ins->data.push_back(list->id);
                                    for(int i = 0; i < names.size(); ++i){
                                        auto fetched = buildAccessProxy(dataId, names[i], prog);
                                        ins->params.push_back(fetched->id);
                                        list->value.push_back(-1);
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
                            ins->data.push_back(dataId);
                            return ins;                            
                        }
                    };  
                    default: {
                        if(token->inner.size() > 0){
                            return bListConstructFromToken(token, ctx);
                        }else{  
                            auto ins = prog->createInstruction(CV::InstructionType::PROXY_STATIC, token);
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



static CV::Data *__flow(  const CV::InsType &ins,
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
        // case CV::InstructionType::PROXY_ACCESS: {
        //     auto ctxId = ins->data[0];
        //     auto dataId = ins->data[1];
        //     std::string mname = ins->literal[0];
        //     auto &quant = prog->getCtx(ctxId)->get(dataId);
        //     auto store = std::static_pointer_cast<CV::TypeStore>(quant);
        //     // TODO: Perhaps some error checking?
        //     return store->v[mname];
        // };
        // case CV::InstructionType::PROXY_PARALELER: {
        //     auto tctxId = ins->data[2];
        //     auto dataId = ins->data[3];
        //     auto entrypoint = ins->params[0];

        //     auto tctxt = prog->getCtx(tctxId);
        //     auto quant = tctxt->get(dataId);
        //     auto thread = std::static_pointer_cast<CV::TypeThread>(quant);
            
        //     if(thread->state == CV::ThreadState::CREATED){
        //         thread->setState(CV::ThreadState::STARTED);
        //         auto handle = std::make_shared<InvokeThread>();
        //         handle->core = thread.get();
        //         handle->prog = prog;
        //         handle->upcursor = cursor;
        //         handle->root = ins->token;
        //         prog->threadMutex.lock();
        //         prog->threads[thread->id] = std::thread(&RUN_THREAD, handle);
        //         prog->threadMutex.unlock();
        //     }
            
        //     return thread;
        // };
        case CV::InstructionType::PROXY_EXPANDER: {
            
            if(prog->isPrefetched(ins->id)){
                return prog->getData(prog->getPrefetch(ins->id));
            }
            
            auto &entrypoint = prog->getIns(ins->params[0]);
            auto data = CV::Execute(entrypoint, prog, cursor, ctx, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            prog->setPrefetch(ins->id, data->id);

            return data;            
        };
        case CV::InstructionType::PROXY_NAMER: {
            
            if(prog->isPrefetched(ins->id)){
                return prog->getData(prog->getPrefetch(ins->id));
            }

            CV::Data *data = prog->getData(ins->data[2]);
            
            if(ins->params.size() > 0){
                auto &entrypoint = prog->getIns(ins->params[0]);
                data = CV::Execute(entrypoint, prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }
                if(!ctx->isDataIn(data->id)){
                    data->incRefCount();
                    prog->swapId(data->id, ins->data[2]);
                }
            }

            prog->setPrefetch(ins->id, data->id);

            return data;
        };
        case CV::InstructionType::PROXY_STATIC: {
            return prog->getData(ins->data[0]);
        };
        // case CV::InstructionType::PROXY_PROMISE: {
        //     // if(ctx->prefetched.count(ins->id) > 0){
        //     //     auto dir = ctx->prefetched[ins->id];
        //     //     return prog->ctx[dir[0]]->memory[dir[1]];
        //     // }
        //     auto cctx = prog->getCtx(ins->data[0]);
        //     auto targetDataId = ins->data[1];
        //     // Execute (The instruction needs to fulfill the promise on its side)
        //     auto &entrypoint = prog->getIns(ins->params[0]);
        //     auto quant = CV::Execute(entrypoint, cctx, prog, cursor, st);
        //     if(cursor->error){
        //         st->state = CV::ControlFlowState::CRASH;
        //         return cctx->get(targetDataId);
        //     }            
        //     // ctx->prefetched[ins->id] = {(unsigned)cctx->id, (unsigned)targetDataId};
        //     return cctx->get(targetDataId);
        // };                    

        /*
            CONSTRUCTORS        
        */
        case CV::InstructionType::CONSTRUCT_LIST: {
            auto list = static_cast<CV::DataList*>(prog->getData(ins->data[0]));
            for(int i = 0; i < ins->params.size(); ++i){
                auto cins = prog->getIns(ins->params[i]);
                bool isExpander = cins->type == CV::InstructionType::PROXY_EXPANDER;
                auto data = CV::Execute(cins, prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }
                if(isExpander && data->type == CV::DataType::LIST){
                    auto subject = static_cast<CV::DataList*>(data);
                    for(int j = 0; j < subject->value.size(); ++j){
                        list->value.push_back(subject->value[j]);
                    }
                }else{
                    data->incRefCount();
                    list->value[i] = data->id;
                }
            }   
            return list;
        };
        case CV::InstructionType::CONSTRUCT_STORE: {
            auto store = static_cast<CV::DataStore*>(prog->getData(ins->data[0]));
            for(int i = 0; i < ins->params.size(); ++i){
                auto quant = CV::Execute(prog->getIns(ins->params[i]), prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }
                quant->incRefCount();
                store->value[ins->literal[i]] = quant->id;
            }   
            return store;
        };

        case CV::InstructionType::MUT: {
            
            if(prog->isPrefetched(ins->id)){
                return prog->getData(prog->getPrefetch(ins->id));
            }
            
            auto data = prog->getData(ins->data[0]);
            auto &entrypoint = prog->getIns(ins->params[0]);

            auto v = CV::Execute(entrypoint, prog, cursor, ctx, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }
            
            if(data->type != CV::DataType::NUMBER && data->type != CV::DataType::STRING){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: Only numbers and string types might be mutated. Target is '"+CV::DataTypeName(data->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }
            if(v->type != CV::DataType::NUMBER && v->type != CV::DataType::STRING){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: Only numbers and string types might be mutated. New value is '"+CV::DataTypeName(v->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }

            if(v->type != data->type){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "mut: both target and new value must match in type. Target is '"+CV::DataTypeName(data->type)+"' and new value is '"+CV::DataTypeName(v->type)+"'", ins->token);
                st->state = CV::ControlFlowState::CRASH;
                return data;
            }

            switch(data->type){
                case CV::DataType::STRING: {
                    static_cast<CV::DataString*>(data)->value = static_cast<CV::DataString*>(v)->value;
                } break;
                case CV::DataType::NUMBER: {
                    static_cast<CV::DataNumber*>(data)->value = static_cast<CV::DataNumber*>(v)->value;
                } break;
            }

            prog->setPrefetch(ins->id, v->id);

            return data;
        };

        case CV::InstructionType::LET: {

            if(prog->isPrefetched(ins->id)){
                return prog->getData(prog->getPrefetch(ins->id));
            }

            auto &name = ins->literal[0];
            auto &entrypoint = prog->getIns(ins->params[0]);
            auto dataId = ins->data[0];

            auto v = CV::Execute(entrypoint, prog, cursor, ctx, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return v;
            }
            v->incRefCount();
            ctx->setData(prog, v->id);
            prog->swapId(dataId, v->id);
            
            prog->setPrefetch(ins->id, v->id);

            return v;
        };

        // case CV::InstructionType::CARBON_COPY: {
        //     if(ctx->isPrefetched(ins->id)){
        //         auto dir = ctx->getPrefetch(ins->id);
        //         return prog->getCtx(dir[0])->get(dir[1]);
        //     }

        //     auto &cctx = prog->getCtx(ins->data[0]);
        //     auto &nv = cctx->get(ins->data[1]);

        //     auto &entrypoint = prog->getIns(ins->params[0]);

        //     auto v = CV::Execute(entrypoint, cctx, prog, cursor, st);
        //     if(cursor->error){
        //         st->state = CV::ControlFlowState::CRASH;
        //         return v;
        //     }
        //     auto copied = CV::Build::Copy(v);
        //     cctx->set(ins->data[1], copied);

        //     ctx->setPrefetch(ins->id, {(unsigned)cctx->id, (unsigned)copied->id});
        
        //     return copied;
        // };
        
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
                auto v = CV::Execute(prog->getIns(ins->params[0]), prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return v;
                }   
                st->payload = v;
                return v;
            }
            return prog->buildNil();
        };
        case CV::InstructionType::CF_INVOKE_FUNCTION: {

            auto fn = static_cast<CV::DataFunction*>(prog->getData( ins->data[0] ));
            auto paramCtx = prog->createContext(ctx->id); 

            // Prepare params
            std::vector<std::shared_ptr<CV::Instruction>> params;
            std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> allParams;
            for(int i = 0; i < ins->params.size(); ++i){
                params.push_back(prog->getIns(ins->params[i]));
            }

            auto checkName = [&](const std::string &name){
                for(int i = 0; i < allParams.size(); ++i){
                    if(allParams[i].first == name){
                        return true;
                    }
                }
                return false;
            };

            // Fulfill names
            for(int i = 0; i < params.size(); ++i){
                allParams.push_back(
                    {
                        ins->literal[i+1],
                        params[i]
                    }
                );
            }

            // Check if there are unfulfilled names
            if(!fn->isVariadic){
                for(int i = 0; i < fn->params.size(); ++i){
                    if(!checkName(fn->params[i])){
                        std::string err = "expects argument '"+fn->params[i]+"' but was not provided (with or without named positional arguments)";
                        cursor->setError(CV_ERROR_MSG_MISUSED_FUNCTION, "Function '"+ins->literal[0]+"' "+err, ins->token);    
                        st->state = CV::ControlFlowState::CRASH;
                        return prog->buildNil();                        
                    }
                }
            }

            CV::Data *result;

            // Execute
            if(fn->isBinary){
                // a binary function is in charge of compiling the instructions
                auto v = fn->lambda(
                    prog,
                    ins->literal[0],
                    allParams,
                    ins->token,
                    cursor,
                    paramCtx,
                    st
                );
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }                
                result = v;
            }else{
                // Execute params
                if(fn->isVariadic){
                    auto variadic = new CV::DataList();
                    prog->allocateData(variadic);
                    paramCtx->setName("@", variadic->id);
                    paramCtx->setData(prog, variadic->id);
                    for(int i = 0; i < allParams.size(); ++i){
                        auto &cins = allParams[i].second;
                        auto v = CV::Execute(cins, prog, cursor, paramCtx, st);
                        if(cursor->error){
                            return prog->buildNil();
                        }                            
                        variadic->value.push_back(v->id);
                    }                    
                }else{
                    for(int i = 0; i < allParams.size(); ++i){
                        auto &name = allParams[i].first;
                        auto &cins = allParams[i].second;
                        auto v = CV::Execute(cins, prog, cursor, paramCtx, st);
                        if(cursor->error){
                            return prog->buildNil();
                        }                            
                        if(!paramCtx->isName(name)){
                            paramCtx->setName(name, v->id);
                        }
                        if(!paramCtx->isDataIn(v->id)){
                            paramCtx->setData(prog, v->id);
                        }
                    }
                }
                // Compile body
				auto entrypoint = CV::Compile(fn->body, prog, cursor, paramCtx);
				if(cursor->error){
                    return prog->buildNil();
				}
				// Execute
				auto v = CV::Execute(entrypoint, prog, cursor, paramCtx, st);
				if(cursor->error){
                    return prog->buildNil();
				}
                result = v;
            }

            prog->removeContext(paramCtx->id);
        
            return result;
        } break;
        // case CV::InstructionType::CF_INVOKE_BINARY_FUNCTION: {
        //     auto &fnData = prog->getCtx(ins->data[0])->get(ins->data[1]);
        //     auto fn = std::static_pointer_cast<CV::TypeFunctionBinary>(fnData);

        //     auto &paramCtx = prog->getCtx(ins->data[2]);

        //     int targetCtxId = ins->data[4];
        //     int targetDataId = ins->data[5];

        //     // Compile operands
        //     int nArgs = ins->data[3];
        //     std::vector<std::shared_ptr<CV::Instruction>> arguments;
        //     for(int i = 0; i < nArgs; ++i){
        //         auto cins = prog->getIns(ins->params[i]);
        //         if(cins->type == CV::InstructionType::PROXY_EXPANDER){
        //             auto innerIns = prog->getIns(cins->params[0]);
        //             if(innerIns->type == CV::InstructionType::CONSTRUCT_LIST){
        //                 for(int j = 0; j < innerIns->params.size(); ++j){
        //                     arguments.push_back( prog->getIns(innerIns->params[j]) );
        //                 }
        //             }else{
        //                 arguments.push_back(cins);
        //             }
        //         }else{
        //             arguments.push_back(cins);
        //         }
        //     }

        //     // Invoke it in a temporary context
        //     auto tempCtx = prog->createContext(paramCtx);
        //     if(fn->ref == NULL){
        //         // Use lambda
        //         fn->lambda(arguments, fn->name, ins->token, cursor, tempCtx->id, targetCtxId, targetDataId, prog, st);
        //     }else{
        //         // Cast ref into pointer function
        //         void (*ref)(CV_BINARY_FN_PARAMS) = (void (*)(CV_BINARY_FN_PARAMS))fn->ref;
        //         // Execute
        //         ref(arguments, fn->name, ins->token, cursor, tempCtx->id, targetCtxId, targetDataId, prog, st);
        //     }
        //     if(cursor->error){
        //         st->state = CV::ControlFlowState::CRASH;
        //     }             
        //     // Delete context afterwards. The target value must be within the level above
        //     prog->deleteContext(tempCtx->id);

        //     return std::shared_ptr<CV::Quant>(NULL);
        // };
        // /*
        //     LIBRARY IMPORT
        // */
        // case CV::InstructionType::LIBRAY_IMPORT: {
        //     return CV::Build::Number(ins->data[1]); // returns library id handle            
        // } break;
        /*
            NOOP
        */
        case CV::InstructionType::NOOP: {
            st->state = CV::ControlFlowState::CRASH;
            return prog->buildNil();            
        } break;
        /*
            INVALID
        */
        default: {
            cursor->setError(CV_ERROR_MSG_INVALID_INSTRUCTION, "Instruction Type '"+std::to_string(ins->type)+"' is undefined/invalid", ins->token);
            st->state = CV::ControlFlowState::CRASH;
            return prog->buildNil();
        };
    }
}

CV::Data *CV::Execute(const CV::InsType &entry, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx, CFType cf){

    CV::InsType ins = entry;
    CV::Data *result;
    auto st = cf.get() == NULL ? std::make_shared<CV::ControlFlow>() : cf;

    if( 
        cursor->error ||
        st->state == CV::ControlFlowState::YIELD ||
        st->state == CV::ControlFlowState::SKIP ||
        st->state == CV::ControlFlowState::CRASH ||
        st->state == CV::ControlFlowState::RETURN
    ){
        return prog->buildNil();
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

CV::Context::Context(int root){
    this->top = root;
    this->id = GEN_ID();
}

bool CV::Context::setData(const std::shared_ptr<CV::Program> &prog, int dataId){
    mutexData.lock();
    if(this->data.count(dataId) == 0){
        mutexData.unlock();
        return false;
    }
    auto subject = prog->getData(dataId);
    if(subject == NULL){
        mutexData.unlock();
        return false;
    }
    subject->incRefCount();
    this->data.insert(dataId);
    mutexData.unlock();
    return true;
}

bool CV::Context::removeData(const std::shared_ptr<CV::Program> &prog, int dataId){
    mutexData.lock();
    if(this->data.count(dataId) == 0){
        mutexData.unlock();
        return false;
    }
    auto subject = prog->getData(dataId);
    if(subject == NULL){
        mutexData.unlock();
        return false;
    }
    if(this->isNamed(dataId)){
        this->removeName(dataId);
    }
    subject->decRefCount();
    this->data.erase(dataId);
    mutexData.unlock();
    return true;
}

bool CV::Context::isNamed(int dataId){
    mutexNames.lock();
    for(auto &it : this->names){
        if(it.second == dataId){
            mutexNames.unlock();
            return true;
        }
    }
    mutexNames.unlock();
    return false;
}

std::pair<int,int> CV::Context::getNamed(const std::shared_ptr<CV::Program> &prog, const std::string &name, bool localOnly){
    mutexNames.lock();
    auto it = this->names.find(name);
    bool found = it != this->names.end();
    mutexNames.unlock();

    auto upCtx = prog->getContext(this->top);

    return found ? std::pair<int,int>{this->id, it->second} : (
        !localOnly && upCtx.get() ? upCtx->getNamed(prog, name) :  std::pair<int,int>{-1, -1}
    );
}

bool CV::Context::isName(const std::string &name){
    mutexNames.lock();
    auto v = this->names.count(name) != 0;
    mutexNames.unlock();
    return v;
}

void CV::Context::setName(const std::string &name, int id){
    mutexNames.lock();
    this->names[name] = id;
    mutexNames.unlock();
}

bool CV::Context::removeName(int dataId){
    if(!this->isNamed(id)){
        return false;
    }
    mutexNames.lock();
    for(auto &it : this->names){
        if(it.second == dataId){
            this->names.erase(it.first);
            mutexNames.unlock();
            return true;
        }
    }    
    mutexNames.unlock();
    return false;
}

bool CV::Context::isDataIn(int id){
    this->mutexData.lock();
    int c = this->data.count(id);
    this->mutexData.unlock();
    return c > 0;
}

void CV::Context::clear(const std::shared_ptr<CV::Program> &prog){
    this->mutexNames.lock();
    this->names.clear();
    this->mutexNames.unlock();
    std::vector<int> toRemove;
    this->mutexData.lock();
    for(auto &it : this->data){
        toRemove.push_back(it);
    }
    this->mutexData.unlock();
    for(int i = 0; i < toRemove.size(); ++i){
        this->removeData(prog, toRemove[i]);
    }   
    this->mutexDependants.lock();
    for(auto &it : this->dependants){
        prog->removeContext(it);
    }
    this->dependants.clear();
    this->mutexDependants.unlock();
}

void CV::Context::insertDependant(int ctxId){
    this->mutexDependants.lock();
    this->dependants.insert(ctxId);
    this->mutexDependants.unlock();
}

bool CV::Context::removeDependant(int id){
    this->mutexDependants.lock();
    if(this->dependants.count(id) == 0){
        this->mutexDependants.unlock();
        return false;
    }
    this->dependants.erase(id);
    this->mutexDependants.unlock();
    return true;
}

void CV::Context::registerFunction(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::string> &params,
    const CV::Lambda &lambda
){
    auto fn = new CV::DataFunction();
    prog->allocateData(fn);
    fn->params = params;
    fn->isBinary = true;
    fn->lambda = lambda;
    fn->isVariadic = false;
    this->setData(prog, fn->id);
    this->setName(name, fn->id);
}


void CV::Context::registerFunction(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const CV::Lambda &lambda
){
    auto fn = new CV::DataFunction();
    prog->allocateData(fn);
    fn->isVariadic = true;
    fn->isBinary = true;
    fn->lambda = lambda;
    this->setData(prog, fn->id);
    this->setName(name, fn->id);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  PROGRAM
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CV::Program::placeData(int dataId, int ctxId){
    auto ctx = getContext(ctxId);
    if(ctx == NULL){
        return false;
    }
    return ctx->setData(shared_from_this(), dataId);
}

bool CV::Program::swapId(int fdataId, int tdataId){
    auto fData = this->getData(fdataId);
    if(fData == NULL){
        return false;
    }
    auto tData = this->getData(tdataId);
    if(tData == NULL){
        return false;
    }    
    this->mutexStack.lock();
    tData->id = fdataId;
    fData->id = tdataId;
    this->stack[fdataId] = tData;
    this->stack[tdataId] = fData;
    this->mutexStack.unlock();
    return true;
}

CV::Data *CV::Program::getData(int dataId){
    this->mutexStack.lock();
    if(this->stack.count(dataId) == 0){
        this->mutexStack.unlock();
        return NULL;
    }
    auto v = this->stack[dataId];
    this->mutexStack.unlock();
    return v;
}

bool CV::Program::moveData(int dataId, int fctxId, int tctxId){
    auto fctx = getContext(fctxId);
    if(fctx == NULL){
        return false;
    }
    auto tctx = getContext(tctxId);
    if(tctx == NULL){
        return false;
    }        
    if(!fctx->isDataIn(dataId)){
        return false;
    }
    return tctx->setData(shared_from_this(), dataId) && fctx->removeData(shared_from_this(), dataId);
}

CV::ContextType CV::Program::getContext(int id){
    this->mutexContext.lock();
    auto it = this->contexts.find(id);
    if(it == this->contexts.end()){
        this->mutexContext.unlock();
        return NULL;
    }
    this->mutexContext.unlock();
    return it->second;
}

CV::ContextType CV::Program::createContext(int root){
    auto rctx = getContext(root);

    auto ctx = std::make_shared<CV::Context>();
    if(rctx){
        rctx->insertDependant(ctx->id);
        ctx->top = rctx->id;
    }
    
    this->mutexContext.lock();
    this->contexts[ctx->id] = ctx;
    this->mutexContext.unlock();

    return ctx;
}

bool CV::Program::removeContext(int id){
    std::vector<int> toRemove;
    auto ctx = getContext(id);
    if(ctx == NULL){
        return false;
    }
    ctx->clear(shared_from_this());
    this->mutexContext.lock();
    this->contexts.erase(id);
    this->mutexContext.unlock();
    return true;
}

int CV::Program::allocateData(CV::Data *ref){
    this->mutexStack.lock();
    this->stack[ref->id] = ref;
    this->mutexStack.unlock();
    return ref->id;
}

bool CV::Program::deallocateData(int id){

    // Check if the id is indeed in the stack
    this->mutexStack.lock();
    auto it = this->stack.find(id);
    if(it == this->stack.end()){
        this->mutexStack.unlock();
        return false;
    }
    auto ref = it->second;
    this->mutexStack.unlock();

    // Remove prefetch references
    this->prefetchMutex.lock();
    std::vector<int> prefetchToDelete;
    for(auto &it : this->prefetched){
        if(it.second == ref->id){
            prefetchToDelete.push_back(it.first);
        }
    }
    this->prefetchMutex.unlock();
    for(int i = 0; i < prefetchToDelete.size(); ++i){
        this->removePrefetch(prefetchToDelete[i]);
    }

    // Call data's own clear method
    ref->clear(shared_from_this());
    
    // Erase reference
    this->mutexStack.lock();
    this->stack.erase(id);
    this->mutexStack.unlock();
    
    // Delete its pointer
    delete ref;
    
    return true;
}

CV::Program::Program(){
    this->root = this->createContext();
}

bool CV::Program::removePrefetch(int insId){
    prefetchMutex.lock();
    if(this->prefetched.count(insId) == 0){
        prefetchMutex.unlock();
        return false;
    }
    this->prefetched.erase(insId);
    prefetchMutex.unlock();
    return true;
}

void CV::Program::setPrefetch(int insId, int dataId){
    prefetchMutex.lock();
    this->prefetched[insId] = dataId;
    prefetchMutex.unlock();
}

int CV::Program::getPrefetch(int id){
    prefetchMutex.lock();
    auto vs = this->prefetched[id];
    prefetchMutex.unlock();
    return vs;
}  

bool CV::Program::isPrefetched(int id){
    prefetchMutex.lock();
    auto v = this->prefetched.count(id);
    prefetchMutex.unlock();
    return v > 0;
}

void CV::Program::clearPrefetch(){
    prefetchMutex.lock();
    this->prefetched.clear();
    prefetchMutex.unlock();
}


void CV::Program::end(){
    // Clear root
    if(this->root){
        this->root->clear(shared_from_this());
        this->root = NULL;
    }
    std::vector<int> trailing;
    // Clear other contexts
    this->mutexContext.lock();
    for(auto &it : this->contexts){
        trailing.push_back(it.first);
    }
    this->mutexContext.unlock();
    for(int i = 0; i < trailing.size(); ++i){
        this->removeContext(trailing[i]);
    }
    trailing.clear();
    // Clear stack
    this->mutexStack.lock();
    for(auto &it : this->stack){
        trailing.push_back(it.first);
    }
    this->mutexStack.unlock();
    for(int i = 0; i < trailing.size(); ++i){
        this->deallocateData(trailing[i]);
    }    
    trailing.clear();
    // Quick GC just in case
    this->quickGC();
}

CV::Data* CV::Program::buildNil(){
    auto nil = new CV::Data();
    this->allocateData(nil);
    return nil;
}

void CV::Program::quickGC(){
    std::vector<int> trailing;
    this->mutexStack.lock();
    for(auto &it : this->stack){
        if(it.second->getRefCount() == 0){
            trailing.push_back(it.first);
        }
    }
    this->mutexStack.unlock();
    for(int i = 0; i < trailing.size(); ++i){
        this->deallocateData(trailing[i]);
    }
}

CV::Program::~Program(){
    this->end();
}

CV::InsType CV::Program::createInstruction(unsigned type, const std::shared_ptr<CV::Token> &token){
    auto ins = std::make_shared<CV::Instruction>();
    ins->id = GEN_ID();
    ins->type = type;
    ins->token = token;
    this->mutexIns.lock();
    this->instructions[ins->id] = ins;
    this->mutexIns.unlock();
    return ins;
}

std::shared_ptr<CV::Instruction> &CV::Program::getIns(int id){
    this->mutexIns.lock();
    auto &i = this->instructions[id];
    this->mutexIns.unlock();
    return i;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  API
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


        // std::shared_ptr<CV::Quant> Copy(const std::shared_ptr<CV::Quant> &subject){
        //     std::shared_ptr<CV::Quant> c = subject->copy();
        //     switch(subject->type){
        //         case CV::QuantType::LIST: {
        //             auto orig = std::static_pointer_cast<CV::TypeList>(subject);
        //             auto target = std::static_pointer_cast<CV::TypeList>(c);
        //             for(int i = 0; i < orig->v.size(); ++i){
        //                 target->v.push_back(Copy(orig->v[i]));
        //             }
        //         } break;
        //         case CV::QuantType::STORE: {
        //             auto orig = std::static_pointer_cast<CV::TypeStore>(subject);
        //             auto target = std::static_pointer_cast<CV::TypeStore>(c);
        //             for(auto &it : orig->v){
        //                 target->v[it.first] = Copy(it.second);
        //             }
        //         } break;
        //     }
        //     return c;
        // }



        // std::shared_ptr<CV::TypeThread> Thread(){
        //     auto t = std::make_shared<CV::TypeThread>();
        //     t->threadId = GEN_ID();
        //     return t; 
        // }



        // std::shared_ptr<CV::Quant> Type(int type){
        //     switch(type){
        //         case CV::QuantType::NUMBER: {
        //             return Number();
        //         };
        //         case CV::QuantType::STRING: {
        //             return String();
        //         };
        //         case CV::QuantType::LIST: {
        //             return List();
        //         };
        //         case CV::QuantType::STORE: {
        //             return Store();
        //         };
        //         case CV::QuantType::THREAD: {
        //             return Thread();
        //         };
        //         default:
        //         case CV::QuantType::NIL: {
        //             return Nil();
        //         };
        //     }
        // }



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  CORE
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CV::SetupCore(const CV::ProgramType &prog){
    ////////////////////////////
    //// ARITHMETIC
    ///////////////////////////    
    prog->root->registerFunction(prog, "+",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st             
        ) -> CV::Data* {

            auto result = new CV::DataNumber();
            prog->allocateData(result);

            for(int i = 0; i < args.size(); ++i){
                auto &arg = args[i].second;
                auto v = CV::Execute(arg, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                } 
                if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, token, CV::DataType::NUMBER)){
                    return prog->buildNil();
                }
                result->value += static_cast<CV::DataNumber*>(v)->value;
            }

            return static_cast<CV::Data*>(result);
        }
    );
    ////////////////////////////
    //// LISTS
    ///////////////////////////  
    // prog->root->registerFunction(prog, "nth",
    //     {
    //         "list",
    //         "index"
    //     },
    //     [&](
    //         const std::shared_ptr<CV::Program> &prog,
    //         const std::string &name,
    //         const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
    //         const std::shared_ptr<CV::Token> &token,
    //         const std::shared_ptr<CV::Cursor> &cursor,
    //         const std::shared_ptr<CV::Context> &ctx,
    //         const std::shared_ptr<CV::ControlFlow> &st             
    //     ) -> CV::Data* {

            
    //         auto target = CV::Execute(arg, prog, cursor, ctx, st);
    //         if(cursor->error){
    //             return prog->buildNil();
    //         } 

            
    //         auto result = new CV::DataNumber();
    //         prog->allocateData(result);

    //         for(int i = 0; i < args.size(); ++i){
    //             auto &arg = args[i].second;
    //             auto v = CV::Execute(arg, prog, cursor, ctx, st);
    //             if(cursor->error){
    //                 return prog->buildNil();
    //             } 
    //             if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, token, CV::DataType::NUMBER)){
    //                 return prog->buildNil();
    //             }
    //             result->value += static_cast<CV::DataNumber*>(v)->value;
    //         }

    //         return static_cast<CV::Data*>(result);
    //     }
    // );          
}