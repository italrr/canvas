#include <algorithm>
#include <functional>
#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <inttypes.h>


#define __CV_NUMBER_NATIVE_TYPE double

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
        STOP,
        SKIP,
        RET		
    };
}

struct Item;
struct Context;
struct Cursor;

struct Trait {
    std::string name;
    std::function<Item*(Item* subject, const std::string &value, Cursor *cursor, Context *ctx)> action;
    Trait(){
        this->action = [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            return (Item*)NULL;
        };
    }
};


struct Item {
    uint8_t type;
    uint32_t size;
    void *data;
    uint32_t refc;
    std::unordered_map<std::string, Trait> traits;

    Item(){
        registerTraits();
        setToNil();
    }
    void incRefC(){
        ++this->refc;
    }
    void decRefC(){
        if(this->refc == 0) return;
        --this->refc;
        deconstruct();
    }
    
    void registerTrait(const std::string &name, const std::function<Item*(Item* subject, const std::string &value, Cursor *cursor, Context *ctx)> &action){
        Trait trait;
        trait.name = name;
        trait.action = action;
        this->traits[name] = trait;
    }

    Item *runTrait(const std::string &name, const std::string &value, Cursor *cursor, Context *ctx){
        return this->traits.find(name)->second.action(this, value, cursor, ctx);
    }

    bool hasTrait(const std::string &name){
        auto it = this->traits.find(name);
        if(it == this->traits.end()){
            return false;
        }
        return true;
    }
    

    virtual void registerTraits(){

    }

    virtual bool isEq(Item *item){
        return (this->type == ItemTypes::NIL && item->type == ItemTypes::NIL) || this == item;
    }
    virtual Item *copy(bool deep = true) const {
        auto item = new Item();
        item->size = this->size;
        item->data = malloc(this->size);
        item->type = this->type;
        item->refc = 1;
        memcpy(item->data, this->data, this->size);
        return item;
    }
    void deconstruct(){
        // if(this->refc == 0 || this->type == ItemTypes::NIL){
        //     return;
        // }
        // --this->refc;
        // if(this->refc == 0){
        //     this->clear();
        // }
    }
    void setToNil(){
        this->data = NULL;
        this->size = 0;
        this->refc = 0;
        this->type = ItemTypes::NIL;        
    }
    virtual bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        // free(this->data);
        // this->setToNil();
        return true;
    }
};

struct Number : Item {

    Number(){
        registerTraits();
    }

    Number(__CV_NUMBER_NATIVE_TYPE v){
        registerTraits();        
        write(v);
    }

    bool isEq(Item *item){
        return  item->type == this->type && *static_cast<__CV_NUMBER_NATIVE_TYPE*>(this->data)
                == *static_cast<__CV_NUMBER_NATIVE_TYPE*>(item->data);
    }    

    void registerTraits(){
        this->registerTrait("is-neg", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number(  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(subject->data) < 0 ? 1 : 0 ));
        });
        this->registerTrait("inv", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number(  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(subject->data) * -1 ));
        });    
        this->registerTrait("is-odd", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number(  (int)(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(subject->data)) % 2 != 0 ));
        });   
        this->registerTrait("is-even", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number(  (int)(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(subject->data)) % 2 == 0 ));
        });                       
    }

    Item *copy(bool deep = true) const {
        auto item = new Number();
        item->size = this->size;
        item->data = malloc(this->size);
        item->type = this->type;
        item->refc = 1;
        memcpy(item->data, this->data, this->size);
        return item;
    }    

    void write(const __CV_NUMBER_NATIVE_TYPE v){
        this->type = ItemTypes::NUMBER;
        this->size = sizeof(__CV_NUMBER_NATIVE_TYPE);
        this->data = malloc(this->size);
        this->refc = 1;        
        memcpy(this->data, &v, sizeof(__CV_NUMBER_NATIVE_TYPE));
    }

    void read(__CV_NUMBER_NATIVE_TYPE *v){
        if(this->type == ItemTypes::NIL){
            return;
        }
        memcpy(v, this->data, sizeof(__CV_NUMBER_NATIVE_TYPE));
    }
    
    __CV_NUMBER_NATIVE_TYPE get(){
        return this->type == ItemTypes::NIL ? 0 : *static_cast<__CV_NUMBER_NATIVE_TYPE*>(this->data);
    }

};

struct String : Item {
    
    String(){
        // Empty constructor makes this string NIL
        registerTraits();
    }

    String(const std::string &str){
        registerTraits();
        this->write(str);
    }    

