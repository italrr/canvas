#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <sys/stat.h>
#include <fstream>

#include "CV.hpp"

// DYNAMIC LIBRARY STUFF
#if defined(CV_PLATFORM_TYPE_LINUX)
    #include <dlfcn.h> 
#elif defined(CV_PLATFORM_TYPE_WINDOWS)
    // should prob move the entire DLL loading routines to its own file
    #include <Libloaderapi.h>
#endif

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
                "let", "import", "import:dynamic-library", "~", ".", "|", "`", "cc", "mut", "fn",
                "return", "yield", "skip", "b:list", "b:store", "await"
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
        
        bool isInList(const std::string &v, const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &list){
            for(int i = 0; i < list.size(); ++i){
                if(list[i].first == v){
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
    }

}

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
//  TYPES
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//
// DATA (NIL)
//
CV::Data::Data(){
    this->type = CV::DataType::NIL;
}

//
// NUMBER
//
CV::DataNumber::DataNumber(){
    this->type = CV::DataType::NUMBER;
}
std::shared_ptr<CV::Data> CV::DataNumber::unwrap(){
    return shared_from_this();
}

//
// STRING
//
CV::DataString::DataString(){
    this->type = CV::DataType::STRING;
}
std::shared_ptr<CV::Data> CV::DataString::unwrap(){
    return shared_from_this();
}

//
// LIST
//
CV::DataList::DataList(){
    this->type = CV::DataType::LIST;
}
std::shared_ptr<CV::Data> CV::DataList::unwrap(){
    return shared_from_this();
}

//
// STORE
//
CV::DataStore::DataStore(){
    this->type = CV::DataType::STORE;
}
std::shared_ptr<CV::Data> CV::DataStore::unwrap(){
    return shared_from_this();
}

//
// FUNCTION
//
CV::DataFunction::DataFunction(){
    this->type = CV::DataType::FUNCTION;
    this->isVariadic = false;
    this->isLambda = false;
}
std::shared_ptr<CV::Data> CV::DataFunction::unwrap(){
    return shared_from_this();
}

//
// PROXY
//
CV::DataProxy::DataProxy(){
    this->type = CV::DataType::PROXY;
    this->ptype = CV::Prefixer::NAMER;
}

std::shared_ptr<CV::Data> CV::DataProxy::unwrap(){
    return this->target ? this->target : shared_from_this();
}

//
// CONTEXT
//
CV::Context::Context(){
    this->head = NULL;
    this->type = CV::DataType::CONTEXT;
}

std::shared_ptr<CV::Context> CV::Context::buildContext(bool inherit){
    auto nctx = std::make_shared<CV::Context>();
    if(inherit){
        nctx->head = shared_from_this(); 
    }
    return nctx;
}

typedef std::pair<std::shared_ptr<CV::Context>, std::shared_ptr<CV::Data>> NamedV;
std::pair<std::shared_ptr<CV::Context>, std::shared_ptr<CV::Data>> CV::Context::getNamed(const std::string &name){
    auto lCount = this->data.count(name);
    return lCount == 0 && head ? head->getNamed(name) : (lCount == 1 ? (NamedV{ shared_from_this(), this->data[name] }) : (NamedV{NULL, NULL}) );
}

std::shared_ptr<CV::Data> CV::Context::buildNil(){
    return std::make_shared<CV::Data>();
}

std::shared_ptr<CV::DataNumber> CV::Context::buildNumber(CV_NUMBER v){
    auto b = std::make_shared<CV::DataNumber>();
    b->v = v;
    return b;
}

std::shared_ptr<CV::DataString> CV::Context::buildString(const std::string &v){
    auto b = std::make_shared<CV::DataString>();
    b->v = v;
    return b;
}

std::shared_ptr<CV::DataList> CV::Context:: buildList(){
    return std::make_shared<CV::DataList>();
}

std::shared_ptr<CV::DataStore> CV::Context::buildStore(){
    return std::make_shared<CV::DataStore>();
}

std::shared_ptr<CV::Data> CV::Context::unwrap(){
    return shared_from_this();
}

void CV::Context::registerFunction(
    const std::string &name,
    const std::vector<std::string> &params,
    const std::function<std::shared_ptr<CV::Data>(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const std::shared_ptr<CV::Context> &ctx,
        const std::shared_ptr<CV::Cursor> &cursor,
        const std::shared_ptr<CV::Token> &token
    )> &lambda
){
    if(!CV::Tools::isValidVarName(name)){
        return;
    }

    if(CV::Tools::isReservedWord(name)){
        return;
    }

    auto fn = std::make_shared<CV::DataFunction>();
    fn->isLambda = true;
    fn->isVariadic = false;
    fn->params = params;
    fn->lambda = lambda;

    this->data[name] = fn;
}

void CV::Context::registerFunction(
    const std::string &name,
    const std::function<std::shared_ptr<CV::Data>(
        const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
        const std::shared_ptr<CV::Context> &ctx,
        const std::shared_ptr<CV::Cursor> &cursor,
        const std::shared_ptr<CV::Token> &token
    )> &lambda
){
    if(!CV::Tools::isValidVarName(name)){
        return;
    }

    if(CV::Tools::isReservedWord(name)){
        return;
    }

    auto fn = std::make_shared<CV::DataFunction>();
    fn->isLambda = true;
    fn->isVariadic = true;
    fn->lambda = lambda;

    this->data[name] = fn;
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

static std::vector<CV::TokenType> ParseTokens(std::string input, char sep, const std::shared_ptr<CV::Cursor> &cursor, int startLine){
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
            onComment = true;
            continue;
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
                    tokens.push_back(std::make_shared<CV::Token>(buffer, cline));
                }
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            if(buffer != ""){
                tokens.push_back(std::make_shared<CV::Token>(buffer+c, cline));
            }
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
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

static std::vector<CV::TokenType> RebuildTokenHierarchy(const std::vector<CV::TokenType> &input, char sep, const CV::CursorType &cursor){
    
    if(input.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, 1);
        return {};
    }

    std::function<CV::TokenType(const CV::TokenType &token)> unwrap = [&](const CV::TokenType &token){
        auto innerTokens = ParseTokens(token->first, sep, cursor, token->line);
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

std::vector<CV::TokenType> CV::BuildTree(
    const std::string &input,
    const CV::CursorType &cursor
){
    if(input.size() == 0){
        cursor->setError("Syntax Error", "Empty input", 1);
        return {};
    }

    auto isBracketedToken = [](const CV::TokenType &token) -> bool {
        if(!token){
            return false;
        }
        auto s = CV::Tools::cleanTokenInput(token->first);
        return s.size() >= 2 && s.front() == '[' && s.back() == ']';
    };

    auto isNamerAssignmentToken = [](const CV::TokenType &token) -> bool {
        if(!token){
            return false;
        }
        return token->first.size() >= 2 &&
               token->first[0] == '~' &&
               token->inner.size() == 1;
    };

    auto needsProgramWrap = [&](const std::string &src) -> bool {
        auto top = ParseTokens(src, ' ', cursor, 1);
        if(cursor->error){
            return false;
        }

        if(top.empty()){
            return false;
        }

        bool allBracketed = true;
        for(const auto &t : top){
            if(!isBracketedToken(t)){
                allBracketed = false;
                break;
            }
        }

        if(!allBracketed){
            return true;
        }

        std::vector<CV::TokenType> rebuilt;
        for(int i = 0; i < static_cast<int>(top.size()); ++i){
            auto built = RebuildTokenHierarchy({top[i]}, ' ', cursor);
            if(cursor->error){
                return false;
            }
            for(int j = 0; j < static_cast<int>(built.size()); ++j){
                rebuilt.push_back(built[j]);
            }
        }

        bool allNamerAssignments = !rebuilt.empty();
        for(const auto &t : rebuilt){
            if(!isNamerAssignmentToken(t)){
                allNamerAssignments = false;
                break;
            }
        }

        if(allNamerAssignments){
            return true;
        }

        return false;
    };

    auto cleanInput = CV::Tools::cleanTokenInput(input);
    if(cleanInput.empty()){
        cursor->setError("Syntax Error", "Empty input", 1);
        return {};
    }

    std::string fixedInput = input;

    if(cleanInput.front() != '[' || cleanInput.back() != ']'){
        fixedInput = "[" + fixedInput;
        if(!fixedInput.empty() && fixedInput.back() == '\n'){
            fixedInput.back() = ']';
        }else{
            fixedInput += "]";
        }
    }

    cleanInput = CV::Tools::cleanTokenInput(fixedInput);
    if(cleanInput.empty()){
        cursor->setError("Syntax Error", "Empty input", 1);
        return {};
    }

    if(needsProgramWrap(cleanInput)){
        fixedInput = "[" + fixedInput + "]";
    }

    auto tokens = ParseTokens(fixedInput, ' ', cursor, 1);
    if(cursor->error){
        return {};
    }

    std::vector<CV::TokenType> root;

    for(int i = 0; i < static_cast<int>(tokens.size()); ++i){
        auto built = RebuildTokenHierarchy({tokens[i]}, ' ', cursor);
        if(cursor->error){
            return {};
        }

        for(int j = 0; j < static_cast<int>(built.size()); ++j){
            root.push_back(built[j]);
        }
    }
    if(root.size() > 1){
        bool allTopLevelNamerAssignments = true;

        for(const auto &t : root){
            if(!isNamerAssignmentToken(t)){
                allTopLevelNamerAssignments = false;
                break;
            }
        }

        if(allTopLevelNamerAssignments){
            auto grouped = std::make_shared<CV::Token>("", root[0]->line);
            grouped->first = "";
            grouped->inner = root;
            grouped->solved = true;
            grouped->refresh();

            root.clear();
            root.push_back(grouped);
        }
    }    

    if(root.empty()){
        return {};
    }

    return root;
}

static std::shared_ptr<CV::Data> __cv_run_children(
    const CV::TokenType &parent,
    int from,
    const CV::CursorType &cursor,
    const CV::ControlFlowType &cf,
    const CV::ContextType &ctx
){
    auto result = ctx->buildNil();

    for(int i = from; i < static_cast<int>(parent->inner.size()); ++i){
        result = CV::Interpret(parent->inner[i], cursor, cf, ctx);
        if(cursor->error){
            return ctx->buildNil();
        }

        if(cf->state == CV::ControlFlowState::RETURN ||
           cf->state == CV::ControlFlowState::YIELD ||
           cf->state == CV::ControlFlowState::SKIP){
            return result;
        }
    }

    return result;
}

static bool __cv_get_boolean_value(const std::shared_ptr<CV::Data> &target){
    auto v = target ? target->unwrap() : std::shared_ptr<CV::Data>(nullptr);
    if(!v){
        return false;
    }

    switch(v->type){
        case CV::DataType::NIL:
            return false;
        case CV::DataType::NUMBER:
            return std::static_pointer_cast<CV::DataNumber>(v)->v != 0;
        case CV::DataType::STRING:
            return !std::static_pointer_cast<CV::DataString>(v)->v.empty();
        case CV::DataType::LIST:
            return !std::static_pointer_cast<CV::DataList>(v)->v.empty();
        case CV::DataType::STORE:
            return !std::static_pointer_cast<CV::DataStore>(v)->v.empty();
        default:
            return true;
    }
}

static bool __cv_is_iterator_clause(const CV::TokenType &token){
    return token &&
           token->first.size() >= 2 &&
           token->first[0] == '~' &&
           token->inner.size() == 1;
}

#if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX) || (_CV_PLATFORM == _CV_PLATFORM_TYPE_OSX)
    using __cv_dynlib_handle_t = void*;
#elif (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
    using __cv_dynlib_handle_t = HMODULE;
#endif

static std::mutex __cv_loaded_dynamic_libs_mutex;
static std::unordered_map<int, std::pair<__cv_dynlib_handle_t, std::string>> __cv_loaded_dynamic_libs;

static std::string __cv_resolve_import_path(
    const std::string &fname,
    const std::string &defaultExt
){
    std::string resolved = fname;

    if(CV::Tools::fileExtension(resolved) == ""){
        resolved += defaultExt;
    }

    if(CV::Tools::fileExists("./" + resolved)){
        resolved = "./" + resolved;
    }else{
        resolved = CV_LIB_HOME + "/" + resolved;
    }

    return resolved;
}

std::shared_ptr<CV::Data> CV::Interpret(
    const CV::TokenType &token,
    const CV::CursorType &cursor,
    const CV::ControlFlowType &cf,
    const CV::ContextType &ctx
){
    if(token->first.size() == 0 && token->inner.size() == 0){
        cursor->setError("NOOP", CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS, token);
        return ctx->buildNil();
    }

    auto bListConstruct = [cursor, cf](const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto list = ctx->buildList();

        for(int i = 0; i < static_cast<int>(tokens.size()); ++i){
            auto &inc = tokens[i];

            auto data = Interpret(inc, cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = origin;
                return ctx->buildNil();
            }

            if(cf->state == CV::ControlFlowState::YIELD){
                return data;
            }

            if(cf->state == CV::ControlFlowState::RETURN ||
            cf->state == CV::ControlFlowState::SKIP){
                return data;
            }

            if(data && data->type == CV::DataType::PROXY){
                auto proxy = std::static_pointer_cast<CV::DataProxy>(data);

                if(proxy->ptype == CV::Prefixer::EXPANDER){
                    if(!proxy->target){
                        cursor->setError(
                            CV_ERROR_MSG_MISUSED_PREFIX,
                            "Expander Prefix '^' produced a proxy without target",
                            inc
                        );
                        cursor->subject = origin;
                        return ctx->buildNil();
                    }

                    auto expanded = proxy->target->unwrap();
                    if(!expanded || expanded->type != CV::DataType::LIST){
                        cursor->setError(
                            CV_ERROR_MSG_MISUSED_PREFIX,
                            "Expander Prefix '^' expects target to resolve into a LIST",
                            inc
                        );
                        cursor->subject = origin;
                        return ctx->buildNil();
                    }

                    auto expandedList = std::static_pointer_cast<CV::DataList>(expanded);

                    for(int j = 0; j < static_cast<int>(expandedList->v.size()); ++j){
                        list->v.push_back(expandedList->v[j]);
                    }

                    continue;
                }
            }

            list->v.push_back(data ? data->unwrap() : ctx->buildNil());
        }

        return std::static_pointer_cast<CV::Data>(list);
    };


    auto bStoreConstruct = [cursor, cf](const std::string &name, const CV::TokenType &origin, std::vector<CV::TokenType> &tokens, const CV::ContextType &ctx){
        auto store = ctx->buildStore();
        for(int i = 0; i < tokens.size(); ++i){
            auto &inc = tokens[i];
            auto fetched = Interpret(inc, cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = origin;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return fetched;
            }
            if(fetched->type != CV::DataType::PROXY){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' may only be able to construct named types using NAMER prefixer", origin);
                return ctx->buildNil();
            }
            auto proxy = std::static_pointer_cast<CV::DataProxy>(fetched);
            // We eat up the instruction itself as we don't really execute it for this situation
            if(!proxy->target){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' expects Namer prefixed tokens to have an appended body so it defines a value", origin);
                return ctx->buildNil();
            }
            
            auto vname = proxy->pname;
            if(!CV::Tools::isValidVarName(vname)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' is attempting to construct store with invalidly named type '"+vname+"'", origin);
                return ctx->buildNil();
            }
            if(CV::Tools::isReservedWord(vname)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+name+"' is attempting to construct store with reserved name named type '"+vname+"'", origin);
                return ctx->buildNil();               
            }
            store->v[vname] = proxy->target;
        }
        return std::static_pointer_cast<CV::Data>(store);
    };    

    auto bListConstructFromToken = [bListConstruct, cursor](const CV::TokenType &origin, const CV::ContextType &ctx){
        auto cpy = origin->emptyCopy();
        std::vector<CV::TokenType> inners { cpy };
        for(int i = 0; i < origin->inner.size(); ++i){
            inners.push_back(origin->inner[i]);
        }
        return bListConstruct(origin, inners, ctx);
    };

    auto isNameFunction = [](const CV::TokenType &token, const CV::ContextType &ctx){
        if(!token->solved){
            return false;
        }
        if(CV::Tools::isReservedWord(token->first)){
            return true;
        }
        auto dataRef = ctx->getNamed(token->first);
        if(!dataRef.first || !dataRef.second){
            return false;
        }
        auto data = dataRef.second;
        if(data->type == CV::DataType::PROXY){
            data = std::static_pointer_cast<CV::DataProxy>(data)->target;
        }
        return data && (data->type == CV::DataType::FUNCTION ||
                data->type == CV::DataType::STORE);
    };
    
    auto areAllNames = [](const CV::TokenType &token, const CV::ContextType &ctx){
        (void)ctx;

        if(token->inner.size() < 1){
            return false;
        }

        for(int i = 0; i < token->inner.size(); ++i){
            auto &child = token->inner[i];

            if(child->first.size() < 2 || child->first[0] != '~'){
                return false;
            }

            if(child->inner.size() == 0){
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
            return Interpret(token, cursor, cf, ctx);
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
                return ctx->buildNil();
            }
            
            int type = CV::ControlFlowState::CONTINUE;
            if(token->first == "skip"){
                type = CV::ControlFlowState::SKIP;
            }else
            if(token->first == "yield"){
                type = CV::ControlFlowState::YIELD;
            }else
            if(token->first == "return"){
                type = CV::ControlFlowState::RETURN;
            }

            auto r = ctx->buildNil();
            cf->state = type;

            if(token->inner.size() > 0){
                r = Interpret(token->inner[0], cursor, cf, ctx);
                if(cursor->error){
                    cursor->subject = token;
                    return ctx->buildNil();
                }
            }

            return r;
        }else   
        /*
            FN
        */
        if(token->first == "fn"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" [arguments][code])", token);
                return ctx->buildNil();
            }
            auto fn = std::make_shared<CV::DataFunction>();
            fn->isLambda = false;
            fn->body = token->inner[1];

            auto paramNameList = token->inner[0]->inner;
            paramNameList.insert(paramNameList.begin(), token->inner[0]);

            auto hasParam = [&](const std::string &name){
                for(const auto &p : fn->params){
                    if(p == name){
                        return true;
                    }
                }
                return false;
            };

            if(paramNameList.size() == 1 && paramNameList[0]->first == "@"){
                fn->isVariadic = true;
            }else{
                for(int i = 0; i < paramNameList.size(); ++i){
                    auto &name = paramNameList[i]->first;
                    if(!CV::Tools::isValidVarName(name)){
                        cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' argument name '"+name+"' is an invalid", token);
                        return ctx->buildNil();                 
                    }
                    if(CV::Tools::isReservedWord(name)){
                        cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' argument name '"+name+"' is a name of native constructor which cannot be overriden", token);
                        return ctx->buildNil();
                    }  
                    if(hasParam(name)){
                        cursor->setError(
                            CV_ERROR_MSG_MISUSED_IMPERATIVE,
                            "'"+token->first+"' argument name '"+name+"' is duplicated",
                            token
                        );
                        return ctx->buildNil();
                    }                    
                    if(name == "@"){
                        cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' argument name '"+name+"' is illegal in definitions with more than one argument", token);
                        return ctx->buildNil();                 
                    }
                    fn->params.push_back(name);
                }
            }

            return fn;
            
        }else  
        /*
            CC
        */
        if(token->first == "cc"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" VALUE)",
                    token
                );  
                return ctx->buildNil();
            }

            auto target = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return target;
            }            

            return ctx->copy(target->unwrap());
        }else
        /*
            MUT
        */
        if(token->first == "mut"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" NAME VALUE)", token);
                return ctx->buildNil();
            }
            
            auto nameRef = ctx->getNamed(token->inner[0]->first);
            if(!nameRef.first || !nameRef.second){
                cursor->setError(CV_ERROR_MSG_UNDEFINED_IMPERATIVE, "Name '"+token->first+"'", token);
                return ctx->buildNil();
            }
            auto subject = nameRef.second->unwrap();

            auto target = Interpret(token->inner[1], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            } 
            if(cf->state == CV::ControlFlowState::YIELD){
                return target;
            }     
            target = target->unwrap();

            if(subject->type != CV::DataType::NUMBER && subject->type != CV::DataType::STRING){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' may only accept NUMBER or STRING types", token);
                return ctx->buildNil();
            }

            if(target->type != subject->type){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' may only mutate same types", token);
                return ctx->buildNil();
            }

            switch(subject->type){
                case CV::DataType::STRING: {
                    std::static_pointer_cast<CV::DataString>(subject)->v = std::static_pointer_cast<CV::DataString>(target)->v;
                };
                case CV::DataType::NUMBER: {
                    std::static_pointer_cast<CV::DataNumber>(subject)->v = std::static_pointer_cast<CV::DataNumber>(target)->v;
                };                
            }

            return subject;
        }else  
        /*
            IMPORT
        */
        if(token->first == "import"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)",
                    token
                );
                return ctx->buildNil();
            }

            auto fnamev = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return fnamev;
            }

            fnamev = fnamev ? fnamev->unwrap() : nullptr;

            if(!fnamev || fnamev->type != CV::DataType::STRING){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "'"+token->first+"' expects a STRING operand",
                    token
                );
                return ctx->buildNil();
            }

            auto fname = std::static_pointer_cast<CV::DataString>(fnamev)->v;
            auto resolved = __cv_resolve_import_path(fname, ".cv");

            if(!CV::Tools::fileExists(resolved)){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "No canvas library '"+resolved+"' was found",
                    token
                );
                return ctx->buildNil();
            }

            auto result = CV::Import(resolved, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }

            return result ? result : ctx->buildNil();
        }else
        /*
            IMPORT:DYNAMIC-LIBRARY
        */
        if(token->first == "import:dynamic-library"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)",
                    token
                );
                return ctx->buildNil();
            }

            auto fnamev = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return fnamev;
            }

            fnamev = fnamev ? fnamev->unwrap() : nullptr;

            if(!fnamev || fnamev->type != CV::DataType::STRING){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "'"+token->first+"' expects a STRING operand",
                    token
                );
                return ctx->buildNil();
            }

            auto fname = std::static_pointer_cast<CV::DataString>(fnamev)->v;

            std::string ext = ".so";
            if(CV::PLATFORM == CV::SupportedPlatform::WINDOWS){
                ext = ".dll";
            }else
            if(CV::PLATFORM == CV::SupportedPlatform::OSX){
                ext = ".dylib";
            }

            auto resolved = __cv_resolve_import_path(fname, ext);

            if(!CV::Tools::fileExists(resolved)){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "No dynamic library '"+resolved+"' was found",
                    token
                );
                return ctx->buildNil();
            }

            auto result = CV::ImportDynamicLibrary(resolved, fname, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }

            return result ? result : ctx->buildNil();
        }else                
        /*
            LET
        */
        if(token->first == "let"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" NAME VALUE)", token);
                return ctx->buildNil();
            }

            auto &nameToken = token->inner[0];
            auto &insToken = token->inner[1];

            if(nameToken->inner.size() != 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' expects NAME as second operand", token);
                return ctx->buildNil();             
            }
            auto &name = nameToken->first;
            if(!CV::Tools::isValidVarName(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' second operand '"+name+"' is an invalid name", token);
                return ctx->buildNil();
            }
            if(CV::Tools::isReservedWord(name)){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' second operand '"+name+"' is a name of native constructor which cannot be overriden", token);
                return ctx->buildNil();
            }            

            auto nameRef = ctx->getNamed(name);
            if(nameRef.first && nameRef.second && ctx->namedNames.count(name) == 0){
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "Name '"+name+"' is already defined", token);
                return ctx->buildNil();
            }

            auto target = Interpret(insToken, cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return target;
            }                             

            ctx->data[name] = target;
            if(ctx->namedNames.count(name) == 1){
                ctx->namedNames.erase(name);
            }

            return target;

        }else   
        /*
            IF
        */
        if(token->first == "if"){
            if(token->inner.size() != 2 && token->inner.size() != 3){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects 3 or 4 tokens ("+token->first+" CONDITION TRUE-BRANCH [FALSE-BRANCH])",
                    token
                );
                return ctx->buildNil();
            }

            auto condition = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }

            if(cf->state == CV::ControlFlowState::YIELD){
                return condition;
            }

            if(cf->state == CV::ControlFlowState::RETURN ||
               cf->state == CV::ControlFlowState::SKIP){
                return condition;
            }

            if(__cv_get_boolean_value(condition)){
                return Interpret(token->inner[1], cursor, cf, ctx);
            }

            if(token->inner.size() == 3){
                return Interpret(token->inner[2], cursor, cf, ctx);
            }

            return ctx->buildNil();
        }else            
        /*
            WHILE
        */
        if(token->first == "while"){
            if(token->inner.size() < 2){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects at least 3 tokens ("+token->first+" CONDITION BODY...)",
                    token
                );
                return ctx->buildNil();
            }

            auto result = ctx->buildNil();

            while(true){
                auto iterCtx = ctx->buildContext(true);

                auto condition = Interpret(token->inner[0], cursor, cf, iterCtx);
                if(cursor->error){
                    cursor->subject = token;
                    return ctx->buildNil();
                }

                if(cf->state == CV::ControlFlowState::RETURN){
                    return condition;
                }

                if(cf->state == CV::ControlFlowState::YIELD){
                    return condition;
                }

                if(cf->state == CV::ControlFlowState::SKIP){
                    cf->state = CV::ControlFlowState::CONTINUE;
                    continue;
                }

                if(!__cv_get_boolean_value(condition)){
                    break;
                }

                result = __cv_run_children(token, 1, cursor, cf, iterCtx);
                if(cursor->error){
                    cursor->subject = token;
                    return ctx->buildNil();
                }

                if(cf->state == CV::ControlFlowState::RETURN){
                    return result;
                }

                if(cf->state == CV::ControlFlowState::YIELD){
                    return result;
                }

                if(cf->state == CV::ControlFlowState::SKIP){
                    cf->state = CV::ControlFlowState::CONTINUE;
                    continue;
                }
            }

            return result;
        }else
        /*
            FOR
        */
        if(token->first == "for"){
            if(token->inner.size() < 2){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects at least 3 tokens ("+token->first+" [~name [from to [step]]] BODY...)",
                    token
                );
                return ctx->buildNil();
            }

            auto clauseRaw = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return clauseRaw;
            }

            if(!clauseRaw || clauseRaw->type != CV::DataType::PROXY){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' expects first operand to evaluate into a named proxy like [~x [0 10]]",
                    token
                );
                return ctx->buildNil();
            }

            auto clauseProxy = std::static_pointer_cast<CV::DataProxy>(clauseRaw);

            if(clauseProxy->pname.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator is missing a name",
                    token
                );
                return ctx->buildNil();
            }

            if(!clauseProxy->target){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator proxy is missing a range target",
                    token
                );
                return ctx->buildNil();
            }

            auto iterName = clauseProxy->pname;
            auto rangeData = clauseProxy->target->unwrap();

            if(!rangeData || rangeData->type != CV::DataType::LIST){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator target must be a LIST like [from to] or [from to step]",
                    token
                );
                return ctx->buildNil();
            }

            auto range = std::static_pointer_cast<CV::DataList>(rangeData);

            if(range->v.size() != 2 && range->v.size() != 3){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator target expects [from to] or [from to step]",
                    token
                );
                return ctx->buildNil();
            }

            auto fromData = range->v[0] ? range->v[0]->unwrap() : nullptr;
            auto toData   = range->v[1] ? range->v[1]->unwrap() : nullptr;

            if(!fromData || fromData->type != CV::DataType::NUMBER ||
            !toData   || toData->type   != CV::DataType::NUMBER){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' bounds must be NUMBER values",
                    token
                );
                return ctx->buildNil();
            }

            CV_NUMBER current = std::static_pointer_cast<CV::DataNumber>(fromData)->v;
            CV_NUMBER end     = std::static_pointer_cast<CV::DataNumber>(toData)->v;
            CV_NUMBER step    = current <= end ? 1 : -1;

            if(range->v.size() == 3){
                auto stepData = range->v[2] ? range->v[2]->unwrap() : nullptr;
                if(!stepData || stepData->type != CV::DataType::NUMBER){
                    cursor->setError(
                        CV_ERROR_MSG_ILLEGAL_ITERATOR,
                        "'"+token->first+"' step must be NUMBER",
                        token
                    );
                    return ctx->buildNil();
                }
                step = std::static_pointer_cast<CV::DataNumber>(stepData)->v;
            }

            if(step == 0){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' step cannot be zero",
                    token
                );
                return ctx->buildNil();
            }

            auto shouldRun = [&](CV_NUMBER n) -> bool {
                return step > 0 ? (n < end) : (n > end);
            };

            auto loopCtx = ctx->buildContext(true);
            auto iterValue = loopCtx->buildNumber(current);
            loopCtx->data[iterName] = iterValue;

            auto result = ctx->buildNil();

            while(shouldRun(iterValue->v)){
                auto iterCtx = loopCtx->buildContext(true);

                for(int i = 1; i < static_cast<int>(token->inner.size()); ++i){
                    result = Interpret(token->inner[i], cursor, cf, iterCtx);
                    if(cursor->error){
                        cursor->subject = token;
                        return ctx->buildNil();
                    }

                    if(cf->state == CV::ControlFlowState::RETURN){
                        return result;
                    }

                    if(cf->state == CV::ControlFlowState::YIELD){
                        return result;
                    }

                    if(cf->state == CV::ControlFlowState::SKIP){
                        cf->state = CV::ControlFlowState::CONTINUE;
                        break;
                    }
                }

                iterValue->v += step;
            }

            return result;
        }else
        /*
            FOREACH
        */
        if(token->first == "foreach"){
            if(token->inner.size() < 2){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects at least 3 tokens ("+token->first+" [~item SUBJECT] BODY...)",
                    token
                );
                return ctx->buildNil();
            }

            auto clauseRaw = Interpret(token->inner[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state == CV::ControlFlowState::YIELD){
                return clauseRaw;
            }

            if(!clauseRaw || clauseRaw->type != CV::DataType::PROXY){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' expects first operand to evaluate into a named proxy like [~item someList]",
                    token
                );
                return ctx->buildNil();
            }

            auto clauseProxy = std::static_pointer_cast<CV::DataProxy>(clauseRaw);

            if(clauseProxy->pname.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator is missing a name",
                    token
                );
                return ctx->buildNil();
            }

            if(!clauseProxy->target){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator proxy is missing a subject target",
                    token
                );
                return ctx->buildNil();
            }

            auto iterName = clauseProxy->pname;
            auto subject = clauseProxy->target->unwrap();

            std::vector<std::shared_ptr<CV::Data>> values;

            if(!subject){
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' iterator target is null",
                    token
                );
                return ctx->buildNil();
            }

            if(subject->type == CV::DataType::LIST){
                auto list = std::static_pointer_cast<CV::DataList>(subject);
                values = list->v;
            }else
            if(subject->type == CV::DataType::STORE){
                auto store = std::static_pointer_cast<CV::DataStore>(subject);
                for(auto &it : store->v){
                    values.push_back(it.second);
                }
            }else{
                cursor->setError(
                    CV_ERROR_MSG_ILLEGAL_ITERATOR,
                    "'"+token->first+"' expects subject to be LIST or STORE",
                    token
                );
                return ctx->buildNil();
            }

            auto loopCtx = ctx->buildContext(true);
            auto result = ctx->buildNil();

            for(int i = 0; i < static_cast<int>(values.size()); ++i){
                loopCtx->data[iterName] = values[i];

                auto iterCtx = loopCtx->buildContext(true);

                for(int j = 1; j < static_cast<int>(token->inner.size()); ++j){
                    result = Interpret(token->inner[j], cursor, cf, iterCtx);
                    if(cursor->error){
                        cursor->subject = token;
                        return ctx->buildNil();
                    }

                    if(cf->state == CV::ControlFlowState::RETURN){
                        return result;
                    }

                    if(cf->state == CV::ControlFlowState::YIELD){
                        return result;
                    }

                    if(cf->state == CV::ControlFlowState::SKIP){
                        cf->state = CV::ControlFlowState::CONTINUE;
                        break;
                    }
                }
            }

            return result;
        }else
        // TRY / IGNORE-ERROR PREFIXER
        if(token->first[0] == '?'){
            if(token->inner.size() != 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Try Prefix '"+token->first+"' expects no values",
                    token
                );
                return ctx->buildNil();
            }

            if(token->first.size() == 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Try Prefix '"+token->first+"' expects an appended body",
                    token
                );
                return ctx->buildNil();
            }

            std::string statement = std::string(token->first.begin() + 1, token->first.end());

            // Shadow cursor so failures do not touch the outer/global cursor
            auto shadowCursor = std::make_shared<CV::Cursor>();

            auto root = CV::BuildTree(statement, shadowCursor);
            if(shadowCursor->error){
                return ctx->buildNil();
            }

            if(root.size() == 0){
                return ctx->buildNil();
            }

            auto previousState = cf->state;
            auto previousPayload = cf->payload;

            std::shared_ptr<CV::Data> result = ctx->buildNil();

            for(int i = 0; i < static_cast<int>(root.size()); ++i){
                result = CV::Interpret(root[i], shadowCursor, cf, ctx);

                if(shadowCursor->error){
                    // Swallow the error and restore control-flow
                    cf->state = previousState;
                    cf->payload = previousPayload;
                    return ctx->buildNil();
                }

                if(cf->state == CV::ControlFlowState::YIELD ||
                   cf->state == CV::ControlFlowState::RETURN ||
                   cf->state == CV::ControlFlowState::SKIP){
                    return result;
                }
            }

            return result ? result : ctx->buildNil();
        }else        
        // TEMPLATE FORMATTER
        if(token->first[0] == '%'){
            auto templateCtx = ctx->buildContext(true);
            std::string output = "";

            auto resolveTemplateName = [&](const std::string &name) -> std::string {
                auto nameRef = templateCtx->getNamed(name);
                if(nameRef.first && nameRef.second){
                    auto value = nameRef.second->unwrap();
                    if(!value){
                        return "nil";
                    }

                    if(value->type == CV::DataType::STRING){
                        return std::static_pointer_cast<CV::DataString>(value)->v;
                    }

                    return CV::DataToText(value);
                }

                // unresolved placeholders are left as-is
                return "{" + name + "}";
            };

            auto expandTemplateString = [&](const std::string &src) -> std::string {
                std::string out;
                std::size_t i = 0;

                while(i < src.size()){
                    if(src[i] == '{'){
                        auto end = src.find('}', i + 1);
                        if(end != std::string::npos){
                            auto key = src.substr(i + 1, end - i - 1);

                            if(!key.empty()){
                                out += resolveTemplateName(key);
                                i = end + 1;
                                continue;
                            }
                        }
                    }

                    out += src[i];
                    ++i;
                }

                return out;
            };

            for(int i = 0; i < static_cast<int>(token->inner.size()); ++i){
                auto &child = token->inner[i];

                auto data = Interpret(child, cursor, cf, templateCtx);
                if(cursor->error){
                    cursor->subject = token;
                    return ctx->buildNil();
                }

                if(cf->state == CV::ControlFlowState::YIELD){
                    return data;
                }

                if(cf->state == CV::ControlFlowState::RETURN ||
                   cf->state == CV::ControlFlowState::SKIP){
                    return data;
                }

                if(!data){
                    continue;
                }

                // Named values define template-local names
                if(data->type == CV::DataType::PROXY){
                    auto proxy = std::static_pointer_cast<CV::DataProxy>(data);

                    if(proxy->ptype == CV::Prefixer::NAMER && !proxy->pname.empty()){
                        if(proxy->target){
                            templateCtx->data[proxy->pname] = proxy->target->unwrap();
                        }
                    }

                    // proxies themselves do not directly contribute to output
                    continue;
                }

                auto out = data->unwrap();
                if(!out){
                    continue;
                }

                // Only strings are appended directly
                if(out->type == CV::DataType::STRING){
                    auto raw = std::static_pointer_cast<CV::DataString>(out)->v;
                    output += expandTemplateString(raw);
                }
            }

            return std::static_pointer_cast<CV::Data>(
                ctx->buildString(output)
            );
        }else        
        // EXPANDER
        if(token->first[0] == '^'){
            if(token->inner.size() != 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Expander Prefix '"+token->first+"' expects no values",
                    token
                );
                return ctx->buildNil();
            }

            if(token->first.size() == 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Expander Prefix '"+token->first+"' expects an appended body",
                    token
                );
                return ctx->buildNil();
            }

            std::string statement = std::string(token->first.begin() + 1, token->first.end());

            auto root = CV::BuildTree(statement, cursor);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }

            if(root.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Expander Prefix '"+token->first+"' apppended body must a singular LIST",
                    token
                );
                return ctx->buildNil();                
            }

            auto result = CV::Interpret(root[0], cursor, cf, ctx);
            if(cursor->error){
                cursor->subject = token;
                return ctx->buildNil();
            }
            if(cf->state != CV::ControlFlowState::CONTINUE){
                return result;
            }
            result = result->unwrap();
            if(result->type != CV::DataType::LIST){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Expander Prefix '"+token->first+"' apppended body must a singular LIST",
                    token
                );
                return ctx->buildNil();                   
            }

            auto proxy = std::make_shared<CV::DataProxy>();
            proxy->ptype = CV::Prefixer::EXPANDER;
            proxy->target = result;

            return std::static_pointer_cast<CV::Data>(proxy);
        }else        
        /*
            NAMER
        */
        if(token->first[0] == '~'){
            auto name = std::string(token->first.begin() + 1, token->first.end());

            // Test if the first field (name) is a complex token
            auto testSplit = ParseTokens(name, ' ', cursor, token->line);
            if(cursor->error){
                return ctx->buildNil();
            }

            if(testSplit.size() > 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Namer Prefix '"+token->first+"' cannot take any complex token",
                    token
                );
                return ctx->buildNil();
            }

            // Is it a valid name?
            if(!CV::Tools::isValidVarName(name)){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "'"+token->first+"' prefixer '"+name+"' is an invalid name",
                    token
                );
                return ctx->buildNil();
            }

            // Is it a reserved word?
            if(CV::Tools::isReservedWord(name)){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "'"+token->first+"' prefixer '"+name+"' is a name of native constructor which cannot be overriden",
                    token
                );
                return ctx->buildNil();
            }

            // Only expects a single value
            if(token->inner.size() > 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Namer Prefix '"+token->first+"' no more than 1 value. Provided "+std::to_string(token->inner.size()),
                    token
                );
                return ctx->buildNil();
            }

            auto proxy = std::make_shared<CV::DataProxy>();
            proxy->ptype = CV::Prefixer::NAMER;
            proxy->pname = name;


            // Evaluate referred value if present
            if(token->inner.size() == 1){
                auto target = Interpret(token->inner[0], cursor, cf, ctx);
                if(cursor->error){
                    cursor->subject = token;
                    return ctx->buildNil();
                }

                if(cf->state != CV::ControlFlowState::CONTINUE){
                    return target;
                }

                proxy->target = target;
            }

            // Name into context if the name is not used
            if(proxy->target){
                auto exists = ctx->getNamed(name);
                if(!exists.first && !exists.second){
                    ctx->data[name] = proxy->target;
                    ctx->namedNames.insert(name);
                }
            }

            return std::static_pointer_cast<CV::Data>(proxy);
        }else                                              
        /*
            NIL
        */
        if(token->first == "nil"){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                return ctx->buildNil();
            } 
        }else
        /*
            NUMBER
        */
        if(CV::Tools::isNumber(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{
                return ctx->buildNumber(std::stod(token->first));
            }
        }else
        /*
            STRING
        */
        if(CV::Tools::isString(token->first)){
            if(token->inner.size() > 0){
                return bListConstructFromToken(token, ctx);
            }else{            
                return ctx->buildString(token->first.substr(1, token->first.length() - 2));
            }
        }else{
            /*
                IS NAME?        
            */   
            auto hctx = ctx;
            std::string qname = token->first;
            auto nameRef = ctx->getNamed(qname);
            if(nameRef.first && nameRef.second){
                auto &data = nameRef.second;

                switch(data->type){
                    case CV::DataType::FUNCTION: {
                        auto fn = std::static_pointer_cast<CV::DataFunction>(data);

                        auto fnCtx = ctx->buildContext(true);

                        std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> allParams;


                        std::vector<std::string> usedNames;
                        int positionalCursor = 0;

                        auto nextPositionalName = [&](int outerIndex, int innerIndex = -1) -> std::string {
                            if(!fn->isVariadic){
                                while(
                                    positionalCursor < static_cast<int>(fn->params.size()) &&
                                    CV::Tools::isInList(fn->params[positionalCursor], usedNames)
                                ){
                                    ++positionalCursor;
                                }

                                if(positionalCursor < static_cast<int>(fn->params.size())){
                                    auto picked = fn->params[positionalCursor++];
                                    usedNames.push_back(picked);
                                    return picked;
                                }
                            }

                            std::string fallback =
                                innerIndex >= 0
                                    ? CV::Tools::format("arg-%i-%i", outerIndex, innerIndex)
                                    : CV::Tools::format("arg-%i", outerIndex);

                            std::string picked = fallback;
                            int suffix = 1;
                            while(CV::Tools::isInList(picked, usedNames)){
                                picked = fallback + "-" + std::to_string(suffix++);
                            }

                            usedNames.push_back(picked);
                            return picked;
                        };

                        auto inferParamName = [&](const std::shared_ptr<CV::Data> &raw, int outerIndex, int innerIndex = -1) -> std::string {
                            if(raw && raw->type == CV::DataType::PROXY){
                                auto proxy = std::static_pointer_cast<CV::DataProxy>(raw);

                                if(proxy->ptype != CV::Prefixer::EXPANDER && !proxy->pname.empty()){
                                    std::string picked = proxy->pname;

                                    if(!CV::Tools::isInList(picked, usedNames)){
                                        usedNames.push_back(picked);
                                        return picked;
                                    }
                                }
                            }

                            return nextPositionalName(outerIndex, innerIndex);
                        };

                        for(int i = 0; i < static_cast<int>(token->inner.size()); ++i){
                            auto &c = token->inner[i];

                            auto first = Interpret(c, cursor, cf, fnCtx);
                            if(cursor->error){
                                cursor->subject = c;
                                return ctx->buildNil();
                            }

                            if(cf->state == CV::ControlFlowState::YIELD){
                                return first;
                            }

                            if(cf->state == CV::ControlFlowState::RETURN ||
                            cf->state == CV::ControlFlowState::SKIP){
                                return first;
                            }

                            if(first && first->type == CV::DataType::PROXY){
                                auto proxy = std::static_pointer_cast<CV::DataProxy>(first);

                                if(proxy->ptype == CV::Prefixer::EXPANDER){
                                    if(!proxy->target){
                                        cursor->setError(
                                            CV_ERROR_MSG_MISUSED_PREFIX,
                                            "Expander Prefix '^' produced a proxy without target",
                                            c
                                        );
                                        return ctx->buildNil();
                                    }

                                    auto expanded = proxy->target->unwrap();
                                    if(!expanded || expanded->type != CV::DataType::LIST){
                                        cursor->setError(
                                            CV_ERROR_MSG_MISUSED_PREFIX,
                                            "Expander Prefix '^' expects target to resolve into a LIST",
                                            c
                                        );
                                        return ctx->buildNil();
                                    }

                                    auto list = std::static_pointer_cast<CV::DataList>(expanded);

                                    for(int j = 0; j < static_cast<int>(list->v.size()); ++j){
                                        auto &memberRaw = list->v[j];
                                        auto memberOut = memberRaw ? memberRaw->unwrap() : fnCtx->buildNil();

                                        std::string pickedName = inferParamName(memberRaw, i, j);

                                        allParams.push_back({
                                            pickedName,
                                            memberOut
                                        });
                                    }

                                    continue;
                                }
                            }

                            auto out = first ? first->unwrap() : fnCtx->buildNil();
                            std::string pickedName = inferParamName(first, i);

                            allParams.push_back({
                                pickedName,
                                out
                            });
                        }


                        // Check for missing names
                        if(!fn->isVariadic){
                            for(int i = 0; i < fn->params.size(); ++i){
                                if(!CV::Tools::isInList(fn->params[i], allParams)){
                                    cursor->setError(
                                        CV_ERROR_MSG_WRONG_OPERANDS,
                                        CV::Tools::format(
                                            "Function '%s' is expecting param '%s' which wasn't provided",
                                            qname.c_str(),
                                            fn->params[i].c_str()
                                        ),
                                        token
                                    );
                                    return ctx->buildNil();                                    
                                }
                            }
                        }

                        if(fn->isLambda){
                            auto r = fn->lambda(allParams, fnCtx, cursor, token);
                            if(cursor->error){
                                cursor->subject = token;
                                return ctx->buildNil();
                            }
                            return r;
                        }else{
                            auto paramCtx = fnCtx->buildContext(true);
                            if(fn->isVariadic){
                                auto list = paramCtx->buildList();
                                paramCtx->data["@"] = list;
                                for(int i = 0; i < allParams.size(); ++i){
                                    list->v.push_back(allParams[i].second);
                                }
                                auto r = Interpret(fn->body, cursor, cf, paramCtx);
                                if(cursor->error){
                                    cursor->subject = token;
                                    return ctx->buildNil();
                                }  
                                return r;                              
                            }else{
                                for(int i = 0; i < allParams.size(); ++i){
                                    auto &a = allParams[i];
                                    paramCtx->data[a.first] = a.second;
                                }
                                auto r = Interpret(fn->body, cursor, cf, paramCtx);
                                if(cursor->error){
                                    cursor->subject = token;
                                    return ctx->buildNil();
                                }  
                                return r;                                
                            }
                        }

                        return ctx->buildNil();
                    };
                    case CV::DataType::STORE: {
                        auto store = std::static_pointer_cast<CV::DataStore>(data);
                        // Does this named proxy come with parameters?
                        if(token->inner.size() > 0){
                            std::vector<std::string> names;
                            // Gather names
                            for(int i = 0; i < token->inner.size(); ++i){                               
                                auto fetched = Interpret(token->inner[i], cursor, cf, ctx);
                                if(cursor->error){
                                    cursor->subject = token;
                                    return ctx->buildNil();
                                }          
                                if(cf->state == CV::ControlFlowState::YIELD){
                                    return fetched;
                                }        
                                std::string v = "";       
                                if(fetched->type == CV::DataType::PROXY){
                                    auto p = std::static_pointer_cast<CV::DataProxy>(fetched);
                                    v = p->pname;
                                }else
                                if(fetched->type == CV::DataType::STRING){
                                    auto s = std::static_pointer_cast<CV::DataString>(fetched);
                                    v = s->v; 
                                }else{
                                    cursor->setError(CV_ERROR_MSG_INVALID_ACCESOR, "Store access expects named proxy or a string as key", token);
                                    return ctx->buildNil();
                                }
                                names.push_back(v);
                            }
                            // Check if they're indeed part of this
                            for(int i = 0; i < names.size(); ++i){
                                if(store->v.count(names[i]) == 0){
                                    cursor->setError(CV_ERROR_MSG_STORE_UNDEFINED_MEMBER, "Store '"+qname+"' has no member named '"+names[i]+"'", token);
                                    return ctx->buildNil();             
                                }
                            }
                            // One name returns the value itself
                            if(names.size() == 1){
                                return store->v[names[0]];
                            }else{
                            // Several names returns a list of values
                                auto list = ctx->buildList();
                                for(int i = 0; i < names.size(); ++i){
                                    list->v.push_back( store->v[names[i]] );
                                }
                                return list;
                            }
                        }else{
                        // Otherwise this must be a list
                            return store;
                        }
                    };  
                    default: {
                        if(token->inner.size() > 0){
                            return bListConstructFromToken(token, ctx);
                        }else{  
                            return data;
                        }
                    };
                }
            }else{
                cursor->setError(CV_ERROR_MSG_UNDEFINED_IMPERATIVE, "Name '"+token->first+"'", token);
                return ctx->buildNil();
            }
        }                            
    }
}


