#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <inttypes.h>


#define __CV_NUMBER_NATIVE_TYPE double

struct Cursor {
    bool error;
    int line;
    std::string message;
    Cursor(){
        error = false;
        line = 1;
        message = "";
    }
    void setError(const std::string &v, int line = -1){
        if(line != -1){
            this->line = line;
        }
        this->error = true;
        this->message = v;
    }
};

namespace Tools {
    bool isNumber(const std::string &s){
        return (s.find_first_not_of( "-.0123456789") == std::string::npos);
    }

    double positive(double n){
        return n < 0 ? n * -1.0 : n;
    }

    bool isString(const std::string &s){
        return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
    }  

    bool isList(const std::string &s){
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

    std::string compileList(const std::vector<std::string> &strings){
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

namespace ModifierTypes {
    enum ModifierTypes : uint8_t {
        UNDEFINED,
        NAMER,
        SCOPE,
        LINKER,
        EXPAND,
        TRAIT
    };
    static uint8_t getToken(char mod){
        if(mod == '~'){
            return ModifierTypes::NAMER;
        }else
        if(mod == ':'){
            return ModifierTypes::SCOPE;
        }else
        if(mod == '<'){
            return ModifierTypes::LINKER;
        }else
        if(mod == '|'){
            return ModifierTypes::TRAIT;
        }else        
        if(mod == '^'){
            return ModifierTypes::EXPAND;            
        }else{
            return ModifierTypes::UNDEFINED;
        }
    }
    static std::string str(uint8_t type){
        switch(type){
            case ModifierTypes::NAMER: {
                return "~";
            };
            case ModifierTypes::SCOPE: {
                return ":";
            };   
            case ModifierTypes::TRAIT: {
                return "|";
            };              
            case ModifierTypes::LINKER: {
                return "<";
            }; 
            case ModifierTypes::EXPAND: {
                return "^";
            };                          
            default: {
                return "";
            };       
        }
    }
}

namespace ItemTypes {
    enum ItemTypes : uint8_t {
        NIL,
        NUMBER,
        STRING,
        LIST,
        CONTEXT,
        FUNCTION,
        INTERRUPT
    };
    static std::string str(uint8_t type){
        switch(type){
            case NIL: {
                return "NIL";
            };
            case NUMBER: {
                return "NUMBER";
            };  
            case STRING: {
                return "STRING";
            };  
            case LIST: {
                return "LIST";
            }; 
            case CONTEXT: {
                return "CONTEXT";
            };    
            case FUNCTION: {
                return "FUNCTION";
            }; 
            case INTERRUPT: {
                return "INTERRUPT";
            };    
            default: {
                return "UNDEFINED";
            };         
        }
    }
}

namespace InterruptTypes {
    enum InterruptTypes : uint8_t {
        SKIP,
        STOP
    };
    std::string str(uint8_t type){
        switch(type){
            case InterruptTypes::SKIP:{
                return "SKIP";
            };
            case InterruptTypes::STOP:{
                return "STOP";
            };    
            default: {
                return "UNDEFINED";    
            }        
        }
    }
}

struct Item;
struct Context;
struct Cursor;

namespace TraitType {
    enum TraitType : uint8_t {
        ANY_NUMBER,
        ANY_STRING,
        SPECIFIC
    };
}

struct Trait {
    std::string name;
    uint8_t type;
    std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx)> action;
    Trait(){
        this->type = TraitType::SPECIFIC;
        this->action = [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            return std::shared_ptr<Item>(NULL);
        };
    }
};


struct Item {
    uint8_t type;
    std::unordered_map<std::string, Trait> traits;
    std::unordered_map<uint8_t, Trait> traitsAny;

    Item(){
        registerTraits();
        this->type = ItemTypes::NIL;
    }

    void registerTrait(uint8_t type, const std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx)> &action){
        Trait trait;
        trait.action = action;
        trait.type = type;
        this->traitsAny[type] = trait;
    }    
    
    void registerTrait(const std::string &name, const std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx)> &action){
        Trait trait;
        trait.name = name;
        trait.action = action;
        this->traits[name] = trait;
    }

    std::shared_ptr<Item> runTrait(const std::string &name, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
        auto &trait = this->traits.find(name)->second;
        return trait.action( this, value, cursor, ctx);
    }

    std::shared_ptr<Item> runTrait(uint8_t type, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
        return this->traitsAny.find(type)->second.action(this, value, cursor, ctx);
    }    

    bool hasTrait(const std::string &name){
        auto it = this->traits.find(name);
        if(it == this->traits.end()){
            return false;
        }
        return true;
    }

    bool hasTrait(uint8_t type){
        auto it = this->traitsAny.find(type);
        if(it == this->traitsAny.end()){
            return false;
        }
        return true;
    }
        
    

    virtual void registerTraits(){

    }

    virtual bool isEq(std::shared_ptr<Item> &item){
        return (this->type == ItemTypes::NIL && item->type == ItemTypes::NIL) || this == item.get();
    }
    virtual std::shared_ptr<Item> copy(bool deep = true) const {
        return std::make_shared<Item>(Item());
    }

    virtual bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }

        return true;
    }
};

struct Interrupt : Item {
    uint8_t intType;
    std::shared_ptr<Item> payload;

    Interrupt(uint8_t intType){
        this->type = ItemTypes::INTERRUPT;
        this->intType = intType;
    }

    std::shared_ptr<Item> copy(bool deep = true) const {
        auto item = std::make_shared<Interrupt>(this->intType);
        item->payload = this->payload;
        return std::static_pointer_cast<Item>(item);
    }  

    bool hasPayload(){
        return this->payload.get() != NULL;
    }

    void setPayload(std::shared_ptr<Item> &item){
        this->payload = item;
    }

    std::shared_ptr<Item> getPayload(){
        return this->payload;
    }

};

struct Number : Item {
    __CV_NUMBER_NATIVE_TYPE n;

    Number(){
        registerTraits();
    }

    Number(__CV_NUMBER_NATIVE_TYPE v){
        registerTraits();        
        set(v);
    }

    bool isEq(std::shared_ptr<Item> &item){
        return item->type == this->type && this->n == std::static_pointer_cast<Number>(item)->n;
    }    

    void registerTraits(){
        this->registerTrait("is-neg", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(static_cast<Number*>(subject)->n < 0)));
        });
        this->registerTrait("inv", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(static_cast<Number*>(subject)->n * -1)));
        });    
        this->registerTrait("is-odd", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( (int)static_cast<Number*>(subject)->n % 2 != 0)));
        });   
        this->registerTrait("is-even", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( (int)static_cast<Number*>(subject)->n % 2 == 0)));
        });                       
    }

    std::shared_ptr<Item> copy(bool deep = true) const {
        return std::make_shared<Number>(Number(this->n));
    }    

    void set(const __CV_NUMBER_NATIVE_TYPE v){
        this->type = ItemTypes::NUMBER;
        this->n = v;
    }
    
    __CV_NUMBER_NATIVE_TYPE get(){
        return this->n;
    }

};

static std::string ItemToText(Item *item);

struct List : Item {

    std::vector<std::shared_ptr<Item>> data;

    List(){
        registerTraits();
    }

    List(const std::vector<std::shared_ptr<Item>> &list, bool toCopy = true){
        registerTraits();        
        set(list, toCopy);
    }

