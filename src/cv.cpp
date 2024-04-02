#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <cmath>
#include <mutex> 
#include <io.h>

#include "CV.hpp"

static bool useColorOnText = false;


namespace CV {
    namespace Tools {

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
            return (s.find_first_not_of( "-.0123456789") == std::string::npos);
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






CV::Cursor::Cursor(){
    clear();
    line = 1;
}

void CV::Cursor::clear(){
    error = false;
    message = "";
}

void CV::Cursor::setError(const std::string &v, int line){
    if(line != -1){
        this->line = line;
    }
    this->error = true;
    this->message = Tools::setTextColor(Tools::Color::RED) +
                    v +
                    Tools::setTextColor(Tools::Color::RESET);
}





/*

    TRAIT

*/
CV::Trait::Trait(){
    this->type = CV::TraitType::SPECIFIC;
    this->action = [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        return std::shared_ptr<CV::Item>(NULL);
    };
}

/* 

    ITEM

*/
CV::Item::Item(){
    registerTraits();
    this->copyable = true;
    this->type = CV::ItemTypes::NIL;
}

void CV::Item::registerTraits(){
    this->registerTrait("copy", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        effects.postEval = false;
        return std::static_pointer_cast<CV::Item>(subject->copy(true));
    });
    this->registerTrait("type", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        return std::static_pointer_cast<CV::Item>( std::make_shared<CV::String>(  CV::ItemTypes::str(subject->type)  ));
    });    
}

void CV::Item::registerTrait(uint8_t type, const std::function<std::shared_ptr<CV::Item>(Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects)> &action){
    this->accessMutex.lock();
    Trait trait;
    trait.action = action;
    trait.type = type;
    this->traitsAny[type] = trait;
    this->accessMutex.unlock();
}    

void CV::Item::registerTrait(const std::string &name, const std::function<std::shared_ptr<CV::Item>(Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects)> &action){
    this->accessMutex.lock();
    Trait trait;
    trait.name = name;
    trait.action = action;
    this->traits[name] = trait;
    this->accessMutex.unlock();
}

std::shared_ptr<CV::Item> CV::Item::runTrait(const std::string &name, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    auto &trait = this->traits.find(name)->second;
    return trait.action( this, value, cursor, ctx, effects);
}

std::shared_ptr<CV::Item> CV::Item::runTrait(uint8_t type, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    return this->traitsAny.find(type)->second.action(this, value, cursor, ctx, effects);
}    

bool CV::Item::hasTrait(const std::string &name){
    this->accessMutex.lock();
    auto it = this->traits.find(name);
    if(it == this->traits.end()){
        this->accessMutex.unlock();
        return false;
    }
    this->accessMutex.unlock();
    return true;
}

bool CV::Item::hasTrait(uint8_t type){
    this->accessMutex.lock();
    auto it = this->traitsAny.find(type);
    if(it == this->traitsAny.end()){
        this->accessMutex.unlock();
        return false;
    }
    this->accessMutex.unlock();
    return true;
}

bool CV::Item::isEq(std::shared_ptr<CV::Item> &item){
    return (this->type == CV::ItemTypes::NIL && item->type == CV::ItemTypes::NIL) || this == item.get();
}

std::shared_ptr<CV::Item> CV::Item::copy(bool deep){
    return std::make_shared<CV::Item>(CV::Item());
}

/*

    INTERRUPT

*/
CV::Interrupt::Interrupt(uint8_t intType){
    this->type = CV::ItemTypes::INTERRUPT;
    this->intType = intType;
}

std::shared_ptr<CV::Item> CV::Interrupt::copy(bool deep){
    auto item = std::make_shared<CV::Interrupt>(CV::Interrupt(this->intType));
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
    registerTraits();
}

CV::Number::Number(__CV_NUMBER_NATIVE_TYPE v){
    registerTraits();        
    set(v);
}

bool CV::Number::isEq(std::shared_ptr<CV::Item> &item){
    this->accessMutex.lock();
    bool v = item->type == this->type && this->n == std::static_pointer_cast<CV::Number>(item)->get();
    this->accessMutex.unlock();
    return v;
}    

void CV::Number::registerTraits(){
    this->registerTrait("is-neg", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(static_cast<Number*>(subject)->get() < 0));
    });
    this->registerTrait("inv", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(static_cast<Number*>(subject)->get() * -1));
    });    
    this->registerTrait("is-odd", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( (int)static_cast<Number*>(subject)->get() % 2 != 0));
    });   
    this->registerTrait("is-even", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( (int)static_cast<Number*>(subject)->get() % 2 == 0));
    });                       
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
    registerTraits();
}

CV::List::List(const std::vector<std::shared_ptr<CV::Item>> &list, bool toCopy){
    registerTraits();        
    set(list, toCopy);
}

size_t CV::List::size(){
    this->accessMutex.lock();
    auto n = this->data.size();
    this->accessMutex.unlock();
    return n;
}

void CV::List::registerTraits(){
    this->registerTrait(CV::TraitType::ANY_NUMBER, [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto list = static_cast<List*>(subject); 
        auto index = std::stod(value);
        if(index < 0 || index >= list->size()){
            cursor->setError("'List|*ANY_NUMBER*': Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(list->size())+")");
            return std::make_shared<CV::Item>(CV::Item());
        }
        return list->get(index); 
    });        
    this->registerTrait("length", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto list = static_cast<List*>(subject); 
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(list->size()));
    });
    this->registerTrait("reverse", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        std::vector<std::shared_ptr<CV::Item>> result;
        auto list = static_cast<List*>(subject); 
        list->accessMutex.lock();
        for(int i = 0; i < list->data.size(); ++i){
            result.push_back(list->data[list->data.size()-1 - i]);
        }
        list->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result));
    });                           
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
    registerTraits();
}