std::shared_ptr<CV::Data> CV::Context::copy(
    const std::shared_ptr<CV::Data> &target
){
    if(!target){
        return this->buildNil();
    }

    switch(target->type){
        case CV::DataType::NUMBER: {
            return this->buildNumber(
                std::static_pointer_cast<CV::DataNumber>(target)->v
            );
        }

        case CV::DataType::STRING: {
            return this->buildString(
                std::static_pointer_cast<CV::DataString>(target)->v
            );
        }

        case CV::DataType::LIST: {
            auto result = this->buildList();
            auto from = std::static_pointer_cast<CV::DataList>(target);
            for(int i = 0; i < from->v.size(); ++i){
                result->v.push_back(
                    this->copy(from->v[i])
                );
            }
            return result;
        }

        case CV::DataType::STORE: {
            auto result = this->buildStore();
            auto from = std::static_pointer_cast<CV::DataStore>(target);
            for(auto &it : from->v){
                result->v[it.first] = this->copy(it.second);
            }
            return result;
        }

        case CV::DataType::FUNCTION: {
            auto result = std::make_shared<CV::DataFunction>();
            auto from = std::static_pointer_cast<CV::DataFunction>(target);
            result->params = from->params;
            result->isLambda = from->isLambda;
            result->isVariadic= from->isVariadic;
            result->body = from->body;
            result->lambda = from->lambda;
            return result;
        }

        case CV::DataType::PROXY: {
            auto result = std::make_shared<CV::DataProxy>();
            auto from = std::static_pointer_cast<CV::DataProxy>(target);
            result->pname = from->pname;
            result->ptype = from->ptype;
            result->target = from->target;
            return result;
        }        

        default:
        case CV::DataType::NIL: {
            return this->buildNil();
        }        
    }
}

