#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <cmath>
#include <mutex> 
#include <chrono>

#include "CV.hpp"

static std::mutex genIdMutex;
static std::mutex gJobsMutex;
static unsigned lastGenId = 0;
static bool useColorOnText = false;
static const bool ShowHEADRef = false;

static const std::string GENERIC_UNDEFINED_SYMBOL_ERROR = "Undefined constructor, operator/name within this context or above, or type.";


static std::string FunctionToText(CV::Function *fn);
// static std::shared_ptr<CV::Item> processPreInterpretModifiers(std::shared_ptr<CV::Item> &item, std::vector<CV::ModifierPair> &modifiers, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects);
static void processPostInterpretExpandListOrContext(std::shared_ptr<CV::Item> &solved, CV::ModifierEffect &effects, std::vector<std::shared_ptr<CV::Item>> &items, std::shared_ptr<CV::Cursor> &cursor, const std::shared_ptr<CV::Context> &ctx);
static std::shared_ptr<CV::Item> RunInstruction(std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);
static std::shared_ptr<CV::Item> Execute(std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);
static std::shared_ptr<CV::Item> unwrapInterrupt(const std::shared_ptr<CV::Item> &item);

static std::shared_ptr<CV::TokenByteCode> ParseInputToByteToken(const std::string &input, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool future = false);
static std::shared_ptr<CV::TokenByteCode> ParseInputToByteToken(CV::Token &input, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool future = false);
static std::shared_ptr<CV::TokenByteCode> ProcessToken(std::shared_ptr<CV::Stack> &program, CV::Token &token, const std::vector<CV::Token> &params, std::vector<std::shared_ptr<CV::ModifierPair>> &headMods, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool future = false);


static std::unordered_map<int, std::shared_ptr<CV::Job>> gjobs;

std::unordered_map<std::string, CV::Trait> GENERIC_TRAITS = {
    {"copy",
        CV::Trait("copy", CV::TraitType::SPECIFIC, [](std::shared_ptr<CV::Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
            if(subject->type == CV::ItemTypes::NIL){
                return subject;
            }
            effects.postEval = false;
            return std::static_pointer_cast<CV::Item>(subject->copy(true));
        })
    },
    {"solid",
        CV::Trait("solid", CV::TraitType::SPECIFIC, [](std::shared_ptr<CV::Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
            if(subject->type == CV::ItemTypes::NIL){
                return subject;
            }
            subject->solid = true;
            return subject;
        })
    },
    {"type",
        CV::Trait("type", CV::TraitType::SPECIFIC, [](std::shared_ptr<CV::Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
            if(subject->type == CV::ItemTypes::NIL){
                return subject;
            }
            return std::static_pointer_cast<CV::Item>( std::make_shared<CV::String>(  CV::ItemTypes::str(subject->type)  ));
        })
    }     
};

std::unordered_map<uint8_t, std::unordered_map<std::string, CV::Trait>> traits = {};
std::unordered_map<uint8_t, std::unordered_map<uint8_t, CV::Trait>> traitsAny = {};

static void registerGenericTraits(){
    std::vector<uint8_t> types = {
        CV::ItemTypes::NIL,
        CV::ItemTypes::NUMBER,
        CV::ItemTypes::STRING,
        CV::ItemTypes::LIST,
        CV::ItemTypes::FUNCTION,
        CV::ItemTypes::CONTEXT,
        CV::ItemTypes::JOB
    };
    for(int i = 0; i < types.size(); ++i){
        traits[types[i]] = std::unordered_map<std::string, CV::Trait>();
        for(auto &it : GENERIC_TRAITS){
            traits[types[i]][it.first] = it.second;
        }        
    }
}

static void registerTrait(uint8_t itemType, const std::string &name, const std::function<std::shared_ptr<CV::Item>(std::shared_ptr<CV::Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects)> &fn){
    CV::Trait trait;
    trait.type = CV::TraitType::SPECIFIC;
    trait.name = name;
    trait.action = fn;
    traits[itemType][name] = trait;
}

static void registerTrait(uint8_t itemType, const std::string &name, uint8_t traitType, const std::function<std::shared_ptr<CV::Item>(std::shared_ptr<CV::Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects)> &fn){
    CV::Trait trait;
    trait.type = traitType;
    trait.name = name;
    trait.action = fn;
    traitsAny[itemType][traitType] = trait;
}

static bool hasTrait(std::shared_ptr<CV::Item> &item, std::string &name){
    auto it = traits.find(item->type);
    if(it == traits.end()){
        return false;
    }
    auto itInner = it->second.find(name);
    if(itInner == it->second.end()){
        return false;
    }
    return true;
}   

static bool hasTrait(std::shared_ptr<CV::Item> &item, uint8_t traitType){
    auto it = traitsAny.find(item->type);
    if(it == traitsAny.end()){
        return false;
    }
    auto itInner = it->second.find(traitType);
    if(itInner == it->second.end()){
        return false;
    }
    return true;    
}


std::shared_ptr<CV::Item> CV::runTrait(std::shared_ptr<Item> &item, const std::string &name, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    auto &trait = traits.find(item->type)->second.find(name)->second;
    return trait.action( item, value, cursor, ctx, effects);
}

std::shared_ptr<CV::Item> CV::runTrait(std::shared_ptr<Item> &item, uint8_t type, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    return traitsAny.find(item->type)->second.find(type)->second.action(item, value, cursor, ctx, effects);
}    




static void gAddJob(int id, std::shared_ptr<CV::Job> &job){
    gJobsMutex.lock();
    gjobs[id] = job;
    gJobsMutex.unlock();
}

static std::shared_ptr<CV::Job> gGetJob(int id){
    gJobsMutex.lock();
    auto it = gjobs.find(id);
    if(it == gjobs.end()){
        gJobsMutex.unlock();
        return std::shared_ptr<CV::Job>(NULL);
    }
    auto job = it->second;
    gJobsMutex.unlock();
    return job;
}

static bool gKillJob(int id){
    gJobsMutex.lock();
    auto it = gjobs.find(id);
    if(it == gjobs.end()){
        gJobsMutex.unlock();
        return false;
    }
    it->second->setStatus(CV::JobStatus::DONE);
    gJobsMutex.unlock();   
    return true;
}

static bool gRemoveJob(int id){
    gJobsMutex.lock();
    auto it = gjobs.find(id);
    if(it == gjobs.end()){
        gJobsMutex.unlock();
        return false;
    }
    gjobs.erase(it);
    gJobsMutex.unlock();   
    return true;
}

static unsigned genId(){
    genIdMutex.lock();
    unsigned id = ++lastGenId;
    genIdMutex.unlock();
    return id;
}


void CV::setUseColor(bool v){
    useColorOnText = v;
}

static const std::vector<std::string> RESERVED_WORDS = {
    "top", "nil", "ct", "fn", "iter", "do", "if", "skip",
    "stop", "continue", "set", "with", "mut", "proxy", "async", "untether"
};

static const std::vector<std::string> RESERVED_SYMBOLS = {
    CV::ModifierTypes::str(CV::ModifierTypes::NAMER),
    CV::ModifierTypes::str(CV::ModifierTypes::SCOPE),
    CV::ModifierTypes::str(CV::ModifierTypes::TRAIT),
    CV::ModifierTypes::str(CV::ModifierTypes::EXPAND),
    "[", "]", "'", "#", ".", "\n", "$"
};

namespace CV {
    namespace Tools {

        uint64_t ticks(){
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        }

        namespace Color {
            enum Color : int {
                BLACK,
                RED,
                GREEN,
                YELLOW,
                BLUE,
                MAGENTA,
                CYAN,
                WHITE,
                RESET
            };
        }

        namespace UnixColor {

            static const std::unordered_map<int, std::string> TextColorCodes = {
                {0, "\e[0;30m"},
                {1, "\e[0;31m"},
                {2, "\e[0;32m"},
                {3, "\e[0;33m"},
                {4, "\e[0;34m"},
                {5, "\e[0;35m"},
                {6, "\e[0;36m"},
                {7, "\e[0;37m"},
                {8, "\e[0m"},
            };

            static const std::unordered_map<int, std::string> BoldTextColorCodes = {
                {0, "\e[1;30m"},
                {1, "\e[1;31m"},
                {2, "\e[1;32m"},
                {3, "\e[1;33m"},
                {4, "\e[1;34m"},
                {5, "\e[1;35m"},
                {6, "\e[1;36m"},
                {7, "\e[1;37m"},
                {8, "\e[0m"},
            };        

            static const std::unordered_map<int, std::string> BackgroundColorCodes = {
                {0, "\e[40m"},
                {1, "\e[41m"},
                {2, "\e[42m"},
                {3, "\e[43m"},
                {4, "\e[44m"},
                {5, "\e[45m"},
                {6, "\e[46m"},
                {7, "\e[47m"},
                {8, "\e[0m"},
            };     
        }   

        static std::string setTextColor(int color, bool bold = false){
            return useColorOnText ? (bold ? UnixColor::BoldTextColorCodes.find(color)->second : UnixColor::TextColorCodes.find(color)->second) : "";
        }

        static std::string setBackgroundColor(int color){
            return useColorOnText ? (UnixColor::BackgroundColorCodes.find(color)->second) : "";

        }    

        void sleep(uint64_t t){
            std::this_thread::sleep_for(std::chrono::milliseconds(t));
        }   

        static void printList(const std::vector<std::string> &list){ // for debugging
            std::cout << "START" << std::endl;
            for(int i = 0 ; i < list.size(); ++i){
                std::cout << "'" << list[i] << "'" << std::endl;
            }
            std::cout << "END\n" << std::endl;
        }

        static std::string removeExtraSpace(const std::string &in){
            std::string cpy;
            for(int i = 0; i < in.length(); ++i){
                if(i > 0 && in[i] == ' ' && in[i - 1] == ' '){
                    continue;
                }
                if((i == 0 || i == in.length() - 1) && in[i] == ' '){
                    continue;
                }
                cpy += in[i];
            }
            return cpy;
        }

        bool isLineComplete(const std::string &input){
            auto clean = Tools::removeExtraSpace(input);
            int open = 0;
            bool comm = false;
            bool str = false;
            for(int i = 0; i < input.length(); ++i){
                char c = input[i];
                // ++c;
                if(c == '[' && !str) ++open;
                if(c == ']' && !str) --open;
                if(c == '#' && !str){
                    comm = !comm;
                    continue;
                }
                if(comm){
                    continue;
                }     
                if(c == '\''){
                    str = !str;
                }   				
            }	
            return open == 0;		
        }

        static bool isNumber(const std::string &s){
            return (s.find_first_not_of( "-.0123456789") == std::string::npos) && (s != "-" && s != "--" && s != "+" && s != "++");
        }

        bool isReservedWord(const std::string &input){
            for(int i = 0; i < RESERVED_SYMBOLS.size(); ++i){
                if(input == RESERVED_SYMBOLS[i] || input.find(RESERVED_SYMBOLS[i]) != -1){
                    return true;
                }
            }            
            for(int i = 0; i < RESERVED_WORDS.size(); ++i){
                if(input == RESERVED_WORDS[i]){
                    return true;
                }
            }
            return false;
        }

        bool isValidVarName(const std::string &varname){
            return  !CV::Tools::isReservedWord(varname) && !CV::Tools::isNumber(varname);
        }

        static __CV_NUMBER_NATIVE_TYPE positive(__CV_NUMBER_NATIVE_TYPE n){
            return n < 0 ? n * -1.0 : n;
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
    }
}


std::string CV::getPrompt(){
    std::string start = Tools::setTextColor(Tools::Color::MAGENTA) + "[" + Tools::setTextColor(Tools::Color::RESET);
    std::string cv = Tools::setTextColor(Tools::Color::CYAN) + "~" + Tools::setTextColor(Tools::Color::RESET);
    std::string end = Tools::setTextColor(Tools::Color::MAGENTA) + "]" + Tools::setTextColor(Tools::Color::RESET);
    return start+cv+end+"> ";
}




CV::Cursor::Cursor(){
    clear();
    line = 1;
}

void CV::Cursor::clear(){
    error = false;
    message = "";
    line = 1;
}

void CV::Cursor::setError(const std::string &origin, const std::string &v, int line){
    if(line != -1){
        this->line = line;
    }
    this->error = true;
    this->message = (origin.size() > 0 ? "'"+getOrigin(origin)+"': " : "")+
                    Tools::setTextColor(Tools::Color::RED) +
                    v +
                    Tools::setTextColor(Tools::Color::RESET);
}

std::string CV::Cursor::getOrigin(const std::string &origin){
    return  Tools::setTextColor(Tools::Color::YELLOW) +
            origin +
            Tools::setTextColor(Tools::Color::RESET);
}
/*

    TRAIT

*/
CV::Trait::Trait(){
    this->type = CV::TraitType::SPECIFIC;
    this->action = [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        return std::shared_ptr<CV::Item>(NULL);
    };
}

/* 

    ITEM

*/
CV::Item::Item(){
    this->id = genId();
    this->copyable = true;
    this->solid = false;
    this->type = CV::ItemTypes::NIL;
}

bool CV::Item::isEq(std::shared_ptr<CV::Item> &item){
    return (this->type == CV::ItemTypes::NIL && item->type == CV::ItemTypes::NIL) || this == item.get();
}

std::shared_ptr<CV::Item> CV::Item::copy(bool deep){
    return std::make_shared<CV::Item>();
}

/*

    INTERRUPT

*/
CV::Interrupt::Interrupt(uint8_t intType){
    this->type = CV::ItemTypes::INTERRUPT;
    this->intType = intType;
}

std::shared_ptr<CV::Item> CV::Interrupt::copy(bool deep){
    auto item = std::make_shared<CV::Interrupt>(this->intType);
    if(this->hasPayload()){
        auto pl = this->getPayload();
        item->setPayload(pl);
    }
    return std::static_pointer_cast<CV::Item>(item);
}  

bool CV::Interrupt::hasPayload(){
    this->accessMutex.lock();
    auto v = this->payload.get() != NULL;
    this->accessMutex.unlock();
    return v;
}

void CV::Interrupt::setPayload(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    this->payload = item;
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::Interrupt::getPayload(){
    this->accessMutex.lock();
    auto v = this->payload;
    this->accessMutex.unlock();
    return v;
}


/*

    NUMBER

*/
CV::Number::Number(){
}

CV::Number::Number(__CV_NUMBER_NATIVE_TYPE v){
    set(v);
}

bool CV::Number::isEq(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    bool v = item->type == this->type && this->n == std::static_pointer_cast<CV::Number>(item)->get();
    this->accessMutex.unlock();
    return v;
}    

std::shared_ptr<CV::Item> CV::Number::copy(bool deep){
    accessMutex.lock();
    auto copy = std::make_shared<CV::Number>(this->n);
    accessMutex.unlock();
    return copy;
}    

void CV::Number::set(const __CV_NUMBER_NATIVE_TYPE v){
    this->accessMutex.lock();
    this->type = CV::ItemTypes::NUMBER;
    this->n = v;
    this->accessMutex.unlock();
}

__CV_NUMBER_NATIVE_TYPE CV::Number::get(){
    this->accessMutex.lock();
    auto v = this->n;
    this->accessMutex.unlock();
    return v;
}

/*

    LIST

*/
CV::List::List(){
}

CV::List::List(const std::vector<std::shared_ptr<CV::Item>> &list, bool toCopy){
    set(list, toCopy);
}

void CV::List::clear(){
    this->accessMutex.lock();
    this->data.clear();
    this->accessMutex.unlock();
}

void CV::List::append(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    this->data.push_back(item);
    this->accessMutex.unlock();
}

size_t CV::List::size(){
    this->accessMutex.lock();
    auto n = this->data.size();
    this->accessMutex.unlock();
    return n;
}

std::shared_ptr<CV::Item> CV::List::copy(bool deep){
    this->accessMutex.lock();
    std::vector<std::shared_ptr<Item>> toCopy;
    for(int i = 0; i < this->data.size(); ++i){
        toCopy.push_back(deep && this->data[i]->copyable ? this->data[i]->copy() : this->data[i]);
    }            
    this->accessMutex.unlock();
    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(toCopy));
}

void CV::List::set(const std::vector<std::shared_ptr<CV::Item>> &list, bool toCopy){
    this->accessMutex.lock();
    this->type = CV::ItemTypes::LIST;
    for(int i = 0; i < list.size(); ++i){
        this->data.push_back(toCopy && list[i]->copyable ? list[i]->copy() : list[i]);
    }        
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::List::get(size_t index){
    this->accessMutex.lock();
    auto v =  this->data[index];
    this->accessMutex.unlock();
    return v;
}

/*

    STRING

*/
CV::String::String(){
    // Empty constructor makes this string NIL
}

size_t CV::String::size(){
    this->accessMutex.lock();
    auto n = this->data.size();
    this->accessMutex.unlock();
    return n;
}

CV::String::String(const std::string &str){
    this->set(str);
}    

bool CV::String::isEq(std::shared_ptr<CV::Item> &item){
    if(this->type != item->type || this->size() != std::static_pointer_cast<CV::String>(item)->size()){
        return false;
    }
    auto v = this->data == std::static_pointer_cast<CV::String>(item)->get();
    return v;
} 

std::shared_ptr<CV::Item> CV::String::copy(bool deep){
    this->accessMutex.lock();
    auto copied = std::make_shared<CV::String>();
    copied->set(this->data);
    this->accessMutex.unlock();
    return copied;
}

void CV::String::set(const std::string &str){
    this->accessMutex.lock();
    this->type = CV::ItemTypes::STRING;
    this->data = str;
    this->accessMutex.unlock();
} 

std::string CV::String::get(){     
    this->accessMutex.lock();
    auto v = this->data;   
    this->accessMutex.unlock();
    return v;
}


/*

    FunctionConstraints

*/
CV::FunctionConstraints::FunctionConstraints(){
    enabled = true;
    allowNil = true;
    allowMisMatchingTypes = true;
    useMinParams = false;
    useMaxParams = false;
}

void CV::FunctionConstraints::setExpectType(uint8_t type){
    this->expectedTypes.push_back(type);
}

void CV::FunctionConstraints::setExpectedTypeAt(int pos, uint8_t type){
    this->expectedTypeAt[pos] = type;
}

void CV::FunctionConstraints::clearExpectedTypes(){
    this->expectedTypeAt.clear();
    this->expectedTypes.clear();
}

void CV::FunctionConstraints::setMaxParams(unsigned n){
    if(n == 0){
        useMaxParams = false;
        return;
    }
    enabled = true;
    maxParams = n;
    useMaxParams = true;
}

void CV::FunctionConstraints::setMinParams(unsigned n){
    if(n == 0){
        useMinParams = false;
        return;
    }
    enabled = true;
    minParams = n;
    useMinParams = true;
}

/*

    FUNCTION

*/
CV::Function::Function(){

}


CV::Function::Function(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx)> &fn, bool variadic){
    set(params, fn, variadic);
}    


std::shared_ptr<CV::Item> CV::Function::copy(bool deep) {
    this->accessMutex.lock();
    auto fn = std::make_shared<Function>(Function());
    fn->params = this->params;
    fn->binary = this->binary;
    fn->variadic = this->variadic;
    fn->threaded = false;
    fn->async = false;
    fn->type = this->type;
    fn->fn = this->fn;
    this->accessMutex.unlock();
    return std::static_pointer_cast<Item>(fn);
}

std::vector<std::string> CV::Function::getParams(){
    this->accessMutex.lock();
    auto copy = this->params;
    this->accessMutex.unlock();
    return copy;
}


void CV::Function::set(const std::vector<std::string> &params, std::string &body, bool variadic){
    this->accessMutex.lock();
    this->params = params;
    this->type = CV::ItemTypes::FUNCTION;
    this->body = body;
    this->variadic = variadic;
    this->binary = false;
    this->threaded = false;
    this->async = false;      
    this->accessMutex.unlock();     
}

void CV::Function::set(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx)> &fn, bool variadic){
    this->accessMutex.lock();
    this->params = params;
    this->type = CV::ItemTypes::FUNCTION;
    this->fn = fn;
    this->variadic = variadic;
    this->binary = true;
    this->threaded = false;
    this->async = false;      
    this->accessMutex.unlock();  
}