size_t CV::String::size(){
    this->accessMutex.lock();
    auto n = this->data.size();
    this->accessMutex.unlock();
    return n;
}

CV::String::String(const std::string &str){
    registerTraits();
    this->set(str);
}    

void CV::String::registerTraits(){ 
    this->registerTrait(CV::TraitType::ANY_NUMBER, [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto str = static_cast<String*>(subject)->get();
        auto index = std::stod(value);
        if(index < 0 || index >= str.size()){
            cursor->setError("'String|*ANY_NUMBER*': Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(str.size())+")");
            return std::make_shared<CV::Item>(CV::Item());
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  std::string("")+str[index]  ));
    });           
    this->registerTrait("length", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto str = static_cast<String*>(subject);
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( str->size() ));
    });
    this->registerTrait("list", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        std::vector<std::shared_ptr<CV::Item>> result;
        auto str = static_cast<String*>(subject)->get();

        for(int i = 0; i < str.size(); ++i){
            result.push_back(std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  std::string("")+str[i]  )));
        }


        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(result));
    });        
    this->registerTrait("reverse", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        std::string result;
        auto str = static_cast<String*>(subject);

        auto copy = str->get();

        for(int i = 0; i < copy.size(); ++i){
            result += copy[copy.size()-1 - i];
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  result  ));

    });                           
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
    registerTraits();
}

CV::Function::Function(const std::vector<std::string> &params, const std::string &body, bool variadic){
    registerTraits();
    set(params, body, variadic);
}

CV::Function::Function(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx)> &fn, bool variadic){
    registerTraits();
    set(params, fn, variadic);
}    


std::shared_ptr<CV::Item> CV::Function::copy(bool deep) {
    this->accessMutex.lock();
    auto fn = std::make_shared<Function>(Function());
    fn->params = this->params;
    fn->body = this->body;
    fn->binary = this->binary;
    fn->variadic = this->variadic;
    fn->threaded = this->threaded;
    fn->async = this->async;
    fn->type = this->type;
    fn->fn = this->fn;
    this->accessMutex.unlock();
    return std::static_pointer_cast<Item>(fn);
}

std::string CV::Function::getBody(){
     this->accessMutex.lock();
    auto copy = this->body;
    this->accessMutex.unlock();
    return copy;   
}

std::vector<std::string> CV::Function::getParams(){
    this->accessMutex.lock();
    auto copy = this->params;
    this->accessMutex.unlock();
    return copy;
}

void CV::Function::registerTraits(){
    this->registerTrait("is-variadic", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto func = static_cast<Function*>(subject);
        subject->accessMutex.lock();
        auto v = func->variadic;
        subject->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v ));
    });

    this->registerTrait("is-binary", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto func = static_cast<Function*>(subject);
        subject->accessMutex.lock();
        auto v = func->binary;
        subject->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v ));
    }); 

    this->registerTrait("async", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto copy = std::static_pointer_cast<Function>(static_cast<Function*>(subject)->copy());
        copy->async = true;
        copy->threaded = false;
        return std::static_pointer_cast<CV::Item>(copy);
    }); 

    this->registerTrait("untether", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto copy = std::static_pointer_cast<Function>(static_cast<Function*>(subject)->copy());
        copy->async = false;
        copy->threaded = true;
        return std::static_pointer_cast<CV::Item>(copy);
    });     

    this->registerTrait("n-args", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto func = static_cast<Function*>(subject);
        func->accessMutex.lock();
        auto v = func->params.size();
        func->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>( v  ));            
    });   

    this->registerTrait("proto", [](Item *subject, const std::string &value, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
        if(subject->type == CV::ItemTypes::NIL){
            return std::make_shared<Item>();
        }
        auto func = static_cast<Function*>(subject);
        func->accessMutex.lock();
        auto v = func->binary ? "[]" : func->body;
        func->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(  v  ));
        
    });                       
                        
}

void CV::Function::set(const std::vector<std::string> &params, const std::string &body, bool variadic){
    this->accessMutex.lock();
    this->body = body;
    this->params = params;
    this->type = CV::ItemTypes::FUNCTION;
    this->fn = [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        return std::shared_ptr<CV::Item>(NULL);
    };
    this->binary = false;
    this->variadic = variadic;
    this->accessMutex.unlock();
}

void CV::Function::set(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx)> &fn, bool variadic){
    this->accessMutex.lock();
    this->params = params;
    this->type = CV::ItemTypes::FUNCTION;
    this->fn = fn;
    this->binary = true;
    this->variadic = variadic;
    this->accessMutex.unlock();
}

/*

CONTEXT

*/
std::shared_ptr<CV::Item> CV::Context::get(const std::string &name){
    this->accessMutex.lock();
    auto it = vars.find(name);
    if(it == vars.end()){
        auto v = this->head ? this->head->get(name) : std::shared_ptr<CV::Item>(NULL);
        this->accessMutex.unlock();
        return v;
    }
    auto v = it->second;
    this->accessMutex.unlock();
    return v;
}

std::shared_ptr<CV::Item> CV::Context::copy(bool deep){
    this->accessMutex.lock();
    auto item = std::make_shared<CV::Context>(this->head);
    item->type = this->type;
    for(auto &it : this->vars){
        item->vars[it.first] = deep && it.second->copyable ? it.second->copy() : it.second;
    }        
    this->accessMutex.unlock();
    return item;
}

void CV::Context::setTop(std::shared_ptr<CV::Context> &nctx){
    auto ctop = this->get("top");
    if(ctop.get()){
        return;
    }
    if(ctop.get() == this){
        // Avoid redundancy
        return;
    }
    this->set("top", nctx);
}

