#include <sstream>
#include <iomanip>
#include "Object.hpp"
#include "Eval.hpp"

static unsigned uniqueIdCount = 1;

Object::Object(){
    type = ObjectType::NONE;
    data = NULL;
    size = 0;
    innerType = false;
    id = ++uniqueIdCount;
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
        default:
            // TODO: add some error
            std::exit(1);
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

std::shared_ptr<Object> Object::copy(){
    auto r = std::make_shared<Object>(Object());
    r->type = type;
    r->innerType = innerType;
    r->name = name;
    r->traits = traits;
    r->n = n;
    r->size = size;
    r->data = new char[size];
    memcpy(r->data, data, size);
    return r;
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
    this->innerType = false;
    this->params = params;
}

std::shared_ptr<Object> Function::copy(){
    auto r = std::make_shared<Function>(Function());
    r->name = name;
    r->traits = traits;
    r->lambda = lambda;
    r->params = params;
    r->body = body;
    return r;
}

Function::Function() : Object(){
    this->type = ObjectType::LAMBDA;
    this->body = body = "";
    this->innerType = true;
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
    this->innerType = true;
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
        default:
            std::exit(1);
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

std::shared_ptr<Object> List::copy(){
    auto r = std::make_shared<List>(List());
    r->name = name;
    r->traits = traits;
    for(int i = 0; i < this->n; ++i){
        r->list.push_back(this->list[i]->copy());
    }
    return r;
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
        default: 
            std::exit(1);
    }
}

String::String() :  Object(){
    this->type = ObjectType::STRING;
    this->literal = "";
}

std::shared_ptr<Object> String::copy(){
    auto r = std::make_shared<String>(String());
    r->literal = literal;
    r->name = name;
    r->traits = traits;
    return r;
}

std::string String::str() const {
    switch(type){
        case ObjectType::STRING: {
            return "'"+this->literal+"'";
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
        default: 
            std::exit(1);
    }
}

Interrupt::Interrupt(int intype, std::shared_ptr<Object> payload){
    this->type = ObjectType::INTERRUPT;
    this->intype = type;
    this->payload = payload;
}

std::shared_ptr<Object> Interrupt::copy(){
    auto r = std::make_shared<Interrupt>();
    r->intype = intype;
    r->name = name;
    r->traits = traits;
    return r;
}

std::string Interrupt::str() const {
    switch(type){
        case ObjectType::INTERRUPT: {
            switch(intype){
                case InterruptType::BREAK: {
                    return "[INTERRUPT-BREAK]";
                } break;
                case InterruptType::CONTINUE: {
                    return "[INTERRUPT-CONTINUE]";
                } break;
                case InterruptType::RETURN: {
                    return "[INTERRUPT-RETURN]";
                } break;
                default: {
                    return "[INTERRUPT]";
                } break;                             
            }
        } break;
        case ObjectType::NONE: {
            return "nil";
        } break;
        default: 
            std::exit(1);
    }
}