    void registerTraits(){
        this->registerTrait("n", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number( subject->size ));
        });
        this->registerTrait("reverse", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            std::string result;
            for(int i = 0; i < subject->size; ++i){
                result += *(char*)((size_t)subject->data + subject->size-1 - i);
            }
            return static_cast<Item*>(new String( result ));
        });                           
    }

    bool isEq(Item *item){
        if(this->type != item->type || this->size != item->size){
            return false;
        }
        for(int i = 0; i < this->size; ++i){
            char cA = static_cast<uint8_t>(*(char*)((size_t)this->data + i));
            char cB =  static_cast<uint8_t>(*(char*)((size_t)item->data + i));
            if(cA != cB){
                return false;
            }
        }

        return true;
    } 

    void read(std::string &dst, uint32_t start, uint32_t amount = 0){
        if(amount == 0){
            amount =  this->size;
        }
        for(auto i = start; i < this->size; ++i){
            dst += *static_cast<char*>( static_cast<char*>(this->data) + i );
        }
    }

    void write(const std::string &str){
        this->type = ItemTypes::STRING;
        this->size = str.size();
        this->data = malloc(this->size);
        this->refc = 1;
        memcpy(this->data, &str[0], str.size());
    } 

    std::string get(){
        std::string r;
        for(int i = 0; i < this->size; ++i){
            r += static_cast<uint8_t>(*(char*)((size_t)this->data + i));
        }        
        return r;   
    }

};

static std::string ItemToText(Item *item);

struct List : Item {

    List(){
        registerTraits();
    }

    List(const std::vector<Item*> &list, bool toCopy = true){
        registerTraits();        
        write(list, toCopy);
    }

    void registerTraits(){
        this->registerTrait("n", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            return static_cast<Item*>(new Number( subject->size ));
        });
        this->registerTrait("reverse", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            std::vector<Item*> result;
            auto list = static_cast<List*>(subject);
            for(int i = 0; i < list->size; ++i){
                result.push_back(list->get(list->size-1 - i));
            }
            return static_cast<Item*>(new List(result));
        });                           
    }

    Item *copy(bool deep = true) const {
        auto item = new List();
        item->size = this->size;
        item->type = this->type;
        item->refc = 1;
        item->data = malloc(this->size * sizeof(size_t));       

        for(int i = 0; i < this->size; ++i){
            auto copy = this->get(i)->copy();
            size_t address = (size_t)copy;   
            memcpy((void*)((size_t)item->data + i * sizeof(size_t)), &address, sizeof(size_t));
        }        

        return item;
    }

    void write(const std::vector<Item*> &list, bool toCopy = true){

        this->type = ItemTypes::LIST;
        this->size = list.size();
        this->refc = 1;
        this->data = malloc(this->size * sizeof(size_t));

        for(int i = 0; i < list.size(); ++i){
            auto copy = toCopy ? list[i]->copy() : list[i];
            size_t address = (size_t)copy;   
            memcpy((void*)((size_t)this->data + i * sizeof(size_t)), &address, sizeof(size_t));
        }
    }

    Item *get(size_t index) const {
        // This insane dereference can be improved
        // maybe later
        return static_cast<Item*>((void*)*(size_t*)((size_t)this->data + index * sizeof(size_t)));
    }

    bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        // if(deep){
        //     for(int i = 0; i < this->size; ++i){
        //         size_t address = (size_t)this->data + i * sizeof(size_t);
        //         Item *item = (Item*)( (void*)address );
        //         // item->decRefC();
        //     }
        // }
        // free(this->data);
        // this->setToNil();
        return true;
    }       

};

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
        enabled = false;
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
    bool test(const std::vector<Item*> &items, std::string &errormsg);

};
struct Function : Item {
    std::string body;
    std::vector<std::string> params;
    bool binary;
    bool variadic;
    std::function< Item* (const std::vector<Item*> &params, Cursor *cursor, Context *ctx) > fn;
    FunctionConstraints constraints;

    Function(){
        registerTraits();
    }

    Function(const std::vector<std::string> &params, const std::string &body, bool variadic = false){
        registerTraits();
        set(params, body, variadic);
    }

    Function(const std::vector<std::string> &params, const std::function<Item*(const std::vector<Item*> &params, Cursor *cursor, Context *ctx)> &fn, bool variadic = false){
        registerTraits();
        set(params, fn, variadic);
    }    


    void registerTraits(){
        this->registerTrait("is-variadic", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            auto func = static_cast<Function*>(subject);
            return static_cast<Item*>(new Number( func->variadic ));
        });