bool CV::Context::isDone(){
    bool f = true;

    // for(auto &it : )

    return f;
}


CV::ItemContextPair CV::Context::getWithContext(const std::string &name){
    this->accessMutex.lock();
    auto it = vars.find(name);
    if(it == vars.end()){
        auto v = this->head ? this->head->getWithContext(name) : ItemContextPair();
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
    auto v = this->head ? this->head->getWithContext(item) : ItemContextPair();
    this->accessMutex.unlock();
    return v;
}        

std::shared_ptr<CV::Item> CV::Context::set(const std::string &name, const std::shared_ptr<CV::Item> &item){
    auto v = get(name);
    this->accessMutex.lock();
    this->vars[name] = item;
    this->accessMutex.unlock();    
    return item;
}

CV::Context::Context(){
    this->readOnly = false;
    this->type = CV::ItemTypes::CONTEXT;
    this->head = NULL;
}

CV::Context::Context(std::shared_ptr<CV::Context> &ctx){
    this->readOnly = false;    
    this->type = CV::ItemTypes::CONTEXT;        
    this->head = ctx;
    this->setTop(ctx);
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
        std::cout << it.first << getSpace(it.first.length()) << " -> " << getSpace(0) << CV::ItemToText(it.second.get()) << std::endl;
    }
}

/*

    ITEM TO TEXT

*/
std::string CV::ItemToText(CV::Item *item, bool useColors){
    useColorOnText = useColors;
    switch(item->type){
        default:
        case CV::ItemTypes::NIL: {
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
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

            return start+name+" "+start+(fn->variadic ? "..." : Tools::compileList(fn->getParams()))+end+" "+start+(fn->binary ? binary : fn->getBody() )+end+end;
        };
        case CV::ItemTypes::CONTEXT: {
            std::string output = Tools::setTextColor(Tools::Color::YELLOW, true)+"["+Tools::setTextColor(Tools::Color::YELLOW, true)+"ct "+Tools::setTextColor(Tools::Color::RESET);
            auto proto = static_cast<CV::Context*>(item);
            int n = proto->vars.size();
            int c = 0;
            proto->accessMutex.lock();
            for(auto &it : proto->vars){
                auto name = it.first;
                auto item = it.second;
                auto body = name == "top" ? Tools::setTextColor(Tools::Color::YELLOW, true)+"[ct]"+Tools::setTextColor(Tools::Color::RESET) : CV::ItemToText(item.get(), useColors);
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
            for(int i = 0; i < list->data.size(); ++i){
                auto item = list->data[i];
                output += CV::ItemToText(item.get(), useColors);
                if(i < list->data.size()-1){
                    output += " ";
                }
            }
            list->accessMutex.unlock();
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
        };         
    }
}



static std::vector<std::string> parseTokens(std::string input, char sep, CV::Cursor *cursor){
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
        cursor->setError("Mismatching brackets", cline);
        return {};
    }
    // Throw mismatching quotes
    if(quotes != 0){
        cursor->setError("Mismatching quotes", cline);
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
        char c = input[input.length()-1 - i];
        auto t = CV::ModifierTypes::getToken(c);
        if(t != CV::ModifierTypes::UNDEFINED){
                std::reverse(buffer.begin(), buffer.end());            
                token.modifiers.push_back(CV::ModifierPair(t, buffer));
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

std::vector<CV::Token> buildTokens(const std::vector<std::string> &literals, CV::Cursor *cursor){

    std::vector<CV::Token> tokens;
    for(int i = 0; i < literals.size(); ++i){
        auto innLiterals = parseTokens(literals[i], ' ', cursor);
        // Complex tokens (It's usually tokens that actually are complete statements) need to be evaluated further        
        if(innLiterals.size() > 1){
            CV::Token token;
            token.first = literals[i];
            tokens.push_back(token);  
        // Simple can be inserted right away
        }else{
            auto token = parseTokenWModifier(literals[i]);
            if(token.first == ""){ // This means this modifier belongs to the token before this one
                if(i > 0){
                    auto &prev = tokens[tokens.size()-1];
                    for(int j = 0; j < token.modifiers.size(); ++j){
                        prev.modifiers.push_back(token.modifiers[j]);
                    }
                    continue;
                }else{
                    cursor->setError("Provided dangling modifier. Modifiers can only go accompanying something.");
                    return {};
                }
            }else{
                tokens.push_back(token);
            }
        }
    }

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
        errormsg = "Provided ("+std::to_string(items.size())+") argument(s). Expected no more than ("+std::to_string(maxParams)+") arguments.";
        return false;
    }
    if(useMinParams && items.size() < minParams){
        errormsg = "Provided ("+std::to_string(items.size())+") argument(s). Expected no less than ("+std::to_string(minParams)+") arguments.";
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


static std::shared_ptr<CV::Item> eval(const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, bool postEval = true);


static std::shared_ptr<CV::Item> processPreInterpretModifiers(std::shared_ptr<CV::Item> &item, std::vector<CV::ModifierPair> modifiers, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, CV::ModifierEffect &effects){
    effects.reset();
    for(int i = 0; i < modifiers.size(); ++i){
        auto &mod = modifiers[i];
        switch(mod.type){
            case CV::ModifierTypes::TRAIT: {
                auto name = mod.subject;
                if(name.size() == 0){
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::SCOPE)+"': Trait modifier requires a name or type group.");
                    return item;                    
                }
                // ANY_NUMBER type trait
                if(CV::Tools::isNumber(name) && item->hasTrait(CV::TraitType::ANY_NUMBER)){
                    auto result = item->runTrait(CV::TraitType::ANY_NUMBER, name, cursor, ctx, effects);  
                    if(cursor->error){
                        return std::make_shared<CV::Item>(CV::Item());
                    }                         
                    item = result;
                }else
                // ANY_STRING type trait
                if(!CV::Tools::isNumber(name) && item->hasTrait(CV::TraitType::ANY_STRING)){
                    auto result = item->runTrait(CV::TraitType::ANY_STRING, name, cursor, ctx, effects);    
                    if(cursor->error){
                        return std::make_shared<CV::Item>(CV::Item());
                    }                       
                    item = result;
                }else                
                // Regular trait
                if(!item->hasTrait(name)){
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::SCOPE)+"': Trait '"+name+"' doesn't belong to '"+CV::ItemTypes::str(item->type)+"' type.");
                    return item;                      
                }else{
                    auto result = item->runTrait(name, "", cursor, ctx, effects);
                    if(cursor->error){
                        return std::make_shared<CV::Item>(CV::Item());
                    }                       
                    item = result;
                }
            } break;            
            case CV::ModifierTypes::LINKER: {

            } break;
            case CV::ModifierTypes::UNTETHERED: {

            } break;            
            case CV::ModifierTypes::NAMER: {
                effects.named = mod.subject;
                if(ctx->readOnly){
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::NAMER)+"': Cannot name this variable '"+mod.subject+"' within this context as it's readonly.");
                    return std::make_shared<CV::Item>(CV::Item());
                }
                if(ctx.get() == item.get()){
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::NAMER)+"': Naming a context within a context is prohibited (circular referencing).");
                    return std::make_shared<CV::Item>(CV::Item());                    
                }
                ctx->set(mod.subject, item);
            } break;        
            case CV::ModifierTypes::SCOPE: {
                if(item->type != CV::ItemTypes::CONTEXT){
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::SCOPE)+"': Scope modifier can only be used on contexts.");
                    return item;
                }
                if(mod.subject.size() > 0){
                    auto subsequent = std::static_pointer_cast<CV::Context>(item)->get(mod.subject);
                    if(!subsequent){
                        cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::SCOPE)+"':'"+mod.subject+"': Undefined constructor, or operator within this context or above.");
                        return item;
                    }
                    effects.toCtx = std::static_pointer_cast<CV::Context>(item);
                    effects.ctxTargetName = mod.subject;
                    item = subsequent;
                }else{
                    effects.toCtx = std::static_pointer_cast<CV::Context>(item);
                    effects.ctxSwitch = true;
                }
            } break;   
            case CV::ModifierTypes::EXPAND: {
                if(i == modifiers.size()-1){
                    effects.expand = true;
                }else{
                    cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::EXPAND)+"': Expand modifier can only be used at the end of a modifier chain.");
                    return item;
                }
            } break;                                   
        }
    }
    return item;
}