void CV::SetUseColor(bool v){
    UseColorOnText = v;   
}
std::string CV::GetPrompt(){
    std::string start = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "[" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string cv = CV::Tools::setTextColor(Tools::Color::CYAN) + "~" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string end = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "]" + CV::Tools::setTextColor(Tools::Color::RESET);
    return start+cv+end;
}


std::string CV::DataToText(const std::shared_ptr<CV::Data> &t){
    if(!t){
        return  Tools::setTextColor(Tools::Color::BLUE) +
                "nil" +
                Tools::setTextColor(Tools::Color::RESET);
    }

    auto c_nil      = Tools::setTextColor(Tools::Color::BLUE);
    auto c_num      = Tools::setTextColor(Tools::Color::CYAN);
    auto c_str      = Tools::setTextColor(Tools::Color::GREEN);
    auto c_kw       = Tools::setTextColor(Tools::Color::RED, true);
    auto c_bracket  = Tools::setTextColor(Tools::Color::MAGENTA);
    auto c_prefix   = Tools::setTextColor(Tools::Color::YELLOW, true);
    auto c_meta     = Tools::setTextColor(Tools::Color::BLUE, true);
    auto c_reset    = Tools::setTextColor(Tools::Color::RESET);

    switch(t->type){

        default:
        case CV::DataType::NIL: {
            return c_nil + "nil" + c_reset;
        };

        case CV::DataType::NUMBER: {
            return  c_num +
                    CV::Tools::removeTrailingZeros(std::static_pointer_cast<CV::DataNumber>(t)->v) +
                    c_reset;
        };

        case CV::DataType::STRING: {
            auto out = std::static_pointer_cast<CV::DataString>(t)->v;

            return  c_str +
                    "'" + out + "'" +
                    c_reset;
        };

        case CV::DataType::FUNCTION: {
            auto fn = std::static_pointer_cast<CV::DataFunction>(t);

            std::string start    = c_kw + "[" + c_reset;
            std::string end      = c_kw + "]" + c_reset;
            std::string name     = c_kw + "fn" + c_reset;
            std::string binary   = c_meta + "BINARY" + c_reset;
            std::string body     = fn->body.get() ? fn->body->str() : "";
            std::string variadic = c_prefix + "@" + c_reset;

            std::string params = fn->isVariadic
                ? variadic
                : CV::Tools::compileList(fn->params);

            return  start +
                    name + " " +
                    start + params + end + " " +
                    (fn->isLambda ? binary : body) +
                    end;
        };

        case CV::DataType::LIST: {
            std::string output = c_bracket + "[" + c_reset;

            auto list = std::static_pointer_cast<CV::DataList>(t);

            int total = static_cast<int>(list->v.size());
            int limit = total > 30 ? 10 : total;

            for(int i = 0; i < limit; ++i){
                auto &q = list->v[i];
                output += q ? CV::DataToText(q) : (c_nil + "nil" + c_reset);

                if(i < limit - 1){
                    output += " ";
                }
            }

            if(limit != total){
                if(limit > 0){
                    output += " ";
                }
                output += c_prefix + "...(" + std::to_string(total - limit) + " hidden)" + c_reset;
            }

            return output + c_bracket + "]" + c_reset;
        };

        case CV::DataType::STORE: {
            std::string output = c_bracket + "[" + c_reset;

            auto store = std::static_pointer_cast<CV::DataStore>(t);

            int total = static_cast<int>(store->v.size());
            int limit = total > 30 ? 10 : total;
            int i = 0;

            for(auto it = store->v.begin(); it != store->v.end() && i < limit; ++it, ++i){
                auto &q = it->second;

                output +=   c_prefix + "[" + c_reset +
                            c_prefix + "~" + it->first + c_reset + " " +
                            (q ? CV::DataToText(q) : (c_nil + "nil" + c_reset)) +
                            c_prefix + "]" + c_reset;

                if(i < limit - 1){
                    output += " ";
                }
            }

            if(limit != total){
                if(limit > 0){
                    output += " ";
                }
                output += c_prefix + "...(" + std::to_string(total - limit) + " hidden)" + c_reset;
            }

            return output + c_bracket + "]" + c_reset;
        };

        case CV::DataType::PROXY: {
            auto proxy = std::static_pointer_cast<CV::DataProxy>(t);

            std::string out = c_prefix + "~" + proxy->pname + c_reset;

            if(proxy->target){
                out += " " + CV::DataToText(proxy->target);
            }

            return out;
        };

        case CV::DataType::CONTEXT: {
            return c_meta + "<context>" + c_reset;
        };
    }
}


