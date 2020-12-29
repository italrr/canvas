#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <functional>
#include <memory>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <string.h>


const std::string KEYWORDLIST[] = {
    "let", "object", "splice", "width", "as", "func",
    "for", "while", "nil"
};
const static int KEYWORDN = sizeof(KEYWORDLIST) / sizeof(std::string);

std::vector<std::string> split(const std::string &str, const std::string &sep){
    std::vector<std::string> tokens;
    char *token = strtok((char*)str.c_str(), sep.c_str()); 
    while(token != NULL){ 
        tokens.push_back(std::string(token));
        token = strtok(NULL, "-"); 
    } 
    return tokens;
}

bool isNumber(const std::string& s){
    return (s.find_first_not_of( "-.0123456789" ) == std::string::npos);
}

enum ObjectType : int {
    NONE,
    NUMBER,
    STRING,
    LIST,
    OBJECT,
    LAMBDA
};

struct Object {
    std::string name;
    int n;
    void *data;
    size_t size;
    int type;
    Object();
    std::string str() const;
    void clear();
    void write(void *data, size_t size, int type);
    ~Object();
    operator std::string() const;
};

struct Context {
    std::unordered_map<std::string, std::shared_ptr<Object>> objecs;
    Context();
    void add(const std::shared_ptr<Object> &object);
    std::shared_ptr<Object> find(const std::string &name);
};

std::shared_ptr<Object> eval(const std::string &input, Context &ctx);
std::shared_ptr<Object> infer(const std::string &input);

template<typename T>
std::shared_ptr<Object> __sum(const std::shared_ptr<Object> &a, const std::shared_ptr<Object> &b){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = *static_cast<T*>(a->data) + *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
std::shared_ptr<Object> __sub(const std::shared_ptr<Object> &a, const std::shared_ptr<Object> &b){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = *static_cast<T*>(a->data) - *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
std::shared_ptr<Object> __mult(const std::shared_ptr<Object> &a, const std::shared_ptr<Object> &b){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = *static_cast<T*>(a->data) * *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
std::shared_ptr<Object> __div(const std::shared_ptr<Object> &a, const std::shared_ptr<Object> &b){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = *static_cast<T*>(a->data) / *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
std::shared_ptr<Object> __pow(const std::shared_ptr<Object> &a, const std::shared_ptr<Object> &b){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = std::pow(*static_cast<T*>(a->data), *static_cast<T*>(b->data));
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
std::shared_ptr<Object> __sqrt(const std::shared_ptr<Object> &a){
    std::shared_ptr<Object> result = std::make_shared<Object>(Object());
    T r = std::sqrt(*static_cast<T*>(a->data));
    result->write(&r, sizeof(r), a->type);
    return result;
}



struct Function : Object {
    std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands)> lambda;
    Function();
    Function(const std::string &name, const std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands)> &lambda);
    void set(const std::string &body);
};

Object::Object(){
    type = ObjectType::NONE;
    data = NULL;
    size = 0;
}

std::string Object::str() const {
    switch(type){
        case ObjectType::NUMBER: {
            std::ostringstream oss;
            oss << std::setprecision(8) << std::noshowpoint << *static_cast<double*>(data);
            return oss.str();
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
    }
}

void Object::clear(){
    if(type != ObjectType::NONE){
        free(data);
    }
    type = ObjectType::NONE;
    data = NULL;
    size = 0;
}

void Object::write(void *data, size_t size, int type){
    if(this->data != NULL){
        clear();
    }
    this->data = new char[size];
    this->size = size;
    this->type = type;
    memcpy(this->data, data, size);
}

Object::~Object(){
    clear();
}

Object::operator std::string() const { 
    return str();
}

void Function::set(const std::string &body){
    this->lambda = [body](const std::vector<std::shared_ptr<Object>> &operands){
        Context ctx;
        for(int i = 0; i < operands.size(); ++i){
            ctx.add(operands[i]);
        }
        return eval(body, ctx);
    };    
}

Function::Function() : Object(){
    this->type = ObjectType::LAMBDA;
    this->lambda = [](const std::vector<std::shared_ptr<Object>> &operands){
        return std::make_shared<Object>(Object());
    };  
} 

Function::Function(const std::string &name, const std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands)> &lambda){
    this->name = name;
    this->lambda = lambda;
    this->type = ObjectType::LAMBDA;
}

Context::Context(){
    // default operators
    this->add(std::make_shared<Function>(Function("+", [](const std::vector<std::shared_ptr<Object>> &operands){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sum<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("-", [](const std::vector<std::shared_ptr<Object>> &operands){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sub<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("*", [](const std::vector<std::shared_ptr<Object>> &operands){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __mult<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("/", [](const std::vector<std::shared_ptr<Object>> &operands){
        auto r = operands[0];
        for(int i = 0; i < operands.size(); ++i){
            r = __div<double>(r, operands[i]);
        }
        return r;
    })));  

    this->add(std::make_shared<Function>(Function("pow", [](const std::vector<std::shared_ptr<Object>> &operands){
        auto r = operands[0];
        for(int i = 0; i < operands.size(); ++i){
            r = __pow<double>(r, operands[i]);
        }
        return r;
    })));               
}

void Context::add(const std::shared_ptr<Object> &object){
    if(object->name.empty()){
        return;
    }
    this->objecs[object->name] = object;
}

std::shared_ptr<Object> Context::find(const std::string &name){
    auto it = objecs.find(name);
    if(it == objecs.end()){
        return std::make_shared<Object>(Object());
    }
    return it->second;
}

std::string removeExtraSpace(const std::string &in){
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

std::vector<std::string> parse(const std::string &input){
    auto clean = removeExtraSpace(input);
    std::vector<std::string> tokens;
    int open = 0;
    bool comm = false;
    std::string hold;
    for(int i = 0; i < input.length(); ++i){
        char c = input[i];
        if(c == '(') ++open;
        if(c == ')') --open;
        if(c == '#'){
            comm = !comm;
            continue;
        }
        if(comm){
            continue;
        }
        hold += c;
        if(open == 0){
            if(c == ')' || c == ' ' || i ==  input.length()-1){
                if(hold != " "){
                    tokens.push_back(removeExtraSpace(hold));
                }
                hold = "";
            }
        }
    }
    return tokens;
}

std::shared_ptr<Object> infer(const std::string &input){
    if(isNumber(input)){
        auto result = std::make_shared<Object>(Object());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ObjectType::NUMBER);
        return result;
    }
    return std::make_shared<Object>(Object());
}

std::shared_ptr<Object> eval(const std::string &input, Context &ctx){
    auto tokens = parse(input);
    auto imp = tokens[0];    
    auto result = std::make_shared<Object>(Object());
    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '(' && tokens[0][tokens[0].length()-1] == ')'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);
        return eval(imp, ctx);
    }

    auto op = ctx.find(imp);
    if(op->type != ObjectType::LAMBDA){
        return infer(imp);
    }

    std::vector<std::shared_ptr<Object>> operands;
    for(int i = 1; i < tokens.size(); ++i){
        operands.push_back(eval(tokens[i], ctx));
    }

    return static_cast<Function*>(op.get())->lambda(operands);
}

int main(int argc, const char *argv[]){
    Context main;

    // std::cout << eval("for (.. 0 10) as i ((print i))# lol comment #(let a (+ 2 2))", main)->str() << std::endl;
    std::cout << eval("(pow 2 2 2 2)", main)->str() << std::endl;

    // auto toks = parse("for (.. 0 10) as i ((print i))");
    // // auto toks = parse("(for (0..10) as i ((print i)))# lol comment #(let a (+ 2 2))");



    return 0;
}