// Interpret from STRING
std::shared_ptr<CV::Item> CV::interpret(const std::string &input, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
    auto literals = parseTokens(input, ' ', cursor);
    if(cursor->error){
        return std::make_shared<CV::Item>(CV::Item());
    }    
    auto tokens = buildTokens(literals, cursor);
    if(cursor->error){
        return std::make_shared<CV::Item>(CV::Item());
    }   
    return eval(tokens, cursor, ctx);
}

// Interpret from single TOKEN without dropping modifiers. Should be only used by `eval`
std::shared_ptr<CV::Item> CV::interpret(const CV::Token &token, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
    auto literals = parseTokens(token.first, ' ', cursor);
    if(cursor->error){
        return std::make_shared<CV::Item>(CV::Item());
    }
    auto tokens = buildTokens(literals, cursor);
    if(cursor->error){
        return std::make_shared<CV::Item>(CV::Item());
    }       
    return eval(tokens, cursor, ctx);
}

static std::shared_ptr<CV::Item> processIteration(const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, bool postEval);
static void processPostInterpretExpandListOrContext(std::shared_ptr<CV::Item> &solved, CV::ModifierEffect &effects, std::vector<std::shared_ptr<CV::Item>> &items, CV::Cursor *cursor, const std::shared_ptr<CV::Context> &ctx);


static std::shared_ptr<CV::Item> runFunction(std::shared_ptr<CV::Function> &fn, const std::vector<std::shared_ptr<CV::Item>> items, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx);
static std::shared_ptr<CV::Item> runFunction(std::shared_ptr<CV::Function> &fn, const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx);

static std::shared_ptr<CV::Item> runFunction(std::shared_ptr<CV::Function> &fn, const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
    std::vector<std::shared_ptr<CV::Item>> items;
    for(int i = 0; i < tokens.size(); ++i){
        CV::ModifierEffect effects;
        auto interp = interpret(tokens[i], cursor, ctx);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }        
        auto solved = processPreInterpretModifiers(interp, tokens[i].modifiers, cursor, ctx, effects);                     
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }
        processPostInterpretExpandListOrContext(solved, effects, items, cursor, std::shared_ptr<CV::Context>((CV::Context*)NULL));
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        } 
    }
    return runFunction(fn, items, cursor, ctx);
}


static std::shared_ptr<CV::Item> runFunction(std::shared_ptr<CV::Function> &fn, const std::vector<std::shared_ptr<CV::Item>> items, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
    fn->accessMutex.lock();
    if(fn->binary){
        auto v = fn->fn(items, cursor, ctx);  
        fn->accessMutex.unlock();

        return v;
    }else{
        auto tctx = std::make_shared<CV::Context>(ctx);
        tctx->setTop(ctx);
        if(fn->variadic){
            auto list = std::make_shared<CV::List>(items, false);
            tctx->set("...", std::static_pointer_cast<CV::Item>(list));
        }else{
            if(items.size() != fn->params.size()){
                cursor->setError("'"+CV::ItemToText(fn.get())+"': Expects exactly "+(std::to_string(fn->params.size()))+" arguments");
                fn->accessMutex.unlock();
                return std::make_shared<CV::Item>(CV::Item());
            }
            for(int i = 0; i < items.size(); ++i){
                tctx->set(fn->params[i], items[i]);
            }
        }
        auto v = interpret(fn->body, cursor, tctx);
        fn->accessMutex.unlock();
        return v;
    }
}