static std::shared_ptr<CV::Data> __cv_unwrap(const std::shared_ptr<CV::Data> &d){
    return d ? d->unwrap() : std::shared_ptr<CV::Data>(nullptr);
}

static bool __cv_bool_value(const std::shared_ptr<CV::Data> &d){
    auto v = __cv_unwrap(d);
    if(!v){
        return false;
    }

    switch(v->type){
        case CV::DataType::NIL:
            return false;
        case CV::DataType::NUMBER:
            return std::static_pointer_cast<CV::DataNumber>(v)->v != 0;
        case CV::DataType::STRING:
            return !std::static_pointer_cast<CV::DataString>(v)->v.empty();
        case CV::DataType::LIST:
            return !std::static_pointer_cast<CV::DataList>(v)->v.empty();
        case CV::DataType::STORE:
            return !std::static_pointer_cast<CV::DataStore>(v)->v.empty();
        default:
            return true;
    }
}

static bool __cv_expect_type(
    const std::string &fname,
    const std::shared_ptr<CV::Data> &value,
    CV::DataType expected,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    auto v = __cv_unwrap(value);
    if(!v || v->type != expected){
        cursor->setError(
            CV_ERROR_MSG_WRONG_OPERANDS,
            "Function '"+fname+"' expected "+CV::DataTypeName(expected)+
            ", provided "+(v ? CV::DataTypeName(v->type) : std::string("NULL")),
            token
        );
        return false;
    }
    return true;
}

