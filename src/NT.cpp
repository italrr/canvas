#include <string.h>
#include <string>
#include <sstream>

#include "Tools.hpp"
#include "Obj.hpp"

cv::Obj *interp(const std::string &val);

/*
    check types
*/
// TODO: is Unsigned Int
static inline bool __isInt(const std::string &s){
   if(s.empty() || ((!isdigit(s[0])) && (s[0] != '-') && (s[0] != '+'))) return false;
   char *p;
   strtol(s.c_str(), &p, 10);
   return (*p == 0);
}

static inline bool __isFloat(const std::string &s) {
    std::istringstream iss(s);
    float dummy;
    iss >> std::noskipws >> dummy;
    return iss && iss.eof();     // Result converted to bool
}

static inline bool __isBool(const std::string &s){
    return s == "true" || s == "false";
}

static inline bool __isString(const std::string &s){
    return s.length() > 2 && s[0] == '"' && s[s.length()-1] == '"';
}

static bool __isList(const std::string &s){
    int n = 0;
    auto tokens = cv::split(s, ",");
    for(int i = 0; i < s.length(); ++i){
        if(s[i] == ','){
            ++n;
        }
    }
    return tokens.size() == (n-1);
}

/*
    converters
*/

static inline float __toFloat(const std::string &s){
    return strtof(s.c_str(), NULL);
}

// TODO: add support for unsigned vals  
static inline long long int __toInt(const std::string &s){
    return atoll(s.c_str());
}

static inline bool __toBool(const std::string &s){
    return s == "true";
}

static void* __toList(const std::string &s, size_t &outsize){
    // format   
    // [4 bytes -> n][DATA[0]]...[DATA[n]]
    auto tokens = cv::split(s, ",");
    uint32_t n = tokens.size();
    size_t size = sizeof(uint32_t) + sizeof(void*) * tokens.size();
    size_t index = 0;
    char *data = new char[size];
    auto write = [&](void *src, size_t size){
        if(src == NULL){
            memset(data + index, 0, size);
        }else{
            memcpy(data + index, src, size);
        }
        index += size;
    };
    // size
    write(&n, sizeof(n));

    // objects
    for(int i = 0; i < n; ++i){
        auto obj = interp(tokens[i]);
        write(obj, sizeof(void*));
    }

    outsize = size;
    return data;
}

/* 
    integrated math operators
*/
template<typename T>
inline static cv::Obj *__sum(const cv::Obj *a, const cv::Obj *b){
    if(a == NULL || b == NULL){
        return NULL;
    }
    cv::Obj *result = new cv::Obj();
    T r = *static_cast<T*>(a->data) + *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
inline static cv::Obj *__subs(const cv::Obj *a, const cv::Obj *b){
    if(a == NULL || b == NULL){
        return NULL;
    }
    cv::Obj *result = new cv::Obj();
    T r = *static_cast<T*>(a->data) - *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

template<typename T>
inline static cv::Obj *__mult(const cv::Obj *a, const cv::Obj *b){
    if(a == NULL || b == NULL){
        return NULL;
    }
    cv::Obj *result = new cv::Obj();
    T r = *static_cast<T*>(a->data) * *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

/*
    interp
*/
template<typename T>
inline static cv::Obj *__div(const cv::Obj *a, const cv::Obj *b){
    if(a == NULL || b == NULL){
        return NULL;
    }
    cv::Obj *result = new cv::Obj();
    T r = *static_cast<T*>(a->data) / *static_cast<T*>(b->data);
    result->write(&r, sizeof(r), a->type);
    return result;
}

cv::Obj *interp(const std::string &val){
    cv::Obj *r = NULL;
    if(__isFloat(val)){
        cv::Obj *var = new cv::Obj();
        auto v = __toFloat(val);
        var->write(&v, sizeof(v), cv::DataType::Float);
    }else    
    if(__isInt(val)){
        cv::Obj *var = new cv::Obj();
        auto v = __toInt(val);
        var->write(&v, sizeof(v), cv::DataType::Int);
    }else
    if(__isBool(val)){
        cv::Obj *var = new cv::Obj();
        auto v = __toBool(val);
        var->write(&v, sizeof(v), cv::DataType::Bool);
    }else
    if(__isList(val)){
        size_t size;
        cv::Obj *var = new cv::Obj();
        auto v = __toList(val, size);
        var->write(v, size, cv::DataType::List);
    }
    return r;
}