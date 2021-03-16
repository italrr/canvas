#ifndef CANVAS_CONTEXT_HPP
    #define CANVAS_CONTEXT_HPP

    #include <unordered_map>
    #include <memory>
    
    #include "Object.hpp"

    struct Context {
        Context *head;
        std::unordered_map<std::string, std::shared_ptr<Object>> objecs;
        Context(Context *head = NULL);
        void add(const std::shared_ptr<Object> &object);
        std::shared_ptr<Object> find(const std::string &name);
        operator std::string() const;
        std::string str(bool ignoreInner = false) const;
    };
    

#endif