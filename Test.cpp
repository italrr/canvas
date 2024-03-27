#include <algorithm>
#include <sstream>
#include <iomanip>
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
}

namespace ModifierTypes {
    enum ModifierTypes : uint8_t {
        UNDEFINED,
        NAMER,
        SCOPE,
        LINKER
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
        PROTO,
        FUNCTION,
        INTERRUPT,
        CONTEXT
    };
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
    virtual bool clear(){
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
            list[i]->incRefC();
            memcpy(static_cast<Item*>(this->data) + i * sizeof(Item), list[i], sizeof(Item));
        }
    }

    Item *get(int32_t index){
        return static_cast<Item*>( static_cast<Item*>(this->data) + index * sizeof(Item));
    }

    bool clear(){
        if(this->type == ItemTypes::NIL){
            return false;
        }
        for(int i = 0; i < this->size; ++i){
            Item *item = static_cast<Item*>( static_cast<Item*>(this->data) + i * sizeof(Item));
            item->decRefC();
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

    FunctionConstraints(){
        enabled = false;
        allowNil = true;
        allowMisMatchingTypes = true;
        useMinParams = false;
        useMaxParams = false;
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
    std::function< Item* (const std::vector<std::string> &params, const std::string &body, Cursor *cursor, Context *ctx) > fn;
    FunctionConstraints constraints;

    Function(){

    }

    Function(const std::vector<std::string> &params, const std::string &body){
        set(params, body);
    }

    void set(const std::vector<std::string> &params, const std::string &body){
        this->body = body;
        this->params = params;
        this->type = ItemTypes::FUNCTION;
        this->fn = [](const std::vector<std::string> &params, const std::string &body, Cursor *cursor, Context *ctx){
            return (Item*)NULL;
        };
    }

    void set(const std::vector<std::string> &params, const std::function<Item*(const std::vector<std::string> &params, const std::string &body, Cursor *cursor, Context *ctx)> &fn){
        this->params = params;
        this->type = ItemTypes::FUNCTION;
        this->fn = fn;       
    }    
};
    


static std::string ItemToText(Item *item){
    switch(item->type){
        default:
        case ItemTypes::NIL: {
            return "nil";
        };
        case ItemTypes::NUMBER: {
			std::ostringstream oss;
            oss << std::setprecision(8) << std::noshowpoint << *static_cast<double*>(item->data);
            return oss.str();
        };
        case ItemTypes::STRING: {
            std::string output;
            static_cast<String*>(item)->read(output, 0);
            return "'"+output+"'";
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

struct Token {
    std::string first;
    std::vector<ModifierPair> modifiers;
    std::string literal(){
        std::string part = this->first;
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

std::vector<Token> buildTokens(const std::vector<std::string> &literals){
    std::vector<Token> tokens;

    for(int i = 0; i < literals.size(); ++i){
        auto &literal = literals[i];
        auto token = parseTokenWModifier(literal);
        if(token.first == "" && i > 0){ // This means this modifier belongs to the token before this one
            auto &prev = tokens[tokens.size()-1];
            prev.modifiers = token.modifiers;
            continue;
        }else{
            tokens.push_back(token);
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
            ++open;
        }else
        if(c == ']' && !onString){
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

struct Context {
    std::unordered_map<std::string, Item*> vars;
    Context *head;

    Item *get(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return this->head ? this->head->get(name) : NULL;
        }
        return it->second;
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
        this->head = NULL;
    }

    Context(Context *ctx){
        this->head = ctx;
    }    

    ~Context(){
        for(auto &it : vars){
            it.second->deconstruct();
        }
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
            std::cout << it.first << getSpace(it.first.length()) << " -> " << getSpace(it.first.length()) << ItemToText(it.second) << std::endl;
        }
    }

};

bool FunctionConstraints::test(const std::vector<Item*> &items, std::string &errormsg){
    if(!enabled){
        return true;
    }

    if(useMaxParams && items.size() > maxParams){
        errormsg = "Provided ("+std::to_string(items.size())+"). Expected no more than ("+std::to_string(maxParams)+") arguments.";
        return false;
    }

    if(useMinParams && items.size() < minParams){
        errormsg = "Provided ("+std::to_string(items.size())+"). Expected no less than ("+std::to_string(minParams)+") arguments.";
        return false;
    } 



    return true;
}

static Item* infer(const Token &token, Cursor *cursor, Context *ctx){
    if(Tools::isNumber(token.first)){
        return new Number(std::stod(token.first));
    }else
    if(Tools::isString(token.first)){
        return new String(token.first.substr(1, token.first.length() - 2));
    }else{
        // Otherwise it must be a list
    }
    return new Item();
}

static Item* eval(const std::vector<Token> &tokens, Cursor *cursor, Context *ctx);


static Item *interpret(const std::string &input, Cursor *cursor, Context *ctx){
    auto literals = parseTokens(input, ' ', cursor);
    auto tokens = buildTokens(literals);
    return eval(tokens, cursor, ctx);
}

static Item* eval(const std::vector<Token> &tokens, Cursor *cursor, Context *ctx){
    FunctionConstraints consts;

    auto first = tokens[0];
    auto imp = first.first;

    auto params = compileTokens(tokens, 1, tokens.size()-1);

    if(imp == "set"){
        if(params.size() > 2 || params.size() < 2){
            cursor->setError("'set': Expects 2 arguments: <NAME> <VALUE>.");
            return new Item();
        }
        return ctx->set(params[0].first, eval({params[1]}, cursor, ctx));
    }else
    if(imp == "if"){

    }else
    if(imp == "iter"){

    }else{
        auto var = ctx->get(imp);
        // Is it variable?
        if(var){
            
        }else
        // Is it single item?
        if(tokens.size() == 1){
            return infer(first, cursor, ctx);
        }else
        // Must be a list
        if(tokens.size() > 1){
            std::vector<Item*> items;
            for(int i = 0; i < tokens.size(); ++i){
                items.push_back(eval({tokens[i]}, cursor, ctx));
            }
            return new List(items);
        }
    }

    return new Item();
}

int main(){

    Context ctx;
    Cursor cursor;


    std::cout << ItemToText(interpret("set a [1 2 3]", &cursor, &ctx)) << std::endl;



    ctx.debug();



    return 0;
}