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
#include <cstdlib>
#include <string.h>


const std::string KEYWORDLIST[] = {
    "def", "object", "splice", "func",
    "for", "while", "nil", "print"
};
const static int KEYWORDN = sizeof(KEYWORDLIST) / sizeof(std::string);

struct Cursor {
    int line;
    int column;
    bool error;
    std::string message;
    Cursor(){
        line = 0;
        column = 0;
        error = false;
        message = "";
    }
    void setError(const std::string &message, int line = 0, int column = 0){
        this->message = message;
        this->line = line;
        this->column = column;
        this->error = true;
    }
};

bool isKeyword(const std::string &input){
    for(int i = 0; i < KEYWORDN; ++i){
        if(input == KEYWORDLIST[i]){
            return true;
        }
    }
    return false;
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

std::string removeEndOfLine(const std::string &in){
    std::string cpy;
    for(int i = 0; i < in.length(); ++i){
        if(in[i] == '\n' || in[i] == '\r'){
            continue;
        }
        cpy += in[i];
    }
    return cpy;
}

std::vector<std::string> split(const std::string &str, const std::string &sep){
    std::vector<std::string> tokens;
    char *token = strtok((char*)str.c_str(), sep.c_str()); 
    while(token != NULL){ 
        tokens.push_back(std::string(token));
        token = strtok(NULL, "-"); 
    } 
    return tokens;
}

bool isNumber(const std::string &s){
    return (s.find_first_not_of( "-.0123456789" ) == std::string::npos);
}

bool isString (const std::string &s){
    return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
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
    std::unordered_map<std::string, std::string> traits;
    bool hasTrait(const std::string &trait);
    int n;
    void *data;
    size_t size;
    int type;
    Object();
    virtual std::string str() const;
    void clear();
    void write(void *data, size_t size, int type);
    ~Object();
    operator std::string() const;
};

struct Context {
    Context *head;
    std::unordered_map<std::string, std::shared_ptr<Object>> objecs;
    Context(Context *head = NULL);
    void add(const std::shared_ptr<Object> &object);
    std::shared_ptr<Object> find(const std::string &name);
};

struct Function : Object {
    std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor)> lambda;
    std::vector<std::string> params;
    std::string body;
    Function();
    Function(   const std::string &name,
                const std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor)> &lambda,
                const std::vector<std::string> &params = {});
    void set(const std::string &body, const std::vector<std::string> &params = {});
    std::string str() const;
};

struct List : Object {
    int n;
    std::vector<std::shared_ptr<Object>> list;
    List();
    void build(const std::vector<std::shared_ptr<Object>> &objects);
    std::string str() const;
};

struct String : Object {
    std::string literal;
    String();
    std::string str() const;
};

std::shared_ptr<Object> eval(const std::string &input, Context &ctx, Cursor &cursor);
std::shared_ptr<Object> infer(const std::string &input, Context &ctx, Cursor &cursor);

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