    void registerTraits(){
        this->registerTrait(TraitType::ANY_NUMBER, [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto list = static_cast<List*>(subject); 
            auto index = std::stod(value);
            if(index < 0 || index >= list->data.size()){
                cursor->setError("'List|*ANY_NUMBER*': Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(list->data.size())+")");
                return std::make_shared<Item>(Item());
            }
            return list->get(index); 
        });        
        this->registerTrait("n", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto list = static_cast<List*>(subject); 
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(list->data.size())));
        });
        this->registerTrait("reverse", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            std::vector<std::shared_ptr<Item>> result;
            auto list = static_cast<List*>(subject); 
            for(int i = 0; i < list->data.size(); ++i){
                result.push_back(list->data[list->data.size()-1 - i]);
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(result)));
        });                           
    }

    std::shared_ptr<Item> copy(bool deep = true) const {
        auto nlist = std::make_shared<List>(List());
        for(int i = 0; i < this->data.size(); ++i){
            nlist->data.push_back(deep ? this->data[i]->copy() : this->data[i]);
        }        
        return std::static_pointer_cast<Item>(nlist);
    }

    void set(const std::vector<std::shared_ptr<Item>> &list, bool toCopy = true){
        this->type = ItemTypes::LIST;
        for(int i = 0; i < list.size(); ++i){
            this->data.push_back(toCopy ? list[i]->copy() : list[i]);
        }        
    }

    std::shared_ptr<Item> get(size_t index) const {
        return this->data[index];
    }

    bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        return true;
    }       

};

struct String : Item {
    
    std::string data;

    String(){
        // Empty constructor makes this string NIL
        registerTraits();
    }

    String(const std::string &str){
        registerTraits();
        this->set(str);
    }    

    void registerTraits(){ 
        this->registerTrait(TraitType::ANY_NUMBER, [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto str = static_cast<String*>(subject);
            auto index = std::stod(value);
            if(index < 0 || index >= str->data.size()){
                cursor->setError("'String|*ANY_NUMBER*': Index("+Tools::removeTrailingZeros(index)+") is out of range(0-"+std::to_string(str->data.size())+")");
                return std::make_shared<Item>(Item());
            }
            return std::static_pointer_cast<Item>(std::make_shared<String>(String(  std::string("")+str->data[index]  )));
        });           
        this->registerTrait("n", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto str = static_cast<String*>(subject);
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( str->data.size() )));
        });
        this->registerTrait("list", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            std::vector<std::shared_ptr<Item>> result;
            auto str = static_cast<String*>(subject);

            for(int i = 0; i < str->data.size(); ++i){
                result.push_back(std::static_pointer_cast<Item>(std::make_shared<String>(String(  std::string("")+str->data[i]  ))));
            }


            return std::static_pointer_cast<Item>(std::make_shared<List>(List(result)));
        });        
        this->registerTrait("reverse", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            std::string result;
            auto str = static_cast<String*>(subject);

            for(int i = 0; i < str->data.size(); ++i){
                result += str->data[str->data.size()-1 - i];
            }
            return std::static_pointer_cast<Item>(std::make_shared<String>(String(  result  )));

        });                           
    }

    bool isEq(std::shared_ptr<Item> &item){
        if(this->type != item->type || this->data.size() != std::static_pointer_cast<String>(item)->data.size()){
            return false;
        }
        return this->data == std::static_pointer_cast<String>(item)->data;
    } 

    std::shared_ptr<Item> copy(bool deep = true) const {
        auto copied = std::make_shared<String>(String());
        copied->set(this->data);
        return copied;
    }

    void set(const std::string &str){
        this->type = ItemTypes::STRING;
        this->data = str;
    } 

    std::string &get(){     
        return this->data;   
    }

};

struct Context;
struct Token;

struct FunctionConstraints {
    bool enabled;

    bool useMinParams;
    int minParams;

    bool useMaxParams;
    int maxParams;

    bool allowMisMatchingTypes;
    bool allowNil;

    std::vector<uint8_t> expectedTypes;
    std::unordered_map<int, uint8_t> expectedTypeAt;

    FunctionConstraints(){
        enabled = true;
        allowNil = true;
        allowMisMatchingTypes = true;
        useMinParams = false;
        useMaxParams = false;
    }

    void setExpectType(uint8_t type){
        this->expectedTypes.push_back(type);
    }

    void setExpectedTypeAt(int pos, uint8_t type){
        this->expectedTypeAt[pos] = type;
    }

    void clearExpectedTypes(){
        this->expectedTypeAt.clear();
        this->expectedTypes.clear();
    }

    void setMaxParams(unsigned n){
        if(n == 0){
            useMaxParams = false;
            return;
        }
        enabled = true;
        maxParams = n;
        useMaxParams = true;
    }

    void setMinParams(unsigned n){
        if(n == 0){
            useMinParams = false;
            return;
        }
        enabled = true;
        minParams = n;
        useMinParams = true;
    }

    bool test(List *list, std::string &errormsg);
    bool test(const std::vector<std::shared_ptr<Item>> &items, std::string &errormsg);

};
struct Function : Item {
    std::string body;
    std::vector<std::string> params;
    bool binary;
    bool variadic;
    std::function< std::shared_ptr<Item> (const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx) > fn;
    FunctionConstraints constraints;

    Function(){
        registerTraits();
    }

    Function(const std::vector<std::string> &params, const std::string &body, bool variadic = false){
        registerTraits();
        set(params, body, variadic);
    }

    Function(const std::vector<std::string> &params, const std::function<std::shared_ptr<Item> (const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic = false){
        registerTraits();
        set(params, fn, variadic);
    }    


    void registerTraits(){
        this->registerTrait("is-variadic", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto func = static_cast<Function*>(subject);
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( func->variadic )));
        });

        this->registerTrait("is-binary", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto func = static_cast<Function*>(subject);
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( func->binary )));
        }); 

        this->registerTrait("n-args", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto func = static_cast<Function*>(subject);
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number( func->params.size()  )));            
        });   

        this->registerTrait("proto", [](Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx){
            if(subject->type == ItemTypes::NIL){
                return std::make_shared<Item>();
            }
            auto func = static_cast<Function*>(subject);
            return std::static_pointer_cast<Item>(std::make_shared<String>(String(  func->binary ? "[]" : func->body  )));
            
        });                       
                           
    }

    void set(const std::vector<std::string> &params, const std::string &body, bool variadic){
        this->body = body;
        this->params = params;
        this->type = ItemTypes::FUNCTION;
        this->fn = [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
            return std::shared_ptr<Item>(NULL);
        };
        this->binary = false;
        this->variadic = variadic;
    }

    void set(const std::vector<std::string> &params, const std::function<std::shared_ptr<Item> (const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic){
        this->params = params;
        this->type = ItemTypes::FUNCTION;
        this->fn = fn;
        this->binary = true;
        this->variadic = variadic;
    }
    
};    

struct Context;
struct ItemContextPair {
    Item *item;
    Context *context;
    std::string name;
    ItemContextPair(){
        item = NULL;   
        context = NULL;
    }
    ItemContextPair(Item *item, Context *ctx, const std::string &name){
        this->item = item;
        this->context = ctx;
        this->name = name;
    }
}; 

static std::string ItemToText(Item *item);
struct Context : Item {
    std::unordered_map<std::string, std::shared_ptr<Item>> vars;
    std::shared_ptr<Context> head;