static void processPostInterpretExpandListOrContext(std::shared_ptr<CV::Item> &solved, CV::ModifierEffect &effects, std::vector<std::shared_ptr<CV::Item>> &items, CV::Cursor *cursor, const std::shared_ptr<CV::Context> &ctx){
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
        cursor->setError("'"+CV::ModifierTypes::str(CV::ModifierTypes::EXPAND)+"': Expand modifier can only be used on iterables.");
        items.push_back(solved);
    }else{   
        items.push_back(solved);
    } 
}

static std::shared_ptr<CV::Item> processIteration(const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx, bool postEval){
    std::vector<std::shared_ptr<CV::Item>> items;
    for(int i = 0; i < tokens.size(); ++i){
        CV::ModifierEffect effects;
        auto solved = eval({tokens[i]}, cursor, ctx, postEval);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }
        if(solved->type == CV::ItemTypes::INTERRUPT){
            auto interrupt =  std::static_pointer_cast<CV::Interrupt>(solved);
            if(interrupt->intType == CV::InterruptTypes::SKIP){
                if(interrupt->hasPayload()){
                    items.push_back(interrupt->getPayload());
                }
                continue;
            }else
            if(interrupt->intType == CV::InterruptTypes::STOP){
                return std::static_pointer_cast<CV::Item>(interrupt);
            }
        }   
        processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }                           
    }
    auto r = tokens.size() == 1 ? items[0] : std::make_shared<CV::List>(items);
    return r;
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

static CV::ItemContextPair findTokenOrigin(CV::Token &token, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
    auto findName = token.first;
    // If token has modifiers, then we proceed to process them
    if(token.modifiers.size() > 0){
        auto var = ctx->get(token.first);
        if(var.get()){
            CV::ModifierEffect effects;
            auto solved = processPreInterpretModifiers(var, token.modifiers, cursor, ctx, effects);
            if(cursor->error){
                return CV::ItemContextPair(NULL, NULL, "", true);
            }
            if(effects.toCtx && effects.ctxTargetName.size() > 0){
                return CV::ItemContextPair(solved.get(), effects.toCtx.get(), effects.ctxTargetName, false);
            }
        }
    }
    return ctx->getWithContext(findName);
}