void CV::Function::set(std::shared_ptr<CV::TokenByteCode> &entry, bool variadic){
    this->accessMutex.lock();
    this->type = CV::ItemTypes::FUNCTION;
    this->entry = entry;
    this->variadic = variadic;
    this->binary = false;
    this->threaded = false;
    this->async = false;       
    this->accessMutex.unlock();
}

/*

CONTEXT

*/

std::string CV::Context::getNameById(unsigned id){
    for(auto &t : this->vars){
        if(t.second->id == id){
            return t.first;
        }
    }
    return "";
}



std::shared_ptr<CV::Item> CV::Context::getLocal(const std::string &name){
    this->accessMutex.lock();
    auto it = vars.find(name);
    if(it == vars.end()){
        this->accessMutex.unlock();
        return std::shared_ptr<CV::Item>(NULL);
    }
    auto v = it->second;
    this->accessMutex.unlock();
    return v;
}


std::shared_ptr<CV::Item> CV::Context::get(const std::string &name){
    this->accessMutex.lock();
    auto it = vars.find(name);
    if(it == vars.end()){
        this->accessMutex.unlock();
        auto v = this->upper && this->upper.get() != this ? this->upper->get(name) : std::shared_ptr<CV::Item>(NULL);
        return v;
    }
    auto v = it->second;
    this->accessMutex.unlock();
    return v;
}

std::shared_ptr<CV::Item> CV::Context::copy(bool deep){
    this->accessMutex.lock();
    auto item = std::make_shared<CV::Context>(this->upper);
    item->type = this->type;
    for(auto &it : this->vars){
        item->vars[it.first] = deep && it.second->copyable ? it.second->copy() : it.second;
    }        
    this->accessMutex.unlock();
    return item;
}

void CV::Context::setTop(std::shared_ptr<CV::Context> &nctx){
    this->top = nctx;
    this->set("top", nctx);
}

std::shared_ptr<CV::Job> CV::Context::getJobById(__CV_NUMBER_NATIVE_TYPE id){
    this->accessMutex.lock();
    for(int i = 0; i < this->jobs.size(); ++i){
        if(this->jobs[i]->id == id){
            auto v = this->jobs[i];
            this->accessMutex.unlock();
            return v;
        }
    }
    this->accessMutex.unlock();
    return std::shared_ptr<Job>(NULL);
}

static void ThreadedFunction(std::shared_ptr<CV::Job> job, std::shared_ptr<CV::Context> ctx){
    while(job->getStatus() == CV::JobStatus::IDLE){ CV::Tools::sleep(20); }
    auto last = std::make_shared<CV::Item>();
    while(job->getStatus() == CV::JobStatus::RUNNING){
        auto result = Execute(job->entry, job->program, ctx, job->cursor);
        if(job->cursor->error){
            job->setStatus(CV::JobStatus::DONE);
            std::cout << CV::Tools::setTextColor(CV::Tools::Color::GREEN)+"JOB["<< job->id << "]: "+CV::Tools::setTextColor(CV::Tools::Color::RESET) << job->cursor->message << std::endl;
            auto nil = std::make_shared<CV::Item>();
            job->setPayload(nil);
            break;
        }    
        if(result->type != CV::ItemTypes::INTERRUPT && std::static_pointer_cast<CV::Interrupt>(result)->intType != CV::InterruptTypes::CONTINUE){
            job->setStatus(CV::JobStatus::DONE);
            last = unwrapInterrupt(result);
        }        
    }
    job->setPayload(last);
    job->setStatus(CV::JobStatus::DONE);
}

static void AsyncFunction(std::shared_ptr<CV::Job> &job, std::shared_ptr<CV::Context> &ctx){
    auto result = Execute(job->entry, job->program, ctx, job->cursor);
    if(job->cursor->error){
        std::cout << CV::Tools::setTextColor(CV::Tools::Color::GREEN)+"JOB["<< job->id << "]: "+CV::Tools::setTextColor(CV::Tools::Color::RESET) << job->cursor->message << std::endl;
        job->setStatus(CV::JobStatus::DONE);
        auto nil = std::make_shared<CV::Item>();
        job->setPayload(nil);
    }
    if(result->type != CV::ItemTypes::INTERRUPT && std::static_pointer_cast<CV::Interrupt>(result)->intType != CV::InterruptTypes::CONTINUE){
        job->setStatus(CV::JobStatus::DONE);
    }
    if(job->getStatus() == CV::JobStatus::DONE){
        auto pl = unwrapInterrupt(result);
        job->setPayload(pl);
    }
}

std::shared_ptr<CV::DisplayItem> CV::Context::addDisplayItemFunction(const std::string &name, unsigned insId, unsigned argN, unsigned origin){
    this->accessMutex.lock();
    auto ditem = std::make_shared<CV::DisplayItem>();
    ditem->Function(insId, argN, origin);
    ditem->name = name;
    ditem->insType = CV::ByteCodeType::JMP_FUNCTION;
    this->displayItems[name] = ditem;
    this->accessMutex.unlock();
    return ditem;
}

std::shared_ptr<CV::DisplayItem> CV::Context::addDisplayItem(const std::string &name, std::shared_ptr<CV::DisplayItem> &ditem){
    this->accessMutex.lock();
    ditem->name = name;
    this->displayItems[name] = ditem;
    this->accessMutex.unlock();
    return this->displayItems[name];
}


std::shared_ptr<CV::DisplayItem> CV::Context::addDisplayItem(const std::string &name, unsigned insId, unsigned origin){
    this->accessMutex.lock();
    auto ditem = std::make_shared<CV::DisplayItem>();
    ditem->set(CV::ItemTypes::NIL, insId, origin);
    ditem->name = name;
    ditem->insType = CV::ByteCodeType::PROXY;
    this->displayItems[name] = ditem;
    this->accessMutex.unlock();
    return ditem;
}

bool CV::Context::removeDisplayItem(unsigned id){
    this->accessMutex.lock();
    for(auto it : displayItems){
        if(it.second->insId == id){
            displayItems.erase(it.first);
            this->accessMutex.unlock();
            return true;
        }
    }
    this->accessMutex.unlock();
    return false;
}

void CV::Context::flushDisplayItems(){
    this->accessMutex.lock();
    // downstream
    for(auto &it : this->staticValues){
        if(it.second->type == CV::ItemTypes::CONTEXT){
            std::static_pointer_cast<CV::Context>(it.second)->flushDisplayItems();
        }
    }
    for(auto &it : this->vars){
        if(it.second->type == CV::ItemTypes::CONTEXT && it.first != "top"){
            std::static_pointer_cast<CV::Context>(it.second)->flushDisplayItems();
        }
    }    
    this->displayItems.clear();
    this->accessMutex.unlock();
}

std::shared_ptr<CV::DisplayItem> CV::Context::findDisplayItem(const std::string &name){
    this->accessMutex.lock();
    auto it = this->displayItems.find(name);
    if(it != this->displayItems.end()){
        auto v = it->second;
        this->accessMutex.unlock();    
        return v;
    }    
    auto v = this->upper.get() ? this->upper->findDisplayItem(name) : std::shared_ptr<CV::DisplayItem>(NULL);
    this->accessMutex.unlock();
    return v;
}