    std::shared_ptr<Item> get(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->get(name) : std::shared_ptr<Item>(NULL);
        }
        return it->second;
    }

    std::shared_ptr<Item> copy(bool deep = true) const {

        auto item = std::make_shared<Context>(Context());
        item->type = this->type;  
        item->head = this->head;    

        for(auto &it : this->vars){
            item->vars[it.first] = deep ? it.second->copy() : it.second;
        }        

        return item;
    }

    void setTop(std::shared_ptr<Context> &nctx){
        auto ctop = this->get("top");
        if(ctop){
            return;
        }
        auto ctx = std::static_pointer_cast<Item>(nctx);
        this->set("top", ctx);
    }

    ItemContextPair getWithContext(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->getWithContext(name) : ItemContextPair();
        }
        return ItemContextPair(it->second.get(), this, it->first);
    }    

    ItemContextPair getWithContext(std::shared_ptr<Item> &item){
        for(auto &it : this->vars){
            if(it.second.get() == item.get()){
                return ItemContextPair(it.second.get(), this, it.first);
            }
        }
        return this->head ? this->head->getWithContext(item) : ItemContextPair();
    }        

    std::shared_ptr<Item> set(const std::string &name, const std::shared_ptr<Item> &item){
        auto v = get(name);
        this->vars[name] = item;
        return item;
    }

    Context(){
        this->type = ItemTypes::CONTEXT;
        this->head = NULL;
    }

    Context(std::shared_ptr<Context> &ctx){
        this->type = ItemTypes::CONTEXT;        
        this->head = ctx;
    }    


    bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }

        return true;
    }

    void debug(){
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
            std::cout << it.first << getSpace(it.first.length()) << " -> " << getSpace(0) << ItemToText(it.second.get()) << std::endl;
        }
    }

};

static std::string ItemToText(Item *item){
    switch(item->type){
        default:
        case ItemTypes::NIL: {
            return "nil";
        };
        case ItemTypes::INTERRUPT: {
            auto interrupt = static_cast<Interrupt*>(item);
            return "[INT-"+InterruptTypes::str(interrupt->intType)+(interrupt->hasPayload() ? " "+ItemToText(interrupt->getPayload().get())+"" : "")+"]";
        };                
        case ItemTypes::NUMBER: {
            auto r = Tools::removeTrailingZeros(static_cast<Number*>(item)->n);
            return r;
        };
        case ItemTypes::STRING: {
            return "'"+static_cast<String*>(item)->data+"'";
        };        
        case ItemTypes::FUNCTION: {
            auto fn = static_cast<Function*>(item);        
            return "[fn ["+(fn->variadic ? "..." : Tools::compileList(fn->params))+"] ["+(fn->binary ? "BINARY" : fn->body )+"]]";
        };
        case ItemTypes::CONTEXT: {
            std::string output = "[ct ";
            auto proto = static_cast<Context*>(item);
            int n = proto->vars.size();
            int c = 0;
            for(auto &it : proto->vars){
                auto name = it.first;
                auto item = it.second;
                output += ItemToText(item.get())+ModifierTypes::str(ModifierTypes::NAMER)+name;
                if(c < n-1){
                    output += " ";
                }
                ++c;                
            }
            return output + "]";            
        };
        case ItemTypes::LIST: {
            std::string output = "[";
            auto list = static_cast<List*>(item);
            for(int i = 0; i < list->data.size(); ++i){
                auto item = list->get(i);
                output += ItemToText(item.get());
                if(i < list->data.size()-1){
                    output += " ";
                }
            }
            return output + "]";
        };         
    }
}

static void printList(const std::vector<std::string> &list){
    std::cout << "START" << std::endl;
    for(int i = 0 ; i < list.size(); ++i){
        std::cout << "'" << list[i] << "'" << std::endl;
    }
    std::cout << "END\n" << std::endl;
}

struct ModifierPair {
    uint8_t type;
    std::string subject;
    ModifierPair(){
        type = ModifierTypes::UNDEFINED;
    }
    ModifierPair(uint8_t type, const std::string &subject){
        this->type = type;
        this->subject = subject;

    }
};

struct ModifierEffect {
    bool expand;
    bool ctxSwitch;
    std::shared_ptr<Context> toCtx;
    std::string named;
    ModifierEffect(){

    }
    void reset(){
        this->expand = false;
        this->ctxSwitch = false;
    }
};

std::vector<std::string> parseTokens(std::string input, char sep, Cursor *cursor){
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
        if(c == '\'' && !onComment){
            onString = !onString;
            buffer += c;
        }else        
        if(c == '#' && !onString){
            onComment = !onComment;
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


struct Token {
    std::string first;
    std::vector<ModifierPair> modifiers;
    ModifierPair getModifier(uint8_t type) const {
        for(int i = 0; i < modifiers.size(); ++i){
            if(modifiers[i].type == type){
                return modifiers[i];
            }
        }
        return ModifierPair();
    }
    std::string literal() const {
        std::string part = "<"+this->first+">";
        for(int i = 0; i < modifiers.size(); ++i){
            auto &mod = modifiers[i];
            part += "("+ModifierTypes::str(mod.type)+mod.subject+")";
        }
        return part;
    }
};

Token parseTokenWModifier(const std::string &_input){
    std::string buffer;
    Token token;
    std::string input = _input;
    if(input[0] == '[' && input[input.size()-1] == ']'){
        input = input.substr(1, input.length() - 2);
    }
    for(int i = 0; i < input.length(); ++i){
        char c = input[input.length()-1 - i];
        auto t = ModifierTypes::getToken(c);
        if(t != ModifierTypes::UNDEFINED){
                std::reverse(buffer.begin(), buffer.end());            
                token.modifiers.push_back(ModifierPair(t, buffer));
                buffer = "";   
        }else
        if(t == ModifierTypes::UNDEFINED && i == input.length()-1){
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

std::vector<Token> buildTokens(const std::vector<std::string> &literals, Cursor *cursor){

    std::vector<Token> tokens;
    for(int i = 0; i < literals.size(); ++i){
        auto innLiterals = parseTokens(literals[i], ' ', cursor);
        // Complex tokens (It's usually tokens that actually are complete statements) need to be evaluated further        
        if(innLiterals.size() > 1){
            Token token;
            token.first = literals[i];
            tokens.push_back(token);  
        // Simple can be inserted right away
        }else{
            auto token = parseTokenWModifier(literals[i]);
            if(token.first == "" && i > 0){ // This means this modifier belongs to the token before this one
                auto &prev = tokens[tokens.size()-1];
                for(int j = 0; j < token.modifiers.size(); ++j){
                    prev.modifiers.push_back(token.modifiers[j]);
                }
                continue;
            }else{
                tokens.push_back(token);
            }
        }
    }

    return tokens;
};

std::vector<Token> compileTokens(const std::vector<Token> &tokens, int from, int amount){
	std::vector<Token> compiled;
	for(int i = 0; i < amount; ++i){
        compiled.push_back(tokens[from + i]);
	}
	return compiled;
}

bool FunctionConstraints::test(List *list, std::string &errormsg){
    std::vector<std::shared_ptr<Item>> items;
    for(int i = 0; i < list->data.size(); ++i){
        items.push_back(list->data[i]);
    }
    return this->test(items, errormsg);
}

bool FunctionConstraints::test(const std::vector<std::shared_ptr<Item>> &items, std::string &errormsg){
    if(!enabled || items.size() == 0){
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
            if(items[i]->type == ItemTypes::NIL){
                errormsg = "Provided nil value";
                return false;
            }
        }        
    }


    if(!allowMisMatchingTypes){
        uint8_t firstType = ItemTypes::NIL;
        for(int i = 0; i < items.size(); ++i){
            if(items[i]->type != ItemTypes::NIL && i < items.size()-1){
                firstType = items[i]->type;
            }
        }        
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type != firstType && items[i]->type != ItemTypes::NIL){
                errormsg = "Provided arguments with mismatching types";
                return false;
            }
        }
    }

    if(expectedTypeAt.size() > 0){
        for(int i = 0; i < items.size(); ++i){
            if(expectedTypeAt.count(i) > 0){
                if(items[i]->type != expectedTypeAt[i]){
                    errormsg = "Provided '"+ItemTypes::str(items[i]->type)+"' at position "+std::to_string(i)+". Expected type is '"+ItemTypes::str(expectedTypeAt[i])+"'.";
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
                    exTypes += "'"+ItemTypes::str(expectedTypes[i])+"'";
                    if(i < expectedTypes.size()-1){
                        exTypes += ", ";
                    }
                }
                errormsg = "Provided '"+ItemTypes::str(items[i]->type)+"'. Expected types are only "+exTypes+".";
                return false;
            }

        }        
    }    

    return true;
}


static std::shared_ptr<Item> eval(const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx);


static std::shared_ptr<Item> processPreInterpretModifiers(std::shared_ptr<Item> &item, std::vector<ModifierPair> modifiers, Cursor *cursor, std::shared_ptr<Context> &ctx, ModifierEffect &effects){
    effects.reset();
    for(int i = 0; i < modifiers.size(); ++i){
        auto &mod = modifiers[i];
        switch(mod.type){
            case ModifierTypes::TRAIT: {
                auto name = mod.subject;
                if(name.size() == 0){
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"': Trait modifier requires a name");
                    return item;                    
                }
                // ANY_NUMBER type trait
                if(Tools::isNumber(name) && item->hasTrait(TraitType::ANY_NUMBER)){
                    return item->runTrait(TraitType::ANY_NUMBER, name, cursor, ctx);    
                }else
                // ANY_STRING type trait
                if(!Tools::isNumber(name) && item->hasTrait(TraitType::ANY_STRING)){
                    return item->runTrait(TraitType::ANY_STRING, name, cursor, ctx);    
                }else                
                // Regular trait
                if(!item->hasTrait(name)){
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"': Trait '"+name+"' doesn't belong to '"+ItemTypes::str(item->type)+"' type");
                    return item;                      
                }
                item = item->runTrait(name, "", cursor, ctx);
            } break;            
            case ModifierTypes::LINKER: {

            } break;
            case ModifierTypes::NAMER: {
                effects.named = mod.subject;
                ctx->set(mod.subject, item);
            } break;        
            case ModifierTypes::SCOPE: {
                if(item->type != ItemTypes::CONTEXT){
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"': Scope modifier can only be used on contexts.");
                    return item;
                }
                if(mod.subject.size() > 0){
                    auto subsequent = std::static_pointer_cast<Context>(item)->get(mod.subject);
                    if(!subsequent){
                        cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"':"+mod.subject+"': Undefined symbol or imperative.");
                        return item;
                    }
                    item = subsequent;
                }else{
                    effects.toCtx = std::static_pointer_cast<Context>(item);
                    effects.ctxSwitch = true;
                }
            } break;   
            case ModifierTypes::EXPAND: {
                if(i == modifiers.size()-1){
                    effects.expand = true;
                }else{
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::EXPAND)+"': Expand modifier can only be used at the end of a modifier chain.");
                    return item;
                }
            } break;                                   
        }
    }
    return item;
}

