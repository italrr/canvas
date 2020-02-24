#include "Ctx.hpp"

cv::Ctx::Ctx(cv::Ctx *head){
    this->head = head;
}

cv::Ctx::Ctx(){
    this->head = NULL;
}

cv::Obj* cv::Ctx::get(const std::string &name){
    auto it = data.find(name);
    if(it == data.end()){
        return head != NULL ? head->get(name) : NULL;
    }
    return data[name];
}

cv::Obj *cv::Ctx::def(const std::string &name, char *data, size_t size, uint8_t type){
    auto obj = get(name);
    auto it = this->data.find(name);
    if(obj == NULL && it == this->data.end()){
        obj = new cv::Obj();
        this->data[name] = obj;
    }
    obj->clear();
    obj->write(data, size, type);
    return obj;
}

bool cv::Ctx::rm(const std::string &name){
    auto obj = get(name);
    if(obj == NULL){
        return false;
    }
    size -= obj->size;
    obj->clear();
    return true;
}

bool cv::Ctx::clear(){
    bool r = true;
    for(auto &cl : data){
        if(!rm(cl.first)){
            r = false;
        }
    }
    return r;
}