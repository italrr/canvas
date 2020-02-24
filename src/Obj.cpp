#include "Obj.hpp"
#include <string.h>

void cv::Obj::write(void *data, size_t size, int8_t type){
    if(this->data != NULL){
        clear();
    }
    this->data = new char[size];
    this->size = size;
    this->type = type;
    memcpy(this->data, data, size);
}

void cv::Obj::read(void *target, size_t size){
    memcpy(target, data, size);
}

void cv::Obj::clear(){
    if(data == NULL){
        return;
    }
    if(type == DataType::List){
        int32_t size;
        memcpy(&size, data, sizeof(size));
        size_t index = sizeof(size);
        for(int i = 0; i < size; ++i){
            cv::Obj *obj;
            size_t psize = sizeof(cv::Obj*);
            memcpy(obj, data + index, psize);
            index += sizeof(obj);
            if(obj == NULL) continue;
            obj->clear();
        }
    }
    delete data;
    size = 0;
}

cv::Obj::Obj(){
    this->data = NULL;
    this->size = 0;
}

cv::Obj::~Obj(){
    clear();
}

void cv::Obj::copy(const cv::Obj &src){
    if(src.data == NULL) return;
    write(src.data, src.size, src.type);    
}

cv::Obj& cv::Obj::operator= (const cv::Obj &src){
    copy(src);    
    return *this;
}