// Interpret from STRING
static std::shared_ptr<Item> interpret(const std::string &_input, Cursor *cursor, std::shared_ptr<Context> &ctx){
    std::string input = _input;
    auto literals = parseTokens(input, ' ', cursor);
    // printList(literals);
    if(cursor->error){
        return std::make_shared<Item>(Item());
    }    

    // printList(literals);

    auto tokens = buildTokens(literals, cursor);


    // for(int i = 0; i < tokens.size(); ++i){
    //     std::cout << tokens[i].literal() << std::endl;
    // }


    // std::exit(1);

    return eval(tokens, cursor, ctx);
}

// Interpret from single TOKEN without dropping modifiers. Should be only used by `eval`
static std::shared_ptr<Item>  interpret(const Token &token, Cursor *cursor, std::shared_ptr<Context> &ctx){
    auto literals = parseTokens(token.first, ' ', cursor);
    if(cursor->error){
        return std::make_shared<Item>(Item());
    }
    auto tokens = buildTokens(literals, cursor);
    return eval(tokens, cursor, ctx);
}

static std::shared_ptr<Item> processIteration(const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx);
static void processPostInterpretExpandListOrContext(std::shared_ptr<Item> &solved, ModifierEffect &effects, std::vector<std::shared_ptr<Item>> &items, Cursor *cursor, const std::shared_ptr<Context> &ctx);


static std::shared_ptr<Item> runFunction(std::shared_ptr<Function> &fn, const std::vector<std::shared_ptr<Item>> items, Cursor *cursor, std::shared_ptr<Context> &ctx);
static std::shared_ptr<Item> runFunction(std::shared_ptr<Function> &fn, const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx);

static std::shared_ptr<Item> runFunction(std::shared_ptr<Function> &fn, const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx){
    std::vector<std::shared_ptr<Item>> items;
    for(int i = 0; i < tokens.size(); ++i){
        ModifierEffect effects;
        auto interp = interpret(tokens[i], cursor, ctx);
        auto solved = processPreInterpretModifiers(interp, tokens[i].modifiers, cursor, ctx, effects);                     
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }
        processPostInterpretExpandListOrContext(solved, effects, items, cursor, std::shared_ptr<Context>((Context*)NULL));
        if(cursor->error){
            return std::make_shared<Item>(Item());
        } 
    }
    return runFunction(fn, items, cursor, ctx);
}


static std::shared_ptr<Item> runFunction(std::shared_ptr<Function> &fn, const std::vector<std::shared_ptr<Item>> items, Cursor *cursor, std::shared_ptr<Context> &ctx){
    if(fn->binary){
        return fn->fn(items, cursor, ctx);  
    }else{
        auto tctx = std::make_shared<Context>(Context(ctx));
        tctx->setTop(ctx);
        if(fn->variadic){
            auto list = std::make_shared<List>(List(items, false));
            tctx->set("...", std::static_pointer_cast<Item>(list));
        }else{
            if(items.size() != fn->params.size()){
                cursor->setError("'"+ItemToText(fn.get())+"': Expects exactly "+(std::to_string(fn->params.size()))+" arguments");
                return std::make_shared<Item>(Item());
            }
            for(int i = 0; i < items.size(); ++i){
                tctx->set(fn->params[i], items[i]);
            }
        }
        return interpret(fn->body, cursor, tctx);
    }
}

static void processPostInterpretExpandListOrContext(std::shared_ptr<Item> &solved, ModifierEffect &effects, std::vector<std::shared_ptr<Item>> &items, Cursor *cursor, const std::shared_ptr<Context> &ctx){
    if(effects.expand && solved->type == ItemTypes::LIST){
        auto list = std::static_pointer_cast<List>(solved);
        for(int j = 0; j < list->data.size(); ++j){
            items.push_back(list->get(j));
        }
    }else
    if(effects.expand && solved->type == ItemTypes::CONTEXT){
        if(effects.expand && solved->type == ItemTypes::CONTEXT){
            auto proto = std::static_pointer_cast<Context>(solved);
            for(auto &it : proto->vars){
                if(ctx == NULL){
                    items.push_back(it.second);
                }else{
                    ctx->set(it.first, it.second);
                }
            }
        }  
    }else
    if(effects.expand && solved->type != ItemTypes::CONTEXT && solved->type != ItemTypes::LIST){
        cursor->setError("'"+ModifierTypes::str(ModifierTypes::EXPAND)+"': Expand modifier can only be used on iterables.");
        items.push_back(solved);
    }else{   
        items.push_back(solved);
    } 
}

static std::shared_ptr<Item> processIteration(const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx){
    std::vector<std::shared_ptr<Item>> items;
    for(int i = 0; i < tokens.size(); ++i){
        ModifierEffect effects;
        auto interp = interpret(tokens[i], cursor, ctx);
        auto solved = processPreInterpretModifiers(interp, tokens[i].modifiers, cursor, ctx, effects);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }
        if(solved->type == ItemTypes::INTERRUPT){
            auto interrupt =  std::static_pointer_cast<Interrupt>(solved);
            if(interrupt->intType == InterruptTypes::SKIP){
                if(interrupt->hasPayload()){
                    items.push_back(interrupt->getPayload());
                }
                continue;
            }else
            if(interrupt->intType == InterruptTypes::STOP){
                return std::static_pointer_cast<Item>(interrupt);
            }
        }   
        processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }                           
    }          
    return tokens.size() == 1 ? items[0] : std::make_shared<List>(List(items));    
}