bool Object::hasTrait(const std::string &trait){
    return traits.count(trait) > 0;
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

void Function::set(const std::string &body, const std::vector<std::string> &params){
    this->lambda = [body](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        return eval(body, ctx, cursor);
    };    
    this->body = body;
    this->params = params;
}

Function::Function() : Object(){
    this->type = ObjectType::LAMBDA;
    this->body = body = "";
    this->lambda = [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        return std::make_shared<Object>(Object());
    };  
} 

Function::Function( const std::string &name, 
                    const std::function<std::shared_ptr<Object>(const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor)> &lambda,
                    const std::vector<std::string> &params){

    this->type = ObjectType::LAMBDA;
    this->name = name;
    this->lambda = lambda;
    this->params = params;
    this->body = "[INNER-FUNCTION]";
}

std::string Function::str() const {
    switch(type){
        case ObjectType::LAMBDA: {
            std::string _params;
            for(int i = 0; i < params.size(); ++i){
                _params += params[i];
                if(i < params.size()-1){
                    _params += " ";
                }
            }
            return "("+_params+")("+body+")";
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
    }
}

List::List() :  Object() {
    type = ObjectType::LIST;
    n = 0;
}

void List::build(const std::vector<std::shared_ptr<Object>> &objects){
    this->list = objects;
    this->n = objects.size();
}

std::string List::str() const {
    switch(type){
        case ObjectType::LIST: {
            std::string _params;
            std::string buff = "";
            for(int i = 0; i < n; ++i){
                buff += list[i]->str();
                if(i < n-1){
                    buff += " ";
                }                
            }
            return "("+buff+")";
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
    }
}

String::String() :  Object(){
    this->type = ObjectType::STRING;
    this->literal = "";
}

std::string String::str() const {
    switch(type){
        case ObjectType::STRING: {
            return "'"+this->literal+"'";
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
    }
}

Context::Context(Context *head){
    // default operators
    this->add(std::make_shared<Function>(Function("+", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sum<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("-", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sub<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("*", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __mult<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("/", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __div<double>(r, operands[i]);
        }
        return r;
    })));  

    this->add(std::make_shared<Function>(Function("pow", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __pow<double>(r, operands[i]);
        }
        return r;
    })));    

    this->add(std::make_shared<Function>(Function("=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) != *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));        

    this->add(std::make_shared<Function>(Function("/=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) == *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));    

    this->add(std::make_shared<Function>(Function(">", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) <= *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));          

    this->add(std::make_shared<Function>(Function("<", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) >= *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));      

    this->add(std::make_shared<Function>(Function(">=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) < *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));          

    this->add(std::make_shared<Function>(Function("<=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) > *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));      

    this->add(std::make_shared<Function>(Function("..", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        if(operands.size() < 2 || operands[0]->type != ObjectType::NUMBER || operands[1]->type != ObjectType::NUMBER){
            cursor.setError("operator '..' expects real number");
            return r;
        }
        std::vector<std::shared_ptr<Object>> elements;
        double step = 1; // hard coded for now
        double s = *static_cast<double*>(operands[0]->data);
        double e = *static_cast<double*>(operands[1]->data);
        if(s == e){
            return r;
        }
        bool inc = e > s;
        for(double i = s; (inc ? i < e : i > e); i += (inc ? step : -step)){
            elements.push_back(infer(std::to_string(i), ctx, cursor));
        }
        r->build(elements);
        return r;
    })));    

    this->add(std::make_shared<Function>(Function("reverse", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        std::vector<std::shared_ptr<Object>> elements;
        for(int j = 0; j < operands.size(); ++j){
            std::vector<std::shared_ptr<Object>> _elements;
            auto list = std::dynamic_pointer_cast<List>(operands[operands.size() - 1 - j]);
            for(double i = 0; i < list->n; ++i){
                auto &item = list->list[list->n - 1 - i];
                _elements.push_back(item);
            }            
            if(operands.size() == 1){
                elements = _elements;
            }else{
                auto r = std::make_shared<List>(List());
                r->build(_elements);
                elements.push_back(r);
            }
        }
        r->build(elements);
        return r;
    })));    


    this->add(std::make_shared<Function>(Function("splice", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        std::vector<std::shared_ptr<Object>> elements;
        auto list = std::dynamic_pointer_cast<List>(operands[0]);
        for(double i = 0; i < list->n; ++i){
            elements.push_back(list->list[list->n - 1 - i]);
        }
        r->build(elements);
        return r;
    })));           

    this->head = head;              
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
        return head == NULL ? std::make_shared<Object>(Object()) : head->find(name);
    }
    return it->second;
}

std::vector<std::string> parse(const std::string &input){
    auto clean = removeExtraSpace(input);
    std::vector<std::string> tokens;
    int open = 0;
    bool comm = false;
    bool str = false;
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
        if(c == '\''){
            str = !str;
        }           
        hold += c;     
        if(!str && open == 0){
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

std::shared_ptr<Object> infer(const std::string &input, Context &ctx, Cursor &cursor){
    // number
    if(isNumber(input) && input.find(' ') == -1){
        auto result = std::make_shared<Object>(Object());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ObjectType::NUMBER);
        return result;
    }
    // string
    if(isString(input)){
        auto result = std::make_shared<String>(String());
        result->literal = input.substr(1, input.length() - 2);
        return result;        
    }
    // variable
    auto v = ctx.find(input);
    if(v->type != ObjectType::NONE){
        return v;
    }else{
        // consider it a list otherwise list
        auto result = std::make_shared<List>(List());
        auto tokens = parse(input);
        std::vector<std::shared_ptr<Object>> list;
        for(int i = 0; i < tokens.size(); ++i){
            list.push_back(eval(tokens[i], ctx, cursor));
        }
        result->build(list);
        return result;      
    }

    return std::make_shared<Object>(Object());
}

std::unordered_map<std::string, std::string> fetchSpecs(std::vector<std::string> input, std::vector<std::string> &output, Context &ctx){
    std::unordered_map<std::string, std::string> traits;
    for(int i = 1; i < input.size(); ++i){
        if((input[i] == "with" || input[i] == "as")  && i < input.size()){
            traits[input[i]] = input[i + 1];
            input.erase(input.begin() + i, input.begin() + i + 1);
        }
    }
    output = input;
    return traits;
}

std::shared_ptr<Object> eval(const std::string &input, Context &ctx, Cursor &cursor){
    auto tokens = parse(input);
    auto imp = tokens[0];    

    if(cursor.error){
        std::cout << "exception line " << cursor.line << " col " << cursor.column << ": " << cursor.message << std::endl;
        std::exit(1);
    }

    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '(' && tokens[0][tokens[0].length()-1] == ')'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);
        return eval(imp, ctx, cursor);
    }

    auto op = ctx.find(imp);
    // keyword
    if(isKeyword(imp)){
        auto specs = fetchSpecs(tokens, tokens, ctx);
        // def
        if(imp == "def"){
            std::string vname = tokens[1];
            auto v = eval(specs["as"], ctx, cursor);
            v->name = vname;
            ctx.add(v);
            return v;
        }else
        // for
        if(imp == "for"){
            Context fctx(ctx);;
            auto range = eval(tokens[1], ctx, cursor);
            auto itn = specs["with"];
            if(range->type != ObjectType::LIST){
                std::cout << "for expects a List" << std::endl;
                return std::make_shared<Object>(Object());
            }
            auto list = std::dynamic_pointer_cast<List>(range);
            std::shared_ptr<Object> last;
            for(int i = 0; i < list->n; ++i){
                auto _it = list->list[i];
                _it->name = itn;
                fctx.add(_it);
                last = eval(specs["as"], fctx, cursor);
            }
            return last;
        }
        // print (temporarily hardcoded)
        if(imp == "print"){
            auto r = eval(tokens[1], ctx, cursor);
            std::cout << r->str() << std::endl;
            return r;
        }
    }else
    // lambda
    if(op->type == ObjectType::LAMBDA){
        Context lctx(ctx);
        auto func = static_cast<Function*>(op.get());
        bool useParams = func->params.size() > 0;
        if(useParams && func->params.size() != tokens.size()-1){
            std::cout << "function '" << func->name << "': expected (" << func->params.size() << "), received (" << tokens.size()-1 << ")" << std::endl;
            return std::make_shared<Object>(Object());
        }
        std::vector<std::shared_ptr<Object>> operands;
        for(int i = 1; i < tokens.size(); ++i){
            auto ev = eval(tokens[i], lctx, cursor);
            ev->name = std::to_string(i-1);
            if(useParams){
                ev->name = func->params[i-1];
            }
            lctx.add(ev);
            operands.push_back(ev);
        } 
        return func->lambda(operands, lctx, cursor);       
    }else{
    // deduce type
        return infer(input, ctx, cursor);
    }

    return std::make_shared<Object>(Object());
}

int main(int argc, const char *argv[]){
    Context main;
    Cursor cursor;

    cursor.column += 1;
    cursor.line += 1;

    // std::cout << eval("for (.. 0 10) as i ((print i))# lol comment #(let a (+ 2 2))", main)->str() << std::endl;
    std::cout << eval("(reverse (1 2 3)(4 5 6))", main, cursor)->str() << std::endl;

    // auto toks = parse("for (.. 0 10) as i ((print i))");
    // // auto toks = parse("(for (0..2) as i ((print i)))# lol comment #(let a (+ 2 2))");



    return 0;
}