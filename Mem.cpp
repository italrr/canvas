#include <iostream>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define __CV_NUMBER_NATIVE_TYPE double


namespace ItemTypes {
    enum ItemTypes : uint8_t {
        NIL,
        NUMBER,
        STRING,
        LIST,
        CONTEXT,
        FUNCTION,
        INTERRUPT
    };
    // static std::string str(uint8_t type){
    //     switch(type){
    //         case NIL: {
    //             return "NIL";
    //         };
    //         case NUMBER: {
    //             return "NUMBER";
    //         };  
    //         case STRING: {
    //             return "STRING";
    //         };  
    //         case LIST: {
    //             return "LIST";
    //         }; 
    //         case CONTEXT: {
    //             return "CONTEXT";
    //         };    
    //         case FUNCTION: {
    //             return "FUNCTION";
    //         }; 
    //         case INTERRUPT: {
    //             return "INTERRUPT";
    //         };    
    //         default: {
    //             return "UNDEFINED";
    //         };         
    //     }
    // }
}


struct Item {

    void *data;
    size_t size;
    uint8_t type;
    uint32_t refc;

    Item(){
        this->type = ItemTypes::NIL;
    }

    virtual Item *copy(){
        Item *item = new Item();
        if(this->type != ItemTypes::NIL){
            item->data = malloc(this->size);
            item->type = this->type;
            item->size = this->size;
            item->refc = 1;
            memcpy(item->data, this->data, this->size);
        }
        return item;        
    }

    virtual void write(void *src, size_t size, uint8_t type){
        this->type = type;
        this->size = size;
        this->refc = 1;
        this->data = malloc(size);
        memcpy(this->data, src, size);
    }
};


struct Number : Item {
    Number(){

    }

    Number(__CV_NUMBER_NATIVE_TYPE v){
        write(&v, sizeof(__CV_NUMBER_NATIVE_TYPE), ItemTypes::NUMBER);
    }

};


struct List : Item {

    List(){

    }

    List(const std::vector<Item*> items){
        write(items);
    }

    void write(const std::vector<Item*> items){
        this->type = ItemTypes::LIST;
        this->size = items.size();
        this->data = malloc(sizeof(size_t) * items.size());
        for(int i = 0; i < items.size(); ++i){
            auto copy = items[i]->copy();
            size_t address = (size_t)copy;   
            memcpy((void*)((size_t)this->data + i * sizeof(size_t)), &address, sizeof(size_t));
        }
    }

    Item *get(size_t index){
        return static_cast<Item*>((void*)*(size_t*)((size_t)this->data + index * sizeof(size_t)));
    }

};

int main(){

    auto n = new List({new Number(1), new Number(3), new Number(15), new Number(31)});

    // auto n = (new Number(25))->copy();
    // printf("%f\n", *static_cast<double*>(n->data));

    for(size_t i = 0; i < n->size; ++i){
        
        // auto item = n->get(i);
        
        printf("%f\n", *static_cast<double*>(n->get(i)->data));

    }

    return 0;
}