void CV::Context::flushStaticValue(){
    this->accessMutex.lock();
    for(auto &it : this->staticValues){
        if(it.second->type == CV::ItemTypes::CONTEXT){
            std::static_pointer_cast<CV::Context>(it.second)->flushStaticValue();
        }
    }
    this->staticValues.clear();
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::Context::setProxiedValue(const std::shared_ptr<CV::Item> &item){
    // static values should only be stored in the top context
    if(this->temporary || this->upper.get()){
        return this->upper->setStaticValue(item);
    }
    this->accessMutex.lock();
    this->staticValues[item->id] = item;
    auto &v = this->staticValues[item->id];
    this->accessMutex.unlock();
    return v;    
}

std::shared_ptr<CV::Item> CV::Context::getProxiedValue(unsigned id){
    this->accessMutex.lock();
    auto it = this->staticValues.find(id);
    if(it != this->staticValues.end()){
        auto v = it->second;
        this->accessMutex.unlock();
        return v;
    }
    auto v = this->upper ? this->upper->getStaticValue(id) : std::shared_ptr<CV::Item>(NULL);
    this->accessMutex.unlock();
    return v;
}

std::shared_ptr<CV::Item> CV::Context::setStaticValue(const std::shared_ptr<CV::Item> &item){
    // static values should only be stored in the top context
    if(this->temporary || this->upper.get()){
        this->upper->setStaticValue(item);
    }
    this->accessMutex.lock();
    this->staticValues[item->id] = item;
    auto &v = this->staticValues[item->id];
    this->accessMutex.unlock();
    return v;
}

std::shared_ptr<CV::Item> CV::Context::getStaticValue(unsigned id){
    this->accessMutex.lock();
    auto it = this->staticValues.find(id);
    if(it != this->staticValues.end()){
        auto v = it->second;
        this->accessMutex.unlock();
        return v;
    }
    auto v = this->upper ? this->upper->getStaticValue(id) : std::shared_ptr<CV::Item>(NULL);
    this->accessMutex.unlock();
    return v;
}

unsigned CV::Context::getJobNumber(){
    this->accessMutex.lock();
    auto n = this->jobs.size();
    this->accessMutex.unlock();
    return n;
}

void CV::Context::addJob(std::shared_ptr<Job> &job){    
    this->accessMutex.lock();
    job->id = genId();
    gAddJob(job->id, job);
    this->jobs.push_back(job);




    this->accessMutex.unlock();
}

void CV::Context::startJobs(){
    this->accessMutex.lock();

    for(int i = 0; i < this->jobs.size(); ++i){
        auto &j = this->jobs[i];
        if(j->getStatus() == CV::JobStatus::CREATED){
            j->setStatus(CV::JobStatus::IDLE);
        }
    }
    // downstream
    for(auto &it : this->vars){
        if(it.second.get() == this) continue;
        if(it.second->type == CV::ItemTypes::CONTEXT && it.first != "top"){
            std::static_pointer_cast<CV::Context>(it.second)->startJobs();
        }
    }
    for(auto &it : this->staticValues){
        if(it.second->type == CV::ItemTypes::CONTEXT){
            std::static_pointer_cast<CV::Context>(it.second)->startJobs();
        }
    }
    
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::Job::copy(bool deep){
    // Job cannot be copied
    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Job>());
}

static int count = 0;
bool CV::ContextStep(std::shared_ptr<CV::Context> &ctx){
    
    bool busy = false;

    // Make a copy
    ctx->accessMutex.lock();
    std::vector<std::shared_ptr<CV::Job>> _jobs;
    for(int i = 0; i < ctx->jobs.size(); ++i){
        _jobs.push_back(ctx->jobs[i]);
    }   

    // for(int i = 0; i < ctx->jobs.size(); ++i){

    //     std::cout << ctx->id << " " << CV::ItemToText(ctx->jobs[i].get()) << std::endl;
    // }

    ctx->accessMutex.unlock();

    // Run jobs
    std::vector<__CV_NUMBER_NATIVE_TYPE> done;
    for(int i = 0; i < _jobs.size(); ++i){
        auto &job = _jobs[i];
        if(job->getStatus() == CV::JobStatus::CREATED){
            job->setStatus(CV::JobStatus::IDLE);                    
            continue;
        }
        if(job->getStatus() != CV::JobStatus::DONE && job->ready){
            if(job->getStatus() == JobStatus::IDLE){
                if(job->jobType == CV::JobType::THREAD){
                    job->thread = std::thread(&ThreadedFunction, job, ctx);
                }
                job->setStatus(CV::JobStatus::RUNNING);                    
            }
            if(job->jobType == CV::JobType::ASYNC){
                AsyncFunction(job, ctx);
            }
        }else
        if(job->getStatus() == CV::JobStatus::DONE){
            done.push_back(job->id);
        }
    }

    auto runCb = [&](std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Job> &job){
        auto cb = job->callback;
        if(cb->isCallback){
            auto nctx = std::make_shared<CV::Context>(ctx);
            auto &ins = job->program->instructions[job->inherited];
            auto pl = job->getPayload();
            ctx->setStaticValue(pl);
            ins->data[0] = pl->id;
            cb->ready = true;
        }else{
            cb->cursor->setError("JOB|CALLBACK", "Non-Callback Job '"+CV::Tools::removeTrailingZeros(cb->id)+"' attached as Callback.");
            cb->setStatus(JobStatus::DONE);
        }
    };

    // Delete finished jobs
    ctx->accessMutex.lock();
    for(int i = 0; i < done.size(); ++i){
        for(int j = 0; j < ctx->jobs.size(); ++j){
            auto &job = ctx->jobs[j];
            if(job->id != done[i]){
                continue;
            }
            if(job->jobType == JobType::THREAD){
                job->thread.join();
            }
            /*
                RUN CALL BACK                
            */
            if(job->callback.get()){
                ctx->accessMutex.unlock();
                runCb(ctx, job);
                ctx->accessMutex.lock();
            }
            /*
                Then delete
            */
            gRemoveJob(job->id);
            ctx->jobs.erase(ctx->jobs.begin() + j);
            break;            
        }
    }
    busy = ctx->jobs.size() > 0;
    ctx->accessMutex.unlock();

    // Run children
    ctx->accessMutex.lock();
    for(auto &it : ctx->vars){
        auto &var = it.second;
        if(var->type == CV::ItemTypes::CONTEXT && it.first != "top"){
            auto cctx = std::static_pointer_cast<Context>(var);
            ctx->accessMutex.unlock();
            busy = CV::ContextStep(cctx) || busy;
            ctx->accessMutex.lock();

        }
    }  
    for(auto &it : ctx->staticValues){
        auto &var = it.second;
        if(var->type == CV::ItemTypes::CONTEXT){
            auto cctx = std::static_pointer_cast<Context>(var);
            ctx->accessMutex.unlock();
            busy = CV::ContextStep(cctx) || busy;
            ctx->accessMutex.lock();

        }
    }    
    ctx->accessMutex.unlock();

    return busy;
}

void CV::Job::setCallback(std::shared_ptr<CV::Job> &cb){
    this->accessMutex.lock();
    this->callback = cb;
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::Job::getPayload(){
    this->accessMutex.lock();
    auto v = this->payload;
    this->accessMutex.unlock();
    return v;
}

bool CV::Job::hasPayload(){
    this->accessMutex.lock();
    auto v = this->payload.get();
    this->accessMutex.unlock();
    return v;
}

void CV::Job::setPayload(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    this->payload = item;
    this->accessMutex.unlock();
}

void CV::Job::setStatus(int nstatus){
    this->accessMutex.lock();
    this->status = nstatus;
    this->accessMutex.unlock();
}

int CV::Job::getStatus(){
    this->accessMutex.lock();
    int v = this->status;
    this->accessMutex.unlock();
    return v;
}

CV::Job::Job(){
    this->isCallback = false;
    this->ready = true;    
    this->id = 0;
    this->type = ItemTypes::JOB;
    this->copyable = false;
    this->status = CV::JobStatus::CREATED;
    this->inherited = 0;
}

void CV::Job::set(int type, std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program){
    this->accessMutex.lock();
    this->status = CV::JobStatus::CREATED;
    this->entry = entry;
    this->program = program;
    this->jobType = type;
    this->accessMutex.unlock();
}

// std::shared_ptr<CV::Job> CV::Job::setCallBack(std::shared_ptr<CV::Function> &cb){
//     auto job = std::make_shared<CV::Job>();
//     job->accessMutex.lock();
//     job->cursor = this->cursor;
//     job->fn = cb;
//     job->jobType = CV::JobType::CALLBACK;
//     job->status = CV::JobStatus::RUNNING;
//     job->accessMutex.unlock();

//     this->accessMutex.lock();
//     this->callback = job;
//     this->accessMutex.unlock();
//     return job;
// }

CV::ItemContextPair CV::Context::getWithContext(const std::string &name){
    this->accessMutex.lock();
    auto it = vars.find(name);
    if(it == vars.end()){
        auto v = this->upper ? this->upper->getWithContext(name) : ItemContextPair();
        this->accessMutex.unlock();
        return v;
    }   
    auto v = ItemContextPair(it->second.get(), this, it->first);
    this->accessMutex.unlock();
    return v;
}    

CV::ItemContextPair CV::Context::getWithContext(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    for(auto &it : this->vars){
        if(it.second.get() == item.get()){
            auto v = ItemContextPair(it.second.get(), this, it.first);
            this->accessMutex.unlock();
            return v;
        }
    }
    auto v = this->upper ? this->upper->getWithContext(item) : ItemContextPair();
    this->accessMutex.unlock();
    return v;
}        

std::shared_ptr<CV::Item> CV::Context::set(const std::string &name, const std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    this->vars[name] = item;
    this->accessMutex.unlock();    
    return item;
}

CV::Context::Context(){
    this->readOnly = false;
    this->type = CV::ItemTypes::CONTEXT;
    this->upper = NULL;
    this->temporary = false;
}

CV::Context::Context(std::shared_ptr<CV::Context> &ctx){
    this->readOnly = false;    
    this->type = CV::ItemTypes::CONTEXT;        
    this->upper = ctx;
    this->temporary = false;
    auto ftop = this->get("top");
    if(ftop.get()){
        this->top = std::static_pointer_cast<CV::Context>(ftop);
    }
}    

void CV::Context::solidify(bool downstream){
    this->accessMutex.lock();
    for(auto &it : this->vars){
        auto &item = it.second;
        item->solid = true;
        if(item.get() == this || (this->upper.get() && this->upper.get() == item.get()) || (this->top.get() && this->top.get() == item.get())) continue;
        if(downstream && item->type == CV::ItemTypes::CONTEXT){
            std::static_pointer_cast<CV::Context>(item)->solidify(downstream);
        }
    }

    this->accessMutex.unlock();
}

void CV::Context::reset(bool downstream){
    this->accessMutex.lock();
    // Find non-solid mutable items to delete
    std::vector<std::string> names;
    for(auto &it : this->vars){
        auto &item = it.second;
        if(item->solid){ //  || (item->type == CV::ItemTypes::CONTEXT && std::static_pointer_cast<CV::Context>(item)->readOnly)
            continue;
        }     
        if(it.first == "top"){
            continue;
        }        
        names.push_back(it.first);
    }    
    // delete them
    for(int i = 0; i < names.size(); ++i){
        auto it = this->vars.find(names[i]);
        if(it == this->vars.end()){
            // what? shouldn't happen :|
            continue;
        }
        this->vars.erase(it);
    }        
    // tell solid contexts left to delete reset in downstream chain
    if(downstream){
        for(auto &it : this->vars){
            auto &item = it.second;
            if(item->type != CV::ItemTypes::CONTEXT){
                continue;
            }
            if(it.first == "top"){
                continue;
            }
            std::static_pointer_cast<CV::Context>(item)->reset(downstream);
        }       
    } 
    this->accessMutex.unlock();
}

void CV::Context::debug(){
    std::cout << "\n\n==================CONTEXT DEBUG==================" << std::endl;
    size_t maxVarName = 0;
    auto getSpace = [&](int w){
        std::string space = "";
        int max = std::max(maxVarName, (size_t)10) - w;
        for(int i = 0; i < max; ++i) space += " ";
        return space;
    };
    for(auto &it : this->vars){
        maxVarName = std::max(maxVarName, it.first.length());
    }

    for(auto &it : this->vars){
        if(it.first == "top") continue;
        std::cout << it.first << getSpace(it.first.length()) << " -> " << getSpace(0) << CV::ItemToText(it.second.get()) << std::endl;
    }
}

/*

    ITEM TO TEXT

*/
static std::string FunctionToText(CV::Function *fn){

    std::string start = "[";
    std::string end = "]";
    std::string name = "fn";
    std::string binary = "BINARY";
    std::string body = fn->body;
    return start+name+" "+start+(fn->variadic ? "args" : CV::Tools::compileList(fn->getParams()))+end+" "+start+(fn->binary ? binary : body )+end+end;
}

std::string CV::ItemToText(CV::Item *item){
    switch(item->type){
        default:
        case CV::ItemTypes::NIL: {
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
        };
        case CV::ItemTypes::JOB: {
            auto job = static_cast<CV::Job*>(item);
            return Tools::setTextColor(Tools::Color::GREEN)+"[JOB ("+(job->ready ? "R" : " " )+") "+(job->isCallback ? "CALLBACK " : "")+Tools::removeTrailingZeros(job->id)+" '"+JobType::str(job->jobType)+"' '"+JobStatus::str(job->status)+"']"+Tools::setTextColor(Tools::Color::RESET);
        };  

        case CV::ItemTypes::INTERRUPT: {
            auto interrupt = static_cast<CV::Interrupt*>(item);
            return Tools::setTextColor(Tools::Color::YELLOW)+"[INT-"+CV::InterruptTypes::str(interrupt->intType)+(interrupt->hasPayload() ? " "+CV::ItemToText(interrupt->getPayload().get())+"" : "")+"]"+Tools::setTextColor(Tools::Color::RESET);
        };                
        case CV::ItemTypes::NUMBER: {
            auto r = Tools::removeTrailingZeros(static_cast<CV::Number*>(item)->get());
            return  Tools::setTextColor(Tools::Color::CYAN) +
                    r +
                    Tools::setTextColor(Tools::Color::RESET);
        };
        case CV::ItemTypes::STRING: {
            return Tools::setTextColor(Tools::Color::GREEN)+"'"+static_cast<CV::String*>(item)->get()+"'"+Tools::setTextColor(Tools::Color::RESET);
        };        
        case CV::ItemTypes::FUNCTION: {
            auto fn = static_cast<CV::Function*>(item);      

            std::string start = Tools::setTextColor(Tools::Color::RED, true)+"["+Tools::setTextColor(Tools::Color::RESET);
            std::string end = Tools::setTextColor(Tools::Color::RED, true)+"]"+Tools::setTextColor(Tools::Color::RESET);
            std::string name = Tools::setTextColor(Tools::Color::RED, true)+"fn"+Tools::setTextColor(Tools::Color::RESET);
            std::string binary = Tools::setTextColor(Tools::Color::BLUE, true)+"BINARY"+Tools::setTextColor(Tools::Color::RESET);
            std::string body = Tools::setTextColor(Tools::Color::BLUE, true)+fn->body+Tools::setTextColor(Tools::Color::RESET);

            return start+name+" "+start+(fn->variadic ? "args" : Tools::compileList(fn->getParams()))+end+" "+start+(fn->binary ? binary : body )+end+end;
        };
        case CV::ItemTypes::CONTEXT: {
            std::string output = Tools::setTextColor(Tools::Color::YELLOW, true)+"["+Tools::setTextColor(Tools::Color::YELLOW, true)+"ct"+Tools::setTextColor(Tools::Color::RESET);
            auto proto = static_cast<CV::Context*>(item);
            int n = proto->vars.size();
            int c = 0;
            proto->accessMutex.lock();
            if(!ShowHEADRef && proto->vars.find("top") != proto->vars.end()){
                --n;
            }
            for(auto &it : proto->vars){
                auto name = it.first;
                auto item = it.second;
                if(c == 0){
                    output += " ";
                }
                if(!ShowHEADRef && name == "top") {
                    continue;
                }
                auto body = name == "top" ? Tools::setTextColor(Tools::Color::YELLOW, true)+"[ct]"+Tools::setTextColor(Tools::Color::RESET) : CV::ItemToText(item.get());
                output += body+CV::ModifierTypes::str(CV::ModifierTypes::NAMER)+Tools::setTextColor(Tools::Color::GREEN)+name+Tools::setTextColor(Tools::Color::RESET);
                if(c < n-1){
                    output += " ";
                }
                ++c;
            }
            proto->accessMutex.unlock();
            return output + Tools::setTextColor(Tools::Color::YELLOW, true)+"]"+Tools::setTextColor(Tools::Color::RESET);     
        };
        case CV::ItemTypes::LIST: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);
            auto list = static_cast<CV::List*>(item);

            list->accessMutex.lock();
            int total = list->data.size();
            int limit = total > 30 ? 10 : total;
            for(int i = 0; i < limit; ++i){
                auto item = list->data[i];
                output += CV::ItemToText(item.get());
                if(i < list->data.size()-1){
                    output += " ";
                }
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);
            }
                        
            list->accessMutex.unlock();
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };         
    }
}

static void DebugStaticValues(CV::Context *ctx){
    std::cout << "\t\t\tSTATIC VALUES\t\t\t" << std::endl;
    std::vector<std::shared_ptr<CV::Item>> values;
    for(auto &it : ctx->staticValues){
        values.push_back(it.second);
    }
    std::sort(values.begin(), values.end(), [](std::shared_ptr<CV::Item> &a, std::shared_ptr<CV::Item> &b){
        return a->id < b->id;
    }); 
    for(int i = 0; i < values.size(); ++i){
        std::cout << values[i]->id << "|" << CV::ItemToText(values[i].get()) << std::endl;
    }  
}

static void DebugByteCode(std::shared_ptr<CV::TokenByteCode> &entry, CV::Context *ctx, CV::Stack *stack){

    
    std::cout << "\t\t\tINSTRUCTIONS\t\t\t" << std::endl;
    std::vector<std::shared_ptr<CV::TokenByteCode>> ins;
    // for(auto &it : stack->instructions){
    //     ins.push_back(it.second);
    // }
    std::sort(ins.begin(), ins.end(), [](std::shared_ptr<CV::TokenByteCode> &a, std::shared_ptr<CV::TokenByteCode> &b){
        return a->id < b->id;
    });
    for(int i = 0; i < ins.size(); ++i){
        std::cout << ins[i]->id << "|" << ins[i]->literal() << std::endl;
    }


    std::cout << "\t\t\tSTATIC VALUES\t\t\t" << std::endl;
    std::vector<std::shared_ptr<CV::Item>> values;
    for(auto &it : ctx->staticValues){
        values.push_back(it.second);
    }
    std::sort(values.begin(), values.end(), [](std::shared_ptr<CV::Item> &a, std::shared_ptr<CV::Item> &b){
        return a->id < b->id;
    }); 
    for(int i = 0; i < values.size(); ++i){
        std::cout << values[i]->id << "|" << CV::ItemToText(values[i].get()) << std::endl;
    }   


    // ctx->debug();    

}

static std::vector<std::string> parseTokens(std::string input, char sep, std::shared_ptr<CV::Cursor> &cursor){
    std::vector<std::string> tokens; 
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
        cursor->setError("", "Mismatching brackets", cline);
        return {};
    }
    // Throw mismatching quotes
    if(quotes != 0){
        cursor->setError("", "Mismatching quotes", cline);
        return {};
    }
    cline = 0;


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
            buffer += '\'';
            ++i;
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
        if(c == '\n' && !onString){
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
                    tokens.push_back(buffer);
                }
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            if(buffer != ""){
                tokens.push_back(buffer+c);
            }
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
            if(buffer != ""){
                tokens.push_back(buffer);
            }
            buffer = "";
        }else{
            buffer += c;
        }
    }

    return tokens;
};

CV::Token parseTokenWModifier(const std::string &_input){
    std::string buffer;
    CV::Token token;
    std::string input = _input;
    if(input[0] == '[' && input[input.size()-1] == ']'){
        input = input.substr(1, input.length() - 2);
    }
    for(int i = 0; i < input.length(); ++i){
        char c = input[input.length  ()-1 - i];
        auto t = CV::ModifierTypes::getToken(c);
        if(t != CV::ModifierTypes::UNDEFINED){
                std::reverse(buffer.begin(), buffer.end());            
                token.modifiers.push_back(std::make_shared<CV::ModifierPair>(t, buffer));
                buffer = "";   
        }else
        if(t == CV::ModifierTypes::UNDEFINED && i == input.length()-1){
            buffer += c;
            token.first = buffer;
            std::reverse(token.first.begin(), token.first.end());
            buffer = "";
        }else{
            buffer += c;
        }
    }
    std::reverse(token.modifiers.begin(), token.modifiers.end());
    return token;
}

std::vector<CV::Token> buildTokens(const std::vector<std::string> &literals, std::shared_ptr<CV::Cursor> &cursor){
    std::vector<CV::Token> tokens;
    for(int i = 0; i < literals.size(); ++i){
        auto innLiterals = parseTokens(literals[i], ' ', cursor);
        // Complex tokens type 1 (more than parsed token)     
        if(innLiterals.size() > 1){
            CV::Token token;
            token.first = literals[i];
            token.complex = true;
            tokens.push_back(token);  
        }else{
            auto token = parseTokenWModifier(literals[i]);
            // Complex token type 2 (no space, but chained blocks)
            if(token.first != "" && literals[i][0] == '[' && literals[i][literals[i].size()-1] == ']' && token.getModifierNumber() > 0){
                CV::Token token;
                token.first = literals[i];
                token.complex = true;
                tokens.push_back(token);                 
            // Otherwise simple tokens
            }else
            if(token.first == "" && token.getModifierNumber() > 0){ 
                if(i > 0){
                    auto &prev = tokens[tokens.size()-1];
                    for(int j = 0; j < token.getModifierNumber(); ++j){
                        prev.modifiers.push_back(token.modifiers[j]);
                    }
                    continue;
                }else{
                    cursor->setError(CV::ModifierTypes::str(token.modifiers[0]->type), "Provided dangling modifier. Modifiers can only go accompanying something.");
                    return {};
                }
            }else{
                tokens.push_back(token);
            }
        }
    }

    // CV::Tools::printList(literals);
    //     for(int i = 0; i < tokens.size(); ++i){
    //         std::cout << tokens[i].literal() << std::endl;
    //     }

    // std::exit(1);

    return tokens;
};

static std::vector<CV::Token> compileTokens(const std::vector<CV::Token> &tokens, int from, int amount){
	std::vector<CV::Token> compiled;
	for(int i = 0; i < amount; ++i){
        compiled.push_back(tokens[from + i]);
	}
	return compiled;
}

bool CV::FunctionConstraints::test(List *list, std::string &errormsg){
    std::vector<std::shared_ptr<CV::Item>> items;
    list->accessMutex.lock();
    for(int i = 0; i < list->size(); ++i){
        items.push_back(list->data[i]);
    }
    list->accessMutex.unlock();
    return this->test(items, errormsg);
}

bool CV::FunctionConstraints::test(const std::vector<std::shared_ptr<CV::Item>> &items, std::string &errormsg){
    if(!enabled){
        return true;
    }

    if(useMaxParams && items.size() > maxParams){
        errormsg = "Provided ("+std::to_string(items.size())+") argument(s). Expected no more than ("+std::to_string(maxParams)+") argument(s).";
        return false;
    }
    if(useMinParams && items.size() < minParams){
        errormsg = "Provided ("+std::to_string(items.size())+") argument(s). Expected no less than ("+std::to_string(minParams)+") argument(s).";
        return false;
    } 


    if(!allowNil){
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type == CV::ItemTypes::NIL){
                errormsg = "Provided nil value.";
                return false;
            }
        }        
    }


    if(!allowMisMatchingTypes){
        uint8_t firstType = CV::ItemTypes::NIL;
        for(int i = 0; i < items.size(); ++i){
            if(items[i]->type != CV::ItemTypes::NIL && i < items.size()-1){
                firstType = items[i]->type;
            }
        }        
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type != firstType && items[i]->type != CV::ItemTypes::NIL){
                errormsg = "Provided arguments with mismatching types.";
                return false;
            }
        }
    }

    if(expectedTypeAt.size() > 0){
        for(int i = 0; i < items.size(); ++i){
            if(expectedTypeAt.count(i) > 0){
                if(items[i]->type != expectedTypeAt[i]){
                    errormsg = "Provided '"+CV::ItemTypes::str(items[i]->type)+"' at position "+std::to_string(i)+". Expected type is '"+CV::ItemTypes::str(expectedTypeAt[i])+"'.";
                    return false;
                }
            }
        }
    }

    if(expectedTypes.size() > 0){
        for(int i = 0; i < items.size(); ++i){
            bool f = false;
            for(int j = 0; j < this->expectedTypes.size(); ++j){
                auto exType = this->expectedTypes[j];
                if(items[i]->type == exType){
                    f = true;
                    break;
                }

            }
            if(!f){
                std::string exTypes = "";
                for(int i = 0; i < expectedTypes.size(); ++i){
                    exTypes += "'"+CV::ItemTypes::str(expectedTypes[i])+"'";
                    if(i < expectedTypes.size()-1){
                        exTypes += ", ";
                    }
                }
                errormsg = "Provided '"+CV::ItemTypes::str(items[i]->type)+"'. Expected types are only "+exTypes+".";
                return false;
            }

        }        
    }    

    return true;
}

bool CV::FunctionConstraints::testAgainst(std::shared_ptr<CV::String> &item, const std::vector<std::string> &opts, std::string &errormsg){
    auto str = item->get();
    for(int i = 0; i < opts.size(); ++i){
        if(opts[i] == str){
            return true;
        }
    }
    std::string literalOpts = "";
    for(int i = 0; i < opts.size(); ++i){
        if(i == opts.size()-1){
            literalOpts += " or ";
        }        
        literalOpts += "'"+opts[i]+"'";
        if(i < opts.size()-1){
            literalOpts += ", ";
        }
    }
    errormsg = "Provided '"+str+"'. Expected only one of the following: "+literalOpts+".";
    return false;
}