        this->registerTrait("is-binary", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            auto func = static_cast<Function*>(subject);
            return static_cast<Item*>(new Number( func->binary ));
        }); 

        this->registerTrait("n-args", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            auto func = static_cast<Function*>(subject);
            return static_cast<Item*>(new Number( func->params.size() ));
        });   

        this->registerTrait("proto", [](Item* subject, const std::string &value, Cursor *cursor, Context *ctx){
            if(subject->type == ItemTypes::NIL){
                return subject;
            }
            auto func = static_cast<Function*>(subject);
            return static_cast<Item*>(new String( func->binary ? "[]" : func->body ));
        });                       
                           
    }

    void set(const std::vector<std::string> &params, const std::string &body, bool variadic){
        this->body = body;
        this->params = params;
        this->type = ItemTypes::FUNCTION;
        this->fn = [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
            return (Item*)NULL;
        };
        this->binary = false;
        this->variadic = variadic;
    }

    void set(const std::vector<std::string> &params, const std::function<Item*(const std::vector<Item*> &params, Cursor *cursor, Context *ctx)> &fn, bool variadic){
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
    std::unordered_map<std::string, Item*> vars;
    Context *head;

    Item *get(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->get(name) : NULL;
        }
        return it->second;
    }

    Item *copy(bool deep = true) const {

        auto item = new Context();
        item->type = this->type;  
        item->head = this->head;    

        for(auto &it : this->vars){
            item->vars[it.first] = it.second;
        }        

        return item;
    }

    ItemContextPair getWithContext(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->getWithContext(name) : ItemContextPair();
        }
        return ItemContextPair(it->second, this, it->first);
    }    

    ItemContextPair getWithContext(Item *item){
        for(auto &it : this->vars){
            if(it.second == item){
                return ItemContextPair(it.second, this, it.first);
            }
        }
        return this->head ? this->head->getWithContext(item) : ItemContextPair();
    }        

    Item *set(const std::string &name, Item *item){
        auto v = get(name);
        if(v){
            v->deconstruct();
        }
        this->vars[name] = item;
        return item;
    }

    Context(){
        this->type = ItemTypes::CONTEXT;
        this->head = NULL;
    }

    Context(Context *ctx){
        this->type = ItemTypes::CONTEXT;        
        this->head = ctx;
    }    

    ~Context(){
        for(auto &it : vars){
            it.second->deconstruct();
        }
    }

    bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        if(deep){
            for(auto &it : this->vars){
                auto &item = it.second;
                item->decRefC();
            }
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
            std::cout << it.first << getSpace(it.first.length()) << " -> " << getSpace(0) << ItemToText(it.second) << std::endl;
        }
    }

};

