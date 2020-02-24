#include "Object.hpp"
#include <string.h>

void cv::Object::write(void *data, size_t size, int8_t type){
    if(this->data != NULL){
        clear();
    }
    this->data = new char[size];
    this->size = size;
    this->type = type;
    memcpy(this->data, data, size);
}

void cv::Object::read(void *target, size_t size){
    // ATTENTION: target must be big enough to fit this data
    memcpy(target, data, size);
}

void cv::Object::clear(){
    if(data == NULL){
        return;
    }
    delete data;
    size = 0;
}

cv::Object::Object(){
    this->data = NULL;
    this->size = 0;
}

cv::Object::~Object(){
    clear();
}

void cv::Object::copy(const cv::Object &src){
    if(src.data == NULL) return;
    write(src.data, src.size, src.type);    
}

cv::Object& cv::Object::operator= (const cv::Object &src){
    copy(src);    
}