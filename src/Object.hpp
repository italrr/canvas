#ifndef CANVAS_OBJECT_HPP
    #define CANVAS_OBJECT_HPP
    
    #include <functional>
    #include <memory>
    #include <cmath>
    #include <unordered_map>

    #include "Tools.hpp"

    struct Context;
    
    enum ObjectType : int {
        NONE,
        NUMBER,
        STRING,
        LIST,
        OBJECT,
        LAMBDA,
        INTERRUPT
    };

    enum InterruptType : int {
        BREAK,
        CONTINUE,
        RETURN
    };

    struct Object {
        int id;
        std::string name;
        std::unordered_map<std::string, std::string> traits;
        bool innerType;
        bool hasTrait(const std::string &trait);
        int n;
        void *data;
        size_t size;
        int type;
        Object();
        virtual std::string str() const;
        void clear();
        void write(void *data, size_t size, int type);
        virtual std::shared_ptr<Object> copy();
        ~Object();
        operator std::string() const;
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
        std::shared_ptr<Object> copy();
        std::string str() const;
    };

    struct List : Object {
        int n;
        std::vector<std::shared_ptr<Object>> list;
        List();
        void build(const std::vector<std::shared_ptr<Object>> &objects);
        std::shared_ptr<Object> copy();
        std::string str() const;
    };

    struct String : Object {
        std::string literal;
        String();
        std::shared_ptr<Object> copy();
        std::string str() const;
    };

    struct Interrupt : Object {
        int intype;
        std::shared_ptr<Object> payload;
        std::shared_ptr<Object> copy();
        Interrupt(int intype = InterruptType::BREAK, std::shared_ptr<Object> payload = std::make_shared<Object>(Object()));
        std::string str() const;
    };


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

#endif