static std::string ItemToText(Item *item){
    switch(item->type){
        default:
        case ItemTypes::NIL: {
            return "nil";
        };
        case ItemTypes::NUMBER: {
            auto r = Tools::removeTrailingZeros(*static_cast<double*>(item->data));
            return r;
        };
        case ItemTypes::STRING: {
            std::string output;
            static_cast<String*>(item)->read(output, 0);
            return "'"+output+"'";
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
                output += ItemToText(item)+ModifierTypes::str(ModifierTypes::NAMER)+name;
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
            for(int i = 0; i < list->size; ++i){
                auto item = list->get(i);
                output += ItemToText(item);
                if(i < list->size-1){
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
    Context *toCtx;
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

Token parseTokenWModifier(const std::string &input){
    std::string buffer;
    Token token;
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
                prev.modifiers = token.modifiers;
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
    std::vector<Item*> items;
    for(int i = 0; i < list->size; ++i){
        items.push_back(list->get(i));
    }
    return this->test(items, errormsg);
}

bool FunctionConstraints::test(const std::vector<Item*> &items, std::string &errormsg){
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
            bool f = true;
            for(int j = 0; j < this->expectedTypes.size(); ++j){
                auto exType = this->expectedTypes[j];
                if(items[i]->type != exType){
                    f = false;
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


static Item* eval(const std::vector<Token> &tokens, Cursor *cursor, Context *ctx);


static Item *processPreInterpretModifiers(Item *item, std::vector<ModifierPair> modifiers, Cursor *cursor, Context *ctx, ModifierEffect &effects){
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
                if(!item->hasTrait(name)){
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"': Trait '"+name+"' doesn't belong to '"+ItemTypes::str(item->type)+"' type");
                    return item;                      
                }
                return item->runTrait(name, "", cursor, ctx);
            } break;            
            case ModifierTypes::LINKER: {

            } break;
            case ModifierTypes::NAMER: {
                ctx->set(mod.subject, item);
            } break;        
            case ModifierTypes::SCOPE: {
                if(item->type != ItemTypes::CONTEXT){
                    cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"': Scope modifier can only be used on contexts.");
                    return item;
                }
                if(mod.subject.size() > 0){
                    auto subsequent = static_cast<Context*>(item)->get(mod.subject);
                    if(!subsequent){
                        cursor->setError("'"+ModifierTypes::str(ModifierTypes::SCOPE)+"':"+mod.subject+"': Undefined symbol or imperative.");
                        return item;
                    }
                    item = subsequent;
                }else{
                    effects.toCtx = static_cast<Context*>(item);
                    effects.ctxSwitch = true;
                }
            } break;   
            case ModifierTypes::EXPAND: {
                if(i == modifiers.size()-1){
                    effects.expand = true;
                }else{
                    cursor->setError("'^': expand modifier can only be used at the end of a modifier chain.");
                    return item;
                }
            } break;                                   
        }
    }
    return item;
}

// Interpret from STRING
static Item *interpret(const std::string &_input, Cursor *cursor, Context *ctx){
    std::string input = _input;
    auto literals = parseTokens(input, ' ', cursor);
    // printList(literals);
    if(cursor->error){
        return new Item();
    }    
    auto tokens = buildTokens(literals, cursor);

    return eval(tokens, cursor, ctx);
}

// Interpret from single TOKEN without dropping modifiers. Should be only used by `eval`
static Item *interpret(const Token &token, Cursor *cursor, Context *ctx){
    auto literals = parseTokens(token.first, ' ', cursor);
    if(cursor->error){
        return new Item();
    }
    auto tokens = buildTokens(literals, cursor);
    return eval(tokens, cursor, ctx);
}

static Item *runFunction(Function *fn, const std::vector<Token> &tokens, Cursor *cursor, Context *ctx){
    std::vector<Item*> items;
    for(int i = 0; i < tokens.size(); ++i){
        ModifierEffect effects;
        auto solved = processPreInterpretModifiers(interpret(tokens[i], cursor, ctx), tokens[i].modifiers, cursor, ctx, effects);
        if(effects.expand && solved->type == ItemTypes::LIST){
            auto list = static_cast<List*>(solved);
            for(int i = 0; i < list->size; ++i){
                items.push_back(list->get(i)->copy());
            }
            // We don't deconstruct the list since we're just moving the items to the return here
            // list->clear(true); 
            // delete list;
        }else
        if(effects.expand && solved->type != ItemTypes::LIST){
            cursor->setError("'"+ModifierTypes::str(ModifierTypes::EXPAND)+"': expand modifier can only be used on iterables (Lists, Protos, etc).");
            items.push_back(solved);
        }else{
            items.push_back(solved);
        }                        
        if(cursor->error){
            return new Item();
        }
    }

    if(fn->binary){
        return fn->fn(items, cursor, ctx);  
    }else{
        Context *tctx = new Context(ctx);
        if(fn->variadic){
            auto list = new List(items, false);
            tctx->set("...", list);
        }else{
            if(items.size() != fn->params.size()){
                cursor->setError("'"+ItemToText(fn)+"': Expects exactly "+(std::to_string(fn->params.size()))+" arguments");
                return new Item();
            }
            for(int i = 0; i < items.size(); ++i){
                tctx->set(fn->params[i], items[i]);
            }
        }

        std::cout << fn->body << std::endl;

        return interpret(fn->body, cursor, tctx);
    }

}

static Item* eval(const std::vector<Token> &tokens, Cursor *cursor, Context *ctx){

    auto first = tokens[0];
    auto imp = first.first;

    if(imp == "set"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'set': Expects exactly 2 arguments: <NAME> <VALUE>.");
            return new Item();
        }
        ModifierEffect effects;
        auto solved = processPreInterpretModifiers(interpret(params[1], cursor, ctx), params[1].modifiers, cursor, ctx, effects);
        if(cursor->error){
            return new Item();
        }
        auto f = ctx->getWithContext(params[0].first);
        if(f.item){
            return f.context->set(params[0].first, solved);
        }else{

            return ctx->set(params[0].first, solved);
        }
    }else
    if(imp == "fn"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'fn': Expects exactly 2 arguments: <[ARGS[0]...ARGS[n]]> <[CODE[0]...CODE[n]]>.");
            return new Item();
        }
        bool isVariadic = false;
        auto argNames = parseTokens(params[0].first, ' ', cursor);
        if(cursor->error){
            return new Item();
        }
        if(argNames.size() == 1 && argNames[0] == "..."){
            isVariadic = true;
        }
        auto code = params[1];
        return static_cast<Item*>(new Function(isVariadic ? std::vector<std::string>({}) : argNames, code.first, isVariadic));
    }else
    if(imp == "ct"){
        auto params = compileTokens(tokens, 1, tokens.size()-1);
        Context *pctx = new Context(ctx);
        for(int i = 0; i < params.size(); ++i){
            ModifierEffect effects;
            auto solved = processPreInterpretModifiers(interpret(params[i], cursor, pctx), params[i].modifiers, cursor, pctx, effects);
            
            if(effects.expand && solved->type == ItemTypes::CONTEXT){
                auto proto = static_cast<Context*>(solved);
                for(auto &it : proto->vars){
                    pctx->set(it.first, it.second);
                }
                solved->clear(false); 
                delete solved;
            }            
        }
        return pctx;
    }else{
        auto var = ctx->get(imp);
        // Is it a function?
        if(var && var->type == ItemTypes::FUNCTION){
            auto fn = static_cast<Function*>(var);
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
                auto n = new Number(std::stod(imp));
                ModifierEffect effects;
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return new Item();
                }                      
                return result;
            }else
            if(!var && Tools::isString(imp)){
                ModifierEffect effects;
                auto n = new String(imp.substr(1, imp.length() - 2));
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
                return result;
            }else
            if(!var && Tools::isList(imp)){
                ModifierEffect effects;
                auto result = processPreInterpretModifiers(interpret(first, cursor, ctx), first.modifiers,cursor, ctx, effects);
                if(cursor->error){
                    return new Item();
                }                   
                return result;
            }else
            if(imp == "nil"){
                ModifierEffect effects;
                auto n = new Item();
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return new Item();
                }                      
                return result;
            }else{
                cursor->setError("'"+imp+"': Undefined symbol or imperative."); // TODO: Check if it's running as relaxed
                return new Item();
            }
        }else
        // List or in-line context switch?
        if(tokens.size() > 1){
            auto fmods = tokens[0].getModifier(ModifierTypes::SCOPE);
            // Super rad inline-contex switching d:)
            if(fmods.type == ModifierTypes::SCOPE){
                ModifierEffect effects;
                auto solved = processPreInterpretModifiers(interpret(tokens[0], cursor, ctx), tokens[0].modifiers, cursor, ctx, effects);
                if(cursor->error){
                    return new Item();
                } 
                auto inlineCode = compileTokens(tokens, 1, tokens.size()-1);
                return eval(inlineCode, cursor, effects.toCtx); // Check if toCtx is null? Shouldn't be possible tho
            }else{
            // List it is
                std::vector<Item*> items;
                for(int i = 0; i < tokens.size(); ++i){
                    ModifierEffect effects;
                    auto solved = processPreInterpretModifiers(interpret(tokens[i], cursor, ctx), tokens[i].modifiers, cursor, ctx, effects);
                    if(cursor->error){
                        return new Item();
                    }                       
                    if(effects.expand && solved->type == ItemTypes::LIST){
                        auto list = static_cast<List*>(solved);
                        for(int i = 0; i < list->size; ++i){
                            items.push_back(list->get(i)->copy());
                        }
                        // We don't deconstruct the list since we're just moving the items to the return here
                        // list->clear(true); 
                        // delete list;
                    }else
                    if(effects.expand && solved->type != ItemTypes::LIST){
                        cursor->setError("'^': expand modifier can only be used on iterables (Lists, etc).");
                        items.push_back(solved);
                    }else{   
                        items.push_back(solved);
                    }             
                }          
                return new List(items);
            }
        }
    }

    return new Item();
}