static bool checkCond(std::shared_ptr<Item> &item){
    if(item->type == ItemTypes::NUMBER){
        return std::static_pointer_cast<Number>(item)->n <= 0 ? false : true;
    }else
    if(item->type != ItemTypes::NIL){
        return true;
    }else{
        return false;
    }
};

static std::shared_ptr<Item> eval(const std::vector<Token> &tokens, Cursor *cursor, std::shared_ptr<Context> &ctx){

    auto first = tokens[0];
    auto imp = first.first;

    if(imp == "set"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'set': Expects exactly 2 arguments: <NAME> <VALUE>.");
            return std::make_shared<Item>(Item());
        }
        ModifierEffect effects;
        auto interp = interpret(params[1], cursor, ctx);
        auto solved = processPreInterpretModifiers(interp, params[1].modifiers, cursor, ctx, effects);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }
        auto f = ctx->getWithContext(params[0].first);
        if(f.item){
            return f.context->set(params[0].first, solved);
        }else{

            return ctx->set(params[0].first, solved);
        }
    }else
    if(imp == "skip" || imp == "stop"){
        auto interrupt = std::make_shared<Interrupt>(Interrupt(imp == "skip" ? InterruptTypes::SKIP : InterruptTypes::STOP));
        if(tokens.size() > 1){
            auto params = compileTokens(tokens, 1, tokens.size()-1);
            auto solved = processIteration(params, cursor, ctx);
            if(solved->type == ItemTypes::INTERRUPT){
                return solved;
            }else{
                interrupt->setPayload(solved);
            }
        }
        return std::static_pointer_cast<Item>(interrupt);
    }else
    if(imp == "if"){
        if(tokens.size() > 4){
            cursor->setError("'if': Expects no more than 3 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return std::make_shared<Item>(Item());
        }
        if(tokens.size() < 3){
            cursor->setError("'if': Expects no less than 2 arguments: <CONDITION> <[CODE[0]...CODE[n]]> (ELSE-OPT)<[CODE[0]...CODE[n]]>.");
            return std::make_shared<Item>(Item());
        }        

        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto cond = params[0];
        auto trueBranch = params[1];
        auto tctx = std::make_shared<Context>(Context(ctx));
        tctx->setTop(ctx);

        ModifierEffect origEff;
        auto interp = interpret(cond, cursor, tctx);
        auto condV = processPreInterpretModifiers(interp, cond.modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        } 
        if(checkCond(condV)){
            ModifierEffect effects;
            auto interp = interpret(trueBranch, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, trueBranch.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }
            return solved;      
        }else
        if(params.size() == 3){
            auto falseBranch = params[2];
            ModifierEffect effects;
            auto interp = interpret(falseBranch, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, falseBranch.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }
            return solved;            
        }else{
            return condV;
        }
    }else
    if(imp == "do"){
        if(tokens.size() > 3 || tokens.size() < 3){
            cursor->setError("'do': Expects exactly 2 arguments: <CONDITION> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<Item>(Item());
        }
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto cond = params[0];
        auto code = params[1];
        auto tctx = std::make_shared<Context>(Context(ctx));
        tctx->setTop(ctx);

        ModifierEffect origEff;
        auto interp = interpret(cond, cursor, tctx);
        auto condV = processPreInterpretModifiers(interp, cond.modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }  
        auto last = std::shared_ptr<Item>(NULL);
        while(checkCond(condV)){
            ModifierEffect effects;
            auto interp = interpret(code, cursor, tctx);
            auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }
            if(solved->type == ItemTypes::INTERRUPT){
                auto interrupt =  std::static_pointer_cast<Interrupt>(solved);
                if(interrupt->intType == InterruptTypes::SKIP){
                    continue;
                }else
                if(interrupt->intType == InterruptTypes::STOP){
                    return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                }
            }         
            last = solved;
            // Next iteration
            auto reinterp = interpret(cond, cursor, tctx);
            condV = processPreInterpretModifiers(reinterp, cond.modifiers, cursor, tctx, origEff);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }            
        }             
        return last ? last : std::make_shared<Item>(Item());
    }else
    if(imp == "iter"){
        if(tokens.size() > 3 || tokens.size() < 3){
            cursor->setError("'iter': Expects exactly 2 arguments: <ITERABLE> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<Item>(Item());
        }        
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto code = params[1];
        auto tctx = std::make_shared<Context>(Context(ctx));
        tctx->setTop(ctx);
        ModifierEffect origEff;
        auto interp = interpret(params[0], cursor, tctx);
        auto iterable = processPreInterpretModifiers(interp, params[0].modifiers, cursor, tctx, origEff);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }            
        if(iterable->type == ItemTypes::LIST){
            std::vector<std::shared_ptr<Item>> items;
            auto list = std::static_pointer_cast<List>(iterable);
            for(int i = 0; i < list->data.size(); ++i){
                auto item = list->get(i);
                // We use the namer to name every iteration's current variable
                if(origEff.named.size() > 0){
                    tctx->set(origEff.named, item);
                }                
                ModifierEffect effects;
                auto interp = interpret(code, cursor, tctx);
                auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }                   
                if(solved->type == ItemTypes::INTERRUPT){
                    auto interrupt =  std::static_pointer_cast<Interrupt>(solved);
                    if(interrupt->intType == InterruptTypes::SKIP){
                        if(interrupt->hasPayload()){
                            items.push_back(interrupt->getPayload());
                        }
                        continue;
                    }else
                    if(interrupt->intType == InterruptTypes::STOP){
                        return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                    }
                }           
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }                               
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(items)));
        }else
        if(iterable->type == ItemTypes::CONTEXT){
            std::vector<std::shared_ptr<Item>> items;
            auto list = std::static_pointer_cast<Context>(iterable);
            for(auto &it : list->vars){
                auto item = it.second;
                // We use the namer to name every iteration's current variable
                if(origEff.named.size() > 0){
                    tctx->set(origEff.named, item);
                }                
                ModifierEffect effects;
                auto interp = interpret(code, cursor, tctx);
                auto solved = processPreInterpretModifiers(interp, code.modifiers, cursor, tctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }             
                if(solved->type == ItemTypes::INTERRUPT){
                    auto interrupt =  std::static_pointer_cast<Interrupt>(solved);
                    if(interrupt->intType == InterruptTypes::SKIP){
                        if(interrupt->hasPayload()){
                            items.push_back(interrupt->getPayload());
                        }
                        continue;
                    }else
                    if(interrupt->intType == InterruptTypes::STOP){
                        return interrupt->hasPayload() ?  interrupt->getPayload() : interrupt;
                    }
                }               
                processPostInterpretExpandListOrContext(solved, effects, items, cursor, NULL);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                } 
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(items)));
        }else{
            cursor->setError("'iter': Expects argument(0) to be iterable.");
            return std::make_shared<Item>(Item());        
        }
    }else
    if(imp == "fn"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'fn': Expects exactly 2 arguments: <[ARGS[0]...ARGS[n]]> <[CODE[0]...CODE[n]]>.");
            return std::make_shared<Item>(Item());
        }
        bool isVariadic = false;
        auto argNames = parseTokens(params[0].first, ' ', cursor);
        if(cursor->error){
            return std::make_shared<Item>(Item());
        }
        if(argNames.size() == 1 && argNames[0] == "..."){
            isVariadic = true;
        }
        auto code = params[1];

        return std::static_pointer_cast<Item>(  std::make_shared<Function>(isVariadic ? std::vector<std::string>({}) : argNames, code.first, isVariadic)  );
    }else
    if(imp == "ct"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        auto pctx = std::make_shared<Context>(Context(ctx));
        for(int i = 0; i < params.size(); ++i){
            ModifierEffect effects;
            auto interp = interpret(params[i], cursor, pctx);
            auto solved = processPreInterpretModifiers(interp, params[i].modifiers, cursor, pctx, effects);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }                
            std::vector<std::shared_ptr<Item>> items;
            processPostInterpretExpandListOrContext(solved, effects, items, cursor, pctx);
            if(cursor->error){
                return std::make_shared<Item>(Item());
            }  
            for(int j = 0; j < items.size(); ++j){
                if(effects.named.size() == 0){
                    pctx->set("v-"+std::to_string(j), items[j]);
                }
            }
        }
        return pctx;
    }else{
        auto var = ctx->get(imp);
        // Is it a function?
        if(var && var->type == ItemTypes::FUNCTION){
            auto fn = std::static_pointer_cast<Function>(var);
            return runFunction(fn, compileTokens(tokens, 1, tokens.size()-1), cursor, ctx);
        }else   
        // Is it just a variable?
        if(var && tokens.size() == 1){
            ModifierEffect effects;
            auto r = processPreInterpretModifiers(var, first.modifiers, cursor, ctx, effects);
            return r;
        }else
        // Is it a natural type?
        if(tokens.size() == 1){
            if(!var && Tools::isNumber(imp)){
                auto n = std::make_shared<Number>(Number(std::stod(imp)));
                ModifierEffect effects;
                auto casted = std::static_pointer_cast<Item>(n);
                auto result = processPreInterpretModifiers(casted, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }                      
                return result;
            }else
            if(!var && Tools::isString(imp)){
                ModifierEffect effects;
                auto n = std::make_shared<String>(String(imp.substr(1, imp.length() - 2)));
                auto casted = std::static_pointer_cast<Item>(n);
                auto result = processPreInterpretModifiers(casted, first.modifiers, cursor, ctx, effects);
                return result;
            }else
            if(!var && Tools::isList(imp)){
                ModifierEffect effects;
                auto interp = interpret(first, cursor, ctx);
                auto result = processPreInterpretModifiers(interp, first.modifiers,cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }                   
                return result;
            }else
            if(imp == "nil"){
                ModifierEffect effects;
                auto n = std::make_shared<Item>(Item());
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                }                      
                return result;
            }else{
                cursor->setError("'"+imp+"': Undefined symbol or imperative."); // TODO: Check if it's running as relaxed
                return std::make_shared<Item>(Item());
            }
        }else
        // List or in-line context switch?
        if(tokens.size() > 1){
            auto fmods = tokens[0].getModifier(ModifierTypes::SCOPE);
            // Super rad inline-contex switching d:)
            if(fmods.type == ModifierTypes::SCOPE){
                ModifierEffect effects;
                auto interp = interpret(tokens[0], cursor, ctx);
                auto solved = processPreInterpretModifiers(interp, tokens[0].modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return std::make_shared<Item>(Item());
                } 
                auto inlineCode = compileTokens(tokens, 1, tokens.size()-1);
                return eval(inlineCode, cursor, effects.toCtx); // Check if toCtx is null? Shouldn't be possible tho
            }else{
            // List it is
                return processIteration(tokens, cursor, ctx);
            }
        }
    }

    return std::make_shared<Item>(Item());
}

