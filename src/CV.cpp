#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <functional>
#include <sys/stat.h>
#include <fstream>

// DYNAMIC LIBRARY STUFF
#if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX)
    #include <dlfcn.h> 
#elif  (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)
    // should prob move the entire DLL loading routines to its own file
    #include <Libloaderapi.h>
#endif

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

        std::shared_ptr<CV::Instruction> fetchInstruction(
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins
        ){
            for(int i = 0; i < ins.size(); ++i){
                if(ins[i].first == name){
                    return ins[i].second;
                }
            }
            return NULL;
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

            CV::Data *SolveInstruction(
                const std::shared_ptr<CV::Program> &prog,
                const std::string &fname,
                const std::string &vname,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                const std::shared_ptr<CV::Context> &ctx,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const CV::TokenType &token,
                int expType
            ){

                auto param0 = CV::Tools::ErrorCheck::FetchInstruction(fname, vname, ins, cursor, st, token);
                if(cursor->error){
                    return NULL;
                } 
                auto param0v = CV::Execute(param0, prog, cursor, ctx, st);
                if(cursor->error){
                    return NULL;
                } 
                if(!CV::Tools::ErrorCheck::IsType(fname, vname, param0v, cursor, st, token, expType)){
                    return NULL;
                }
                
                return param0v;
            }

            bool IsType(
                const std::string &fname,
                const std::string &vname,
                CV::Data* v,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const CV::TokenType &token,
                int expType
            ){
                if(v->type != expType){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Imperative '"+fname+"' expects operand '"+vname+"' to be "+CV::DataTypeName(expType)+", provided "+CV::DataTypeName(v->type),
                        token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return false;
                }          
                return true;            
            } 

            bool AreAllType(
                const std::string &name,
                const std::vector<CV::Data*> &params,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const CV::TokenType &token,
                int expType
            ){
                for(int i = 0; i < params.size(); ++i){
                    auto data = params[i];
                    if(data->type != expType){
                        cursor->setError(
                            CV_ERROR_MSG_WRONG_OPERANDS,
                            "Imperative '"+name+"' expects all operands to be "+
                                CV::DataTypeName(expType)+": operand "+
                                std::to_string(i)+" is "+CV::DataTypeName(data->type),
                            token
                        );
                        st->state = CV::ControlFlowState::CRASH;
                        return false;
                    }
                }            
                return true;            
            }    

            std::shared_ptr<CV::Instruction> FetchInstructionIfExists(
                const std::string &vname,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins
            ){
                for(int i = 0; i < ins.size(); ++i){
                    if(ins[i].first == vname){
                        return ins[i].second;
                    }
                }
                return nullptr;
            }     
            
            CV::Data *SolveInstructionIfExists(
                const std::shared_ptr<CV::Program> &prog,
                const std::string &fname,
                const std::string &vname,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                const std::shared_ptr<CV::Context> &ctx,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const std::shared_ptr<CV::Token> &token,
                int expType
            ){
                auto fetched = CV::Tools::ErrorCheck::FetchInstructionIfExists(vname, ins);
                if(!fetched){
                    return NULL;
                }

                auto value = CV::Execute(fetched, prog, cursor, ctx, st);
                if(cursor->error){
                    return NULL;
                }

                if(!CV::Tools::ErrorCheck::IsType(fname, vname, value, cursor, st, token, expType)){
                    return NULL;
                }

                return value;
            }


            bool ExpectsExactlyOperands(
                int provided,
                int expected,
                const std::string &name,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const CV::TokenType &token
            ){
                if(provided != expected){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Imperative '"+name+"' expects exactly "+std::to_string(expected)+
                            " operand"+std::string(expected == 1 ? "" : "s")+
                            ", provided "+std::to_string(provided),
                        token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return false;
                }

                return true;
            }            

            std::shared_ptr<CV::Instruction> FetchInstruction(
                const std::string &fname,
                const std::string &vname,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::ControlFlow> &st,
                const CV::TokenType &token
            ){
                auto gins = CV::Tools::fetchInstruction(vname, ins);
                if(gins == NULL){
                    std::string err = "expects argument '"+vname+"' but was not provided (with or without named positional arguments)";
                    cursor->setError(CV_ERROR_MSG_MISUSED_FUNCTION, "Function '"+fname+"' "+err, token);    
                    st->state = CV::ControlFlowState::CRASH;
                    return NULL;
                }
                return gins;
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
    this->value.shrink_to_fit();
}

//
//  STORE
//
CV::DataStore::DataStore(){
    this->type = CV::DataType::STORE;
}

void CV::DataStore::clear(const std::shared_ptr<CV::Program> &prog){
    for(const auto &it : this->value){
        auto &dataId = it.second;
        auto subject = prog->getData(dataId);
        if(subject == NULL){
            continue;
        }
        subject->decRefCount();
    }

    this->value.clear();
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
    this->closureCtxId = -1;
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
    };}

void CV::DataFunction::clear(const std::shared_ptr<CV::Program> &prog){
    this->body.reset();
    this->params.clear();
    this->blambda = nullptr;
    this->params.shrink_to_fit(); // optional
    this->lambda = {};
}


//
//  THREAD
//
CV::DataThread::DataThread(){
    this->entrypointInsId = 0;
    this->ctxId = 0;
    this->state = CV::ThreadState::CREATED;
    this->type = CV::DataType::THREAD;
}

void CV::DataThread::clear(const std::shared_ptr<CV::Program> &prog){
    std::lock_guard<std::mutex> lock(this->accessMutex);

    if(this->returnId >= 0){
        auto subject = prog->getData(this->returnId);
        if(subject != NULL){
            subject->decRefCount();
        }
        this->returnId = -1;
    }
}

void CV::DataThread::setPayload(const std::shared_ptr<CV::Program> &prog, int dataId){
    auto data = prog->getData(dataId);
    if(!data){
        // This shouldn't happen regardless
        return;
    }
    this->accessMutex.lock();
    this->returnId = dataId;
    data->incRefCount();
    this->accessMutex.unlock();
}

void CV::DataThread::setState(int nstate){
    this->accessMutex.lock();
    this->state = nstate;
    this->accessMutex.unlock();
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
                            "~"+it.first+" "+(q ? CV::DataToText(prog, q):
                                Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET)
                            )+
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

        case CV::DataType::THREAD: {
            auto thread = static_cast<CV::DataThread*>(t);

            std::string payload = "";

            thread->accessMutex.lock();
            if(thread->returnId == 0){
                payload = Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
            }else{
                auto target = prog->getData(thread->returnId);
                if(target){
                    payload += CV::DataToText(prog, target);
                }                
            }
            thread->accessMutex.unlock();

            return Tools::setTextColor(Tools::Color::YELLOW)
                                +"[THREAD "+std::to_string(thread->id)+" "
                                +Tools::setTextColor(Tools::Color::GREEN)
                                +CV::ThreadState::name(thread->state)
                                +Tools::setTextColor(Tools::Color::RESET)+" -> '"+payload+"'"
                                +Tools::setTextColor(Tools::Color::YELLOW)+"]"
                                +Tools::setTextColor(Tools::Color::RESET);
        };

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

