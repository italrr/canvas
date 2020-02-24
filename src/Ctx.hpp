#ifndef CANVAS_CONTEXT_HPP
    #define CANVAS_CONTEXT_HPP

    #include <string>
    #include <map>
    #include "Obj.hpp"

    namespace cv {
        struct Ctx { 
            std::map<std::string, cv::Obj*> data;
            cv::Ctx *head;
            size_t size;
            Ctx(cv::Ctx *head);
            Ctx();
            ~Ctx();
            cv::Obj *get(const std::string &name);
            cv::Obj *def(const std::string &name, char *data, size_t size, uint8_t type);
            bool rm(const std::string &name);
            bool clear();
        };
    }

#endif