bool CV::FunctionConstraints::testRange(std::shared_ptr<CV::Number> &item, __CV_NUMBER_NATIVE_TYPE min, __CV_NUMBER_NATIVE_TYPE max, std::string &errormsg){
    auto n = item->get();
    if(n < min || n > max){
        auto litmin = Tools::removeTrailingZeros(min);
        auto litmax= Tools::removeTrailingZeros(max);
        errormsg = "Provided '"+Tools::removeTrailingZeros(n)+"' which is out range. Expected range is from "+litmin+" to "+litmax+" (inclusive).";
        return false;
    }
    return true;
}

bool CV::FunctionConstraints::testRange(std::shared_ptr<CV::List> &item, __CV_NUMBER_NATIVE_TYPE min, __CV_NUMBER_NATIVE_TYPE max, std::string &errormsg){
    auto n = item->size();
    if(n < min || n > max){
        auto litmin = Tools::removeTrailingZeros(min);
        auto litmax = Tools::removeTrailingZeros(max);
        errormsg = "Provided List of "+Tools::removeTrailingZeros(n)+" element(s) which is out range. Expected range is from "+litmin+" to "+litmax+" (inclusive).";
        return false;
    }
    return true;
}

bool CV::FunctionConstraints::testListUniformType(std::shared_ptr<CV::List> &item, uint8_t type, std::string &errormsg){
    item->accessMutex.lock();
    for(int i = 0; i < item->data.size(); ++i){
        if(item->data[i]->type != type){
            errormsg = "Provided List that includes '"+CV::ItemTypes::str(item->data[i]->type)+"' type. The entire List is expected to be made out of '"+CV::ItemTypes::str(type)+"' type.";
            return false;
        }
    }
    item->accessMutex.unlock();
    return true;
}


static std::shared_ptr<CV::Item> processPreInterpretConversionModifiers(std::shared_ptr<CV::Item> &item, std::vector<std::shared_ptr<CV::ModifierPair>> &modifiers, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    effects.reset();
    for(int i = 0; i < modifiers.size(); ++i){
        auto &mod = modifiers[i];
        // if(mod->used){
        //     continue;
        // }
        switch(mod->type){
            case CV::ModifierTypes::TRAIT: {
                mod->used = true;
                auto name = mod->subject;
                if(name.size() == 0){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::TRAIT), "Trait modifier requires a name or type group.");
                    return item;                    
                }
                // ANY_NUMBER type trait
                if(CV::Tools::isNumber(name) && hasTrait(item, CV::TraitType::ANY_NUMBER)){
                    auto result = CV::runTrait(item, CV::TraitType::ANY_NUMBER, name, cursor, ctx, effects);  
                    if(cursor->error){
                        return std::make_shared<CV::Item>();
                    }                         
                    item = result;
                }else
                // ANY_STRING type trait
                if(!CV::Tools::isNumber(name) && hasTrait(item, CV::TraitType::ANY_STRING)){
                    auto result = CV::runTrait(item, CV::TraitType::ANY_STRING, name, cursor, ctx, effects);    
                    if(cursor->error){
                        return std::make_shared<CV::Item>();
                    }                       
                    item = result;
                }else                
                // Regular trait
                if(!hasTrait(item, name)){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::TRAIT), "Trait '"+name+"' doesn't belong to '"+CV::ItemTypes::str(item->type)+"' type.");
                    return item;                      
                }else{
                    auto result = CV::runTrait(item, name, "", cursor, ctx, effects);
                    if(cursor->error){
                        return std::make_shared<CV::Item>();
                    }                    
                    item = result;
                }
            } break;                   
            case CV::ModifierTypes::SCOPE: {
                if(item->type != CV::ItemTypes::CONTEXT){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::SCOPE), "Scope modifier can only be used on contexts.");
                    return item;
                }
                // Context switching token
                if(mod->subject.size() == 0 && i == modifiers.size()-1){
                    return item;
                }else
                // Everything else
                if(mod->subject.size() > 0){        
                    auto subsequent = std::static_pointer_cast<CV::Context>(item)->get(mod->subject);
                    if(!subsequent){
                        cursor->setError("["+CV::ItemTypes::str(item->type)+"]:"+mod->subject, GENERIC_UNDEFINED_SYMBOL_ERROR);
                        return item;
                    }
                    effects.toCtx = std::static_pointer_cast<CV::Context>(item);
                    effects.ctxTargetName = mod->subject;
                    item = subsequent;
                }else{
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::SCOPE), "Missing scope name");
                    return item;
                }
                mod->used = true;
            } break;                                 
        }
    }
    return item;
}


static void validateNamerMod(std::vector<std::shared_ptr<CV::ModifierPair>> &modifiers, std::shared_ptr<CV::Cursor> &cursor){
    for(int i = 0; i < modifiers.size(); ++i){
        auto &mod = modifiers[i];
        // if(mod->used){
        //     continue;
        // }
        switch(mod->type){
            case CV::ModifierTypes::NAMER: {
                if(i < modifiers.size()-1){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer modifier can only be used at the end of a modifier chain.");
                    return;
                }
                if(mod->subject.size() == 0){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer expects a name for the item.");
                    return;
                }

                if(!CV::Tools::isValidVarName(mod->subject)){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Invalid name '"+mod->subject+"'. The name cannot be a reserved word and/or contain prohibited symbols (Modifiers, numbers, brackets, etc).");
                    return;  
                }       

                if(i == modifiers.size()-1){
                    return;
                }else{
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer modifier can only be used at the end of a modifier chain.");
                    return;
                }
            }
        }
    }
}

static std::shared_ptr<CV::Item> processPreInterpretSituationalModifiers(std::shared_ptr<CV::Item> &item, std::vector<std::shared_ptr<CV::ModifierPair>> &modifiers, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    effects.reset();
    for(int i = 0; i < modifiers.size(); ++i){
        auto &mod = modifiers[i];
        // if(mod->used){
        //     continue;
        // }
        switch(mod->type){
            case CV::ModifierTypes::NAMER: {
                if(i < modifiers.size()-1){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer modifier can only be used at the end of a modifier chain.");
                    return item;
                }
                if(mod->subject.size() == 0){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer expects a name for the item.");
                    return std::make_shared<CV::Item>();
                }

                if(!CV::Tools::isValidVarName(mod->subject)){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Invalid name '"+mod->subject+"'. The name cannot be a reserved word and/or contain prohibited symbols (Modifiers, numbers, brackets, etc).");
                    return std::make_shared<CV::Item>();  
                }       

                effects.named = mod->subject;
                if(effects.named.size() == 0){
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer expects a name for the item.");
                    return std::make_shared<CV::Item>();
                }
                if(i == modifiers.size()-1){
                    mod->used = false; 
                }else{
                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::NAMER), "Namer modifier can only be used at the end of a modifier chain.");
                    return item;
                }
            } break;        
            case CV::ModifierTypes::EXPAND: {
                if(i == modifiers.size()-1){
                    effects.expand = true;
                    mod->used = false; // EXPAND doesn't get "used up". It must carry on up until the type is no longer a List
                }else{

                    cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::EXPAND), "Expand modifier can only be used at the end of a modifier chain.");
                    return item;
                }
            } break;                                   
        }
    }
    return item;
}



static void processPostInterpretExpandListOrContext(std::shared_ptr<CV::Item> &solved, CV::ModifierEffect &effects, std::vector<std::shared_ptr<CV::Item>> &items, std::shared_ptr<CV::Cursor> &cursor, const std::shared_ptr<CV::Context> &ctx){
    if(effects.expand && solved->type == CV::ItemTypes::LIST){
        auto list = std::static_pointer_cast<CV::List>(solved);
        for(int j = 0; j < list->size(); ++j){
            items.push_back(list->get(j));
        }
    }else
    if(effects.expand && solved->type == CV::ItemTypes::CONTEXT){
        if(effects.expand && solved->type == CV::ItemTypes::CONTEXT){
            auto proto = std::static_pointer_cast<CV::Context>(solved);
            for(auto &it : proto->vars){
                if(ctx == NULL){
                    items.push_back(it.second);
                }else{                           
                    ctx->set(it.first, it.second);
                }
            }
        }  
    }else
    if(effects.expand && solved->type != CV::ItemTypes::CONTEXT && solved->type != CV::ItemTypes::LIST){
        cursor->setError(CV::ModifierTypes::str(CV::ModifierTypes::EXPAND), "Expand modifier can only be used on iterables.");
        items.push_back(solved);
    }else{   
        items.push_back(solved);
    } 
}

static bool checkCond(std::shared_ptr<CV::Item> &item){
    if(item->type == CV::ItemTypes::NUMBER){
        return std::static_pointer_cast<CV::Number>(item)->get() <= 0 ? false : true;
    }else
    if(item->type != CV::ItemTypes::NIL){
        return true;
    }else{
        return false;
    }
};

// static CV::ItemContextPair findTokenOrigin(CV::Token &token, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
//     auto findName = token.first;
//     // If token has modifiers, then we proceed to process them
//     if(token.getModifierNumber() > 0){
//         auto var = ctx->get(token.first);
//         if(var.get()){
//             CV::ModifierEffect effects;
//             auto solved = processPreInterpretModifiers(var, token.modifiers, cursor, ctx, effects);
//             if(cursor->error){
//                 return CV::ItemContextPair(NULL, NULL, "", true);
//             }
//             if(effects.toCtx && effects.ctxTargetName.size() > 0){
//                 return CV::ItemContextPair(solved.get(), effects.toCtx.get(), effects.ctxTargetName, false);
//             }
//         }
//     }
//     return ctx->getWithContext(findName);
// }


static bool typicalVariadicArithmeticCheck(const std::string &name, const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, int max = -1){

    return true;
    CV::FunctionConstraints consts;
    consts.setMinParams(2);
    if(max != -1){
        consts.setMaxParams(max);
    }
    consts.setExpectType(CV::ItemTypes::NUMBER);
    consts.allowMisMatchingTypes = false;
    consts.allowNil = false;
    std::string errormsg;
    if(!consts.test(params, errormsg)){
        cursor->setError(name, errormsg);
        return false;
    }
    return true;
};