static std::shared_ptr<CV::Item> eval(const std::vector<CV::Token> &tokens, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx,  bool postEval){

    auto first = tokens[0];
    auto imp = first.first;

    if(imp == "set"){
        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }       
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'"+imp+"': Expects exactly 2 arguments: <NAME> <VALUE>.");
            return std::make_shared<CV::Item>(CV::Item());
        }
        CV::ModifierEffect effects;
        auto solved = eval({params[1]}, cursor, ctx, false);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto f = findTokenOrigin(params[0], cursor, ctx);
        
        //Woops?
        if(f.error){
            return std::make_shared<CV::Item>(CV::Item());
        }else
        // Replace existing item
        if(f.item || f.context){
            if(f.context->readOnly){
                cursor->setError("'"+imp+"': Context '"+params[0].first+"' is readonly.");
                return std::make_shared<CV::Item>(CV::Item());
            }      
            if(f.context == solved.get()){
                cursor->setError("'"+imp+"': Setting a context within a context is prohibited (circular referencing).");
                return std::make_shared<CV::Item>(CV::Item());
            }                  
            return f.context->set(f.name, solved);
        }else
        // If a scope modifier was provided but the context wasn't found, we throw an error
        if(params[0].hasModifier(CV::ModifierTypes::SCOPE)){
            cursor->setError("'"+imp+"': Context '"+params[0].first+"' doesn't exist.");
            return std::make_shared<CV::Item>(CV::Item());
        // Othwerwise just define it within the current context
        }else{
            if(ctx->readOnly){
                cursor->setError("'"+imp+"': Cannot set variable '"+params[0].first+"' within this context as it is readonly.");
                return std::make_shared<CV::Item>(CV::Item());
            }
            if(ctx.get() == solved.get()){
                cursor->setError("'"+imp+"': Setting a context within a context is prohibited (circular referencing).");
                return std::make_shared<CV::Item>(CV::Item());
            }
            return ctx->set(params[0].first, solved);
        }
    }else
    if(imp == "skip" || imp == "stop"){
        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto interrupt = std::make_shared<CV::Interrupt>(CV::Interrupt(imp == "skip" ? CV::InterruptTypes::SKIP : CV::InterruptTypes::STOP));
        if(tokens.size() > 1){
            auto params = compileTokens(tokens, 1, tokens.size()-1);
            auto solved = processIteration(params, cursor, ctx, postEval);
            if(solved->type == CV::ItemTypes::INTERRUPT){
                return solved;
            }else{
                interrupt->setPayload(solved);
            }
        }
        return std::static_pointer_cast<CV::Item>(interrupt);
    }else
    if(imp == "if"){

        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }

        if(tokens.size() > 4){
            cursor->setError("'"+imp+"': Expects no more than 3 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return std::make_shared<CV::Item>(CV::Item());
        }
        if(tokens.size() < 3){
            cursor->setError("'"+imp+"': Expects no less than 2 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return std::make_shared<CV::Item>(CV::Item());
        }        

        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto cond = params[0];
        auto trueBranch = params[1];
        auto tctx = std::make_shared<CV::Context>(ctx);
        tctx->setTop(ctx);

        CV::ModifierEffect origEff;
        auto interp = interpret(cond, cursor, tctx);
        auto condV = processPreInterpretModifiers(interp, cond.modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        } 
        if(checkCond(condV)){
            CV::ModifierEffect effects;
            auto interp = interpret(trueBranch, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, trueBranch.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<CV::Item>(CV::Item());
            }
            return solved;      
        }else
        if(params.size() == 3){
            auto falseBranch = params[2];
            CV::ModifierEffect effects;
            auto interp = interpret(falseBranch, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, falseBranch.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<CV::Item>(CV::Item());
            }
            return solved;            
        }else{
            return condV;
        }
    }else
    if(imp == "do"){

        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }

        if(tokens.size() > 3 || tokens.size() < 3){
            cursor->setError("'"+imp+"': Expects exactly 2 arguments: <CONDITION> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto cond = params[0];
        auto code = params[1];
        auto tctx = std::make_shared<CV::Context>(ctx);
        tctx->setTop(ctx);

        CV::ModifierEffect origEff;
        auto interp = interpret(cond, cursor, tctx);
        auto condV = processPreInterpretModifiers(interp, cond.modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }  
        auto last = std::shared_ptr<CV::Item>(NULL);
        while(checkCond(condV)){
            CV::ModifierEffect effects;
            auto interp = interpret(code, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<CV::Item>(CV::Item());
            }
            if(solved->type == CV::ItemTypes::INTERRUPT){
                auto interrupt =  std::static_pointer_cast<CV::Interrupt>(solved);
                if(interrupt->intType == CV::InterruptTypes::SKIP){
                    continue;
                }else
                if(interrupt->intType == CV::InterruptTypes::STOP){
                    return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                }
            }         
            last = solved;
            // Next iteration
            auto reinterp = interpret(cond, cursor, tctx);
            condV = processPreInterpretModifiers(reinterp, cond.modifiers, cursor, tctx, origEff);
            if(cursor->error){
                return std::make_shared<CV::Item>(CV::Item());
            }            
        }             
        return last ? last : std::make_shared<CV::Item>(CV::Item());
    }else
    if(imp == "iter"){
        
        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }

        if(tokens.size() > 3 || tokens.size() < 3){
            cursor->setError("'"+imp+"': Expects exactly 2 arguments: <ITERABLE> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<CV::Item>(CV::Item());
        }        
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto code = params[1];
        auto tctx = std::make_shared<CV::Context>(ctx);
        tctx->setTop(ctx);
                
        CV::ModifierEffect origEff;
        auto interp = interpret(params[0], cursor, tctx);
        auto iterable = processPreInterpretModifiers(interp, params[0].modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }            
        if(iterable->type == CV::ItemTypes::LIST){
            std::vector<std::shared_ptr<CV::Item>> items;
            auto list = std::static_pointer_cast<CV::List>(iterable);
            for(int i = 0; i < list->size(); ++i){
                auto item = list->get(i);
                // We use the namer to name every iteration's current variable
                if(origEff.named.size() > 0){
                    tctx->set(origEff.named, item);
                }                
                CV::ModifierEffect effects;
                auto interp = interpret(code, cursor, tctx);
                auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                   
                if(solved->type == CV::ItemTypes::INTERRUPT){
                    auto interrupt =  std::static_pointer_cast<CV::Interrupt>(solved);
                    if(interrupt->intType == CV::InterruptTypes::SKIP){
                        if(interrupt->hasPayload()){
                            items.push_back(interrupt->getPayload());
                        }
                        continue;
                    }else
                    if(interrupt->intType == CV::InterruptTypes::STOP){
                        return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                    }
                }           
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                               
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items));
        }else
        if(iterable->type == CV::ItemTypes::CONTEXT){
            std::vector<std::shared_ptr<CV::Item>> items;
            auto list = std::static_pointer_cast<CV::Context>(iterable);
            for(auto &it : list->vars){
                auto item = it.second;
                // We use the namer to name every iteration's current variable
                if(origEff.named.size() > 0){
                    tctx->set(origEff.named, item);
                }                
                CV::ModifierEffect effects;
                auto interp = interpret(code, cursor, tctx);
                auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }             
                if(solved->type == CV::ItemTypes::INTERRUPT){
                    auto interrupt =  std::static_pointer_cast<CV::Interrupt>(solved);
                    if(interrupt->intType == CV::InterruptTypes::SKIP){
                        if(interrupt->hasPayload()){
                            items.push_back(interrupt->getPayload());
                        }
                        continue;
                    }else
                    if(interrupt->intType == CV::InterruptTypes::STOP){
                        return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                    }
                }               
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                } 
            }
            return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items));
        }else{
            cursor->setError("'"+imp+"': Expects argument(0) to be iterable.");
            return std::make_shared<CV::Item>(CV::Item());        
        }
    }else
    if(imp == "fn"){

        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }

        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'"+imp+"': Expects exactly 2 arguments: <[ARGS[0]...ARGS[n]]> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<CV::Item>(CV::Item());
        }
        bool isVariadic = false;
        auto argNames = parseTokens(params[0].first, ' ', cursor);
        if(cursor->error){
            return std::make_shared<CV::Item>(CV::Item());
        }
        if(argNames.size() == 1 && argNames[0] == "..."){
            isVariadic = true;
        }
        auto code = params[1];

        return std::static_pointer_cast<CV::Item>(  std::make_shared<CV::Function>(isVariadic ? std::vector<std::string>({}) : argNames, code.first, isVariadic)  );
    }else
    if(imp == "ct"){

        if(first.modifiers.size() > 0){
            cursor->setError("'"+imp+"': Innate constructors don't have modifiers.");
            return std::make_shared<CV::Item>(CV::Item());
        }

        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto pctx = std::make_shared<CV::Context>(ctx);
        CV::ModifierEffect effects;
        auto casted = std::static_pointer_cast<CV::Item>(pctx);
        auto solved = processPreInterpretModifiers(casted, first.modifiers, cursor, pctx, effects);

        if(solved->type == CV::ItemTypes::CONTEXT){
            for(int i = 0; i < params.size(); ++i){
                CV::ModifierEffect effects;
                auto interp = interpret(params[i], cursor, pctx);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                 
                auto solved = processPreInterpretModifiers(interp, params[i].modifiers, cursor, pctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                
                std::vector<std::shared_ptr<CV::Item>> items;
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, pctx);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }  
                for(int j = 0; j < items.size(); ++j){
                    if(effects.named.size() == 0){
                        pctx->set("v-"+std::to_string(j), items[j]);
                    }
                }
            }
        }
        return solved;
    }else{
        auto var = ctx->get(imp);
        CV::ModifierEffect effects;     
        // If we've got a var as first item, we solve its modifiers   
        if(var){
            var = processPreInterpretModifiers(var, first.modifiers, cursor, ctx, effects);
        }
        // Is it a function?
        if(var && var->type == CV::ItemTypes::FUNCTION && postEval && effects.postEval){
            auto fn = std::static_pointer_cast<CV::Function>(var);
            auto v = runFunction(fn, compileTokens(tokens, 1, tokens.size()-1), cursor, ctx);
            return v;
        }else   
        // Is it just a variable?
        if(var && tokens.size() == 1){
            return var;
        }else
        // Is it a natural type?
        if(tokens.size() == 1){
            if(!var && CV::Tools::isNumber(imp)){
                auto n = std::make_shared<CV::Number>(std::stod(imp));
                CV::ModifierEffect effects;
                auto casted = std::static_pointer_cast<CV::Item>(n);
                auto result = processPreInterpretModifiers(casted, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                      
                return result;
            }else
            if(!var && CV::Tools::isString(imp)){
                CV::ModifierEffect effects;
                auto n = std::make_shared<CV::String>(imp.substr(1, imp.length() - 2));
                auto casted = std::static_pointer_cast<CV::Item>(n);
                auto result = processPreInterpretModifiers(casted, first.modifiers, cursor, ctx, effects);
                return result;
            }else
            if(!var && CV::Tools::isList(imp)){
                CV::ModifierEffect effects;
                auto interp = interpret(first, cursor, ctx);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                 
                auto result = processPreInterpretModifiers(interp, first.modifiers,cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                   
                return result;
            }else
            if(imp == "nil"){
                CV::ModifierEffect effects;
                auto n = std::make_shared<CV::Item>(CV::Item());
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                      
                return result;
            }else{
                cursor->setError("'"+imp+"': Undefined constructor, or operator within this context or above."); // TODO: Check if it's running as relaxed
                return std::make_shared<CV::Item>(CV::Item());
            }
        }else
        // List or in-line context switch?
        if(tokens.size() > 1){
            auto fmods = tokens[0].getModifier(CV::ModifierTypes::SCOPE);
            // Super rad inline-contex switching d:)
            if(fmods.type == CV::ModifierTypes::SCOPE){
                CV::ModifierEffect effects;
                auto interp = interpret(tokens[0], cursor, ctx);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                }                 
                auto solved = processPreInterpretModifiers(interp, tokens[0].modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<CV::Item>(CV::Item());
                } 
                auto inlineCode = compileTokens(tokens, 1, tokens.size()-1);
                return eval(inlineCode, cursor, effects.toCtx); // Check if toCtx is null? Shouldn't be possible tho
            }else{
            // List it is
                auto r = processIteration(tokens, cursor, ctx, false);
                return r;
            }
        }
    }

    return std::make_shared<CV::Item>(CV::Item());
}