static bool __cv_expect_at_least(
    const std::string &fname,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) < n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "'"+fname+"' expects at least ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static bool __cv_expect_exactly(
    const std::string &fname,
    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
    int n,
    const CV::CursorType &cursor,
    const CV::TokenType &token
){
    if(static_cast<int>(args.size()) != n){
        cursor->setError(
            CV_ERROR_MSG_MISUSED_FUNCTION,
            "'"+fname+"' expects exactly ("+std::to_string(n)+") argument(s)",
            token
        );
        return false;
    }
    return true;
}

static void __cv_register_numeric_conditional(
    const std::shared_ptr<CV::Context> &ctx,
    const std::string &fname,
    const std::function<bool(CV_NUMBER, CV_NUMBER)> &comparator
){
    ctx->registerFunction(
        fname,
        {"a", "b"},
        [fname, comparator](
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
            const std::shared_ptr<CV::Context> &fctx,
            const CV::CursorType &cursor,
            const CV::TokenType &token
        ) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly(fname, args, 2, cursor, token)){
                return fctx->buildNil();
            }

            auto a = __cv_unwrap(args[0].second);
            auto b = __cv_unwrap(args[1].second);

            if(!__cv_expect_type(fname, a, CV::DataType::NUMBER, cursor, token) ||
               !__cv_expect_type(fname, b, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto av = std::static_pointer_cast<CV::DataNumber>(a)->v;
            auto bv = std::static_pointer_cast<CV::DataNumber>(b)->v;

            return std::static_pointer_cast<CV::Data>(
                fctx->buildNumber(comparator(av, bv) ? 1 : 0)
            );
        }
    );
}