void CV::AddStandardOperators(std::shared_ptr<CV::Context> &ctx){

    registerGenericTraits();


    /*

        ARITHMETIC OPERATORS

    */
    ctx->set("none", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(256));
    }, true));

    ctx->set("+", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("+", params, cursor)){
            auto r = std::make_shared<CV::Item>();
            return r;
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r += std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return params[0];
    }, true));

    ctx->set("-", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("-", params, cursor)){
            return std::make_shared<CV::Item>();
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r -= std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return params[0];
    }, true));
    ctx->set("*", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("*", params, cursor)){
            return std::make_shared<CV::Item>();
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r *= std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, true));  
    ctx->set("/", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("/", params, cursor)){
            return std::make_shared<CV::Item>();
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            auto n = std::static_pointer_cast<CV::Number>(params[i])->get();
            if(n == 0){
                cursor->setError("/", "Division by zero.");
                return std::make_shared<CV::Item>();
            }
            r /= n;
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, true));
    ctx->set("pow", std::make_shared<CV::Function>(std::vector<std::string>({"a", "b"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("pow", params, cursor, 2)){
            return std::make_shared<CV::Item>();
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r =  std::pow(r, std::static_pointer_cast<CV::Number>(params[i])->get());
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, false)); 
    ctx->set("sqrt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::NUMBER);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("sqrt", errormsg);
            return std::make_shared<CV::Item>();
        }
        if(params.size() == 1){
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(  std::sqrt(std::static_pointer_cast<CV::Number>(params[0])->get())   ));
        }else{
            std::vector<std::shared_ptr<CV::Item>> result;
            for(int i = 0; i < params.size(); ++i){
                result.push_back(
                    std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(  std::sqrt(std::static_pointer_cast<CV::Number>(params[i])->get())   ))
                );
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result, false));
        }
    }, false));    
       
    /* 
        QUICK MATH
    */ 
    ctx->set("++", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("++", errormsg);
            return std::make_shared<CV::Item>();
        }

        if(params.size() == 1){
            auto n = std::static_pointer_cast<CV::Number>(params[0]);
            n->set(n->get() + 1);
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                auto n = std::static_pointer_cast<CV::Number>(params[i]);
                n->set(n->get() + 1);
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(params, false));
    }, true));

    ctx->set("--", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("--", errormsg);
            return std::make_shared<CV::Item>();
        }

        if(params.size() == 1){
            auto n = std::static_pointer_cast<CV::Number>(params[0]);
            n->set(n->get() - 1);
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                auto n = std::static_pointer_cast<CV::Number>(params[i]);
                n->set(n->get() - 1);
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(params, false));
    }, true));    


    ctx->set("//", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("//", errormsg);
            return std::make_shared<CV::Item>();
        }

        if(params.size() == 1){
            auto n = std::static_pointer_cast<CV::Number>(params[0]);
            n->set(n->get() / 2);
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                auto n = std::static_pointer_cast<CV::Number>(params[i]);
                n->set(n->get() / 2);
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(params, false));
    }, true));    

    ctx->set("**", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("**", errormsg);
            return std::make_shared<CV::Item>();
        }

        if(params.size() == 1){
            auto n = std::static_pointer_cast<CV::Number>(params[0]);
            n->set(n->get() * n->get());
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                auto n = std::static_pointer_cast<CV::Number>(params[i]);
                n->set(n->get() * n->get());
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(params, false));
    }, true));


    /*
        LISTS
    */
    ctx->set("l-gen", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("l-gen", params, cursor)){
            return std::make_shared<CV::Item>();
        }
        auto s = std::static_pointer_cast<CV::Number>(params[0])->get();
        auto e = std::static_pointer_cast<CV::Number>(params[1])->get();
        std::vector<std::shared_ptr<CV::Item>> result;
        
        if(s == e){
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result, false));
        }
        bool inc = e > s;
        __CV_NUMBER_NATIVE_TYPE step = Tools::positive(params.size() == 3 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1);
        for(__CV_NUMBER_NATIVE_TYPE i = s; (inc ? i <= e : i >= e); i += (inc ? step : -step)){
            result.push_back(
                std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(i))
            );
        }

        auto resultList = std::make_shared<CV::List>(result, false);
        return std::static_pointer_cast<CV::Item>(resultList);
    }, true));
    
    ctx->set("l-sort", std::make_shared<CV::Function>(std::vector<std::string>({"c", "l"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::STRING);
        consts.setExpectedTypeAt(1, CV::ItemTypes::LIST);
        
        // TODO: add function support

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-sort", errormsg);
            return std::make_shared<CV::Item>();
        }

        auto criteria = std::static_pointer_cast<CV::String>(params[0]);

        auto &order = criteria->data;
        if(order != "DESC" && order != "ASC"){
            cursor->setError("l-sort", "criteria can only be 'DESC' or 'ASC'.");
            return std::make_shared<CV::Item>();
        }        

        auto list = std::static_pointer_cast<CV::List>(params[1]);
        std::vector<std::shared_ptr<CV::Item>> items;
        list->accessMutex.lock();
        for(int i = 0; i < list->data.size(); ++i){
            items.push_back(list->data[i]);
        }        
        list->accessMutex.unlock();

        CV::FunctionConstraints constsList;
        constsList.setExpectType(CV::ItemTypes::NUMBER);
        constsList.allowMisMatchingTypes = false;        
        constsList.allowNil = false;
        if(!constsList.test(items, errormsg)){
            cursor->setError("l-sort", errormsg);
            return std::make_shared<CV::Item>();
        }        

        std::sort(items.begin(), items.end(), [&](std::shared_ptr<CV::Item> &a, std::shared_ptr<CV::Item> &b){
            return order == "DESC" ?     std::static_pointer_cast<CV::Number>(a)->get() > std::static_pointer_cast<CV::Number>(b)->get() :
                                        std::static_pointer_cast<CV::Number>(a)->get() < std::static_pointer_cast<CV::Number>(b)->get();
        });

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));
    }, false));    


    ctx->set("l-filter", std::make_shared<CV::Function>(std::vector<std::string>({"c", "l"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::FUNCTION);
        consts.setExpectedTypeAt(1, CV::ItemTypes::LIST);
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-filter", errormsg);
            return std::make_shared<CV::Item>();
        }

        auto criteria = std::static_pointer_cast<Function>(params[0]);

        if(criteria->variadic || criteria->params.size() != 1){
            cursor->setError("l-filter", "criteria's function must receive exactly 1 argument(a).");
            return std::make_shared<CV::Item>();
        }

        auto list = std::static_pointer_cast<CV::List>(params[1]);
        std::vector<std::shared_ptr<CV::Item>> items;
        list->accessMutex.lock();
        for(int i = 0; i < list->data.size(); ++i){
            auto args = std::vector<std::shared_ptr<CV::Item>>{list->data[i]};
            // auto result = runFunction(criteria, args, cursor, ctx);
            // if(checkCond(result)){
            //     continue;
            // }
            items.push_back(list->get(i));
        }        
        list->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));

    }, false));  


    ctx->set("l-flat", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = true;
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-flat", errormsg);
            return std::make_shared<CV::Item>();
        }


        std::vector<std::shared_ptr<CV::Item>> finalList;

        std::function<void(std::shared_ptr<CV::Item> &item)> recursive = [&](const std::shared_ptr<CV::Item> &item){
            if(item->type == CV::ItemTypes::LIST){
                auto list = std::static_pointer_cast<CV::List>(item);
                list->accessMutex.lock();
                for(int i = 0; i < list->data.size(); ++i){
                    auto item = list->data[i];
                    recursive(item);
                }
                list->accessMutex.unlock();
            }else
            if(item->type == CV::ItemTypes::CONTEXT){
                auto ctx = std::static_pointer_cast<CV::Context>(item);
                ctx->accessMutex.lock();
                for(auto &it : ctx->vars){
                    recursive(it.second);
                }
                ctx->accessMutex.unlock();
            }else{                
                finalList.push_back(item);
            }
        };


        for(int i = 0; i < params.size(); ++i){
            auto item = params[i];
            recursive(item);
        }


        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(finalList, false));
    }, true));   



    ctx->set("splice", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::CONTEXT);
        consts.setExpectType(CV::ItemTypes::LIST);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("splice", errormsg);
            return std::make_shared<CV::Item>();
        }

        auto getAllItems = [](std::shared_ptr<CV::Item> &item){
            std::unordered_map<std::string, std::shared_ptr<CV::Item>> all;
            if(item->type == CV::ItemTypes::LIST){
                auto list = std::static_pointer_cast<CV::List>(item);
                list->accessMutex.lock();
                for(int i = 0; i < list->data.size(); ++i){
                    all[std::to_string(i)] = list->data[i];
                }
                list->accessMutex.unlock();
            }else{
                auto ctx = std::static_pointer_cast<CV::Context>(item);
                ctx->accessMutex.lock();
                for(auto &it : ctx->vars){
                    all[it.first] = it.second;
                }
                ctx->accessMutex.unlock();
            }
            return all;
        };

        auto first = params[0];
        // First item decides the spliced result
        if(first->type == CV::ItemTypes::LIST){
            std::vector<std::shared_ptr<CV::Item>> compiled;
            for(int i = 0; i < params.size(); ++i){
                auto current = params[i];
                auto all = getAllItems(current);
                for(auto &it : all){
                    compiled.push_back(it.second);
                }
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(compiled, false));
        }else{
            auto ctx = std::make_shared<CV::Context>();
            for(int i = 0; i < params.size(); ++i){
                auto current = params[i];
                auto all = getAllItems(current);
                for(auto &it : all){
                    ctx->set(it.first, it.second);
                }
            }
            return std::static_pointer_cast<CV::Item>(ctx);
        }

        return std::make_shared<CV::Item>();
    }, false));    

    ctx->set("l-push", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-push", errormsg);
            return std::make_shared<CV::Item>();
        }

        std::vector<std::shared_ptr<CV::Item>> items;
        auto last = params[params.size()-1];

        if(last->type == CV::ItemTypes::LIST){
            auto list = std::static_pointer_cast<CV::List>(last);
            list->accessMutex.lock();
            for(int i = 0; i < list->data.size(); ++i){
                items.push_back(list->data[i]);
            }
            list->accessMutex.unlock();
        }else{
            items.push_back(last);
        }

        for(int i = 0; i < params.size()-1; ++i){
            items.push_back(params[i]);
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));

    }, false)); 
    
    ctx->set("l-pop", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("<<", errormsg);
            return std::make_shared<CV::Item>();
        }

        std::vector<std::shared_ptr<CV::Item>> items;
        auto first = params[0];

        if(first->type == CV::ItemTypes::LIST){
            auto list = std::static_pointer_cast<CV::List>(first);
            list->accessMutex.lock();            
            for(int i = 0; i < list->data.size(); ++i){
                items.push_back(list->data[i]);
            }
            list->accessMutex.unlock();
        }else{
            items.push_back(first);
        }

        for(int i = 1; i < params.size(); ++i){
            items.push_back(params[i]);
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));

    }, false));     

    ctx->set("l-sub", std::make_shared<CV::Function>(std::vector<std::string>({"l", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::LIST);
        consts.setExpectedTypeAt(1, CV::ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, CV::ItemTypes::NUMBER);
        static const std::string name = "l-sub";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError(name, ""+errormsg);
            return std::make_shared<CV::Item>();
        }

        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto list = std::static_pointer_cast<CV::List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        list->accessMutex.lock();
        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError(name, "position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size()-1)+")");
            list->accessMutex.unlock();
            return std::make_shared<CV::Item>();
        }      

        std::vector<std::shared_ptr<CV::Item>> result;
        for(int i = 0; i < list->data.size(); ++i){
            if(i >= index && i < index+n){
                result.push_back(list->data[i]);
            }
        }
        list->accessMutex.unlock();

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result, false));

    }, false));


    ctx->set("l-cut", std::make_shared<CV::Function>(std::vector<std::string>({"l", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::LIST);
        consts.setExpectedTypeAt(1, CV::ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, CV::ItemTypes::NUMBER);
        static const std::string name = "l-cut";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError(name, errormsg);
            return std::make_shared<CV::Item>();
        }

        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto list = std::static_pointer_cast<CV::List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        list->accessMutex.lock();
        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError(name, "position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size()-1)+")");
            list->accessMutex.unlock();
            return std::make_shared<CV::Item>();
        }      

        std::vector<std::shared_ptr<CV::Item>> result;
        for(int i = 0; i < list->data.size(); ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result.push_back(list->data[i]);
        }
        list->accessMutex.unlock();


        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result, false));


    }, false));      

    ctx->set("l-reverse", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-reverse", errormsg);
            return std::make_shared<CV::Item>();
        }

        auto reverse = [&](std::shared_ptr<CV::List> &lst){
            lst->accessMutex.lock();
            std::vector<std::shared_ptr<CV::Item>> result;
            for(int i = 0; i < lst->data.size(); ++i){
                result.push_back(lst->data[lst->data.size()-1 - i]);
            }
            lst->accessMutex.unlock();
            return std::make_shared<CV::List>(result, false);
        };

        if(params.size() == 1){
            auto item = std::static_pointer_cast<CV::List>(params[0]);
            return std::static_pointer_cast<CV::Item>(reverse(item));
        }else{
            std::vector<std::shared_ptr<CV::Item>> items;
            for(int i = 0; i < params.size(); ++i){
                auto item = std::static_pointer_cast<CV::List>(params[i]);
                items.push_back(
                    std::static_pointer_cast<CV::Item>(reverse(item))
                );
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));
        }



    }, true));        


    /*
        STRING
    */
    ctx->set("s-reverse", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("l-reverse", errormsg);
            return std::make_shared<CV::Item>();
        }

        auto reverse = [&](std::shared_ptr<CV::String> &str){
            std::string result;
            str->accessMutex.lock();
            for(int i = 0; i < str->data.size(); ++i){
                result += str->data[str->data.size()-1 - i];
            }
            str->accessMutex.unlock();
            return result;
        };

        if(params.size() == 1){
            auto casted = std::static_pointer_cast<CV::String>(params[0]);
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(reverse(casted)));
        }else{
            std::vector<std::shared_ptr<CV::Item>> items;
            for(int i = 0; i < params.size(); ++i){
                auto casted = std::static_pointer_cast<CV::String>(params[i]);
                items.push_back(
                    std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(reverse(casted)))
                );
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));
        }



    }, true));  

    ctx->set("s-join", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        // consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::STRING);

        static const std::string name = "s-join";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError(name, ""+errormsg);
            return std::make_shared<CV::Item>();
        }
        std::string result;
        for(int i = 0; i < params.size(); ++i){
            auto str = std::static_pointer_cast<CV::String>(params[i]);
            result += str->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(result));
    }, false));   

    ctx->set("s-cut", std::make_shared<CV::Function>(std::vector<std::string>({"s", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::STRING);
        consts.setExpectedTypeAt(1, CV::ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, CV::ItemTypes::NUMBER);

        static const std::string name = "s-cut";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError(name, ""+errormsg);
            return std::make_shared<CV::Item>();
        }



        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto str = std::static_pointer_cast<CV::String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        str->accessMutex.lock();
        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError(name, "position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size()-1)+")");
            str->accessMutex.unlock();            
            return std::make_shared<CV::Item>();
        }      

        std::string result;
        for(int i = 0; i < str->data.size(); ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result += str->data[i];
        }
        str->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(result));

    }, false));       


    ctx->set("s-sub", std::make_shared<CV::Function>(std::vector<std::string>({"s", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::STRING);
        consts.setExpectedTypeAt(1, CV::ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, CV::ItemTypes::NUMBER);

        static const std::string name = "s-sub";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError(name, ""+errormsg);
            return std::make_shared<CV::Item>();
        }


        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto str = std::static_pointer_cast<CV::String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        str->accessMutex.lock();
        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError(name, "position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size()-1)+")");
            return std::make_shared<CV::Item>();
        }      

        std::string result;
        for(int i = 0; i < str->data.size(); ++i){
            if(i >= index && i < index+n){
                result += str->data[i];
            }
        }
        str->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(result));

    }, false));     


    /*
        LOGIC
    */
    ctx->set("or", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("or", errormsg);
            return std::make_shared<CV::Item>();
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == CV::ItemTypes::NUMBER && std::static_pointer_cast<CV::Number>(params[i])->get() != 0) ||
                (params[i]->type != CV::ItemTypes::NIL && params[i]->type != CV::ItemTypes::NUMBER)){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
    }, true));        

    ctx->set("and", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("and", errormsg);
            return std::make_shared<CV::Item>();
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == CV::ItemTypes::NUMBER && std::static_pointer_cast<CV::Number>(params[i])->get() == 0) || (params[i]->type == CV::ItemTypes::NIL) ){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));    

    ctx->set("not", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("not", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto val = [](std::shared_ptr<CV::Item> &item){
            if(item->type == CV::ItemTypes::NUMBER){
                return std::static_pointer_cast<CV::Number>(item)->get() == 0 ? std::make_shared<CV::Number>(1) : std::make_shared<CV::Number>(0);
            }else
            if(item->type == CV::ItemTypes::NIL){
                return std::make_shared<CV::Number>(1);
            }else{
                return std::make_shared<CV::Number>(0);
            }
        };
        if(params.size() == 1){
            auto item = params[0];
            return std::static_pointer_cast<CV::Item>(val(item));
        }else{
            std::vector<std::shared_ptr<CV::Item>> results;
            for(int i = 0; i < params.size(); ++i){
                auto item = params[i];
                results.push_back(std::static_pointer_cast<CV::Item>(val(item)));
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(results));
        }
        return std::make_shared<CV::Item>();
    }, true));                  


    ctx->set("eq", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("eq", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto first = params[0];
        for(int i = 1; i < params.size(); ++i){
            auto item = params[i];
            if(!first->isEq(item)){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));     


    ctx->set("neq", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("neq", errormsg);
            return std::make_shared<CV::Item>();
        }
        for(int i = 0; i < params.size(); ++i){
            auto p1 = params[i];
            for(int j = 0; j < params.size(); ++j){
                if(i == j){
                    continue;
                }
                auto p2 = params[j];
                if(p1->isEq(p2)){
                    return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
                }
            }
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));      


    ctx->set("gt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("gt", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last <= std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));    


    ctx->set("gte", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("gte", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last <  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));         


    ctx->set("lt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("lt", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last >=  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));    

    ctx->set("lte", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("lte", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last >  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, true));   


    /*
    
        INTERNAL TOOLS
        JOBS
    
    */
    ctx->set("j-get", std::make_shared<CV::Function>(std::vector<std::string>({"id"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setMaxParams(1);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("j-get", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto id = std::static_pointer_cast<CV::Number>(params[0])->get();
        if(id < 0){
            cursor->setError("j-get", "Job ID cannot be negative.");
            return std::make_shared<CV::Item>();            
        }
        auto job = gGetJob(id);
        if(!job.get()){
            cursor->setError("j-get", "Failed to find Job ID '"+CV::Tools::removeTrailingZeros(id)+"'. Perhaps it doesn't exist anymore.");
            return std::make_shared<CV::Item>(); 
        }
        return std::static_pointer_cast<CV::Item>(job);
    }, false)); 


    ctx->set("j-kill", std::make_shared<CV::Function>(std::vector<std::string>({"id"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setMaxParams(1);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("j-kill", errormsg);
            return std::make_shared<CV::Item>();
        }
        auto id = std::static_pointer_cast<CV::Number>(params[0])->get();
        if(id < 0){
            cursor->setError("j-kill", "Job ID cannot be negative.");
            return std::make_shared<CV::Item>();            
        }
        auto job = gGetJob(id);
        if(!job.get()){
            cursor->setError("j-kill", "Failed to find Job ID '"+CV::Tools::removeTrailingZeros(id)+"'. Perhaps it doesn't exist anymore.");
            return std::make_shared<CV::Item>(); 
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(gKillJob(id)));
    }, false));     
    
    ctx->set("j-list", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(0);
        consts.setMaxParams(0);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("j-list", errormsg);
            return std::make_shared<CV::Item>();
        }
        std::vector<std::shared_ptr<CV::Item>> jobs;

        gJobsMutex.lock();

        for(auto it : gjobs){
            jobs.push_back(it.second);
        }

        gJobsMutex.unlock();

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(jobs, false));
    }, false));  


    /*
        TRAITS    
    */

    registerTrait(CV::ItemTypes::NUMBER, "is-neg", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::static_pointer_cast<CV::Number>(subject)->get() < 0));
    });

    registerTrait(CV::ItemTypes::NUMBER, "inv", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::static_pointer_cast<CV::Number>(subject)->get() * -1));
    });    


    registerTrait(CV::ItemTypes::NUMBER, "is-odd", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( (int)std::static_pointer_cast<CV::Number>(subject)->get() % 2 != 0));
    }); 

    registerTrait(CV::ItemTypes::NUMBER, "is-odd", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( (int)std::static_pointer_cast<CV::Number>(subject)->get() % 2 == 0));
    });     

    registerTrait(CV::ItemTypes::LIST, "ANY_NUMBER", CV::TraitType::ANY_NUMBER, [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto list = std::static_pointer_cast<CV::List>(subject); 
        auto index = std::stod(value);
        if(index < 0 || index >= list->size()){
            cursor->setError("List|*ANY_NUMBER*", "Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(list->size()-1)+")");
            return std::make_shared<CV::Item>();
        }
        return list->get(index); 
    });  

    registerTrait(CV::ItemTypes::LIST, "length", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto list = std::static_pointer_cast<CV::List>(subject); 
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(list->size()));
    });  

    registerTrait(CV::ItemTypes::LIST, "reverse", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        std::vector<std::shared_ptr<CV::Item>> result;
        auto list = std::static_pointer_cast<CV::List>(subject); 
        list->accessMutex.lock();
        for(int i = 0; i < list->data.size(); ++i){
            result.push_back(list->data[list->data.size()-1 - i]);
        }
        list->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result));
    });      





    registerTrait(CV::ItemTypes::STRING, "ANY_NUMBER", CV::TraitType::ANY_NUMBER, [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto str = std::static_pointer_cast<CV::String>(subject)->get();
        auto index = std::stod(value);
        if(index < 0 || index >= str.size()){
            cursor->setError("String|*ANY_NUMBER*", "Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(str.size()-1)+")");
            return std::make_shared<CV::Item>();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  std::string("")+str[index]  ));
    });  


    registerTrait(CV::ItemTypes::STRING, "length", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto str = std::static_pointer_cast<CV::String>(subject);
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( str->size() ));
    });      


    registerTrait(CV::ItemTypes::STRING, "list", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        std::vector<std::shared_ptr<CV::Item>> result;
        auto str = std::static_pointer_cast<CV::String>(subject)->get();

        for(int i = 0; i < str.size(); ++i){
            result.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  std::string("")+str[i]  )));
        }


        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result));
    });     


    registerTrait(CV::ItemTypes::STRING, "reverse", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        std::string result;
        auto str = std::static_pointer_cast<CV::String>(subject);

        auto copy = str->get();

        for(int i = 0; i < copy.size(); ++i){
            result += copy[copy.size()-1 - i];
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  result  ));
    });      









    registerTrait(CV::ItemTypes::FUNCTION, "is-variadic", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto func = std::static_pointer_cast<CV::Function>(subject);
        subject->accessMutex.lock();
        auto v = func->variadic;
        subject->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v ));
    });          


    registerTrait(CV::ItemTypes::FUNCTION, "is-binary", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto func = std::static_pointer_cast<CV::Function>(subject);
        subject->accessMutex.lock();
        auto v = func->binary;
        subject->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v ));
    });        

    registerTrait(CV::ItemTypes::FUNCTION, "n-args", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
       if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto func = std::static_pointer_cast<CV::Function>(subject);
        func->accessMutex.lock();
        auto v = func->params.size();
        func->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v  ));     
    });       


    registerTrait(CV::ItemTypes::FUNCTION, "body", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto func = std::static_pointer_cast<CV::Function>(subject);
        func->accessMutex.lock();
        // auto v = func->binary ? "[BINARY]" : func->body;
        func->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  ""  ));    
    });  






    registerTrait(CV::ItemTypes::JOB, "result", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto job = std::static_pointer_cast<CV::Job>(subject);
        if(job->getStatus() != CV::JobStatus::DONE){
            return std::static_pointer_cast<CV::Item>(std::make_shared<String>(JobStatus::str(job->getStatus())));
        }
        return job->getPayload();  
    });     


    registerTrait(CV::ItemTypes::JOB, "copy", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        cursor->setError("JOB|copy", "Jobs cannot be copied.");            
        return std::make_shared<Item>();
    });     

    registerTrait(CV::ItemTypes::JOB, "kill", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto job = std::static_pointer_cast<CV::Job>(subject);
        job->setStatus(CV::JobStatus::DONE);
        return job->getPayload();
    });   

    registerTrait(CV::ItemTypes::JOB, "await", [](std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return subject;
        }
        auto job = std::static_pointer_cast<CV::Job>(subject);
        while(job->getStatus() != CV::JobStatus::DONE){
            CV::Tools::sleep(20);
        }
        return job->getPayload();
    });           


}





