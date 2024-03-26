#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <inttypes.h>

#define __CV_NUMBER_NATIVE_TYPE double

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
    static bool isThereToken(std::string &input){
        if(input[0] == '\'' && input[input.length()-1] == '\''){
            return false;
        }
        for(int i = 0; i < input.length(); ++i){
            char c = input[i];
            if(getToken(c) != ModifierTypes::UNDEFINED){
                return true;
            }
        }
        return false;
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
        this->type = ItemTypes::NUMBER;
        this->size = sizeof(__CV_NUMBER_NATIVE_TYPE);
        this->data = malloc(this->size);
        this->refc = 1;
    }

    void write(const __CV_NUMBER_NATIVE_TYPE v){
        memcpy(this->data, &v, sizeof(__CV_NUMBER_NATIVE_TYPE));
    }

    void read(__CV_NUMBER_NATIVE_TYPE *v){
        memcpy(v, this->data, sizeof(__CV_NUMBER_NATIVE_TYPE));
    }
    
    __CV_NUMBER_NATIVE_TYPE get(){
        return *static_cast<__CV_NUMBER_NATIVE_TYPE*>(this->data);
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


static std::string ItemToText(Item *item){
    switch(item->type){
        default:
        case ItemTypes::NIL: {
            return "NIL";
        };
        case ItemTypes::NUMBER: {
            return std::to_string(*static_cast<__CV_NUMBER_NATIVE_TYPE*>(item->data));
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
                    output += ", ";
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
    std::cout << "END" << std::endl;
}

struct ModifierPair {
    uint8_t type;
    std::string subject;
};

struct Token {
    std::string first;
    std::vector<ModifierPair> modifiers;
    std::string literal(){
        std::string part = this->first;
        for(int i = 0; i < modifiers.size(); ++i){
            auto &mod = modifiers[i];
            part += "<"+ModifierTypes::str(mod.type)+" "+mod.subject+">";
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

        }else
        if(t == ModifierTypes::UNDEFINED || i == input.length()-1){
            buffer += c;
        }
    }
    return token;
}

std::vector<Token> buildTokens(const std::vector<std::string> &literals){
    std::vector<Token> tokens;
    for(int i = 0; i < literals.size(); ++i){
        auto &literal = literals[i];

    }
    return tokens;
};

std::vector<std::string> parseTokens(std::string input, char sep){
    std::vector<std::string> tokens; 
    std::string buffer;
    int open = 0;
    bool onString = false;
    bool onComment = false;
    int leftBrackets = 0;
    int rightBrackets = 0;
    // Count outer brackets
    for(int i = 0; i < input.size(); ++i){
        char c = input[i];
        if(c == '\''){
            onString = !onString;
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
    // Throw mismatching brackets error
    if(leftBrackets != rightBrackets){
        return {};
    }
    // Add missing brackets for simple statements
    if((leftBrackets+rightBrackets)/2 > 1 || leftBrackets+rightBrackets == 0 || input[0] != '[' && input[0] != ']'){
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
        if(c == sep && open == 1 && !onString || i == input.size()-1){
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

    Item *get(const std::string &name){
        auto it = vars.find(name);
        if(it == vars.end()){
            return NULL;
        }
        return it->second;
    }

    void add(const std::string &name, Item *item){
        auto v = get(name);
        if(v){
            v->deconstruct();
        }
        this->vars[name] = item;
    }

    ~Context(){
        for(auto &it : vars){
            it.second->deconstruct();
        }
    }

    void debug(){
        std::cout << "\n\n==================CONTEXT DEBUG==================" << std::endl;
        size_t maxVarName = 0;
        auto getSpace = [&](){
            std::string space = "";
            for(int i = 0; i < maxVarName; ++i) space += " ";
            return space;
        };
        for(auto &it : this->vars){
            maxVarName = std::max(maxVarName, it.first.length());
        }

        for(auto &it : this->vars){
            std::cout << it.first << getSpace() << " -> " << ItemToText(it.second) << std::endl;
        }


    }

};

int main(){

    auto literals = parseTokens("[+ 1 1~k]", ' ');
    auto tokens = buildTokens(literals);

    for(int i = 0; i < tokens.size(); ++i){
        std::cout << "'" << tokens[i].literal() << "'" << std::endl;
    }




    return 0;
}