bool typicalVariadicArithmeticCheck(const std::string &name, const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, int max = -1){
    FunctionConstraints consts;
    consts.setMinParams(2);
    if(max != -1){
        consts.setMaxParams(max);
    }
    consts.setExpectType(ItemTypes::NUMBER);
    consts.allowMisMatchingTypes = false;
    consts.allowNil = false;
    std::string errormsg;
    if(!consts.test(params, errormsg)){
        cursor->setError("'"+name+"': "+errormsg);
        return false;
    }
    return true;
};

static void addStandardOperators(std::shared_ptr<Context> &ctx){

    /*

        ARITHMETIC OPERATORS

    */
    ctx->set("+", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("+", params, cursor)){
            return std::make_shared<Item>(Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            r += std::static_pointer_cast<Number>(params[i])->n;
        }
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(r)));
    }, true)));
    ctx->set("-", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("-", params, cursor)){
            return std::make_shared<Item>(Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            r -= std::static_pointer_cast<Number>(params[i])->n;
        }
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(r)));
    }, true)));
    ctx->set("*", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("*", params, cursor)){
            return std::make_shared<Item>(Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            r *= std::static_pointer_cast<Number>(params[i])->n;
        }
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(r)));
    }, true)));  
    ctx->set("/", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("/", params, cursor)){
            return std::make_shared<Item>(Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            r /= std::static_pointer_cast<Number>(params[i])->n;
        }
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(r)));
    }, true)));
    ctx->set("pow", std::make_shared<Function>(Function({"a", "b"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("pow", params, cursor, 2)){
            return std::make_shared<Item>(Item());
        }
        __CV_NUMBER_NATIVE_TYPE r = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            r =  std::pow(r, std::static_pointer_cast<Number>(params[i])->n);
        }
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(r)));
    }, false))); 
    ctx->set("sqrt", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::NUMBER);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'sqrt': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        if(params.size() == 1){
            return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(  std::sqrt(std::static_pointer_cast<Number>(params[0])->n)   )));
        }else{
            std::vector<std::shared_ptr<Item>> result;
            for(int i = 0; i < params.size(); ++i){
                result.push_back(
                    std::static_pointer_cast<Item>(std::make_shared<Number>(Number(  std::sqrt(std::static_pointer_cast<Number>(params[i])->n)   )))
                );
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(result, false)));
        }
    }, true)));    
       
    /* 
        QUICK MATH
    */ 
    ctx->set("++", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'++': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        if(params.size() == 1){
            ++std::static_pointer_cast<Number>(params[0])->n;
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                ++std::static_pointer_cast<Number>(params[i])->n;
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(params, false)));
    }, true)));

    ctx->set("--", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'--': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        if(params.size() == 1){
            --std::static_pointer_cast<Number>(params[0])->n;
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                --std::static_pointer_cast<Number>(params[i])->n;
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(params, false)));
    }, true)));    


    ctx->set("//", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'//': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        if(params.size() == 1){
            std::static_pointer_cast<Number>(params[0])->n /= 2;
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                std::static_pointer_cast<Number>(params[i])->n /= 2;
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(params, false)));
    }, true)));    

    ctx->set("**", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'**': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        if(params.size() == 1){
            std::static_pointer_cast<Number>(params[0])->n *= std::static_pointer_cast<Number>(params[0])->n;
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                std::static_pointer_cast<Number>(params[i])->n *= std::static_pointer_cast<Number>(params[i])->n;
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(params, false)));
    }, true)));


    /*
        LISTS
    */
    ctx->set("l-range", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        if(!typicalVariadicArithmeticCheck("l-range", params, cursor)){
            return std::make_shared<Item>(Item());
        }

        auto s = std::static_pointer_cast<Number>(params[0])->n;
        auto e = std::static_pointer_cast<Number>(params[1])->n;
        
        std::vector<std::shared_ptr<Item>> result;
        if(s == e){
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(result, false)));
        }

        bool inc = e > s;
        __CV_NUMBER_NATIVE_TYPE step = Tools::positive(params.size() == 3 ? std::static_pointer_cast<Number>(params[2])->n : 1);

        for(__CV_NUMBER_NATIVE_TYPE i = s; (inc ? i <= e : i >= e); i += (inc ? step : -step)){
            result.push_back(
                std::static_pointer_cast<Item>(std::make_shared<Number>(i))
            );
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(result, false)));
    }, true)));
    
    ctx->set("l-sort", std::make_shared<Function>(Function({"c", "l"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::STRING);
        consts.setExpectedTypeAt(1, ItemTypes::LIST);
        
        // TODO: add function support

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-sort': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        auto criteria = std::static_pointer_cast<String>(params[0]);

        auto &order = criteria->data;
        if(order != "DESC" && order != "ASC"){
            cursor->setError("'l-sort': criteria can only be 'DESC' or 'ASC'.");
            return std::make_shared<Item>(Item());
        }        

        auto list = std::static_pointer_cast<List>(params[1]);
        std::vector<std::shared_ptr<Item>> items;
        for(int i = 0; i < list->data.size(); ++i){
            items.push_back(list->get(i));
        }        

        FunctionConstraints constsList;
        constsList.setExpectType(ItemTypes::NUMBER);
        constsList.allowMisMatchingTypes = false;        
        constsList.allowNil = false;
        if(!constsList.test(items, errormsg)){
            cursor->setError("'l-sort': "+errormsg);
            return std::make_shared<Item>(Item());
        }        

        std::sort(items.begin(), items.end(), [&](std::shared_ptr<Item> &a, std::shared_ptr<Item> &b){
            return order == "DESC" ?     std::static_pointer_cast<Number>(a)->n > std::static_pointer_cast<Number>(b)->n :
                                        std::static_pointer_cast<Number>(a)->n < std::static_pointer_cast<Number>(b)->n;
        });

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));
    }, false)));    


    ctx->set("l-filter", std::make_shared<Function>(Function({"c", "l"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::FUNCTION);
        consts.setExpectedTypeAt(1, ItemTypes::LIST);
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-filter': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        auto criteria = std::static_pointer_cast<Function>(params[0]);

        if(criteria->variadic || criteria->params.size() != 1){
            cursor->setError("'l-filter': criteria's function must receive exactly 1 argument(a).");
            return std::make_shared<Item>(Item());
        }

        auto list = std::static_pointer_cast<List>(params[1]);
        std::vector<std::shared_ptr<Item>> items;
        for(int i = 0; i < list->data.size(); ++i){
            auto result = runFunction(criteria, std::vector<std::shared_ptr<Item>>{list->get(i)}, cursor, ctx);
            if(checkCond(result)){
                continue;
            }
            items.push_back(list->get(i));
        }        

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));

    }, false)));  


    ctx->set("l-flat", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = true;
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-flat': "+errormsg);
            return std::make_shared<Item>(Item());
        }


        std::vector<std::shared_ptr<Item>> finalList;

        std::function<void(std::shared_ptr<Item> &item)> recursive = [&](const std::shared_ptr<Item> &item){
            if(item->type == ItemTypes::LIST){
                auto list = std::static_pointer_cast<List>(item);
                for(int i = 0; i < list->data.size(); ++i){
                    auto item = list->get(i);
                    recursive(item);
                }
            }else
            if(item->type == ItemTypes::CONTEXT){
                auto ctx = std::static_pointer_cast<Context>(item);
                for(auto &it : ctx->vars){
                    recursive(it.second);
                }
            }else{                
                finalList.push_back(item);
            }
        };


        for(int i = 0; i < params.size(); ++i){
            auto item = params[i];
            recursive(item);
        }



        return std::static_pointer_cast<Item>(std::make_shared<List>(List(finalList, false)));
    }, true)));   



    ctx->set("splice", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::CONTEXT);
        consts.setExpectType(ItemTypes::LIST);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'splice': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        auto getAllItems = [](std::shared_ptr<Item> &item){
            std::unordered_map<std::string, std::shared_ptr<Item>> all;
            if(item->type == ItemTypes::LIST){
                auto list = std::static_pointer_cast<List>(item);
                for(int i = 0; i < list->data.size(); ++i){
                    all[std::to_string(i)] = list->get(i);
                }
            }else{
                auto ctx = std::static_pointer_cast<Context>(item);
                for(auto &it : ctx->vars){
                    all[it.first] = it.second;
                }
            }
            return all;
        };

        auto first = params[0];
        // First item decides the spliced result
        if(first->type == ItemTypes::LIST){
            std::vector<std::shared_ptr<Item>> compiled;
            for(int i = 0; i < params.size(); ++i){
                auto current = params[i];
                auto all = getAllItems(current);
                for(auto &it : all){
                    compiled.push_back(it.second);
                }
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(compiled, false)));
        }else{
            auto ctx = std::make_shared<Context>(Context());
            for(int i = 0; i < params.size(); ++i){
                auto current = params[i];
                auto all = getAllItems(current);
                for(auto &it : all){
                    ctx->set(it.first, it.second);
                }
            }
            return std::static_pointer_cast<Item>(ctx);
        }

        return std::make_shared<Item>(Item());
    }, true)));    

    ctx->set("l-push", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-push': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        std::vector<std::shared_ptr<Item>> items;
        auto last = params[params.size()-1];

        if(last->type == ItemTypes::LIST){
            auto list = std::static_pointer_cast<List>(last);
            for(int i = 0; i < list->data.size(); ++i){
                items.push_back(list->get(i));
            }
        }else{
            items.push_back(last);
        }

        for(int i = 0; i < params.size()-1; ++i){
            items.push_back(params[i]);
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));

    }, true))); 
    
    ctx->set("l-pop", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'<<': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        std::vector<std::shared_ptr<Item>> items;
        auto first = params[0];

        if(first->type == ItemTypes::LIST){
            auto list = std::static_pointer_cast<List>(first);
            for(int i = 0; i < list->data.size(); ++i){
                items.push_back(list->get(i));
            }
        }else{
            items.push_back(first);
        }

        for(int i = 1; i < params.size(); ++i){
            items.push_back(params[i]);
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));

    }, true)));     

    ctx->set("l-sub", std::make_shared<Function>(Function({"l", "p", "n"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::LIST);
        consts.setExpectedTypeAt(1, ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, ItemTypes::NUMBER);
        static const std::string name = "l-sub";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        int index = std::static_pointer_cast<Number>(params[1])->n;
        auto list = std::static_pointer_cast<List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<Number>(params[2])->n : 1;

        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size())+")");
            return std::make_shared<Item>(Item());
        }      

        std::vector<std::shared_ptr<Item>> result;
        for(int i = 0; i < list->data.size(); ++i){
            if(i >= index && i < index+n){
                result.push_back(list->get(i));
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(result, false)));

    }, false)));


    ctx->set("l-cut", std::make_shared<Function>(Function({"l", "p", "n"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::LIST);
        consts.setExpectedTypeAt(1, ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, ItemTypes::NUMBER);
        static const std::string name = "l-cut";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        int index = std::static_pointer_cast<Number>(params[1])->n;
        auto list = std::static_pointer_cast<List>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<Number>(params[2])->n : 1;

        if(index+n < 0 || index+n > list->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->data.size())+")");
            return std::make_shared<Item>(Item());
        }      

        std::vector<std::shared_ptr<Item>> result;
        for(int i = 0; i < list->data.size(); ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result.push_back(list->get(i));
        }

        return std::static_pointer_cast<Item>(std::make_shared<List>(List(result, false)));


    }, false)));      

    ctx->set("l-reverse", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        auto reverse = [&](std::shared_ptr <List> &lst){
            std::vector<std::shared_ptr<Item>> result;
            for(int i = 0; i < lst->data.size(); ++i){
                result.push_back(lst->get(lst->data.size()-1 - i));
            }
            return std::make_shared<List>(List(result, false));
        };

        if(params.size() == 1){
            auto item = std::static_pointer_cast<List>(params[0]);
            return std::static_pointer_cast<Item>(reverse(item));
        }else{
            std::vector<std::shared_ptr<Item>> items;
            for(int i = 0; i < params.size(); ++i){
                auto item = std::static_pointer_cast<List>(params[i]);
                items.push_back(
                    std::static_pointer_cast<Item>(reverse(item))
                );
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));
        }



    }, true)));        


    /*
        STRING
    */
    ctx->set("s-reverse", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        auto reverse = [&](std::shared_ptr<String> &str){
            std::string result;
            for(int i = 0; i < str->data.size(); ++i){
                result += str->data[str->data.size()-1 - i];
            }
            return result;
        };

        if(params.size() == 1){
            auto casted = std::static_pointer_cast<String>(params[0]);
            return std::static_pointer_cast<Item>(std::make_shared<String>(String(reverse(casted))));
        }else{
            std::vector<std::shared_ptr<Item>> items;
            for(int i = 0; i < params.size(); ++i){
                auto casted = std::static_pointer_cast<String>(params[i]);
                items.push_back(
                    std::static_pointer_cast<Item>(std::make_shared<String>(String(reverse(casted))))
                );
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(items, false)));
        }



    }, true)));  

    ctx->set("s-join", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        // consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::STRING);

        static const std::string name = "s-join";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        std::string result;
        for(int i = 0; i < params.size(); ++i){
            auto str = std::static_pointer_cast<String>(params[i]);
            result += str->get();
        }
        return std::static_pointer_cast<Item>(std::make_shared<String>(String(result)));
    }, true)));   

    ctx->set("s-cut", std::make_shared<Function>(Function({"s", "p", "n"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::STRING);
        consts.setExpectedTypeAt(1, ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, ItemTypes::NUMBER);

        static const std::string name = "s-cut";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<Item>(Item());
        }



        int index = std::static_pointer_cast<Number>(params[1])->n;
        auto str = std::static_pointer_cast<String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<Number>(params[2])->n : 1;

        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size())+")");
            return std::make_shared<Item>(Item());
        }      

        std::string result;
        for(int i = 0; i < str->data.size(); ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result += str->data[i];
        }

        return std::static_pointer_cast<Item>(std::make_shared<String>(String(result)));

    }, false)));       


    ctx->set("s-sub", std::make_shared<Function>(Function({"s", "p", "n"}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(3);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::STRING);
        consts.setExpectedTypeAt(1, ItemTypes::NUMBER);
        consts.setExpectedTypeAt(2, ItemTypes::NUMBER);

        static const std::string name = "s-sub";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return std::make_shared<Item>(Item());
        }


        int index = std::static_pointer_cast<Number>(params[1])->n;
        auto str = std::static_pointer_cast<String>(params[0]);
        int n = params.size() > 2 ? std::static_pointer_cast<Number>(params[2])->n : 1;

        if(index+n < 0 || index+n > str->data.size()){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->data.size())+")");
            return std::make_shared<Item>(Item());
        }      

        std::string result;
        for(int i = 0; i < str->data.size(); ++i){
            if(i >= index && i < index+n){
                result += str->data[i];
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<String>(String(result)));

    }, false)));     


    /*
        LOGIC
    */
    ctx->set("or", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'or': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == ItemTypes::NUMBER && std::static_pointer_cast<Number>(params[i])->n != 0) ||
                (params[i]->type != ItemTypes::NIL && params[i]->type != ItemTypes::NUMBER)){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
    }, true)));        

    ctx->set("and", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'and': "+errormsg);
            return std::make_shared<Item>(Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == ItemTypes::NUMBER && std::static_pointer_cast<Number>(params[i])->n == 0) || (params[i]->type == ItemTypes::NIL) ){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
            }
        }

        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
    }, true)));    

    ctx->set("not", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'not': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto val = [](std::shared_ptr<Item> &item){
            if(item->type == ItemTypes::NUMBER){
                return std::static_pointer_cast<Number>(item)->n == 0 ? std::make_shared<Number>(Number(1)) : std::make_shared<Number>(Number(0));
            }else
            if(item->type == ItemTypes::NIL){
                return std::make_shared<Number>(Number(1));
            }else{
                return std::make_shared<Number>(Number(0));
            }
        };
        if(params.size() == 1){
            auto item = params[0];
            return std::static_pointer_cast<Item>(val(item));
        }else{
            std::vector<std::shared_ptr<Item>> results;
            for(int i = 0; i < params.size(); ++i){
                auto item = params[i];
                results.push_back(std::static_pointer_cast<Item>(val(item)));
            }
            return std::static_pointer_cast<Item>(std::make_shared<List>(List(results)));
        }
        return std::make_shared<Item>(Item());
    }, true)));                  


    ctx->set("eq", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'eq': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto last = params[0];
        for(int i = 1; i < params.size(); ++i){
            auto item = params[i];
            if(!last->isEq(item)){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
            }
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
    }, true)));     


    ctx->set("neq", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'neq': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        for(int i = 0; i < params.size(); ++i){
            auto p1 = params[i];
            for(int j = 0; j < params.size(); ++j){
                if(i == j){
                    continue;
                }
                auto p2 = params[j];
                if(p1->isEq(p2)){
                    return std::static_pointer_cast<Item>(std::make_shared<Number>((Number(0))));
                }
            }
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>((Number(1))));
    }, true)));      


    ctx->set("gt", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gt': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto last = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            if(last <= std::static_pointer_cast<Number>(params[i])->n){
                return std::static_pointer_cast<Item>(std::make_shared<Number>((Number(0))));
            }
            last = std::static_pointer_cast<Number>(params[i])->n;
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>((Number(1))));
    }, true)));    


    ctx->set("gte", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto last = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            if(last <  std::static_pointer_cast<Number>(params[i])->n){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
            }
            last = std::static_pointer_cast<Number>(params[i])->n;
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
    }, true)));         


    ctx->set("lt", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'lt': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto last = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            if(last >=  std::static_pointer_cast<Number>(params[i])->n){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
            }
            last = std::static_pointer_cast<Number>(params[i])->n;
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
    }, true)));    

    ctx->set("lte", std::make_shared<Function>(Function({}, [](const std::vector<std::shared_ptr<Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return std::make_shared<Item>(Item());
        }
        auto last = std::static_pointer_cast<Number>(params[0])->n;
        for(int i = 1; i < params.size(); ++i){
            if(last >  std::static_pointer_cast<Number>(params[i])->n){
                return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(0)));
            }
            last = std::static_pointer_cast<Number>(params[i])->n;
        }				
        return std::static_pointer_cast<Item>(std::make_shared<Number>(Number(1)));
    }, true)));    
         
}


int main(){

    auto ctx = std::make_shared<Context>(Context());
    Cursor cursor;

    addStandardOperators(ctx);
    // Context nctx;
    // nctx.set("a", new Number(25));
    // ctx.set("test", &nctx);

    auto r = interpret("set a [ct 99~n]", &cursor, ctx);
    r = interpret("a: ++ n", &cursor, ctx);
    // auto r = interpret("set a 'hola como estas?'|list", &cursor, ctx);
    // auto r = interpret("+ 1 2 3 4", &cursor, ctx);

    // auto r = interpret("s-join ['hola como estas?'|list]^", &cursor, ctx);
    // auto r = interpret("[1 2 [3 4 5]|n]", &cursor, &ctx);
    // auto r = interpret("[1 [28 28]^ 3]^ [4 5 6]^ [7 8 [99 99]^]^]", &cursor, &ctx);

    std::cout << ItemToText(r.get()) << std::endl;
    // return 0;
    if(cursor.error){
        std::cout << cursor.message << std::endl;
    }
    ctx->debug();

    // ctx.deconstruct();



    return 0;
}