CV::TokenByteCode::TokenByteCode(unsigned type){
    this->id = genId();
    this->type = type;
    this->next = 0;
    this->origin = 0;
    this->expirable = true;
}
CV::TokenByteCode::TokenByteCode(){
    this->expirable = true;
    this->id = genId();
    this->type = CV::ByteCodeType::UNDEFINED;
    this->next = 0;
    this->origin = 0;
}
unsigned CV::TokenByteCode::first(){
    return this->parameter[0];
}
std::string CV::TokenByteCode::literal(CV::Context *ctx, CV::Stack *stack){
    std::string LIT = this->str.size() > 0 ? " LIT:'"+this->str[0]+"'" : ""; // TODO: show other strs

    std::string brSt = CV::Tools::setTextColor(CV::Tools::Color::RED)+"["+CV::Tools::setTextColor(CV::Tools::Color::RESET);
    std::string brE = CV::Tools::setTextColor(CV::Tools::Color::RED)+"]"+CV::Tools::setTextColor(CV::Tools::Color::RESET);

    std::string NEXT =  this->next == 0 ? "" : CV::Tools::setTextColor(CV::Tools::Color::MAGENTA)+" NEXT:"+CV::Tools::setTextColor(CV::Tools::Color::RESET)+
    ((ctx != NULL && stack != NULL && this->next != 0) ? " "+stack->instructions[this->next]->literal(ctx, stack)+" " : " "+std::to_string(this->next)+" ");

    std::string PARAM = this->parameter.size() > 0 ? CV::Tools::setTextColor(CV::Tools::Color::CYAN)+" PARAM" + CV::Tools::setTextColor(CV::Tools::Color::RESET) + ":"+brSt : "";

    std::string DATA = this->data.size() > 0 ? CV::Tools::setTextColor(CV::Tools::Color::GREEN)+" DATA" + CV::Tools::setTextColor(CV::Tools::Color::RESET) + ":"+brSt : "";

    std::string MODS = this->getModifierNumber() > 0 ? CV::Tools::setTextColor(CV::Tools::Color::BLUE)+" MODS" + CV::Tools::setTextColor(CV::Tools::Color::RESET) + ":"+brSt : "";

    std::string ORIGIN = this->origin != 0 ? (CV::Tools::setTextColor(CV::Tools::Color::MAGENTA)+" ORIGIN" + CV::Tools::setTextColor(CV::Tools::Color::RESET) +":"+std::to_string(this->origin)) : "";

    for(int i = 0; i < this->parameter.size(); ++i){

        PARAM += std::to_string(this->parameter[i]);

        if(i < this->parameter.size()-1){
            PARAM += " ";
        }else
        if(i == this->parameter.size()-1){
            PARAM += brE;
        }
    }

    for(int i = 0; i < this->data.size(); ++i){

        DATA += std::to_string(this->data[i]);

        if(i < this->data.size()-1){
            DATA += " ";
        }else
        if(i == this->data.size()-1){
            DATA += brE;
        }
    }

    for(int i = 0; i < this->getModifierNumber(); ++i){

        MODS += "'"+CV::ModifierTypes::str(this->modifiers[i]->type)+this->modifiers[i]->subject+"'";

        if(i < this->getModifierNumber()-1){
            MODS += " ";
        }else
        if(i == this->getModifierNumber()-1){
            MODS += brE;
        }
    }    


    std::string type = CV::Tools::setTextColor(CV::Tools::Color::RED, true)+CV::ByteCodeType::str(this->type)+CV::Tools::setTextColor(CV::Tools::Color::RESET);
    
    return  CV::Tools::setTextColor(CV::Tools::Color::YELLOW)+"["+CV::Tools::setTextColor(CV::Tools::Color::RESET)+type+" ID:"+
            std::to_string(id)+LIT+PARAM+DATA+NEXT+MODS+ORIGIN+CV::Tools::setTextColor(CV::Tools::Color::YELLOW)+"]"+CV::Tools::setTextColor(CV::Tools::Color::RESET);
}

void CV::TokenByteCode::inheritModifiers(CV::Token &token){
    this->modMutex.lock();
    for(int i = 0; i < token.getModifierNumber(); ++i){
        if(token.modifiers[i]->used) continue;
        this->modifiers.push_back(token.modifiers[i]);
    }
    this->modMutex.unlock();
    // token.clearModifiers();
}

void CV::TokenByteCode::inheritModifiers(std::vector<std::shared_ptr<CV::ModifierPair>> &mods){
    this->modMutex.lock();
    for(int i = 0; i < mods.size(); ++i){
        if(mods[i]->used) continue;
        this->modifiers.push_back(mods[i]);
    }
    this->modMutex.unlock();
    // mods.clear();
}

static std::shared_ptr<CV::Item> solveName(CV::Token &token, unsigned &origin, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    std::vector<std::shared_ptr<CV::Item>> items;

    auto item = ctx->get(token.first);
    
    if(!item.get()){
        return item;
    }

    CV::ModifierEffect effects;
    auto solved = processPreInterpretConversionModifiers(item, token.modifiers, cursor, ctx, effects);
    if(cursor->error){
        return item;
    }    
    if(effects.toCtx.get()){
        origin = effects.toCtx->id;
        ctx->setStaticValue(effects.toCtx);
    }
  
    return solved;
}

static std::shared_ptr<CV::Item> solveItem(CV::Token &token, unsigned &origin, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    if(CV::Tools::isNumber(token.first)){
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(std::stod(token.first)));
    }else
    if(CV::Tools::isString(token.first)){
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(token.first.substr(1, token.first.length() - 2)));      
    }else
    if(token.first == "nil"){
        return std::make_shared<CV::Item>();
    }else{
        return solveName(token, origin, ctx, cursor);
    }        
}

static bool constructModCheck(CV::Token &token, std::shared_ptr<CV::Cursor> &cursor, bool allowTraits = false){
    if(token.getModifierNumber() == 0){
        return false;
    }
    if(token.getModifierNumber() > 0 && !allowTraits){
        cursor->setError(token.first, "This constructor doesn't accept modifiers.");
        return true;
    }
    if(token.getModifierNumber() > 1 && allowTraits){
        cursor->setError(token.first, "Constructors don't accept chained modifiers.");
        return true;
    }    
    token.modMutex.lock();
    if(token.modifiers[0]->type != CV::ModifierTypes::TRAIT){
        token.modMutex.unlock();
        cursor->setError(token.first, "Constructors only allow TRAIT modifiers.");
        return true;
    }
    token.modMutex.unlock();
    return false;
}

static std::shared_ptr<CV::TokenByteCode> postTokenCreationFinalization(std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::TokenByteCode> &ins, std::vector<std::shared_ptr<CV::ModifierPair>> &headMods, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    
    auto r = ins;

    if(headMods.size() > 0){
        auto fins = program->create(CV::ByteCodeType::FORWARD_INS);
        fins->parameter.push_back(ins->id);
        fins->inheritModifiers(headMods);
        r = fins;
    }

    if(r->hasModifier(CV::ModifierTypes::NAMER)){
        validateNamerMod(ins->modifiers, cursor);
        if(cursor->error){
            return ins;
        }
        auto namer = r->getModifier(CV::ModifierTypes::NAMER);
        ctx->addDisplayItem(namer->subject, r->id, ctx->id);
    }

    return r;
}

static std::shared_ptr<CV::Context> solveLocalCtx(std::shared_ptr<CV::TokenByteCode> &ins, std::shared_ptr<CV::Context> &ctx){
    if(ins->id == ctx->id){
        return ctx;
    }
    auto f = ctx->staticValues.find(ins->origin);
    if(f != ctx->staticValues.end()){
        return std::static_pointer_cast<CV::Context>(f->second);

    }
    return ctx;
}

static std::shared_ptr<CV::TokenByteCode> ProcessToken(
                                            std::shared_ptr<CV::Stack> &program, CV::Token &token,
                                            const std::vector<CV::Token> &params, std::vector<std::shared_ptr<CV::ModifierPair>> &headMods,
                                            std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor,
                                            bool future
                                            ){
    auto &imp = token.first;
    if(imp == "proxy"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }
        if(params.size() != 1){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly least 1: <NAME OR REFERENCE>.");
            return program->create(CV::ByteCodeType::NOOP);
        }           

        auto target = params[0];
        auto targetToken = ParseInputToByteToken(target, program, ctx, cursor, true); // Tells it to not execute any functions
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }

        auto fins = postTokenCreationFinalization(program, targetToken, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins;      
    }else
    if(imp == "with"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <JOB> <CODE>.");
            return program->create(CV::ByteCodeType::NOOP);
        }  

        auto nctx = std::make_shared<CV::Context>(ctx);
        nctx->temporary = true;

        ctx->setStaticValue(nctx);

        // Payload from the target
        auto inherited = program->create(CV::ByteCodeType::PROXY);
        inherited->origin = nctx->id;
        inherited->data.push_back(0);
        nctx->addDisplayItem("inherited", inherited->id, nctx->id);


        auto target = params[0];
        auto targetToken = ParseInputToByteToken(target, program, nctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        targetToken->inheritModifiers(target);

        auto code = params[1];
        auto codeToken = ParseInputToByteToken(code, program, nctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        codeToken->inheritModifiers(code);

        auto ins = program->create(CV::ByteCodeType::CONSTRUCT_CALLBACK);
        ins->origin = ctx->id;
        ins->inheritModifiers(token);            
        ins->parameter.push_back(targetToken->id);
        ins->parameter.push_back(codeToken->id);
        ins->parameter.push_back(inherited->id);

        auto fins = postTokenCreationFinalization(program, ins, headMods, nctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins;
    }else
    if(imp == "mut"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <NAME> <VALUE>.");
            return program->create(CV::ByteCodeType::NOOP);
        }        
        auto param1 = params[1];
        auto target = params[0];


        auto ins = program->create(CV::ByteCodeType::MUT);
        ins->origin = ctx->id;
        ins->inheritModifiers(token);           

        auto tokenValue = ParseInputToByteToken(param1, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        // tokenValue->inheritModifiers(param1);

        auto tokenTarget = ParseInputToByteToken(target, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        // tokenTarget->inheritModifiers(target);



        ins->parameter.push_back(tokenTarget->id);
        ins->parameter.push_back(tokenValue->id);

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins;        
    }else
    if(imp == "set"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <NAME> <VALUE>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto param1 = params[1];
        auto name = params[0];


        auto v = ParseInputToByteToken(param1, program, ctx, cursor, false); // future tells it to not summon function calls basically
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        v->inheritModifiers(param1);

        auto ins = program->create(CV::ByteCodeType::SET);
        ins->origin = ctx->id;
        ins->inheritModifiers(token);           
        
        if(name.getModifierNumber() == 0){

            ins->data.push_back(0); // Zero means local Context set
            ins->str.push_back(params[0].first);        
            ctx->addDisplayItem(params[0].first, v->id);
            ins->parameter.push_back(0); 
        }else{
            ins->data.push_back(1); // One means remote Context
            auto targetIns = ParseInputToByteToken(name, program, ctx, cursor);
            targetIns->inheritModifiers(name);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
            ins->parameter.push_back(targetIns->id); 
        }

        ins->parameter.push_back(v->id); 

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "async" || imp == "untether"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        auto code = params[0];
        auto codeToken = ParseInputToByteToken(code, program, ctx, cursor);
        codeToken->inheritModifiers(code);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        auto ins = program->create(imp == "async" ? CV::ByteCodeType::SUMMON_ASYNC : CV::ByteCodeType::SUMMON_UNTETHER);
        ins->origin = ctx->id;
        ins->inheritModifiers(token);            
        ins->parameter.push_back(codeToken->id);
        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "do"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <[COND]> <[CODE]>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto cond = params[0];
        auto code = params[1];

        auto condToken = ParseInputToByteToken(cond, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        condToken->inheritModifiers(cond);

        auto codeToken = ParseInputToByteToken(code, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }  
        codeToken->expirable = false;              
        codeToken->inheritModifiers(code);

        auto ins = program->create(CV::ByteCodeType::COND_LOOP);
        ins->inheritModifiers(token);            
        ins->origin = ctx->id;

        ins->parameter.push_back(condToken->id);
        ins->parameter.push_back(codeToken->id);

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else       
    if(imp == "skip" || imp == "stop" || imp == "continue"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto ins = program->create(CV::ByteCodeType::INTERRUPTOR);
        ins->inheritModifiers(token);                  
        ins->origin = ctx->id;
        // First param is the type
        if(imp == "skip"){
            ins->data.push_back(CV::InterruptTypes::SKIP);
        }else
        if(imp == "stop"){
            ins->data.push_back(CV::InterruptTypes::STOP);
        }else
        if(imp == "continue"){
            ins->data.push_back(CV::InterruptTypes::CONTINUE);
        }
        // Second param is the payload (if any)
        if(params.size() > 0){
            auto payload = params[0];
            auto payloadToken = ParseInputToByteToken(payload, program, ctx, cursor);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }   
            payloadToken->inheritModifiers(payload);    
            ins->parameter.push_back(payloadToken->id);
        }else{
            ins->parameter.push_back(0);
        }
        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "if"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        if(params.size() > 3){
            cursor->setError(imp, "Expects no more than 3 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        if(params.size() < 2){
            cursor->setError(imp, "Expects no less than 2 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto cond = params[0];
        auto trueb = params[1];

        auto condToken = ParseInputToByteToken(cond, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        condToken->inheritModifiers(cond);

        auto truebToken = ParseInputToByteToken(trueb, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }          
        truebToken->inheritModifiers(trueb);      

        auto ins = program->create(CV::ByteCodeType::BINARY_BRANCH_COND);
        ins->inheritModifiers(headMods);                         
        ins->origin = ctx->id;
        ins->parameter.push_back(condToken->id);
        ins->parameter.push_back(truebToken->id);

        if(params.size() == 3){
            auto falseb = params[2];
            auto falsebToken = ParseInputToByteToken(falseb, program, ctx, cursor);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }  
            falsebToken->inheritModifiers(falseb);        
            ins->parameter.push_back(falsebToken->id);
        }

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "iter"){
        if(constructModCheck(token, cursor, true)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <[ITERABLE]> <[CODE]>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto iterStep = 1;
        if(token.hasModifier(CV::ModifierTypes::TRAIT)){
            auto mod = token.getLastModifier();
            if(mod->subject.size() == 0){
                cursor->setError(imp, "Provided modifier TRAIT must include a subject name.");
                return program->create(CV::ByteCodeType::NOOP);                
            }
            static const auto traitError = std::string("Provided modifier TRAIT's subject must be a positive number");
            if(!CV::Tools::isNumber(mod->subject)){
                cursor->setError(imp, traitError);
                return program->create(CV::ByteCodeType::NOOP); 
            }
            auto v = (int)std::stod(mod->subject);
            if(v < 0){
                cursor->setError(imp, traitError);
                return program->create(CV::ByteCodeType::NOOP); 
            }       
            iterStep = v;     
        }
        auto iter = params[0];
        auto code = params[1];
        // Allocate name
        auto namerId = 0;
        if(iter.hasModifier(CV::ModifierTypes::NAMER)){
            auto iterName = program->create(CV::ByteCodeType::PROXY);
            iterName->origin = ctx->id;
            iterName->data.push_back(0);
            auto &name = iter.getModifier(CV::ModifierTypes::NAMER)->subject;
            ctx->addDisplayItem(name, iterName->id);
            namerId = iterName->id;
        }
        // Compile iterator
        iter.clearModifiers();
        auto iterToken = ParseInputToByteToken(iter, program, ctx, cursor);

        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }

        // Compile code        
        auto codeToken = ParseInputToByteToken(code, program, ctx, cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }  
        codeToken->expirable = false; // Loopeable instructions cannot expire
        codeToken->inheritModifiers(code);              
        auto ins = program->create(CV::ByteCodeType::ITERATION_LOOP);
        ins->inheritModifiers(headMods);                         
        ins->origin = ctx->id;
        ins->parameter.push_back(iterToken->id);
        ins->parameter.push_back(codeToken->id);
        ins->parameter.push_back(namerId);
        ins->data.push_back(iterStep);

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "fn"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }        
        if(params.size() != 2){
            cursor->setError(imp, "Provided ("+std::to_string(params.size())+") argument(s). Expects exactly 2: <[ARGS]> <[CODE]>.");
            return program->create(CV::ByteCodeType::NOOP);
        }
        auto ins = program->create(CV::ByteCodeType::CONSTRUCT_FN);
        ins->origin = ctx->id;
        ins->inheritModifiers(headMods);
        // Process arguments (Each argument is slotted on the stack with its name and id)
        auto args = params[0];
        auto literals = parseTokens(args.first, ' ', cursor);
        bool variadic = false;
        if(literals.size() > 0 && literals[0] == "..."){
            if(literals.size() > 1){
                cursor->setError(imp, "Variadic functions cannot use more than 1 argument.");
                return program->create(CV::ByteCodeType::NOOP);
            }
            variadic = true;
        }
        ins->data.push_back(variadic);
        ins->data.push_back(variadic ? 1 : literals.size());
        ins->str.push_back(params[1].first);

        if(!variadic){
            for(int i = 0; i < literals.size(); ++i){
                if(!CV::Tools::isValidVarName(literals[i])){
                    cursor->setError(imp, "Invalid name '"+literals[i]+"'. The name cannot be a reserved word and/or contain prohibited symbols (Modifiers, numbers, brackets, etc).");
                    return program->create(CV::ByteCodeType::NOOP);                    
                }
                ins->str.push_back(literals[i]);
            }   
        }

        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins; 
    }else
    if(imp == "ct"){
        if(constructModCheck(token, cursor, false)){
            return program->create(CV::ByteCodeType::NOOP);
        }

        auto pins = program->create(CV::ByteCodeType::PROXY);
        pins->origin = ctx->id;
        pins->data.push_back(0); 
        

        auto ins = program->create(CV::ByteCodeType::CONSTRUCT_CTX);
        ins->origin = ctx->id;
        ins->parameter.push_back(pins->id);
        ins->inheritModifiers(token);

        for(int i = 0; i < params.size(); ++i){
            auto param = params[i];
            auto v = ParseInputToByteToken(param, program, ctx, cursor);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
            ins->parameter.push_back(v->id);
        }        
        
        auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return fins;   
    }else{
        unsigned origin = ctx->id;

        auto var = solveItem(token, origin, ctx, cursor);       
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); } 

        auto tproxy = ctx->findDisplayItem(imp);

        // Inline context switching
        if(token.getModifierNumber() > 0 && token.getLastModifier()->type == CV::ModifierTypes::SCOPE && token.getLastModifier()->subject == ""){
            if(!var.get()){
                cursor->setError(imp, GENERIC_UNDEFINED_SYMBOL_ERROR);
                return program->create(CV::ByteCodeType::NOOP);                    
            }
            
            if(params.size() == 0){
                cursor->setError(imp, "Inline context switching expects at least one imperative block.");
                return program->create(CV::ByteCodeType::NOOP);                
            }

            token.removeLastModifier(); //removes dangling scope modifier
            // Because this is supposed to be an already existing context, we should try solving it now
            CV::ModifierEffect effects;
            auto solved = processPreInterpretConversionModifiers(var, token.modifiers, cursor, ctx, effects);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }        

            if(var->type != CV::ItemTypes::CONTEXT){
                cursor->setError(imp, "Context Switching: This refers a non-context object.");
                return program->create(CV::ByteCodeType::NOOP);                     
            }
            
            auto nctx = std::static_pointer_cast<CV::Context>(solved);
            ctx->setStaticValue(nctx);

            for(int i = 0; i < params.size(); ++i){
                auto code = params[i];
                auto codeToken = ParseInputToByteToken(code, program, nctx, cursor);
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); } 
                if(i == params.size()-1){
                    return codeToken;
                }
            }
        }
        // If it's not a variable nor a displayValue then it's undefined
        if(!var.get() && !tproxy.get()){
            cursor->setError(imp, GENERIC_UNDEFINED_SYMBOL_ERROR);
            return program->create(CV::ByteCodeType::NOOP);   
        }       


        // Solve display Items (they don't exist yet)
        if(tproxy.get() && token.getModifierNumber() == 0 && params.size() == 0){

            auto ins = program->instructions[tproxy->insId];
            // If it's a proxy we send it its way
            if(ins->type == CV::ByteCodeType::PROXY){
                // std::cout << imp << " " << tproxy->name << " " << ins->literal() << " " << token.literal() << std::endl;
                ins->inheritModifiers(token);   
                auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                return fins; 
            }else
            if(var.get()){
                ctx->setStaticValue(var);  
                auto ins = program->create(CV::ByteCodeType::PROXY);                
                ins->origin = origin;
                ins->inheritModifiers(token);      
                ins->data.push_back(var->id);
                return ins;
            }else{
                // Othwerwise, we create a referred proxy
                auto ref = program->create(CV::ByteCodeType::REFERRED_PROXY);
                ref->origin = origin;
                ref->inheritModifiers(token);   
                ref->parameter.push_back(ins->id);
                auto fins = postTokenCreationFinalization(program, ref, headMods, ctx, cursor);        
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                return fins; 
            }
        }

        if(var.get()){
            ctx->setStaticValue(var);       
        }
        if(var.get() && var->type == CV::ItemTypes::FUNCTION && !future){

            auto fn = std::static_pointer_cast<CV::Function>(var);

            if(!fn->variadic && params.size() != fn->params.size()){
                cursor->setError(token.literal(), "Provided ("+std::to_string(params.size())+") argument(s). Expected exactly ("+std::to_string(fn->params.size())+") argument(s).");
                return program->create(CV::ByteCodeType::NOOP);
            }

            // create proxy that will store the result
            auto pins = program->create(CV::ByteCodeType::PROXY);                
            pins->origin = origin;    
            pins->data.push_back(0);      

            // create function instruction
            auto ins = program->create(CV::ByteCodeType::JMP_FUNCTION);
            ins->origin = origin;
            ins->inheritModifiers(token);   
            ins->parameter.push_back(pins->id);
            ins->data.push_back(var->id);
            ins->data.push_back(params.size());

            // If it's binary, we simply store the item's id and eval params
            if(fn->binary){
                for(int i = 0; i < params.size(); ++i){
                    auto param = params[i];
                    auto v = ParseInputToByteToken(param, program, ctx, cursor);
                    // v->inheritModifiers(param);
                    if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                    ins->parameter.push_back(v->id);
                }
            }else{
            // Non-binary so we compile right now
                ins->parameter.push_back(0);
                std::vector<unsigned> clearTemps;
                if(fn->variadic){
                    auto insArgs = program->create(CV::ByteCodeType::PROXY);  // points to an array                  
                    insArgs->origin = ctx->id;
                    insArgs->str.push_back("args");
                    insArgs->data.push_back(0); 
                    ctx->addDisplayItem("args", insArgs->id);
                    clearTemps.push_back(insArgs->id);
                    ins->parameter.push_back(insArgs->id);
                    for(int i = 0; i < params.size(); ++i){
                        auto param = params[i];
                        auto v = ParseInputToByteToken(param, program, ctx, cursor);
                        ins->parameter.push_back(v->id);
                    } 
                }else{
                    for(int i = 0; i < params.size(); ++i){
                        auto param = params[i];
                        auto v = ParseInputToByteToken(param, program, ctx, cursor);
                        ins->parameter.push_back(v->id);
                        ctx->addDisplayItem(fn->params[i], v->id);
                        clearTemps.push_back(v->id);
                    } 
                }

                auto bodyToken = ParseInputToByteToken(fn->body, program, ctx, cursor);
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }

                ins->parameter[1] = bodyToken->id;

                for(int i = 0; i < clearTemps.size(); ++i){
                    ctx->removeDisplayItem(clearTemps[i]);
                }                
            }




            auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
            return fins; 
        }else{
            if(params.size() == 0 && var.get()){

                auto ins = program->create(CV::ByteCodeType::PROXY);                
                ins->origin = origin;
                ins->inheritModifiers(token);      
                ins->data.push_back(var->id);

                auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }


                return fins; 
            }else{

                auto pins = program->create(CV::ByteCodeType::PROXY);
                pins->origin = ctx->id;
                pins->data.push_back(0);

                // Compile first param
                auto firstItem = ParseInputToByteToken(token, program, ctx, cursor);
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                firstItem->inheritModifiers(token);
                // Compile the rest
                auto ins = program->create(CV::ByteCodeType::CONSTRUCT_LIST);
                ins->origin = origin;
                ins->inheritModifiers(token);
                ins->parameter.push_back(pins->id);
                ins->parameter.push_back(firstItem->id);
                for(int i = 0; i < params.size(); ++i){
                    auto param = params[i];
                    auto compiled = ParseInputToByteToken(param, program, ctx, cursor);
                    compiled->inheritModifiers(param);
                    if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                    ins->parameter.push_back(compiled->id);
                }
                ctx->addDisplayItem(imp, ins->id);
                auto fins = postTokenCreationFinalization(program, ins, headMods, ctx, cursor);        
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                return fins; 
            }
        }
    }

    return program->create(CV::ByteCodeType::NOOP);

}

