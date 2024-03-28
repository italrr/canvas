#include <algorithm>
#include <functional>
#include <iostream>
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
        EXPAND
    };
    static uint8_t getToken(char mod){
        if(mod == '~'){
            return ModifierTypes::NAMER;
        }else
        if(mod == ':'){
            return ModifierTypes::SCOPE;
        }else
        if(mod == '|'){
            return ModifierTypes::LINKER;
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
            case ModifierTypes::LINKER: {
                return "|";
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

struct Item {
    uint8_t type;
    uint32_t size;
    void *data;
    uint32_t refc;
    Item(){
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
    void deconstruct(){
        if(this->refc == 0 || this->type == ItemTypes::NIL){
            return;
        }
        --this->refc;
        if(this->refc == 0){
            this->clear();
        }
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
        free(this->data);
        this->setToNil();
        return true;
    }
};

struct Number : Item {

    Number(){

    }

    Number(__CV_NUMBER_NATIVE_TYPE v){
        write(v);
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
    }

    String(const std::string &str){
        this->write(str);
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

};


struct List : Item {

    List(){
        // Again, empty contructor makes this list NIL
    }

    List(const std::vector<Item*> &list){
        write(list);
    }

    void write(const std::vector<Item*> &list){
        this->type = ItemTypes::LIST;
        this->size = list.size();
        this->data = malloc(sizeof(Item) * list.size());
        this->refc = 1;
        for(int i = 0; i < list.size(); ++i){
            memcpy(static_cast<char*>(this->data) + i * sizeof(Item), list[i], sizeof(Item));
        }
    }

    Item *get(int32_t index){
        return (Item*)( (char*)(this->data) + index * sizeof(Item));
    }

    bool clear(bool deep = true){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        if(deep){
            for(int i = 0; i < this->size; ++i){
                Item *item = (Item*)( (char*)(this->data) + i * sizeof(Item));
                item->decRefC();
            }
        }
        free(this->data);
        this->setToNil();
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

    bool useExpectType;
    uint8_t expectedType;

    FunctionConstraints(){
        enabled = false;
        allowNil = true;
        allowMisMatchingTypes = true;
        useMinParams = false;
        useMaxParams = false;
        useExpectType = false;
    }

    void setExpectType(uint8_t type){
        if(type == ItemTypes::NIL){
            this->useExpectType = false;
            return;
        }
        this->useExpectType = true;
        this->expectedType = type;
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
    }

    Function(const std::vector<std::string> &params, const std::string &body, bool variadic = false){
        set(params, body, variadic);
    }

    Function(const std::vector<std::string> &params, const std::function<Item*(const std::vector<Item*> &params, Cursor *cursor, Context *ctx)> &fn, bool variadic = false){
        set(params, fn, variadic);
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
    ItemContextPair(){
        item = NULL;   
        context = NULL;
    }
    ItemContextPair(Item *item, Context *ctx){
        this->item = item;
        this->context = ctx;
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

    ItemContextPair getWithContext(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->getWithContext(name) : ItemContextPair();
        }
        return ItemContextPair(it->second, this);
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
            return Tools::removeTrailingZeros(*static_cast<double*>(item->data));
        };
        case ItemTypes::STRING: {
            std::string output;
            static_cast<String*>(item)->read(output, 0);
            return "'"+output+"'";
        };        
        case ItemTypes::FUNCTION: {
            auto fn = static_cast<Function*>(item);        
            return "fn ["+(fn->variadic ? "..." : Tools::compileList(fn->params))+"] ["+(fn->binary ? "BINARY" : fn->body )+"]";
        };
        case ItemTypes::CONTEXT: {
            std::string output = "[ctx ";
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
        if(c == '[' && open == 0 && !onString){
            ++leftBrackets;
            ++open;
        }else
        if(c == ']' && open == 1 && !onString){
            ++rightBrackets;
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
                tokens.push_back(buffer);   
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            tokens.push_back(buffer+c);
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
            tokens.push_back(buffer);
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
    std::string literal() const{
        std::string part = "["+this->first+"]";
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
    // std::exit(1);

    return tokens;
};

std::vector<Token> compileTokens(const std::vector<Token> &tokens, int from, int amount){
	std::vector<Token> compiled;
	for(int i = 0; i < amount; ++i){
        compiled.push_back(tokens[from + i]);
	}
	return compiled;
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

    if(!allowMisMatchingTypes){
        uint8_t firstType = items[0]->type;
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type != firstType){
                errormsg = "Provided argument with mismatching types";
                return false;
            }
        }
    }

    if(!allowNil){
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type == ItemTypes::NIL){
                errormsg = "Provided nil value";
                return false;
            }
        }        
    }

    if(useExpectType){
        for(int i = 1; i < items.size(); ++i){
            if(items[i]->type != expectedType){
                errormsg = "Provided '"+ItemTypes::str(items[i]->type)+"'. Expected types are only '"+ItemTypes::str(expectedType)+"'.";
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
            case ModifierTypes::LINKER: {

            } break;
            case ModifierTypes::NAMER: {
                ctx->set(mod.subject, item);
            } break;        
            case ModifierTypes::SCOPE: {
                if(item->type != ItemTypes::CONTEXT){
                    cursor->setError("':': Scope modifier can only be used on contexts.");
                    return item;
                }
                if(mod.subject.size() > 0){
                    auto subsequent = static_cast<Context*>(item)->get(mod.subject);
                    if(!subsequent){
                        cursor->setError("':"+mod.subject+"': Undefined symbol or imperative.");
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
static Item *interpret(const std::string &input, Cursor *cursor, Context *ctx){
    auto literals = parseTokens(input, ' ', cursor);
    if(cursor->error){
        return new Item();
    }    
    auto tokens = buildTokens(literals, cursor);
    ModifierEffect effects;
    return processPreInterpretModifiers(eval(tokens, cursor, ctx), tokens[0].modifiers, cursor, ctx, effects);
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
        // expand doesn't do anything here
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
    if(imp == "ctx"){
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
            if(fn->binary){
                std::vector<Item*> items;
                if(tokens.size() > 0){
                    for(int i = 1; i < tokens.size(); ++i){
                        ModifierEffect effects;
                        auto solved = processPreInterpretModifiers(interpret(tokens[i], cursor, ctx), tokens[i].modifiers, cursor, ctx, effects);
                        if(effects.expand && solved->type == ItemTypes::LIST){
                            auto list = static_cast<List*>(solved);
                            for(int i = 0; i < list->size; ++i){
                                items.push_back(list->get(i));
                            }
                            // We don't deconstruct the list since we're just moving the items to the return here
                            list->clear(false); 
                            delete list;
                        }else
                        if(effects.expand && solved->type != ItemTypes::LIST){
                            cursor->setError("'^': expand modifier can only be used on iterables (Lists, Protos, etc).");
                            items.push_back(solved);
                        }else{
                            items.push_back(solved);
                        }                        
                        if(cursor->error){
                            return new Item();
                        }
                    }
                }
                return fn->fn(items, cursor, ctx);
            }
        }else
        // Is it just a variable?
        if(var && tokens.size() == 1){
            return var;
        }else
        // Is it a natural type?
        if(tokens.size() == 1){
            if(!var && Tools::isNumber(imp)){
                auto n = new Number(std::stod(imp));
                ModifierEffect effects;
                auto result = processPreInterpretModifiers(n, first.modifiers, cursor, ctx, effects);
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
                return result;
            }else{
                cursor->setError("'"+imp+"': Undefined symbol or imperative."); // TODO: Check if it's running as relaxed
                return new Item();
            }
        }else
        // Otherwise, we treat it as a list
        if(tokens.size() > 1){
            std::vector<Item*> items;
            for(int i = 0; i < tokens.size(); ++i){
                ModifierEffect effects;
                auto solved = processPreInterpretModifiers(interpret(tokens[i], cursor, ctx), tokens[i].modifiers, cursor, ctx, effects);
                if(effects.expand && solved->type == ItemTypes::LIST){
                    auto list = static_cast<List*>(solved);
                    for(int i = 0; i < list->size; ++i){
                        items.push_back(list->get(i));
                    }
                    // We don't deconstruct the list since we're just moving the items to the return here
                    list->clear(false); 
                    delete list;
                }else
                if(effects.expand && solved->type != ItemTypes::LIST){
                    cursor->setError("'^': expand modifier can only be used on iterables (Lists, Protos, etc).");
                    items.push_back(solved);
                }else{
                    items.push_back(solved);
                }
                if(cursor->error){
                    return new Item();
                }                
            }
            return new List(items);
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
    ctx->set("pow", new Function({}, [](const std::vector<Item*> &params, Cursor *cursor, Context *ctx){
        if(!typicalVariadicArithmeticCheck("^", params, cursor)){
            return new Item();
        }
        __CV_NUMBER_NATIVE_TYPE r = *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[0]->data);
        for(int i = 1; i < params.size(); ++i){
            r =  std::pow(r, *static_cast<__CV_NUMBER_NATIVE_TYPE*>(params[i]->data));
        }
        return static_cast<Item*>(new Number(r));
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





    
}

int main(){

    Context ctx;
    Cursor cursor;

    addStandardOperators(&ctx);

    auto r = interpret("[set a [ctx 10~n]][+ 1 1]", &cursor, &ctx);
    std::cout << ItemToText(r) << std::endl;
    if(cursor.error){
        std::cout << cursor.message << std::endl;
    }
    ctx.debug();

    ctx.deconstruct();


    return 0;
}