bool typicalVariadicArithmeticCheck(const std::string &name, const std::vector<Item*> &params, Cursor *cursor, int max = -1){
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

static void addStandardOperators(Context *ctx){

    /*

        ARITHMETIC OPERATORS

    */
    ctx->set("+", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("+", params, cursor)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r += *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }
        return static_cast<Item*>(new Number(r));
    }, true));
    ctx->set("-", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("-", params, cursor)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r -= *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }
        return static_cast<Item*>(new Number(r));
    }, true));
    ctx->set("*", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("*", params, cursor)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r *= *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }
        return static_cast<Item*>(new Number(r));
    }, true));  
    ctx->set("/", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("/", params, cursor)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r /= *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }
        return static_cast<Item*>(new Number(r));
    }, true));
    ctx->set("pow", new Function({"a", "b"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("pow", params, cursor, 2)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r =  std::pow(r, *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data));
        }
        return static_cast<Item*>(new Number(r));
    }, false)); 
    ctx->set("sqrt", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::NUMBER);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'sqrt': "+errormsg);
            return new Item();
        }
        if(params.size() == 1){
            return static_cast<Item*>(new Number(std::sqrt(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data))));
        }else{
            std::vector<Item*> result;
            for(int i = 0; i < params.size(); ++i){
                result.push_back(
                    static_cast<Item*>(new Number(std::sqrt(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data))))
                );
            }
            return static_cast<Item*>(new List(result, false));
        }
    }, true));    
       
    /* 
        QUICK MATH
    */ 
    ctx->set("++", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'++': "+errormsg);
            return new Item();
        }

        if(params.size() == 1){
            ++(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data));
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                ++(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data));
            }
        }

        return static_cast<Item*>(new List(params, false));
    }, true));

    ctx->set("--", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'--': "+errormsg);
            return new Item();
        }

        if(params.size() == 1){
            --(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data));
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                --(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data));
            }
        }

        return static_cast<Item*>(new List(params, false));
    }, true));    


    ctx->set("//", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'//': "+errormsg);
            return new Item();
        }

        if(params.size() == 1){
            *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data) /= 2;
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data) /= 2;
            }
        }

        return static_cast<Item*>(new List(params, false));
    }, true));    

    ctx->set("**", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.allowMisMatchingTypes = false;        
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::NUMBER);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'**': "+errormsg);
            return new Item();
        }

        if(params.size() == 1){
            *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data) *= *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
            return params[0];
        }else{   
            for(int i = 0; i < params.size(); ++i){
                *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data) *= *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
            }
        }

        return static_cast<Item*>(new List(params, false));
    }, true));


    /*
        LISTS
    */
    ctx->set("..", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("..", params, cursor)){
            return new Item();
        }

        __CV_NUMBER_NATIVE_TYPE s = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        __CV_NUMBER_NATIVE_TYPE e = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[1]->data);
        
        std::vector<Item*> result;
        if(s == e){
            return static_cast<Item*>(new List(result));
        }

        bool inc = e > s;
        __CV_NUMBER_NATIVE_TYPE step = Tools::positive(params.size() == 3 ? *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[2]->data) : 1);

        for(__CV_NUMBER_NATIVE_TYPE i = s; (inc ? i <= e : i >= e); i += (inc ? step : -step)){
            result.push_back(new Number(i));
        }

        return static_cast<Item*>(new List(result));
    }, true));
    
    ctx->set("l-sort", new Function({"c", "l"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.setMaxParams(2);
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = false;
        consts.setExpectedTypeAt(0, ItemTypes::STRING);
        consts.setExpectedTypeAt(1, ItemTypes::LIST);
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-sort': "+errormsg);
            return new Item();
        }

        if(params[0]->type != ItemTypes::STRING){
            cursor->setError("'l-sort': criteria can only be 'DESC' or 'ASC'. Providing a function is not yet supported");
            return new Item();
        }
        auto criteria = static_cast<String*>(params[0]);

        auto order = criteria->get();
        if(order != "DESC" && order != "ASC"){
            cursor->setError("'l-sort': criteria can only be 'DESC' or 'ASC'.");
            return new Item();
        }        

        auto list = static_cast<List*>(params[1]);
        std::vector<Item*> items;
        for(int i = 0; i < list->size; ++i){
            items.push_back(list->get(i));
        }        

        FunctionConstraints constsList;
        constsList.setExpectType(ItemTypes::NUMBER);
        constsList.allowMisMatchingTypes = false;        
        constsList.allowNil = false;
        if(!constsList.test(items, errormsg)){
            cursor->setError("'l-sort': "+errormsg);
            return new Item();
        }        

        std::sort(items.begin(), items.end(), [&](Item *a, Item *b){
            return order == "DESC" ?     *static_cast<__CV_NUMBER_NATIVE_TYPE*>(a->data) > *static_cast<__CV_NUMBER_NATIVE_TYPE*>(b->data) :
                                        *static_cast<__CV_NUMBER_NATIVE_TYPE*>(a->data) < *static_cast<__CV_NUMBER_NATIVE_TYPE*>(b->data);
        });

        return static_cast<Item*>(new List(items));
    }, false));    



    ctx->set("l-flatten", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.allowMisMatchingTypes = true;        
        consts.allowNil = true;
        
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-flatten': "+errormsg);
            return new Item();
        }


        std::vector<Item*> finalList;

        std::function<void(Item *item)> recursive = [&](Item *item){
            if(item->type == ItemTypes::LIST){
                auto list = static_cast<List*>(item);
                for(int i = 0; i < list->size; ++i){
                    recursive(list->get(i));
                }
            }else
            if(item->type == ItemTypes::CONTEXT){
                auto ctx = static_cast<Context*>(item);
                for(auto &it : ctx->vars){
                    recursive(it.second);
                }
            }else{                
                finalList.push_back(item);
            }
        };


        for(int i = 0; i < params.size(); ++i){
            recursive(params[i]);
        }



        return static_cast<Item*>(new List(finalList));
    }, true));   



    ctx->set("splice", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::CONTEXT);
        consts.setExpectType(ItemTypes::LIST);

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'splice': "+errormsg);
            return new Item();
        }

        auto getAllItems = [](Item *item){
            std::unordered_map<std::string, Item*> all;
            if(item->type == ItemTypes::LIST){
                auto list = static_cast<List*>(item);
                for(int i = 0; i < list->size; ++i){
                    all[std::to_string(i)] = list->get(i);
                }
            }else{
                auto ctx = static_cast<Context*>(item);
                for(auto &it : ctx->vars){
                    all[it.first] = it.second;
                }
            }
            return all;
        };

        auto first = params[0];
        // First item decides the spliced result
        if(first->type == ItemTypes::LIST){
            std::vector<Item*> compiled;
            for(int i = 0; i < params.size(); ++i){
                auto all = getAllItems(params[i]);
                for(auto &it : all){
                    compiled.push_back(it.second);
                }
            }
            return static_cast<Item*>(new List(compiled));
        }else{
            auto ctx = new Context();
            for(int i = 0; i < params.size(); ++i){
                auto all = getAllItems(params[i]);
                for(auto &it : all){
                    ctx->set(it.first, it.second);
                }
            }
            return static_cast<Item*>(ctx);
        }

        return new Item();
    }, true));    

    ctx->set("l-push", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-push': "+errormsg);
            return new Item();
        }

        std::vector<Item*> items;
        auto last = params[params.size()-1];

        if(last->type == ItemTypes::LIST){
            auto list = static_cast<List*>(last);
            for(int i = 0; i < list->size; ++i){
                items.push_back(list->get(i));
            }
        }else{
            items.push_back(last);
        }

        for(int i = 0; i < params.size()-1; ++i){
            items.push_back(params[i]);
        }

        return static_cast<Item*>(new List(items, false));

    }, true)); 
    
    ctx->set("l-pop", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = true;
        consts.allowNil = true;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'<<': "+errormsg);
            return new Item();
        }

        std::vector<Item*> items;
        auto first = params[0];

        if(first->type == ItemTypes::LIST){
            auto list = static_cast<List*>(first);
            for(int i = 0; i < list->size; ++i){
                items.push_back(list->get(i));
            }
        }else{
            items.push_back(first);
        }

        for(int i = 1; i < params.size(); ++i){
            items.push_back(params[i]);
        }

        return static_cast<Item*>(new List(items, false));

    }, true));     

    ctx->set("l-sub", new Function({"l", "p", "n"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
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
            return new Item();
        }

        int index = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[1]->data);
        auto list = static_cast<List*>(params[0]);
        int n = params.size() > 2 ? *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[2]->data) : 1;

        if(index+n < 0 || index+n > list->size){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->size)+")");
            return new Item();
        }      

        std::vector<Item*> result;
        for(int i = 0; i < list->size; ++i){
            if(i >= index && i < index+n){
                result.push_back(list->get(i));
            }
        }

        return static_cast<Item*>(new List(result, false));

    }, false));


    ctx->set("l-cut", new Function({"l", "p", "n"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
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
            return new Item();
        }

        int index = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[1]->data);
        auto list = static_cast<List*>(params[0]);
        int n = params.size() > 2 ? *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[2]->data) : 1;

        if(index+n < 0 || index+n > list->size){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(list->size)+")");
            return new Item();
        }      

        std::vector<Item*> result;
        for(int i = 0; i < list->size; ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result.push_back(list->get(i));
        }

        return static_cast<Item*>(new List(result, false));

    }, false));      

    ctx->set("l-reverse", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return new Item();
        }

        auto reverse = [&](List *lst){
            std::vector<Item*> result;
            for(int i = 0; i < lst->size; ++i){
                result.push_back(lst->get(lst->size-1 - i));
            }
            return new List(result);
        };

        if(params.size() == 1){
            return static_cast<Item*>(reverse(static_cast<List*>(params[0])));
        }else{
            std::vector<Item*> items;
            for(int i = 0; i < params.size(); ++i){
                items.push_back(
                    static_cast<Item*>(reverse(static_cast<List*>(params[i])))
                );
            }
            return static_cast<Item*>(new List(items, false));
        }



    }, true));        


    /*
        STRING
    */
    ctx->set("s-reverse", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        consts.setExpectType(ItemTypes::LIST);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;

        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'l-reverse': "+errormsg);
            return new Item();
        }

        auto reverse = [&](String *str){
            std::string result;
            for(int i = 0; i < str->size; ++i){
                result += *(char*)((size_t)str->data + str->size-1 - i);
            }
            return result;
        };

        if(params.size() == 1){
            return static_cast<Item*>(new String(reverse(static_cast<String*>(params[0]))));
        }else{
            std::vector<Item*> items;
            for(int i = 0; i < params.size(); ++i){
                items.push_back(
                    static_cast<Item*>(new String(reverse(static_cast<String*>(params[i]))))
                );
            }
            return static_cast<Item*>(new List(items, false));
        }



    }, true));  

    ctx->set("s-join", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        consts.setExpectType(ItemTypes::STRING);
        static const std::string name = "s-join";
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'"+name+"': "+errormsg);
            return new Item();
        }
        std::string result;
        for(int i = 0; i < params.size(); ++i){
            auto str = static_cast<String*>(params[i]);
            result += str->get();
        }
        return static_cast<Item*>(new String(result));
    }, true));   

    ctx->set("s-cut", new Function({"s", "p", "n"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
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
            return new Item();
        }



        int index = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[1]->data);
        auto str = static_cast<String*>(params[0]);
        int n = params.size() > 2 ? *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[2]->data) : 1;

        if(index+n < 0 || index+n > str->size){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->size)+")");
            return new Item();
        }      

        std::string result;
        for(int i = 0; i < str->size; ++i){
            if(i >= index && i < index+n){
                continue;
            }
            result += *(char*)((size_t)str->data + i);
        }

        return static_cast<Item*>(new String(result));

    }, false));       


    ctx->set("s-sub", new Function({"s", "p", "n"}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
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
            return new Item();
        }


        int index = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[1]->data);
        auto str = static_cast<String*>(params[0]);
        int n = params.size() > 2 ? *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[2]->data) : 1;

        if(index+n < 0 || index+n > str->size){
            cursor->setError("'"+name+"': position("+std::to_string(index+n)+") is out of range("+std::to_string(str->size)+")");
            return new Item();
        }      

        std::string result;
        for(int i = 0; i < str->size; ++i){
            if(i >= index && i < index+n){
                result += *(char*)((size_t)str->data + i);
            }
        }

        return static_cast<Item*>(new String(result));

    }, false));     


    /*
        LOGIC
    */
    ctx->set("or", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'or': "+errormsg);
            return static_cast<Item*>(new Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == ItemTypes::NUMBER && *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data) != 0) ||
                (params[i]->type != ItemTypes::NIL && params[i]->type != ItemTypes::NUMBER)){
                return static_cast<Item*>(new Number(1));
            }
        }

        return static_cast<Item*>(new Number(0));
    }, true));        
    ctx->set("and", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'and': "+errormsg);
            return static_cast<Item*>(new Item());
        }

        for(int i = 0; i < params.size(); ++i){
            if( (params[i]->type == ItemTypes::NUMBER && *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data) == 0) || (params[i]->type == ItemTypes::NIL) ){
                return static_cast<Item*>(new Number(0));
            }
        }

        return static_cast<Item*>(new Number(1));
    }, true));    

    ctx->set("not", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(1);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'not': "+errormsg);
            return new Item();
        }
        auto val = [](Item *item){
            if(item->type == ItemTypes::NUMBER){
                return *static_cast<__CV_NUMBER_NATIVE_TYPE*>(item->data) == 0 ? new Number(1) : new Number(0);
            }else
            if(item->type == ItemTypes::NIL){
                return new Number(1);
            }else{
                return new Number(0);   
            }
        };
        if(params.size() == 1){
            return static_cast<Item*>(val(params[0]));
        }else{
            std::vector<Item*> results;
            for(int i = 0; i < params.size(); ++i){
                results.push_back(val(params[i]));
            }
            return static_cast<Item*>(new List(results));
        }
        return new Item();
    }, true));                  


    ctx->set("eq", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'eq': "+errormsg);
            return new Item();
        }
        auto last = params[0];
        for(int i = 1; i < params.size(); ++i){
            if(!last->isEq(params[i])){
                return static_cast<Item*>(new Number(0));
            }
        }				
        return static_cast<Item*>(new Number(1));
    }, true));     


    ctx->set("neq", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowMisMatchingTypes = false;
        consts.allowNil = false;
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'neq': "+errormsg);
            return new Item();
        }
        for(int i = 0; i < params.size(); ++i){
            auto p1 = params[i];
            for(int j = 0; j < params.size(); ++j){
                if(i == j){
                    continue;
                }
                auto p2 = params[j];
                if(p1->isEq(p2)){
                    return static_cast<Item*>(new Number(0));
                }
            }
        }				
        return static_cast<Item*>(new Number(1));
    }, true));      


    ctx->set("gt", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gt': "+errormsg);
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            if(last <=  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data)){
                return static_cast<Item*>(new Number(0));
            }
            last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }				
        return static_cast<Item*>(new Number(1));
    }, true));    


    ctx->set("gte", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            if(last <  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data)){
                return static_cast<Item*>(new Number(0));
            }
            last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }				
        return static_cast<Item*>(new Number(1));
    }, true));         


    ctx->set("lt", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'lt': "+errormsg);
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            if(last >=  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data)){
                return static_cast<Item*>(new Number(0));
            }
            last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }				
        return static_cast<Item*>(new Number(1));
    }, true));    

    ctx->set("lte", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        FunctionConstraints consts;
        consts.setMinParams(2);
        consts.allowNil = false;
        consts.allowMisMatchingTypes = false;
        consts.setExpectType(ItemTypes::NUMBER);
        std::string errormsg;
        if(!consts.test(params, errormsg)){
            cursor->setError("'gte': "+errormsg);
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            if(last >  *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data)){
                return static_cast<Item*>(new Number(0));
            }
            last = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data);
        }				
        return static_cast<Item*>(new Number(1));
    }, true));    
         
}


int main(){

    Context ctx;
    Cursor cursor;

    addStandardOperators(&ctx);
    // Context nctx;
    // nctx.set("a", new Number(25));
    // ctx.set("test", &nctx);

    // auto r = interpret(":", &cursor, &ctx);


    auto r = interpret("set test [fn [...][...^]]", &cursor, &ctx);
    r = interpret("test 5 2 3", &cursor, &ctx);
    // auto r = interpret("[1 [28 28]^ 3]^ [4 5 6]^ [7 8 [99 99]^]^]", &cursor, &ctx);

    std::cout << ItemToText(r) << std::endl;
    // return 0;
    if(cursor.error){
        std::cout << cursor.message << std::endl;
    }
    ctx.debug();

    // ctx.deconstruct();



    return 0;
}