static std::shared_ptr<CV::TokenByteCode> ParseInputToByteToken(CV::Token &input, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool future){
    auto literals = parseTokens(input.first, ' ', cursor);
    if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }    
    auto tokens = buildTokens(literals, cursor);
    if(tokens.size() == 0){
        cursor->setError("", "Provided source with zero workable tokens.");
        return program->create(CV::ByteCodeType::NOOP);
    }

    
    // for(int i = 0; i < tokens.size(); ++i){
        // std::cout << tokens[i].literal() << std::endl;
    // }

    // std::cout << "Bye bye!" << std::endl;

    // std::exit(1);
    // If the first token is complex, it means we got a chain of commands (ie a program, or function)
    if(tokens.size() > 1 && tokens[0].complex){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        std::vector<std::shared_ptr<CV::TokenByteCode>> instructions;
        for(int i = 0; i < tokens.size(); ++i){
            auto &token = tokens[i];
            auto ins = ParseInputToByteToken(token, program, ctx, cursor, future);
            if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
            // Functions and contexts definitions are executed right away
            if( (ins->type == CV::ByteCodeType::SET && ins->data[0] == 0 && program->instructions[ins->parameter[1]]->type == CV::ByteCodeType::CONSTRUCT_CTX) ||
                (ins->type == CV::ByteCodeType::FORWARD_INS && program->instructions[ins->parameter[0]]->type == CV::ByteCodeType::CONSTRUCT_CTX) ){
                if(ins->type == CV::ByteCodeType::SET){
                    ctx->removeDisplayItem(ins->parameter[1]);
                }
                RunInstruction(ins, program, ctx, cursor);
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                tokens.erase(tokens.begin() + i); --i;
                continue;
            }else
            if( (ins->type == CV::ByteCodeType::SET && ins->data[0] == 0 && program->instructions[ins->parameter[1]]->type == CV::ByteCodeType::CONSTRUCT_FN) ||
                (ins->type == CV::ByteCodeType::FORWARD_INS && program->instructions[ins->parameter[0]]->type == CV::ByteCodeType::CONSTRUCT_FN) ){
                if(ins->type == CV::ByteCodeType::SET){
                    ctx->removeDisplayItem(ins->parameter[1]);
                }                    
                RunInstruction(ins, program, ctx, cursor);
                if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
                tokens.erase(tokens.begin() + i); --i;
                continue;
            }            
            if(i > 0){
                auto &before = instructions[instructions.size()-1];
                if(before->id == ins->id) continue; // avoid loops
                before->next = ins->id;
            }
            instructions.push_back(ins);
        }
        ctx->flushDisplayItems();
        return instructions[0];
    }else
    if(tokens.size() == 1 && tokens[0].complex){
        auto input = tokens[0].first;
        auto origMods = tokens[0].modifiers;
        while(input[0] == '[' && input[input.size()-1] == ']'){
            input = input.substr(1, input.length() - 2);
        }        
        auto literals = parseTokens(input, ' ', cursor);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); } 
        auto tokens = buildTokens(literals, cursor);
        if(tokens.size() == 0){
            cursor->setError("", "Provided source with zero workable tokens.");
            return program->create(CV::ByteCodeType::NOOP);
        }           
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto v = ProcessToken(program, tokens[0], params, origMods, ctx, cursor, future);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return v;
    }else{
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto v = ProcessToken(program, tokens[0], params, input.modifiers, ctx, cursor, future);
        if(cursor->error){ return program->create(CV::ByteCodeType::NOOP); }
        return v;
    }    
}


static std::shared_ptr<CV::TokenByteCode> ParseInputToByteToken(const std::string &input, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool future){
    CV::Token token;
    token.first = input;
    return ParseInputToByteToken(token, program, ctx, cursor, future);
}

static std::shared_ptr<CV::Item> RunInstruction(std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);

static std::shared_ptr<CV::Item> Execute(std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    auto last = std::make_shared<CV::Item>();
    // ctx->setStaticValue(ctx);
    unsigned next = entry->id;
    do {
        auto current = program->instructions[next];
        last = RunInstruction(current, program, ctx, cursor);
        if(cursor->error){
            break;
        }
        if(last->type == CV::ItemTypes::INTERRUPT){
            break;
        }
        next = current->next;
    } while(next != 0);
    // program->instructions.clear();
    // ctx->reset(true);
    return last;
}

static std::shared_ptr<CV::Item> unwrapInterrupt(const std::shared_ptr<CV::Item> &item){
    if(item->type != CV::ItemTypes::INTERRUPT){
        return item;
    }else
    if(!std::static_pointer_cast<CV::Interrupt>(item)->hasPayload()){
        return item;
    }else{
        return std::static_pointer_cast<CV::Interrupt>(item)->getPayload();
    }
}

static uint64_t setTimes = 0;
static uint64_t ctxSetTimes = 0;
static uint64_t mutTimes = 0;
static uint64_t mutTargetTimes = 0;
static uint64_t mutValueTimes = 0;
static uint64_t jumpFnTimes = 0;
static uint64_t forwardInsTimes = 0;
static uint64_t forwardInsMods = 0;
static uint64_t solveLocalCtxTimes = 0;
static uint64_t referredProxyTimes = 0;
static uint64_t proxyTimes = 0;




void showTimes(){
    std::cout << "\n\n\n";
    std::cout << "Set times " << setTimes << std::endl;
    std::cout << "ctxSetTimes  " << ctxSetTimes << std::endl;
    std::cout << "Mut times " << mutTimes << std::endl;
    std::cout << "mutTargetTimes " << mutTargetTimes << std::endl;
    std::cout << "mutValueTimes " << mutValueTimes << std::endl;
    std::cout << "forwardInsTimes " << forwardInsTimes << std::endl;
    std::cout << "forwardInsMods " << forwardInsMods << std::endl;
    std::cout << "solveLocalCtxTimes " << solveLocalCtxTimes << std::endl;
    std::cout << "referredProxyTimes " << referredProxyTimes << std::endl;
    std::cout << "proxyTimes " << proxyTimes << std::endl;



    
    std::cout << "jumpfn times " << jumpFnTimes << std::endl;
}