bool CV::CoreSetup(
    const std::shared_ptr<CV::Context> &ctx
){
    char *cvLibPath = std::getenv("CANVAS_LIB_HOME");
    CV_LIB_HOME = cvLibPath != nullptr ? std::string(cvLibPath) : "./lib";

    ////////////////////////////
    //// ARITHMETIC
    ////////////////////////////

    ctx->registerFunction("+",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            auto result = fctx->buildNumber(0);

            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                auto v = __cv_unwrap(args[i].second);
                if(!__cv_expect_type("+", v, CV::DataType::NUMBER, cursor, token)){
                    return fctx->buildNil();
                }
                result->v += std::static_pointer_cast<CV::DataNumber>(v)->v;
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("-",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_at_least("-", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto first = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("-", first, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto result = fctx->buildNumber(std::static_pointer_cast<CV::DataNumber>(first)->v);

            for(int i = 1; i < static_cast<int>(args.size()); ++i){
                auto v = __cv_unwrap(args[i].second);
                if(!__cv_expect_type("-", v, CV::DataType::NUMBER, cursor, token)){
                    return fctx->buildNil();
                }
                result->v -= std::static_pointer_cast<CV::DataNumber>(v)->v;
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("*",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            auto result = fctx->buildNumber(1);

            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                auto v = __cv_unwrap(args[i].second);
                if(!__cv_expect_type("*", v, CV::DataType::NUMBER, cursor, token)){
                    return fctx->buildNil();
                }
                result->v *= std::static_pointer_cast<CV::DataNumber>(v)->v;
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("/",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_at_least("/", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto first = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("/", first, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto result = fctx->buildNumber(std::static_pointer_cast<CV::DataNumber>(first)->v);

            for(int i = 1; i < static_cast<int>(args.size()); ++i){
                auto v = __cv_unwrap(args[i].second);
                if(!__cv_expect_type("/", v, CV::DataType::NUMBER, cursor, token)){
                    return fctx->buildNil();
                }

                auto tv = std::static_pointer_cast<CV::DataNumber>(v)->v;
                if(tv == 0){
                    cursor->setError(
                        CV_ERROR_MSG_MISUSED_FUNCTION,
                        "'/' Division by zero at argument ("+std::to_string(i)+")",
                        token
                    );
                    return fctx->buildNil();
                }

                result->v /= tv;
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ////////////////////////////
    //// BOOLEAN
    ////////////////////////////

    ctx->registerFunction("and",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            (void)cursor;
            (void)token;

            auto result = fctx->buildNumber(1);

            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                if(!__cv_bool_value(args[i].second)){
                    result->v = 0;
                    break;
                }
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("not", {"value"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("not", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            return std::static_pointer_cast<CV::Data>(
                fctx->buildNumber(__cv_bool_value(args[0].second) ? 0 : 1)
            );
        }
    );

    ////////////////////////////
    //// CONDITIONALS (eager-safe only)
    ////////////////////////////

    __cv_register_numeric_conditional(ctx, "eq",  [](CV_NUMBER a, CV_NUMBER b){ return a == b; });
    __cv_register_numeric_conditional(ctx, "neq", [](CV_NUMBER a, CV_NUMBER b){ return a != b; });
    __cv_register_numeric_conditional(ctx, ">",   [](CV_NUMBER a, CV_NUMBER b){ return a >  b; });
    __cv_register_numeric_conditional(ctx, ">=",  [](CV_NUMBER a, CV_NUMBER b){ return a >= b; });
    __cv_register_numeric_conditional(ctx, "<",   [](CV_NUMBER a, CV_NUMBER b){ return a <  b; });
    __cv_register_numeric_conditional(ctx, "<=",  [](CV_NUMBER a, CV_NUMBER b){ return a <= b; });

    ////////////////////////////
    //// LISTS / STORES
    ////////////////////////////

    ctx->registerFunction("nth", {"list", "index"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("nth", args, 2, cursor, token)){
                return fctx->buildNil();
            }

            auto listData = __cv_unwrap(args[0].second);
            auto indexData = __cv_unwrap(args[1].second);

            if(!__cv_expect_type("nth", listData, CV::DataType::LIST, cursor, token) ||
               !__cv_expect_type("nth", indexData, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto list = std::static_pointer_cast<CV::DataList>(listData);
            int index = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(indexData)->v);

            if(index < 0 || index >= static_cast<int>(list->v.size())){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Provided out-of-bounds index: Expected 0 to "+
                    std::to_string(static_cast<int>(list->v.size()) - 1)+
                    ", provided "+std::to_string(index),
                    token
                );
                return fctx->buildNil();
            }

            return list->v[index];
        }
    );

    ctx->registerFunction("length", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("length", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto data = __cv_unwrap(args[0].second);
            auto result = fctx->buildNumber(0);

            if(data->type == CV::DataType::LIST){
                result->v = static_cast<CV_NUMBER>(std::static_pointer_cast<CV::DataList>(data)->v.size());
                return std::static_pointer_cast<CV::Data>(result);
            }

            if(data->type == CV::DataType::STORE){
                result->v = static_cast<CV_NUMBER>(std::static_pointer_cast<CV::DataStore>(data)->v.size());
                return std::static_pointer_cast<CV::Data>(result);
            }

            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Function 'length' expects LIST or STORE",
                token
            );
            return fctx->buildNil();
        }
    );

    ctx->registerFunction(">>", {"subject", "target"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly(">>", args, 2, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            auto target = __cv_unwrap(args[1].second);

            if(target->type != CV::DataType::LIST){
                auto nl = fctx->buildList();
                nl->v.push_back(target);
                nl->v.push_back(subject);
                return std::static_pointer_cast<CV::Data>(nl);
            }

            auto list = std::static_pointer_cast<CV::DataList>(target);
            list->v.push_back(subject);
            return std::static_pointer_cast<CV::Data>(list);
        }
    );

    ctx->registerFunction("<<", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("<<", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto data = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("<<", data, CV::DataType::LIST, cursor, token)){
                return fctx->buildNil();
            }

            auto list = std::static_pointer_cast<CV::DataList>(data);
            if(list->v.empty()){
                return fctx->buildNil();
            }

            auto result = list->v.back();
            list->v.pop_back();
            return result;
        }
    );

    ctx->registerFunction("l-sub",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(static_cast<int>(args.size()) != 2 && static_cast<int>(args.size()) != 3){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'l-sub' expects 2 or 3 arguments",
                    token
                );
                return fctx->buildNil();
            }

            auto listData = __cv_unwrap(args[0].second);
            auto fromData = __cv_unwrap(args[1].second);

            if(!__cv_expect_type("l-sub", listData, CV::DataType::LIST, cursor, token) ||
               !__cv_expect_type("l-sub", fromData, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto list = std::static_pointer_cast<CV::DataList>(listData);
            int from = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(fromData)->v);

            int to = static_cast<int>(list->v.size()) - 1;
            if(static_cast<int>(args.size()) == 3){
                auto toData = __cv_unwrap(args[2].second);
                if(!__cv_expect_type("l-sub", toData, CV::DataType::NUMBER, cursor, token)){
                    return fctx->buildNil();
                }
                to = static_cast<int>(std::static_pointer_cast<CV::DataNumber>(toData)->v);
            }

            if(from < 0 || from >= static_cast<int>(list->v.size()) ||
               to < 0 || to >= static_cast<int>(list->v.size()) ||
               to < from){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Invalid sub-range for 'l-sub'",
                    token
                );
                return fctx->buildNil();
            }

            auto result = fctx->buildList();
            for(int i = from; i <= to; ++i){
                result->v.push_back(list->v[i]);
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("l-splice",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {
            (void)cursor;
            (void)token;

            auto result = fctx->buildList();
            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                result->v.push_back(__cv_unwrap(args[i].second));
            }
            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ctx->registerFunction("s-splice",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            auto result = fctx->buildStore();

            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                const auto &vname = args[i].first;
                if(vname.empty()){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Function 's-splice' expects every operand to have a name",
                        token
                    );
                    return fctx->buildNil();
                }
                result->v[vname] = __cv_unwrap(args[i].second);
            }

            return std::static_pointer_cast<CV::Data>(result);
        }
    );

    ////////////////////////////
    //// MUTATORS
    ////////////////////////////

    ctx->registerFunction("++", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("++", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("++", subject, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            ++std::static_pointer_cast<CV::DataNumber>(subject)->v;
            return subject;
        }
    );

    ctx->registerFunction("--", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("--", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("--", subject, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            --std::static_pointer_cast<CV::DataNumber>(subject)->v;
            return subject;
        }
    );

    ctx->registerFunction("//", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("//", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("//", subject, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            std::static_pointer_cast<CV::DataNumber>(subject)->v /= static_cast<CV_NUMBER>(2.0);
            return subject;
        }
    );

    ctx->registerFunction("**", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("**", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            if(!__cv_expect_type("**", subject, CV::DataType::NUMBER, cursor, token)){
                return fctx->buildNil();
            }

            auto n = std::static_pointer_cast<CV::DataNumber>(subject);
            n->v = n->v * n->v;
            return subject;
        }
    );

    ////////////////////////////
    //// UTILS
    ////////////////////////////

    ctx->registerFunction("print",
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            std::string out;

            for(int i = 0; i < static_cast<int>(args.size()); ++i){
                auto arg = __cv_unwrap(args[i].second);
                if(!arg){
                    cursor->setError(
                        "Invalid Operand",
                        "Function 'print' received a null operand while printing",
                        token
                    );
                    return fctx->buildNil();
                }

                if(arg->type == CV::DataType::STRING){
                    out += std::static_pointer_cast<CV::DataString>(arg)->v;
                }else{
                    out += CV::DataToText(arg);
                }
            }

            printf("%s\n", out.c_str());
            return fctx->buildNil();
        }
    );

    ctx->registerFunction("typeof", {"subject"},
        [](const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
           const std::shared_ptr<CV::Context> &fctx,
           const CV::CursorType &cursor,
           const CV::TokenType &token) -> std::shared_ptr<CV::Data> {

            if(!__cv_expect_exactly("typeof", args, 1, cursor, token)){
                return fctx->buildNil();
            }

            auto subject = __cv_unwrap(args[0].second);
            return std::static_pointer_cast<CV::Data>(
                fctx->buildString(CV::DataTypeName(subject ? subject->type : CV::DataType::NIL))
            );
        }
    );

    return true;
}

std::shared_ptr<CV::Data> CV::Import(
    const std::string &fname,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    if(!CV::Tools::fileExists(fname)){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "'"+fname+"' does not exist",
            1
        );
        return ctx->buildNil();
    }

    auto literal = CV::Tools::readFile(fname);

    auto root = CV::BuildTree(literal, cursor);
    if(cursor->error){
        return ctx->buildNil();
    }

    auto result = ctx->buildNil();

    for(int i = 0; i < static_cast<int>(root.size()); ++i){
        auto cf = std::make_shared<CV::ControlFlow>();
        cf->state = CV::ControlFlowState::CONTINUE;

        result = CV::Interpret(root[i], cursor, cf, ctx);
        if(cursor->error){
            return ctx->buildNil();
        }

        if(cf->state == CV::ControlFlowState::RETURN ||
           cf->state == CV::ControlFlowState::YIELD){
            break;
        }

        if(cf->state == CV::ControlFlowState::SKIP){
            // top-level skip has no special meaning here
            continue;
        }
    }

    return result ? result : ctx->buildNil();
}

#define CV_IMPORT_LIBRARY_ENTRY_POINT_ARGS \
    const std::shared_ptr<CV::Context> &ctx, \
    const std::shared_ptr<CV::Cursor> &cursor

std::shared_ptr<CV::Data> CV::ImportDynamicLibrary(
    const std::string &path,
    const std::string &fname,
    const CV::ContextType &ctx,
    const CV::CursorType &cursor
){
    using rlib = void (*)(CV_IMPORT_LIBRARY_ENTRY_POINT_ARGS);

#if defined(CV_PLATFORM_TYPE_LINUX) || defined(CV_PLATFORM_TYPE_OSX)

    void *handle = dlopen(path.c_str(), RTLD_LAZY);
    if(!handle){
        const char *err = dlerror();
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to load dynamic library '"+fname+"': "+std::string(err ? err : "unknown error"),
            cursor->line
        );
        return ctx->buildNil();
    }

    dlerror(); // clear previous error

    auto registerlibrary = reinterpret_cast<rlib>(dlsym(handle, "_CV_REGISTER_LIBRARY"));
    const char *error = dlerror();
    if(error){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to find '_CV_REGISTER_LIBRARY' in '"+fname+"': "+std::string(error),
            cursor->line
        );
        dlclose(handle);
        return ctx->buildNil();
    }

    (*registerlibrary)(ctx, cursor);
    if(cursor->error){
        dlclose(handle);
        return ctx->buildNil();
    }

    auto id = GEN_ID();
    {
        std::lock_guard<std::mutex> lock(__cv_loaded_dynamic_libs_mutex);
        __cv_loaded_dynamic_libs[id] = {handle, path};
    }

    return std::static_pointer_cast<CV::Data>(ctx->buildNumber(id));

#elif defined(CV_PLATFORM_TYPE_WINDOWS)

    HMODULE hdll = LoadLibraryA(path.c_str());
    if(hdll == NULL){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to load dynamic library '"+fname+"'",
            cursor->line
        );
        return ctx->buildNil();
    }

    auto entry = reinterpret_cast<rlib>(GetProcAddress(hdll, "_CV_REGISTER_LIBRARY"));
    if(entry == NULL){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to find '_CV_REGISTER_LIBRARY' in '"+fname+"'",
            cursor->line
        );
        FreeLibrary(hdll);
        return ctx->buildNil();
    }

    entry(ctx, cursor);
    if(cursor->error){
        FreeLibrary(hdll);
        return ctx->buildNil();
    }

    auto id = GEN_ID();
    {
        std::lock_guard<std::mutex> lock(__cv_loaded_dynamic_libs_mutex);
        __cv_loaded_dynamic_libs[id] = {hdll, path};
    }

    return std::static_pointer_cast<CV::Data>(ctx->buildNumber(id));

#else

    cursor->setError(
        CV_ERROR_MSG_LIBRARY_NOT_VALID,
        "Dynamic libraries are not supported on this platform",
        1
    );
    return ctx->buildNil();

#endif
}