struct InvokeThread {
    int threadDataId;
    std::shared_ptr<CV::Program> prog;
    std::shared_ptr<CV::Cursor> upcursor;
    std::shared_ptr<CV::Token> root;
};

static void RUN_THREAD(const std::shared_ptr<InvokeThread> &handle){

    auto &prog = handle->prog;
    auto &upcursor = handle->upcursor;
    auto &root = handle->root;

    auto coreBase = prog->getData(handle->threadDataId);
    if(coreBase == NULL || coreBase->type != CV::DataType::THREAD){
        upcursor->setError(
            CV_ERROR_MSG_INVALID_SYNTAX,
            "Crash in Thread: missing or invalid thread handle",
            root
        );
        return;
    }

    auto core = static_cast<CV::DataThread*>(coreBase);

    auto entrypoint = prog->getIns(core->entrypointInsId);
    auto ctx = prog->getContext(core->ctxId);

    auto st = std::make_shared<CV::ControlFlow>();
    st->state = CV::ControlFlowState::CONTINUE;

    auto cursor = std::make_shared<CV::Cursor>();

    auto r = CV::Execute(entrypoint, prog, cursor, ctx, st);
    if(cursor->error){
        st->state = CV::ControlFlowState::CRASH;

        upcursor->setError(
            cursor->title,
            "Crash in Thread "+std::to_string(core->id)+": "+cursor->message,
            root
        );

        core->setState(CV::ThreadState::FINISHED);
        return;
    }

    if(r != NULL){
        core->setPayload(handle->prog, r->id);
    }else{
        core->returnId = 0;
    }

    core->setState(CV::ThreadState::FINISHED);
}

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
            IMPORT
        */
        if(token->first == "import"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Preserve old "no prefixer" behavior: bring expects a plain value expression
            if(CV::Test::IsItPrefixInstruction(target)){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' does not accept prefixed arguments",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto st = std::make_shared<CV::ControlFlow>();
            st->state = CV::ControlFlowState::CONTINUE;

            auto fnamev = CV::Execute(target, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            if(fnamev == NULL || fnamev->type != CV::DataType::STRING){
                cursor->setError(
                    CV_ERROR_MSG_WRONG_OPERANDS,
                    "'"+token->first+"' expects operand 0 to evaluate to STRING",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Get symbolic or literal
            auto fname = static_cast<CV::DataString*>(fnamev)->value;

            // Does it include .cv?
            if(CV::Tools::fileExtension(fname) == ""){
                fname += ".cv";
            }

            // Is it a local file?
            if(CV::Tools::fileExists("./" + fname)){
                fname = "./" + fname;
            }else{
                // Then it must be a 'lib/' library
                fname = CV_LIB_HOME + "/" + fname;
            }

            // Does it exist at all?
            if(!CV::Tools::fileExists(fname)){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "No canvas library '"+fname+"' was found",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Load it now, same as old behavior
            int id = CV::Import(fname, prog, ctx, cursor);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::LIBRAY_IMPORT, token);

            // Keep same metadata layout as the old design unless you have already changed Import semantics:
            // data[0] = caller context id
            // data[1] = imported result id / import handle returned by CV::Import
            // data[2] = eager-import marker
            ins->data.push_back(ctx->id);
            ins->data.push_back(id);
            ins->data.push_back(true);

            ins->literal.push_back(fname);

            return ins;
        }else
        /*
            IMPORT "dynamic-library"
        */
        if(token->first == "import:dynamic-library"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" NAME)",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            if(CV::Test::IsItPrefixInstruction(target)){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' does not accept prefixed arguments",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto st = std::make_shared<CV::ControlFlow>();
            st->state = CV::ControlFlowState::CONTINUE;

            auto fnamev = CV::Execute(target, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            if(fnamev == NULL || fnamev->type != CV::DataType::STRING){
                cursor->setError(
                    CV_ERROR_MSG_WRONG_OPERANDS,
                    "'"+token->first+"' expects operand 0 to evaluate to STRING",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Get symbolic or literal
            auto fname = static_cast<CV::DataString*>(fnamev)->value;

            // Add platform extension if missing
            if(CV::Tools::fileExtension(fname) == ""){
                switch(CV::PLATFORM){
                    case CV::SupportedPlatform::LINUX: {
                        fname += ".so";
                    } break;

                    case CV::SupportedPlatform::WINDOWS: {
                        fname += ".dll";
                    } break;

                    case CV::SupportedPlatform::OSX: {
                        fname += ".dylib";
                    } break;

                    default:
                    case CV::SupportedPlatform::UNKNOWN: {
                        cursor->setError(
                            CV_ERROR_MSG_LIBRARY_NOT_VALID,
                            "Failed to infer dynamic library extension for platform in '"+token->first+"'",
                            token
                        );
                        return prog->createInstruction(CV::InstructionType::NOOP, token);
                    } break;
                }
            }

            // Local file first, otherwise library home
            if(CV::Tools::fileExists("./" + fname)){
                fname = "./" + fname;
            }else{
                fname = CV_LIB_HOME + "/" + fname;
            }

            if(!CV::Tools::fileExists(fname)){
                cursor->setError(
                    CV_ERROR_MSG_LIBRARY_NOT_VALID,
                    "No dynamic library '"+fname+"' was found",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            // Load it eagerly, same style as bring/import
            int id = CV::ImportDynamicLibrary(fname, fname, prog, ctx, cursor);
            if(cursor->error){
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::LIBRAY_IMPORT, token);

            // Keep old metadata layout if LIBRAY_IMPORT execution still expects it:
            // data[0] = caller context id
            // data[1] = import result / handle id
            // data[2] = false => dynamic library import
            ins->data.push_back(ctx->id);
            ins->data.push_back(id);
            ins->data.push_back(false);

            ins->literal.push_back(fname);
            return ins;
        }else
        /*
            FN
        */
        if(token->first == "fn"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" [arguments][code])", token);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }
            auto fn = new CV::DataFunction();
            fn->closureCtxId = ctx->id;
            prog->allocateData(fn);

            fn->isBinary = false;
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
                        return prog->createInstruction(CV::InstructionType::NOOP, token);                   
                    }
                    if(CV::Tools::isReservedWord(name)){
                        cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' argument name '"+name+"' is a name of native constructor which cannot be overriden", token);
                        return prog->createInstruction(CV::InstructionType::NOOP, token);                
                    }  
                    if(hasParam(name)){
                        cursor->setError(
                            CV_ERROR_MSG_MISUSED_IMPERATIVE,
                            "'"+token->first+"' argument name '"+name+"' is duplicated",
                            token
                        );
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
        if(token->first == "cc"){
            if(token->inner.size() != 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects exactly 2 tokens ("+token->first+" VALUE)",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto target = CV::Translate(token->inner[0], prog, ctx, cursor);
            if(cursor->error){
                cursor->subject = token;
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::CARBON_COPY, token);
            ins->params.push_back(target->id);
            return ins;
        }else
        /*
            MUT
        */
        if(token->first == "mut"){
            if(token->inner.size() != 2){
                cursor->setError(CV_ERROR_MSG_MISUSED_IMPERATIVE, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" NAME VALUE)", token);
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
                cursor->setError(CV_ERROR_MSG_MISUSED_CONSTRUCTOR, "'"+token->first+"' expects exactly 3 tokens ("+token->first+" NAME VALUE)", token);
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
            

            auto ins = prog->createInstruction(CV::InstructionType::LET, token);
            ins->literal.push_back(name);

            if(target->type == CV::InstructionType::PROXY_PARALELER){
                ctx->setName(name, target->data[3]);                  
                ctx->setData(prog, target->data[3]);
                ins->data.push_back(target->data[3]);
                ins->params.push_back(target->id);                
            }else            
            if(target->type == CV::InstructionType::PROXY_STATIC){
                ctx->setName(name, target->data[0]);                  
                ctx->setData(prog, target->data[0]);
                ins->data.push_back(target->data[0]);
            }else{
                auto ghostData = prog->buildNil();
                ctx->setData(prog, ghostData->id);
                ctx->setName(name, ghostData->id);                
                ins->data.push_back(ghostData->id);
                ins->params.push_back(target->id);                
            }

            return ins;

        }else   
        if(token->first == "await"){
            if(token->inner.size() < 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_IMPERATIVE,
                    "'"+token->first+"' expects at least 2 tokens (await THREAD ...)",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::CF_AWAIT, token);

            for(int i = 0; i < token->inner.size(); ++i){
                auto target = CV::Translate(token->inner[i], prog, ctx, cursor);
                if(cursor->error){
                    cursor->subject = token;
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }

                ins->data.push_back(target->id);
            }

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
        if(token->first[0] == '|'){
            if(token->inner.size() != 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Paralleler Prefix '"+token->first+"' expects no values",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            if(token->first.size() == 1){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_PREFIX,
                    "Paralleler Prefix '"+token->first+"' expects an appended body",
                    token
                );
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            std::string statement(token->first.begin() + 1, token->first.end());

            // Keep the compile-time context lineage for the thread body
            auto tctx = prog->createContext(ctx->id);

            auto entrypoint = CV::Compile(statement, prog, cursor, tctx);
            if(cursor->error){
                cursor->subject = token;
                prog->deleteContext(tctx->id);
                return prog->createInstruction(CV::InstructionType::NOOP, token);
            }

            auto ins = prog->createInstruction(CV::InstructionType::PROXY_PARALELER, token);
            ins->data.push_back(CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION);
            ins->data.push_back(CV::Prefixer::PARALELLER);

            // data[2] = thread compile-time context id
            ins->data.push_back(tctx->id);

            // params[0] = compiled body entrypoint
            ins->params.push_back(entrypoint->id);

            auto thread = new CV::DataThread();
            prog->allocateData(thread);

            thread->ctxId = tctx->id;
            thread->entrypointInsId = entrypoint->id;
            thread->returnId = 0;
            thread->setState(CV::ThreadState::CREATED);            

            ins->data.push_back(thread->id);

            return ins;

        }else
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
            

            // Build referred instruction
            if(token->inner.size() > 0){
                auto &inc = token->inner[0];
                auto target = CV::Translate(inc, prog, ctx, cursor);
                if(cursor->error){
                    cursor->subject = token;
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }  

                if(target->type == CV::InstructionType::PROXY_PARALELER){
                    ctx->setName(name, target->data[3]);                  
                    ctx->setData(prog, target->data[3]);
                    ins->data.push_back(target->data[3]);
                    ins->params.push_back(target->id);                
                }else            
                if(target->type == CV::InstructionType::PROXY_STATIC){
                    ctx->setName(name, target->data[0]);                  
                    ctx->setData(prog, target->data[0]);
                    ins->data.push_back(target->data[0]);
                }else{
                    auto ghostData = prog->buildNil();
                    ctx->setData(prog, ghostData->id);
                    ctx->setName(name, ghostData->id);                
                    ins->data.push_back(ghostData->id);
                    ins->params.push_back(target->id);                
                }
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

                if(data == NULL){
                    cursor->setError(
                        CV_ERROR_MSG_INVALID_SYNTAX,
                        "Name '"+token->first+"' resolves to dead data id "+std::to_string(dataId),
                        token
                    );
                    return prog->createInstruction(CV::InstructionType::NOOP, token);
                }

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
                            std::string err = "was provided with an invalid number of arguments (with or without named positional arguments): "+comp;
                            cursor->setError(CV_ERROR_MSG_MISUSED_FUNCTION, "Function '"+token->first+"' "+err, token);
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
        case CV::InstructionType::PROXY_PARALELER: {
            // If this paralleler already created its thread handle, return it
            if(prog->isPrefetched(ins->id)){
                auto prefetchedId = prog->getPrefetch(ins->id);
                auto prefetched = prog->getData(prefetchedId);
                if(prefetched != NULL){
                    return prefetched;
                }

                cursor->setError(
                    CV_ERROR_MSG_INVALID_SYNTAX,
                    "Paralleler instruction points to dead prefetched thread handle",
                    ins->token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            auto tctxId = ins->data[2];
            auto entrypointId = ins->params[0];

            auto tctx = prog->getContext(tctxId);
            auto entrypoint = prog->getIns(entrypointId);

            if(!tctx || !entrypoint){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_SYNTAX,
                    "Paralleler instruction is missing thread context or entrypoint",
                    ins->token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            auto threadId = ins->data[3];
            auto threadf = prog->getData(ins->data[3]);

            if(!threadf || threadf->type != CV::DataType::THREAD){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_SYNTAX,
                    "Paralleler instruction is pointing to unallocated data %i",
                    ins->data[3]
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            auto thread = static_cast<CV::DataThread*>(threadf);

            // Root the thread handle in the thread context so GC keeps it alive
            if(!tctx->isDataIn(thread->id)){
                tctx->setData(prog, thread->id);
            }

            // Cache the handle so this instruction always returns the same thread object
            prog->setPrefetch(ins->id, thread->id);

            // Start native thread now
            thread->setState(CV::ThreadState::STARTED);

            auto handle = std::make_shared<InvokeThread>();
            handle->threadDataId = thread->id;
            handle->prog = prog;
            handle->upcursor = cursor;
            handle->root = ins->token;

            {
                std::lock_guard<std::mutex> lock(prog->mutexThreads);
                prog->threads[threadId] = std::thread(&RUN_THREAD, handle);
            }

            return static_cast<CV::Data*>(thread);
        } break;
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
            
            if(ins->params.size() == 0){
                auto dataId = ins->data[2];
                data = prog->getData(dataId);
                if(!data){
                    cursor->setError(
                        CV_ERROR_MSG_ILLEGAL_PREFIXER,
                        "Named instruction points to NULL DATA",
                        ins->token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();                    
                }
                data->incRefCount();
            }else            
            if(ins->params.size() > 0){ 
                auto &name = ins->literal[0];
                auto &entrypoint = prog->getIns(ins->params[0]);
                auto dataId = ins->data[2];

                data = CV::Execute(entrypoint, prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return data;
                }
                data->incRefCount();
                ctx->setData(prog, data->id);
                prog->swapId(dataId, data->id);
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

            CV::Data *v;
            
            if(ins->params.size() == 0){
                auto dataId = ins->data[0];
                v = prog->getData(dataId);
                v->incRefCount();
                return v;
            }else{            
                auto &name = ins->literal[0];
                auto &entrypoint = prog->getIns(ins->params[0]);
                auto dataId = ins->data[0];

                v = CV::Execute(entrypoint, prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return v;
                }
                v->incRefCount();
                ctx->setData(prog, v->id);
                prog->swapId(dataId, v->id);
            }

            prog->setPrefetch(ins->id, v->id);

            return v;
        };
        case CV::InstructionType::CARBON_COPY: {
            if(prog->isPrefetched(ins->id)){
                return prog->getData(prog->getPrefetch(ins->id));
            }

            auto &entrypoint = prog->getIns(ins->params[0]);

            auto data = CV::Execute(entrypoint, prog, cursor, ctx, st);
            if(cursor->error){
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            auto copied = CV::Copy(prog, data);
            prog->setPrefetch(ins->id, copied->id);

            return copied;
        } break;
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
        case CV::InstructionType::CF_AWAIT: {
            CV::Data *result = prog->buildNil();

            for(int i = 0; i < ins->data.size(); ++i){
                auto entrypoint = prog->getIns(ins->data[i]);
                if(!entrypoint){
                    cursor->setError(
                        CV_ERROR_MSG_INVALID_SYNTAX,
                        "Await instruction references missing entrypoint",
                        ins->token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }

                auto data = CV::Execute(entrypoint, prog, cursor, ctx, st);
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }

                if(data == NULL){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Imperative 'await' was provided a NULL operand (not-nil)",
                        ins->token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }

                if(data->type != CV::DataType::THREAD){
                    continue;
                }                

                auto thread = static_cast<CV::DataThread*>(data);

                while(true){
                    int state;
                    {
                        std::lock_guard<std::mutex> lock(thread->accessMutex);
                        state = thread->state;
                    }

                    if(state == CV::ThreadState::FINISHED){
                        break;
                    }

                    CV::Tools::sleep(30);
                }

                if(thread->returnId > 0){
                    auto payload = prog->getData(thread->returnId);
                    if(payload != NULL){
                        result = payload;
                    }
                }
            }

            return result;
        } break;
        case CV::InstructionType::CF_INVOKE_FUNCTION: {

            auto fn = static_cast<CV::DataFunction*>(prog->getData( ins->data[0] ));
            auto paramCtx = prog->createContext(fn->closureCtxId); 
            auto cleanupCtx = [&](){
                if(paramCtx){
                    prog->deleteContext(paramCtx->id);
                    paramCtx.reset();
                }
            };

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
                        cleanupCtx();
                        return prog->buildNil();                        
                    }
                }
            }

            CV::Data *result;

            // Execute
            if(fn->isBinary){
                CV::Data *v;
                if(fn->blambda != nullptr){
                    v = fn->blambda(
                        prog,
                        ins->literal[0],
                        allParams,
                        ins->token,
                        cursor,
                        paramCtx,
                        st
                    );
                }else{
                    v = fn->lambda(
                        prog,
                        ins->literal[0],
                        allParams,
                        ins->token,
                        cursor,
                        paramCtx,
                        st
                    );
                }
                if(cursor->error){
                    st->state = CV::ControlFlowState::CRASH;
                    cleanupCtx();
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
                            cleanupCtx();
                            return prog->buildNil();
                        }                            
                        v->incRefCount();
                        variadic->value.push_back(v->id);
                    }                    
                }else{
                    for(int i = 0; i < allParams.size(); ++i){
                        auto &name = allParams[i].first;
                        auto &cins = allParams[i].second;
                        auto v = CV::Execute(cins, prog, cursor, paramCtx, st);
                        if(cursor->error){
                            cleanupCtx();
                            return prog->buildNil();
                        }                            
                        if(!paramCtx->isName(name)){
                            paramCtx->setName(name, v->id);
                        }else
                        if(!paramCtx->isDataIn(v->id)){
                            paramCtx->setData(prog, v->id);
                        }
                    }
                }
                // Compile body
				auto entrypoint = CV::Compile(fn->body, prog, cursor, paramCtx);
				if(cursor->error){
                    cleanupCtx();
                    return prog->buildNil();
				}
				// Execute
				auto v = CV::Execute(entrypoint, prog, cursor, paramCtx, st);
				if(cursor->error){
                    cleanupCtx();
                    return prog->buildNil();
				}
                result = v;
            }

            cleanupCtx();
        
            return result;
        } break;
        /*
            LIBRARY IMPORT
        */
        case CV::InstructionType::LIBRAY_IMPORT: {
            return prog->buildNumber(ins->data[1]); // returns library id handle            
        } break;
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
    if(this->data.count(dataId) != 0){
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
        prog->deleteContext(it);
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

void CV::Context::registerFunctionPtr(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const std::vector<std::string> &params,
    const CV::LambdaPtr lambda
){
    auto fn = new CV::DataFunction();
    prog->allocateData(fn);

    fn->params = params;
    fn->isBinary = true;
    fn->isVariadic = false;
    fn->blambda = lambda;

    this->setData(prog, fn->id);
    this->setName(name, fn->id);
}

void CV::Context::registerFunctionPtr(
    const std::shared_ptr<CV::Program> &prog,
    const std::string &name,
    const CV::LambdaPtr lambda
){
    auto fn = new CV::DataFunction();
    prog->allocateData(fn);

    fn->isVariadic = true;
    fn->isBinary = true;
    fn->blambda = lambda;

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

bool CV::Program::deleteContext(int id){
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
    // std::cerr << "[gc] deallocate id=" << id
    //           << " type=" << static_cast<int>(ref->type)
    //           << " ref=" << ref->getRefCount()
    //           << "\n";    

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
    this->shuttingDown = false;
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


void CV::Program::deleteThread(unsigned id){
    this->mutexThreads.lock();
    if(this->threads.count(id) > 0){
        auto &thread = this->threads[id];
        thread.join();
        this->threads.erase(id);
    }
    this->mutexThreads.unlock();
}

void CV::Program::end(){
    this->shuttingDown = true;
    // Clear root
    if(this->root){
        this->root->clear(shared_from_this());
        this->root = NULL;
    }
    // Clear threads
    this->mutexThreads.lock();
    for(auto &it : this->threads){
        it.second.join();
    }
    this->threads.clear();
    this->mutexThreads.unlock();     
    std::vector<int> trailing;
    // Clear other contexts
    this->mutexContext.lock();
    for(auto &it : this->contexts){
        trailing.push_back(it.first);
    }
    this->mutexContext.unlock();
    for(int i = 0; i < trailing.size(); ++i){
        this->deleteContext(trailing[i]);
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

CV::DataNumber* CV::Program::buildNumber(CV_NUMBER n){
    auto number = new CV::DataNumber(n);
    this->allocateData(number);
    return number;
}

void CV::Program::quickGC(){
    
    this->mutexThreads.lock();
    for(auto &it : this->threads){
        it.second.join();
    }
    this->threads.clear();
    this->mutexThreads.unlock();   

    std::vector<int> trailing;
    // Clear Stack
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
//  Data Object Tools
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CV::Data *CV::Copy(
    const std::shared_ptr<CV::Program> &prog,
    CV::Data *subject
){
    if(subject == NULL){
        return prog->buildNil();
    }

    switch(subject->type){
        case CV::DataType::NUMBER: {
            auto orig = static_cast<CV::DataNumber*>(subject);
            auto copy = new CV::DataNumber(orig->value);
            prog->allocateData(copy);
            return static_cast<CV::Data*>(copy);
        }

        case CV::DataType::STRING: {
            auto orig = static_cast<CV::DataString*>(subject);
            auto copy = new CV::DataString(orig->value);
            prog->allocateData(copy);
            return static_cast<CV::Data*>(copy);
        }

        case CV::DataType::LIST: {
            auto orig = static_cast<CV::DataList*>(subject);
            auto copy = new CV::DataList();
            prog->allocateData(copy);

            for(int i = 0; i < orig->value.size(); ++i){
                auto child = prog->getData(orig->value[i]);
                if(child == NULL){
                    copy->value.push_back(-1);
                    continue;
                }

                auto childCopy = CV::Copy(prog, child);

                childCopy->incRefCount();
                copy->value.push_back(childCopy->id);
            }

            return static_cast<CV::Data*>(copy);
        }

        case CV::DataType::STORE: {
            auto orig = static_cast<CV::DataStore*>(subject);
            auto copy = new CV::DataStore();
            prog->allocateData(copy);

            for(const auto &it : orig->value){
                auto child = prog->getData(it.second);
                if(child == NULL){
                    copy->value[it.first] = -1;
                    continue;
                }

                auto childCopy = CV::Copy(prog, child);

                childCopy->incRefCount();
                copy->value[it.first] = childCopy->id;
            }

            return static_cast<CV::Data*>(copy);
        }

        case CV::DataType::FUNCTION: {
            auto orig = static_cast<CV::DataFunction*>(subject);
            auto copy = new CV::DataFunction();
            prog->allocateData(copy);

            copy->isBinary = orig->isBinary;
            copy->isVariadic = orig->isVariadic;
            copy->lambda = orig->lambda;
            copy->body = orig->body;
            copy->params = orig->params;

            return static_cast<CV::Data*>(copy);
        }

        case CV::DataType::THREAD: {
            auto copy = new CV::DataThread();
            prog->allocateData(copy);

            auto orig = static_cast<CV::DataThread*>(subject);

            copy->entrypointInsId = orig->entrypointInsId;
            copy->ctxId = orig->ctxId;
            copy->state = orig->state;
            copy->returnId = 0;
            if(orig->returnId != 0){
                auto data = prog->getData(copy->returnId);
                if(data){
                    copy->returnId = orig->returnId;
                    data->incRefCount();
                }
            }

            return static_cast<CV::Data*>(copy);
        }        

        default:
        case CV::DataType::NIL: {
            return prog->buildNil();
        }        
    }
}

bool CV::GetBooleanValue(CV::Data *input){
    if(!input){
        return false;
    }
    switch(input->type){
        case CV::DataType::NUMBER: {
            return static_cast<CV::DataNumber*>(input)->value != 0;
        };
        case CV::DataType::NIL: {
            return false;
        };
        default: {
            return true;
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  CORE
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CV::SetupCore(const CV::ProgramType &prog){
    // Set up PATH to look up libraries
    char *cvLibPath = std::getenv("CANVAS_LIB_HOME");
    CV_LIB_HOME = cvLibPath != nullptr ? std::string(cvLibPath) : "./lib";

    auto UnwrapNamedInstruction = [](const CV::ProgramType &prog, const CV::InsType &ins){
        return ins->type == CV::InstructionType::PROXY_NAMER ? prog->getIns(ins->params[0]) : ins;
    };

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
                if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, st, token, CV::DataType::NUMBER)){
                    return prog->buildNil();
                }
                result->value += static_cast<CV::DataNumber*>(v)->value;
            }

            return static_cast<CV::Data*>(result);
        }
    );

    prog->root->registerFunction(prog, "-",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st             
        ) -> CV::Data* {

            if(args.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'"+name+"' expects at least (1) argument",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            auto result = new CV::DataNumber();
            prog->allocateData(result);            

            // Execute arg0 to get its value so we start substracting from it
            auto &arg0 = args[0].second;
            auto v0 = CV::Execute(arg0, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            } 
            if(!CV::Tools::ErrorCheck::AreAllType(name, {v0}, cursor, st, token, CV::DataType::NUMBER)){
                return prog->buildNil();
            }  
            result->value = static_cast<CV::DataNumber*>(v0)->value;

            // Then we go through the rest substracting from arg0
            for(int i = 1; i < args.size(); ++i){
                auto &arg = args[i].second;
                auto v = CV::Execute(arg, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                } 
                if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, st, token, CV::DataType::NUMBER)){
                    return prog->buildNil();
                }
                result->value -= static_cast<CV::DataNumber*>(v)->value;
            }

            return static_cast<CV::Data*>(result);
        }
    );    

    prog->root->registerFunction(prog, "*",
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
                if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, st, token, CV::DataType::NUMBER)){
                    return prog->buildNil();
                }
                result->value *= static_cast<CV::DataNumber*>(v)->value;
            }

            return static_cast<CV::Data*>(result);
        }
    );

    prog->root->registerFunction(prog, "/",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st             
        ) -> CV::Data* {

            if(args.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'"+name+"' expects at least (1) argument",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            auto result = new CV::DataNumber();
            prog->allocateData(result);            

            // Execute arg0 to get its value so we start substracting from it
            auto &arg0 = args[0].second;
            auto v0 = CV::Execute(arg0, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            } 
            if(!CV::Tools::ErrorCheck::AreAllType(name, {v0}, cursor, st, token, CV::DataType::NUMBER)){
                return prog->buildNil();
            }  
            result->value = static_cast<CV::DataNumber*>(v0)->value;

            // Then we go through the rest substracting from arg0
            for(int i = 1; i < args.size(); ++i){
                auto &arg = args[i].second;
                auto v = CV::Execute(arg, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                } 
                if(!CV::Tools::ErrorCheck::AreAllType(name, {v}, cursor, st, token, CV::DataType::NUMBER)){
                    return prog->buildNil();
                }
                auto tv = static_cast<CV::DataNumber*>(v)->value;
                if(tv == 0){
                    cursor->setError(
                        CV_ERROR_MSG_MISUSED_FUNCTION,
                        "'"+name+"' Division by zero at argument ("+std::to_string(i)+")",
                        token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();                        
                }
                result->value /= tv;
            }

            return static_cast<CV::Data*>(result);
        }
    );      


    ////////////////////////////
    //// BOOLEAN
    ///////////////////////////  
    prog->root->registerFunction(prog, "and",
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

            result->value = 1;

            for(int i = 0; i < args.size(); ++i){
                auto &arg = args[i].second;
                auto v = CV::Execute(arg, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }

                if(!CV::GetBooleanValue(v)){
                    result->value = 0;
                    break;
                }
            }

            return static_cast<CV::Data*>(result);
        }
    );
    prog->root->registerFunction(prog, "not",
        {
            "value"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto param0 = CV::Tools::ErrorCheck::SolveInstruction(
                prog,
                name,
                "value",
                args,
                ctx,
                cursor,
                st,
                token,
                CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto result = new CV::DataNumber();
            prog->allocateData(result);

            result->value = !static_cast<CV::DataNumber*>(param0)->value;

            return static_cast<CV::Data*>(result);
        }
    ); 

    ////////////////////////////
    //// CONDITIONALS
    ///////////////////////////  
    auto __cv_register_numeric_conditional = [&](
        const std::string &fname,
        auto comparator
    ){

        prog->root->registerFunction(prog, fname,
            {
                "a",
                "b"
            },
            [&, comparator](
                const std::shared_ptr<CV::Program> &prog,
                const std::string &name,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
                const std::shared_ptr<CV::Token> &token,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::Context> &ctx,
                const std::shared_ptr<CV::ControlFlow> &st
            ) -> CV::Data* {

                auto param0 = CV::Tools::ErrorCheck::SolveInstruction(
                    prog, name, "a", args, ctx, cursor, st, token, CV::DataType::NUMBER
                );
                if(cursor->error){
                    return prog->buildNil();
                }

                auto param1 = CV::Tools::ErrorCheck::SolveInstruction(
                    prog, name, "b", args, ctx, cursor, st, token, CV::DataType::NUMBER
                );
                if(cursor->error){
                    return prog->buildNil();
                }

                auto a = static_cast<CV::DataNumber*>(param0)->value;
                auto b = static_cast<CV::DataNumber*>(param1)->value;

                bool ok = comparator(a, b);

                auto result = new CV::DataNumber();
                prog->allocateData(result);
                result->value = ok ? 1 : 0;

                return static_cast<CV::Data*>(result);
            }
        );
    };
    __cv_register_numeric_conditional("eq",  [](auto a, auto b){ return a == b; });
    __cv_register_numeric_conditional("neq", [](auto a, auto b){ return a != b; });
    __cv_register_numeric_conditional(">",   [](auto a, auto b){ return a >  b; });
    __cv_register_numeric_conditional(">=",  [](auto a, auto b){ return a >= b; });
    __cv_register_numeric_conditional("<",   [](auto a, auto b){ return a <  b; });
    __cv_register_numeric_conditional("<=",  [](auto a, auto b){ return a <= b; });
    prog->root->registerFunction(prog, "if",
        {
            "condition",
            "true-branch",
            "false-branch"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto condiIns = CV::Tools::ErrorCheck::FetchInstruction(
                name, "condition", args, cursor, st, token
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto trueBranchIns = CV::Tools::ErrorCheck::FetchInstruction(
                name, "true-branch", args, cursor, st, token
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto falseBranchIns = CV::Tools::ErrorCheck::FetchInstructionIfExists(
                "false-branch", args
            );

            auto condi = CV::Execute(condiIns, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            }

            if(CV::GetBooleanValue(condi)){
                auto trueBranch = CV::Execute(trueBranchIns, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }
                return trueBranch;
            }

            if(falseBranchIns){
                auto falseBranch = CV::Execute(falseBranchIns, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }
                return falseBranch;
            }

            return prog->buildNil();
        }
    );    

    ////////////////////////////
    //// LISTS / STORES
    ///////////////////////////  
    prog->root->registerFunction(prog, "nth",
        {
            "list",
            "index"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st             
        ) -> CV::Data* {

            auto param0 = CV::Tools::ErrorCheck::SolveInstruction(prog,name,"list",args,ctx,cursor,st,token,CV::DataType::LIST);
            if(cursor->error){
                return prog->buildNil();
            } 
            auto list = static_cast<CV::DataList*>(param0);

            auto param1 = CV::Tools::ErrorCheck::SolveInstruction(prog,name,"index",args,ctx,cursor,st,token,CV::DataType::NUMBER);
            if(cursor->error){
                return prog->buildNil();
            } 
            auto index = static_cast<int>(static_cast<CV::DataNumber*>(param1)->value);
            
            if(index < 0 || index >= list->value.size()){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Provided out-of-bounds index: Expected 0 to "+std::to_string(list->value.size()-1)+", provided "+std::to_string(index),
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            return prog->getData(list->value[index]);
        }
    ); 
    prog->root->registerFunction(prog, "length",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto ins = CV::Tools::ErrorCheck::FetchInstruction(name, "subject", args, cursor, st, token);
            if(cursor->error){
                return prog->buildNil();
            }

            auto data = CV::Execute(ins, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            }

            auto result = new CV::DataNumber();
            prog->allocateData(result);

            if(data->type == CV::DataType::LIST){
                result->value = static_cast<double>(static_cast<CV::DataList*>(data)->value.size());
                return static_cast<CV::Data*>(result);
            }

            if(data->type == CV::DataType::STORE){
                result->value = static_cast<double>(static_cast<CV::DataStore*>(data)->value.size());
                return static_cast<CV::Data*>(result);
            }

            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE, provided "+CV::DataTypeName(data->type),
                token
            );
            st->state = CV::ControlFlowState::CRASH;
            return prog->buildNil();
        }
    );
    prog->root->registerFunction(prog, ">>",
        {
            "subject",
            "target"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto subjectIns = CV::Tools::ErrorCheck::FetchInstruction(name, "subject", args, cursor, st, token);
            if(cursor->error){
                return prog->buildNil();
            }

            auto targetIns = CV::Tools::ErrorCheck::FetchInstruction(name, "target", args, cursor, st, token);
            if(cursor->error){
                return prog->buildNil();
            }

            auto subject = CV::Execute(subjectIns, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            }

            auto targetData = CV::Execute(targetIns, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            }

            if(targetData->type == CV::DataType::LIST){
                auto target = static_cast<CV::DataList*>(targetData);
                target->value.push_back(subject->id);
                return static_cast<CV::Data*>(target);
            }

            if(targetData->type == CV::DataType::STORE){
                if(subject->type != CV::DataType::STORE){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Imperative '"+name+"' expects operand 'subject' to be STORE when operand 'target' is STORE, provided "+CV::DataTypeName(subject->type),
                        token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }

                auto target = static_cast<CV::DataStore*>(targetData);
                auto source = static_cast<CV::DataStore*>(subject);

                for(auto &it : source->value){
                    target->value[it.first] = it.second;
                }

                return static_cast<CV::Data*>(target);
            }

            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Imperative '"+name+"' expects operand 'target' to be LIST or STORE, provided "+CV::DataTypeName(targetData->type),
                token
            );
            st->state = CV::ControlFlowState::CRASH;
            return prog->buildNil();
        }
    );
    prog->root->registerFunction(prog, "<<",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto ins = CV::Tools::ErrorCheck::FetchInstruction(name, "subject", args, cursor, st, token);
            if(cursor->error){
                return prog->buildNil();
            }

            auto data = CV::Execute(ins, prog, cursor, ctx, st);
            if(cursor->error){
                return prog->buildNil();
            }

            if(data->type == CV::DataType::LIST){
                auto list = static_cast<CV::DataList*>(data);

                if(list->value.size() == 0){
                    return prog->buildNil();
                }

                int subjectId = list->value.back();
                list->value.pop_back();

                return prog->getData(subjectId);
            }

            if(data->type == CV::DataType::STORE){
                auto store = static_cast<CV::DataStore*>(data);

                if(store->value.size() == 0){
                    return prog->buildNil();
                }

                auto it = store->value.begin();
                int subjectId = it->second;
                store->value.erase(it);

                return prog->getData(subjectId);
            }

            cursor->setError(
                CV_ERROR_MSG_WRONG_OPERANDS,
                "Imperative '"+name+"' expects operand 'subject' to be LIST or STORE, provided "+CV::DataTypeName(data->type),
                token
            );
            st->state = CV::ControlFlowState::CRASH;
            return prog->buildNil();
        }
    );
    prog->root->registerFunction(prog, "l-sub",
        {
            "list",
            "from",
            "to"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto listData = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "list", args, ctx, cursor, st, token, CV::DataType::LIST
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto fromData = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "from", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto toData = CV::Tools::ErrorCheck::SolveInstructionIfExists(
                prog, name, "to", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto list = static_cast<CV::DataList*>(listData);
            int from = static_cast<int>(static_cast<CV::DataNumber*>(fromData)->value);

            int to = static_cast<int>(list->value.size()) - 1;
            if(toData){
                to = static_cast<int>(static_cast<CV::DataNumber*>(toData)->value);
            }

            auto result = new CV::DataList();
            prog->allocateData(result);

            if(from < 0){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Imperative '"+name+"' expects non-negative 'from' number",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            if(from >= static_cast<int>(list->value.size())){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Imperative '"+name+"' expects 'from' within bounds (size:"+std::to_string(list->value.size())+")",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            if(to < 0){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Imperative '"+name+"' expects non-negative 'to' number",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            if(to < from){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Imperative '"+name+"' expects 'to' to be greater than or equal to 'from'",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            if(to >= static_cast<int>(list->value.size())){
                cursor->setError(
                    CV_ERROR_MSG_INVALID_INDEX,
                    "Imperative '"+name+"' expects 'to' within bounds (size:"+std::to_string(list->value.size())+")",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();
            }

            for(int i = from; i <= to; ++i){
                result->value.push_back(list->value[i]);
            }

            return static_cast<CV::Data*>(result);
        }
    );   
    
    prog->root->registerFunction(prog, "l-splice",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            if(args.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'"+name+"' expects at least (1) argument",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            auto result = new CV::DataList();
            prog->allocateData(result);

            for(int i = 0; i < args.size(); ++i){
                auto data = CV::Execute(args[i].second, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }

                result->value.push_back(data->id);
            }

            return static_cast<CV::Data*>(result);
        }
    );
    prog->root->registerFunction(prog, "s-splice",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            if(args.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'"+name+"' expects at least (1) argument",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            auto result = new CV::DataStore();
            prog->allocateData(result);

            for(int i = 0; i < args.size(); ++i){
                const auto &vname = args[i].first;

                if(vname.size() == 0){
                    cursor->setError(
                        CV_ERROR_MSG_WRONG_OPERANDS,
                        "Imperative '"+name+"' expects every operand to have a name",
                        token
                    );
                    st->state = CV::ControlFlowState::CRASH;
                    return prog->buildNil();
                }

                auto data = CV::Execute(args[i].second, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }

                result->value[vname] = data->id;
            }

            return static_cast<CV::Data*>(result);
        }
    );
    ////////////////////////////
    //// MUTATORS
    ///////////////////////////     
    prog->root->registerFunction(prog, "++",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto subject = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "subject", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            ++static_cast<CV::DataNumber*>(subject)->value;
            return subject;
        }
    );
    prog->root->registerFunction(prog, "--",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto subject = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "subject", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            --static_cast<CV::DataNumber*>(subject)->value;

            return subject;
        }
    );
    prog->root->registerFunction(prog, "//",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto subject = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "subject", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            static_cast<CV::DataNumber*>(subject)->value /= static_cast<CV_DEFAULT_NUMBER_TYPE>(2.0);

            return subject;
        }
    );

    prog->root->registerFunction(prog, "**",
        {
            "subject"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            auto subject = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "subject", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto value = static_cast<CV::DataNumber*>(subject)->value;
            static_cast<CV::DataNumber*>(subject)->value = value * value;

            return subject;
        }
    );   
    ////////////////////////////
    //// LOOPS
    ///////////////////////////  
    prog->root->registerFunction(prog, "while",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            if(args.size() == 0){
                cursor->setError(
                    CV_ERROR_MSG_MISUSED_FUNCTION,
                    "'"+name+"' expects at least (1) argument",
                    token
                );
                st->state = CV::ControlFlowState::CRASH;
                return prog->buildNil();                
            }

            CV::Data *result = nullptr;

            while(true){
                auto deepExecCtx = prog->createContext(ctx->id);

                auto condBranch = CV::Execute(args[0].second, prog, cursor, deepExecCtx, st);
                if(cursor->error){
                    prog->deleteContext(deepExecCtx->id);
                    return prog->buildNil();
                }

                // RETURN breaks flow upwards
                if(st->state == CV::ControlFlowState::RETURN){
                    result = st->payload;
                    prog->deleteContext(deepExecCtx->id);
                    break;
                }

                // SKIP skips the current iteration
                if(st->state == CV::ControlFlowState::SKIP){
                    st->state = CV::ControlFlowState::CONTINUE;
                    prog->deleteContext(deepExecCtx->id);
                    continue;
                }

                // YIELD breaks flow
                if(st->state == CV::ControlFlowState::YIELD){
                    result = st->payload;
                    prog->deleteContext(deepExecCtx->id);
                    break;
                }

                if(!CV::GetBooleanValue(condBranch)){
                    prog->deleteContext(deepExecCtx->id);
                    break;
                }

                bool shouldBreak = false;
                bool shouldContinue = false;

                for(int i = 1; i < args.size(); ++i){
                    result = CV::Execute(args[i].second, prog, cursor, deepExecCtx, st);
                    if(cursor->error){
                        prog->deleteContext(deepExecCtx->id);
                        return prog->buildNil();
                    }

                    if(st->state == CV::ControlFlowState::RETURN){
                        result = st->payload;
                        shouldBreak = true;
                        break;
                    }

                    if(st->state == CV::ControlFlowState::SKIP){
                        st->state = CV::ControlFlowState::CONTINUE;
                        shouldContinue = true;
                        break;
                    }

                    if(st->state == CV::ControlFlowState::YIELD){
                        result = st->payload;
                        shouldBreak = true;
                        break;
                    }
                }

                prog->deleteContext(deepExecCtx->id);

                if(shouldBreak){
                    break;
                }

                if(shouldContinue){
                    continue;
                }
            }

            if(result == nullptr){
                return prog->buildNil();
            }

            return result;
        }
    );
    prog->root->registerFunction(prog, "for",
        {
            "from",
            "to",
            "step",
            "body"
        },
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            CV::Data *result = nullptr;

            auto fromData = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "from", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto toData = CV::Tools::ErrorCheck::SolveInstruction(
                prog, name, "to", args, ctx, cursor, st, token, CV::DataType::NUMBER
            );
            if(cursor->error){
                return prog->buildNil();
            }

            auto stepIns = CV::Tools::ErrorCheck::FetchInstructionIfExists("step", args);
            if(stepIns){
                stepIns = UnwrapNamedInstruction(
                    prog,
                    stepIns
                );
            }

            auto bodyIns = CV::Tools::ErrorCheck::FetchInstructionIfExists("body", args);
            if(bodyIns){
                bodyIns = UnwrapNamedInstruction(
                    prog,
                    bodyIns
                );
            }            

            auto from = static_cast<CV::DataNumber*>(fromData);
            auto to = static_cast<CV::DataNumber*>(toData);

            bool running = true;

            while(running && from->value != to->value){

                auto deepExecCtx = prog->createContext(ctx->id);

                bool shouldBreak = false;
                bool shouldContinue = false;

                if(bodyIns){
                    result = CV::Execute(bodyIns, prog, cursor, deepExecCtx, st);
                    if(cursor->error){
                        prog->deleteContext(deepExecCtx->id);
                        return prog->buildNil();
                    }

                    if(st->state == CV::ControlFlowState::RETURN){
                        running = false;
                        result = st->payload;
                        shouldBreak = true;
                    }else
                    if(st->state == CV::ControlFlowState::SKIP){
                        st->state = CV::ControlFlowState::CONTINUE;
                        shouldContinue = true;
                    }else
                    if(st->state == CV::ControlFlowState::YIELD){
                        running = false;
                        result = st->payload;
                        shouldBreak = true;
                    }
                }

                if(running){
                    if(stepIns){

                        auto stepResult = CV::Execute(stepIns, prog, cursor, deepExecCtx, st);
                        if(cursor->error){
                            prog->deleteContext(deepExecCtx->id);
                            return prog->buildNil();
                        }

                        (void)stepResult;

                        if(st->state == CV::ControlFlowState::RETURN){
                            running = false;
                            result = st->payload;
                            shouldBreak = true;
                        }else
                        if(st->state == CV::ControlFlowState::SKIP){
                            st->state = CV::ControlFlowState::CONTINUE;
                            shouldContinue = true;
                        }else
                        if(st->state == CV::ControlFlowState::YIELD){
                            running = false;
                            result = st->payload;
                            shouldBreak = true;
                        }
                    }else{
                        ++from->value;
                    }
                }

                prog->deleteContext(deepExecCtx->id);

                if(shouldBreak){
                    break;
                }

                if(shouldContinue){
                    continue;
                }
            }

            if(result == nullptr){
                return prog->buildNil();
            }

            return result;
        }
    );


    ////////////////////////////
    //// UTILS
    /////////////////////////// 
    prog->root->registerFunction(prog, "print",
        [&](
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st
        ) -> CV::Data* {

            std::string v;

            for(int i = 0; i < args.size(); ++i){
                auto arg = CV::Execute(args[i].second, prog, cursor, ctx, st);
                if(cursor->error){
                    return prog->buildNil();
                }

                if(arg->type == CV::DataType::STRING){
                    v += static_cast<CV::DataString*>(arg)->value;
                }else{
                    v += CV::DataToText(prog, arg);
                }
            }

            printf("%s\n", v.c_str());

            return prog->buildNil();
        }
    );    
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  API
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CV::SetUseColor(bool v){
    UseColorOnText = v;   
}
std::string CV::GetPrompt(){
    std::string start = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "[" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string cv = CV::Tools::setTextColor(Tools::Color::CYAN) + "~" + CV::Tools::setTextColor(Tools::Color::RESET);
    std::string end = CV::Tools::setTextColor(Tools::Color::MAGENTA) + "]" + CV::Tools::setTextColor(Tools::Color::RESET);
    return start+cv+end;
}

int CV::Import(
    const std::string &fname,
    const std::shared_ptr<CV::Program> &prog,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::Cursor> &cursor
){
    if(!CV::Tools::fileExists(fname)){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "'"+fname+"' does not exist"
        );
        return 0;
    }

    auto literal = CV::Tools::readFile(fname);

    auto entrypoint = CV::Compile(literal, prog, cursor, ctx);
    if(cursor->error){
        return 0;
    }

    auto st = std::make_shared<CV::ControlFlow>();
    st->state = CV::ControlFlowState::CONTINUE;

    auto result = CV::Execute(entrypoint, prog, cursor, ctx, st);
    if(cursor->error){
        return 0;
    }

    if(result == NULL){
        return 0;
    }

    return result->id;
}

#define CV_IMPORT_LIBRARY_ENTRY_POINT_ARGS \
    const std::shared_ptr<CV::Program> &prog, \
    const std::shared_ptr<CV::Context> &ctx, \
    const std::shared_ptr<CV::Cursor> &cursor

int CV::ImportDynamicLibrary(
    const std::string &path,
    const std::string &fname,
    const std::shared_ptr<CV::Program> &prog,
    const std::shared_ptr<CV::Context> &ctx,
    const std::shared_ptr<CV::Cursor> &cursor
){
    using rlib = void (*)(CV_IMPORT_LIBRARY_ENTRY_POINT_ARGS);

#if (_CV_PLATFORM == _CV_PLATFORM_TYPE_LINUX) || (_CV_PLATFORM == _CV_PLATFORM_TYPE_OSX)

    void *handle = dlopen(path.c_str(), RTLD_LAZY);
    if(!handle){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to load dynamic library '"+fname+"': "+std::string(dlerror()),
            nullptr
        );
        return 0;
    }

    dlerror(); // clear previous error

    auto registerlibrary = reinterpret_cast<rlib>(dlsym(handle, "_CV_REGISTER_LIBRARY"));
    const char *error = dlerror();
    if(error){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to find '_CV_REGISTER_LIBRARY' in '"+fname+"': "+std::string(error),
            nullptr
        );
        dlclose(handle);
        return 0;
    }

    // Register the library into the current runtime
    (*registerlibrary)(prog, ctx, cursor);
    if(cursor->error){
        dlclose(handle);
        return 0;
    }

    auto id = GEN_ID();

    prog->loadedDynamicLibsMutex.lock();
    prog->loadedDynamicLibs[id] = {handle, path};
    prog->loadedDynamicLibsMutex.unlock();

    return id;

#elif (_CV_PLATFORM == _CV_PLATFORM_TYPE_WINDOWS)

    HMODULE hdll = LoadLibraryA(path.c_str());
    if(hdll == NULL){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to load dynamic library '"+fname+"'",
            nullptr
        );
        return 0;
    }

    auto entry = reinterpret_cast<rlib>(GetProcAddress(hdll, "_CV_REGISTER_LIBRARY"));
    if(entry == NULL){
        cursor->setError(
            CV_ERROR_MSG_LIBRARY_NOT_VALID,
            "Failed to find '_CV_REGISTER_LIBRARY' in '"+fname+"'",
            nullptr
        );
        FreeLibrary(hdll);
        return 0;
    }

    entry(prog, ctx, cursor);
    if(cursor->error){
        FreeLibrary(hdll);
        return 0;
    }

    auto id = GEN_ID();

    prog->loadedDynamicLibsMutex.lock();
    prog->loadedDynamicLibs[id] = {hdll, path};
    prog->loadedDynamicLibsMutex.unlock();

    return id;

#else

    cursor->setError(
        CV_ERROR_MSG_LIBRARY_NOT_VALID,
        "Dynamic libraries are not supported on this platform",
        nullptr
    );
    return 0;

#endif
}