static bool typicalVariadicArithmeticCheck(const std::string &name, const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, int max = -1){
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
        cursor->setError("'"+name+"': "+errormsg);
        return false;
    }
    return true;
};

void CV::AddStandardOperators(std::shared_ptr<CV::Context> &ctx){
    /*

        ARITHMETIC OPERATORS

    */
    ctx->set("+", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("+", params, cursor)){
            return std::make_shared<CV::Item>(CV::Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r += std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, true));

    ctx->set("-", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("-", params, cursor)){
            return std::make_shared<CV::Item>(CV::Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r -= std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, true));
    ctx->set("*", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("*", params, cursor)){
            return std::make_shared<CV::Item>(CV::Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r *= std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, false));  
    ctx->set("/", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("/", params, cursor)){
            return std::make_shared<CV::Item>(CV::Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r /= std::static_pointer_cast<CV::Number>(params[i])->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, true));
    ctx->set("pow", std::make_shared<CV::Function>(std::vector<std::string>({"a", "b"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("pow", params, cursor, 2)){
            return std::make_shared<CV::Item>(CV::Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            r =  std::pow(r, std::static_pointer_cast<CV::Number>(params[i])->get());
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(r));
    }, false)); 
    ctx->set("sqrt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::NUMBER);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'sqrt': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    ctx->set("++", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'++': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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

    ctx->set("--", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'--': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    }, false));    


    ctx->set("//", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'//': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    }, false));    

    ctx->set("**", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'**': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    ctx->set("l-range", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        if(!typicalVariadicArithmeticCheck("l-range", params, cursor)){
            return std::make_shared<CV::Item>(CV::Item());
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
    
    ctx->set("l-sort", std::make_shared<CV::Function>(std::vector<std::string>({"c", "l"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
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
            cursor->setError("'l-sort': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        auto criteria = std::static_pointer_cast<CV::String>(params[0]);

        auto &order = criteria->data;
        if(order != "DESC" && order != "ASC"){
            cursor->setError("'l-sort': criteria can only be 'DESC' or 'ASC'.");
            return std::make_shared<CV::Item>(CV::Item());
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
            cursor->setError("'l-sort': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }        

        std::sort(items.begin(), items.end(), [&](std::shared_ptr<CV::Item> &a, std::shared_ptr<CV::Item> &b){
            return order == "DESC" ?     std::static_pointer_cast<CV::Number>(a)->get() > std::static_pointer_cast<CV::Number>(b)->get() :
                                        std::static_pointer_cast<CV::Number>(a)->get() < std::static_pointer_cast<CV::Number>(b)->get();
        });

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));
    }, false));    


    ctx->set("l-filter", std::make_shared<CV::Function>(std::vector<std::string>({"c", "l"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, CV::ItemTypes::FUNCTION);
        consts.setExpectedTypeAt(1, CV::ItemTypes::LIST);
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-filter': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        auto criteria = std::static_pointer_cast<Function>(params[0]);

        if(criteria->variadic || criteria->params.size() != 1){
            cursor->setError("'l-filter': criteria's function must receive exactly 1 argument(a).");
            return std::make_shared<CV::Item>(CV::Item());
        }

        auto list = std::static_pointer_cast<CV::List>(params[1]);
        std::vector<std::shared_ptr<CV::Item>> items;
        list->accessMutex.lock();
        for(int i = 0; i < list->data.size(); ++i){
            auto result = runFunction(criteria, std::vector<std::shared_ptr<CV::Item>>{list->data[i]}, cursor, ctx);
            if(checkCond(result)){
                continue;
            }
            items.push_back(list->get(i));
        }        
        list->accessMutex.unlock();
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::List>(items, false));

    }, false));  


    ctx->set("l-flat", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = true;
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-flat': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    }, false));   



    ctx->set("splice", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::CONTEXT);
        consts.setExpectType(CV::ItemTypes::LIST);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'splice': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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

        return std::make_shared<CV::Item>(CV::Item());
    }, false));    

    ctx->set("l-push", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-push': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    
    ctx->set("l-pop", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'<<': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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

    ctx->set("l-sub", std::make_shared<CV::Function>(std::vector<std::string>({"l", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
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
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto list = std::static_pointer_cast<CV::List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        list->accessMutex.lock();
        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size())+")");
            list->accessMutex.unlock();
            return std::make_shared<CV::Item>(CV::Item());
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


    ctx->set("l-cut", std::make_shared<CV::Function>(std::vector<std::string>({"l", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
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
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto list = std::static_pointer_cast<CV::List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        list->accessMutex.lock();
        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size())+")");
            list->accessMutex.unlock();
            return std::make_shared<CV::Item>(CV::Item());
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

    ctx->set("l-reverse", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    ctx->set("s-reverse", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(CV::ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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

    ctx->set("s-join", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        // consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        consts.setExpectType(CV::ItemTypes::STRING);

        static const std::string name = "s-join";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        std::string result;
        for(int i = 0; i < params.size(); ++i){
            auto str = std::static_pointer_cast<CV::String>(params[i]);
            result += str->get();
        }
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::String>(result));
    }, false));   

    ctx->set("s-cut", std::make_shared<CV::Function>(std::vector<std::string>({"s", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
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
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }



        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto str = std::static_pointer_cast<CV::String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        str->accessMutex.lock();
        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size())+")");
            str->accessMutex.unlock();            
            return std::make_shared<CV::Item>(CV::Item());
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


    ctx->set("s-sub", std::make_shared<CV::Function>(std::vector<std::string>({"s", "p", "n"}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
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
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }


        int index = std::static_pointer_cast<CV::Number>(params[1])->get();
        auto str = std::static_pointer_cast<CV::String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<CV::Number>(params[2])->get() : 1;

        str->accessMutex.lock();
        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size())+")");
            return std::make_shared<CV::Item>(CV::Item());
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
    ctx->set("or", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'or': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == CV::ItemTypes::NUMBER && std::static_pointer_cast<CV::Number>(params[i])->get() != 0) ||
                (params[i]->type != CV::ItemTypes::NIL && params[i]->type != CV::ItemTypes::NUMBER)){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
    }, false));        

    ctx->set("and", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'and': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == CV::ItemTypes::NUMBER && std::static_pointer_cast<CV::Number>(params[i])->get() == 0) || (params[i]->type == CV::ItemTypes::NIL) ){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
        }

        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));    

    ctx->set("not", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'not': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
        return std::make_shared<CV::Item>(CV::Item());
    }, false));                  


    ctx->set("eq", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'eq': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto last = params[0];
        for(int i = 1; i < params.size(); ++i){
            auto item = params[i];
            if(!last->isEq(item)){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));     


    ctx->set("neq", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'neq': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
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
    }, false));      


    ctx->set("gt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gt': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last <= std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));    


    ctx->set("gte", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last <  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));         


    ctx->set("lt", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'lt': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last >=  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));    

    ctx->set("lte", std::make_shared<CV::Function>(std::vector<std::string>({}), [](const std::vector<std::shared_ptr<CV::Item>> &params, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx){
        CV::FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(CV::ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return std::make_shared<CV::Item>(CV::Item());
        }
        auto last = std::static_pointer_cast<CV::Number>(params[0])->get();
        for(int i = 1; i < params.size(); ++i){
            if(last >  std::static_pointer_cast<CV::Number>(params[i])->get()){
                return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(0));
            }
            last = std::static_pointer_cast<CV::Number>(params[i])->get();
        }				
        return std::static_pointer_cast<CV::Item>(std::make_shared<CV::Number>(1));
    }, false));    
         
}