static std::shared_ptr<CV::Item> RunInstruction(std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program, std::shared_ptr<CV::Context> &_ctx, std::shared_ptr<CV::Cursor> &cursor){
    auto t = CV::Tools::ticks();
    auto ctx = solveLocalCtx(entry, _ctx);
    solveLocalCtxTimes += CV::Tools::ticks()-t;
    switch(entry->type){

        case CV::ByteCodeType::JMP_FUNCTION: {
            std::shared_ptr<CV::Function> fn = std::static_pointer_cast<CV::Function>(ctx->getStaticValue(entry->data[0])); 
            auto resultProxy = program->instructions[entry->parameter[0]];
            std::vector<std::shared_ptr<CV::Item>> arguments;
            auto tctx = std::make_shared<CV::Context>(ctx);
            
            // Binary Functions
            if(fn->binary){
                bool insbin = false;
                for(int i = 1; i < entry->parameter.size(); ++i){
                    auto ins = program->instructions[entry->parameter[i]];

            uint64_t __start = CV::Tools::ticks();

                    auto v = RunInstruction(ins, program, tctx, cursor);
            if(ins->type == CV::ByteCodeType::JMP_FUNCTION){
                insbin  = true;
                jumpFnTimes += CV::Tools::ticks()-__start;      
            }


                    if(cursor->error){ return std::make_shared<CV::Item>(); }     
                    CV::ModifierEffect effects;               
                    ins->resetModifiers();
                    auto solved = processPreInterpretSituationalModifiers(v, ins->modifiers, cursor, ctx, effects);
                    if(cursor->error){ return std::make_shared<CV::Item>(); }                
                    processPostInterpretExpandListOrContext(solved, effects, arguments, cursor, ctx);
                    if(cursor->error){ return std::make_shared<CV::Item>(); }      
                }
                auto s = CV::Tools::ticks();
                auto result = fn->fn(arguments, cursor, tctx);
                if(insbin){
                jumpFnTimes += CV::Tools::ticks()-s;
                }
                if(cursor->error){ return std::make_shared<CV::Item>(); } 
                resultProxy->data[0] = result->id; 


                return ctx->setStaticValue(unwrapInterrupt(result));


            // ByteCode Functions
            }else{
                auto codeToken = program->instructions[entry->parameter[1]];
                if(fn->variadic){
                    auto nArgs = entry->data[1];
                    auto lstProxyIns = program->instructions[entry->parameter[2]];
                    for(int i = 3; i < 3+nArgs; ++i){
                        auto ins = program->instructions[entry->parameter[i]];

                        auto v = RunInstruction(ins, program, tctx, cursor);

                        if(cursor->error){ return std::make_shared<CV::Item>(); }     
                        CV::ModifierEffect effects;               
                        ins->resetModifiers();
                        auto solved = processPreInterpretSituationalModifiers(v, ins->modifiers, cursor, ctx, effects);
                        if(cursor->error){ return std::make_shared<CV::Item>(); }                
                        processPostInterpretExpandListOrContext(solved, effects, arguments, cursor, ctx);
                        if(cursor->error){ return std::make_shared<CV::Item>(); }  
                    }
                    auto variadicList = std::make_shared<CV::List>();
                    variadicList->set(arguments);
                    ctx->setStaticValue(variadicList);
                    lstProxyIns->data[0] = variadicList->id;
                }


                auto result = Execute(codeToken, program, ctx, cursor);

                resultProxy->data[0] = result->id;
                if(cursor->error){ return std::make_shared<CV::Item>(); }  


                return ctx->setStaticValue(unwrapInterrupt(ctx->setStaticValue(result)));
            }
        } break;
        case CV::ByteCodeType::CONSTRUCT_CALLBACK: {
            
            auto targetIns = program->instructions[entry->parameter[0]];
            auto targetJobIns = RunInstruction(targetIns, program, ctx, cursor);            
            if(cursor->error){ return std::make_shared<CV::Item>(); }        

            if(targetJobIns->type != CV::ItemTypes::JOB){
                cursor->setError(CV::ByteCodeType::str(entry->type), "Target type is not a Job.");
                return std::make_shared<CV::Item>();
            }

            auto targetJob = std::static_pointer_cast<CV::Job>(targetJobIns);

            auto job = std::make_shared<CV::Job>();
            job->isCallback = true;
            job->ready = false;
            job->cursor = std::make_shared<CV::Cursor>();
            job->set(targetJob->jobType, program->instructions[entry->parameter[1]], targetJob->program);
            ctx->addJob(job);        
            ctx->setStaticValue(job);
            targetJob->setCallback(job);
            targetJob->inherited = entry->parameter[2]; // Proxy for the "inherited"

            auto item = std::static_pointer_cast<CV::Item>(job);
            CV::ModifierEffect effects;
            auto solved = processPreInterpretConversionModifiers(item, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }                
            if(solved->id != item->id){
                ctx->setStaticValue(solved);
            }
            
            return solved;
        } break;
        case CV::ByteCodeType::SUMMON_ASYNC:
        case CV::ByteCodeType::SUMMON_UNTETHER: {
            auto job = std::make_shared<CV::Job>();
            auto c = std::make_shared<CV::Cursor>();
            job->cursor = c;
            job->set(entry->type == CV::ByteCodeType::SUMMON_ASYNC ? CV::JobType::ASYNC : CV::JobType::THREAD, program->instructions[entry->parameter[0]], program);
            ctx->addJob(job);        
            ctx->setStaticValue(job);
            auto item = std::static_pointer_cast<CV::Item>(job);
            CV::ModifierEffect effects;
            auto solved = processPreInterpretConversionModifiers(item, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }                
            if(solved->id != item->id){
                ctx->setStaticValue(solved);
            }
            return solved;
        } break;
        case CV::ByteCodeType::FORWARD_INS: {
            uint64_t __start = CV::Tools::ticks();

            auto ins = program->instructions[entry->parameter[0]];

            auto v = RunInstruction(ins, program, ctx, cursor);

            // std::cout << ins->literal() << std::endl;

            if(cursor->error){ return std::make_shared<CV::Item>(); }  
            uint64_t __start2 = CV::Tools::ticks();

            CV::ModifierEffect effects;
            auto solved = processPreInterpretConversionModifiers(v, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }  
            solved = processPreInterpretSituationalModifiers(solved, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }     

            forwardInsMods += CV::Tools::ticks()-__start2;      

            if(entry->hasModifier(CV::ModifierTypes::NAMER)){
                if(ctx->readOnly){
                    cursor->setError(entry->getModifier(CV::ModifierTypes::NAMER)->subject, "Cannot MAME within this context as it is readonly.");
                    return std::make_shared<CV::Item>();                  
                }                 
                ctx->set(entry->getModifier(CV::ModifierTypes::NAMER)->subject, solved);
            }
            forwardInsTimes += CV::Tools::ticks()-__start;      
            ctx->setStaticValue(solved);
            return solved;
        } break;
        case CV::ByteCodeType::PROXY: {    
            auto __start = CV::Tools::ticks();    
            auto v = ctx->getStaticValue(entry->data[0]);
            if(!v.get()){
                cursor->setError(CV::ByteCodeType::str(entry->type), "Refers to an undefined value.");
                return std::make_shared<CV::Item>(); 
            }
            CV::ModifierEffect effects;
            if(entry->hasModifier(CV::ModifierTypes::NAMER)){
                if(ctx->readOnly){
                    cursor->setError(entry->getModifier(CV::ModifierTypes::NAMER)->subject, "Cannot MAME within this context as it is readonly.");
                    return std::make_shared<CV::Item>();                  
                }                 
                ctx->set(entry->getModifier(CV::ModifierTypes::NAMER)->subject, v);
            }
            auto solved = processPreInterpretConversionModifiers(v, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }        
            solved = processPreInterpretSituationalModifiers(solved, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }   
            proxyTimes += CV::Tools::ticks() - __start;                    
            return solved;
        } break;
        case CV::ByteCodeType::REFERRED_PROXY: {
            auto __start = CV::Tools::ticks();
            auto ins = program->instructions[entry->parameter[0]];
            auto v = RunInstruction(ins, program, ctx, cursor);
            if(cursor->error){ return std::make_shared<CV::Item>(); }  
            CV::ModifierEffect effects;
            auto solved = processPreInterpretConversionModifiers(v, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }        
            solved = processPreInterpretSituationalModifiers(solved, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }
            if(ins->type == CV::ByteCodeType::JMP_FUNCTION) referredProxyTimes += CV::Tools::ticks()-__start;   
            return solved;
        } break;
        case CV::ByteCodeType::CONSTRUCT_FN: {
            auto fn = std::make_shared<CV::Function>();
            // Get argument names
            auto isVariadic = entry->data[0];
            auto nArgs = entry->data[1];
            std::vector<std::string> argList;
            if(!isVariadic){
                for(int i = 1; i < 1+nArgs; ++i){
                    argList.push_back(entry->str[i]);
                }
            }
            fn->set(argList, entry->str[0], isVariadic);
            auto item = std::static_pointer_cast<CV::Item>(fn);
            return ctx->setStaticValue(item);
        } break;
        case CV::ByteCodeType::CONSTRUCT_CTX: {
            auto nctx = std::make_shared<CV::Context>(ctx);
            std::unordered_map<std::string, std::shared_ptr<CV::Item>> items;
            auto item = std::static_pointer_cast<CV::Item>(nctx);
            for(int i = 1; i < entry->parameter.size(); ++i){
                auto ins = program->instructions[entry->parameter[i]];
                auto v = RunInstruction(ins, program, ctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); }   
                CV::ModifierEffect effects;
                auto solved = processPreInterpretConversionModifiers(v, ins->modifiers, cursor, ctx, effects);
                if(cursor->error){ return std::make_shared<CV::Item>(); }                
                solved = processPreInterpretSituationalModifiers(v, ins->modifiers, cursor, ctx, effects);
                if(cursor->error){ return std::make_shared<CV::Item>(); }                

                std::vector<std::shared_ptr<CV::Item>> all;
                processPostInterpretExpandListOrContext(solved, effects, all, cursor, nctx); 
                if(cursor->error){ return std::make_shared<CV::Item>(); }
                // get name
                auto name = "v"+std::to_string(i);
                if(ins->hasModifier(CV::ModifierTypes::NAMER)){
                    auto mod = ins->getModifier(CV::ModifierTypes::NAMER);
                    name = mod->subject;
                }                
                // add to queue
                for(int j = 0; j < all.size(); ++j){
                    items[name + (j > 0 ? std::to_string(j) : "")] = all[j];
                }
            }
            for(auto &it : items){
                nctx->set(it.first, it.second);
            }
            ctx->setStaticValue(item);            
            
            CV::ModifierEffect effects;          
            auto solved = processPreInterpretConversionModifiers(item, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); } 

            program->instructions[entry->parameter[0]]->data[0] = solved->id;
            return ctx->setStaticValue(solved);
        } break;
        case CV::ByteCodeType::CONSTRUCT_LIST: {
            std::vector<std::shared_ptr<CV::Item>> items;
            for(int i = 1; i < entry->parameter.size(); ++i){
                auto ins = program->instructions[entry->parameter[i]];
                auto v = RunInstruction(ins, program, ctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); }     
                CV::ModifierEffect effects;          
                auto solved = processPreInterpretSituationalModifiers(v, ins->modifiers, cursor, ctx, effects);
                if(cursor->error){ return std::make_shared<CV::Item>(); }                
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, ctx);
                if(cursor->error){ return std::make_shared<CV::Item>(); }  
            }
            auto result = std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));
            ctx->setStaticValue(result);            
            // Apply modifiers to final list (if any)
            CV::ModifierEffect effects;          
            auto solved = processPreInterpretConversionModifiers(result, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }                
            program->instructions[entry->parameter[0]]->data[0] = solved->id;
            return  ctx->setStaticValue(solved);            
        };
        case CV::ByteCodeType::BINARY_BRANCH_COND: {
            auto tctx = std::make_shared<CV::Context>(ctx);
            auto cond = RunInstruction(program->instructions[entry->parameter[0]], program, tctx, cursor);
            if(cursor->error){ return std::make_shared<CV::Item>(); }
            if(checkCond(cond)){
                auto v = RunInstruction(program->instructions[entry->parameter[1]], program, tctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); }
                return ctx->setStaticValue(v);            
            }else
            if(entry->parameter.size() > 2){
                auto v = RunInstruction(program->instructions[entry->parameter[2]], program, tctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); }
                return ctx->setStaticValue(v);                  
            }else{
                return ctx->setStaticValue(std::make_shared<CV::Item>());
            }
        } break;

        case CV::ByteCodeType::COND_LOOP: {
            auto tctx = std::make_shared<CV::Context>(ctx);
            auto cond = RunInstruction(program->instructions[entry->parameter[0]], program, tctx, cursor);
            if(cursor->error){ return std::make_shared<CV::Item>(); }
            auto last = ctx->setStaticValue(std::make_shared<CV::Item>());
            while(checkCond(cond)){
                last = Execute(program->instructions[entry->parameter[1]], program, tctx, cursor);
                if(cursor->error){ return last; }  
                if(last->type == CV::ItemTypes::INTERRUPT && std::static_pointer_cast<CV::Interrupt>(last)->intType == CV::InterruptTypes::STOP){
                    break;
                }
                cond = RunInstruction(program->instructions[entry->parameter[0]], program, tctx, cursor);
                if(cursor->error){ return last; }                
            }
            return unwrapInterrupt(last);
        } break;

        case CV::ByteCodeType::INTERRUPTOR: {
            if(entry->parameter[0] != 0){
                auto ins = program->instructions[entry->parameter[0]];
                auto v = RunInstruction(ins, program, ctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); }                
                if(v->type == CV::ItemTypes::INTERRUPT){
                    return v;
                }
                auto interrupt = std::make_shared<CV::Interrupt>(entry->data[0]);
                interrupt->setPayload(v);
                return interrupt;
            }else{
                return std::make_shared<CV::Interrupt>(entry->data[0]);
            }
        } break;

        case CV::ByteCodeType::ITERATION_LOOP: {
            auto tctx = std::make_shared<CV::Context>(ctx);
            // Iterator
            auto iterToken = program->instructions[entry->parameter[0]];
            auto iteration = RunInstruction(iterToken, program, tctx, cursor);
            if(cursor->error){ return std::make_shared<CV::Item>(); }

            auto iterStep = entry->data[0];
            
            if(iteration->type != CV::ItemTypes::LIST){
                cursor->setError(CV::ByteCodeType::str(entry->type), "Provided type '"+CV::ItemTypes::str(iteration->type)+"' as iteratable. Iteratable must be a list.");
                return std::make_shared<CV::Item>();                
            }


            // Name
            auto updNamer = [&](std::shared_ptr<CV::Item> &item){
                if(entry->parameter[2] == 0) return;
                // Get proxy instruction
                auto &proxy = program->instructions[entry->parameter[2]];
                tctx->setStaticValue(item);
                // Update value
                proxy->data[0] = item->id;
            };

           auto updListIter = [&](std::shared_ptr<CV::List> &src, std::shared_ptr<CV::List> &target, int index, int n){
                target->clear();
                src->accessMutex.lock();
                target->accessMutex.lock();
                for(int i = 0; i < n; ++i){
                    target->data.push_back(src->data[index + i]);
                }
                target->accessMutex.unlock();
                src->accessMutex.unlock();
                return std::static_pointer_cast<CV::Item>(target);
            };



            auto lstIter = std::make_shared<CV::List>(std::vector<std::shared_ptr<CV::Item>>({}));
            auto iterList = std::static_pointer_cast<CV::List>(iteration);
            auto last = ctx->setStaticValue(std::make_shared<CV::Item>());
            auto lastIndex = 0;
            

            if(iterStep > 1 && iterList->size() % iterStep != 0){
                cursor->setError(CV::ByteCodeType::str(entry->type), "Provided a step ("+std::to_string(iterStep)+") that's not evenly divisible to the number of elements in the iterable ("+std::to_string(std::static_pointer_cast<CV::List>(iteration)->size())+").");
                return std::make_shared<CV::Item>();                
            }

            auto next = (iterStep == 1) ? iterList->get(lastIndex) : updListIter(iterList, lstIter, lastIndex, iterStep) ;
            updNamer( next );

            while(true){
                last = Execute(program->instructions[entry->parameter[1]], program, tctx, cursor);
                if(cursor->error){ return std::make_shared<CV::Item>(); } 
                if(last->type == CV::ItemTypes::INTERRUPT && std::static_pointer_cast<CV::Interrupt>(last)->intType == CV::InterruptTypes::STOP){
                    break;
                }

                if((lastIndex+iterStep) >= iterList->size()-1){
                    break;
                }                

                lastIndex += iterStep;

                if(iterStep == 1){
                    auto n = iterList->get(lastIndex);
                    updNamer( n );
                }else{
                    updListIter(iterList, lstIter, lastIndex, iterStep);
                }

    
            }
            last = unwrapInterrupt(last);
            return ctx->setStaticValue(last);
        } break;        

        case CV::ByteCodeType::MODIFIER_DIG_PROXY: {
            auto item = entry->data[1] ? ctx->getStaticValue(entry->data[0]) : ctx->get(entry->str[0]);
            if(!item.get()){
                cursor->setError(entry->str[0], GENERIC_UNDEFINED_SYMBOL_ERROR);
                return std::make_shared<CV::Item>();
            }
            CV::ModifierEffect effects;               
            auto solved = processPreInterpretConversionModifiers(item, entry->modifiers, cursor, ctx, effects);
            if(cursor->error){ return std::make_shared<CV::Item>(); }                
            return solved;
        } break;

        case CV::ByteCodeType::MUT: {
            uint64_t __start = CV::Tools::ticks();
            
            auto targetIns = program->instructions[entry->parameter[0]];

            auto valueIns = program->instructions[entry->parameter[1]];


            uint64_t __start3 = CV::Tools::ticks();
            auto v = RunInstruction(valueIns, program, ctx, cursor);
            if(valueIns->type == CV::ByteCodeType::JMP_FUNCTION) mutValueTimes += CV::Tools::ticks()-__start3;      


            if(cursor->error){ return std::make_shared<CV::Item>(); }                
            
            uint64_t __start2 = CV::Tools::ticks();            
            auto t = RunInstruction(targetIns, program, ctx, cursor);
            mutTargetTimes += CV::Tools::ticks()-__start2;      

            if(cursor->error){ return std::make_shared<CV::Item>(); }

            if(v->type != CV::ItemTypes::NUMBER && v->type != CV::ItemTypes::STRING || t->type != v->type){
                bool isMismatch = t->type != v->type;
                auto err = "Provided type is '"+CV::ItemTypes::str(v->type)+"', target type is '"+CV::ItemTypes::str(t->type)+"'. Only types 'STRING' and 'NUMBER' can be mutated, AND new & old values must match types"+(isMismatch ? "(They mismatch)" : "")+".";
                cursor->setError(CV::ByteCodeType::str(entry->type), err);
                return std::make_shared<CV::Item>();
            }
            
            if(t->type == CV::ItemTypes::NUMBER){
                std::static_pointer_cast<CV::Number>(t)->set(std::static_pointer_cast<CV::Number>(v)->get());
            }else
            if(t->type == CV::ItemTypes::STRING){
                std::static_pointer_cast<CV::String>(t)->set(std::static_pointer_cast<CV::String>(v)->get());
            }else{
                cursor->setError(CV::ByteCodeType::str(entry->type), "Fatal error: Mutating non-mutable types(????? this shouldn't happen)");
                return std::make_shared<CV::Item>();
            }

            mutTimes += CV::Tools::ticks()-__start;      


            return t;
        } break;

        case CV::ByteCodeType::SET: {

            uint64_t __start = CV::Tools::ticks();
            auto type = entry->data[0];
            auto tIns = entry->parameter[0];
            auto &proxy = program->instructions[entry->parameter[1]]; // 1 is the actual value

            auto v = RunInstruction(proxy, program, ctx, cursor);
            if(cursor->error){ return std::make_shared<CV::Item>(); }                

            ctx->setStaticValue(v);


            // Set to local context
            if(type == 0){
                if(ctx->readOnly){
                    cursor->setError(CV::ByteCodeType::str(entry->type), "Cannot SET within this context as it is readonly.");
                    return std::make_shared<CV::Item>();                  
                }
                if(!CV::Tools::isValidVarName(entry->str[0])){
                    cursor->setError(entry->str[0], "Invalid name '"+entry->str[0]+"'. The name cannot be a reserved word and/or contain prohibited symbols (Modifiers, numbers, brackets, etc).");
                    return std::make_shared<CV::Item>();                           
                }
                uint64_t __start = CV::Tools::ticks();
                ctx->set(entry->str[0], v);
                ctxSetTimes += CV::Tools::ticks()-__start;      
            // Set within a context
            }else{
                auto name = program->instructions[entry->parameter[0]];
                auto target = RunInstruction(name, program, ctx, cursor);
                auto rmtCtx = std::static_pointer_cast<CV::Context>(ctx->getStaticValue(name->origin));

                if(rmtCtx->readOnly){
                    cursor->setError(CV::ByteCodeType::str(entry->type), "Cannot SET within this context "+(std::to_string(name->origin))+" as it is readonly.");
                    return std::make_shared<CV::Item>();                  
                } 
                auto originName = rmtCtx->getNameById(target->id);
                uint64_t __start = CV::Tools::ticks();
                rmtCtx->set(originName, v);
                ctxSetTimes += CV::Tools::ticks()-__start;      

            }
            setTimes += CV::Tools::ticks()-__start;      
            return v;
        };
        case CV::ByteCodeType::NOOP: {
            // Does nothing
            return std::make_shared<CV::Item>();
        } break;
        default: {
            cursor->setError(CV::ByteCodeType::str(entry->type), "Unimplemented instruction.");
            return std::make_shared<CV::Item>();
        };
    }
    return std::make_shared<CV::Item>();
}

std::shared_ptr<CV::Item> CV::interpret(const std::string &input, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool flushTemps){
    auto program = std::make_shared<CV::Stack>();

    // if(c == 1){

    //     std::cout << CV::ItemToText(ctx->get("m").get()) << std::endl;
    //     ctx->debug();
    //     std::exit(1);

    // }


    auto code = ParseInputToByteToken(input, program, ctx, cursor);
    ctx->flushDisplayItems();

    // DebugByteCode(code, ctx.get(), program.get());
    
    // std::cout << "\n\n\n" << std::endl;
    // std::exit(1);
    // std::cout << code->literal(ctx.get(), program.get()) << std::endl;


    if(cursor->error){
        return std::make_shared<CV::Item>();
    }
    auto v = unwrapInterrupt(Execute(code, program, ctx, cursor));
    if(cursor->error){
        return std::make_shared<CV::Item>();
    }



    // ctx->debug();

    return v;
}



void CV::flushContextTemps(std::shared_ptr<CV::Context> &ctx){
    ctx->flushDisplayItems();
    // ctx->flushStaticValue();   
}

