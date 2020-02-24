#ifndef CANVAS_OBJECT_HPP
    #define CANVAS_OBJECT_HPP

    #include <stdint.h>

    namespace cv {

        // these are native data types
        namespace DataType {
            enum DataType : int8_t {
                None = 0,
                Bool,       // single byte
                Byte,       // single byte
                Binary,     // array of chars
                Float,      // 32 bit IEEE 754   
                Int,        // 32 bit signed int
                UInt,       // 32 bit unsigned int
                String,     // ASCII support by default. Unicode might be added later
                List,       // lists are immutable (elements are not)
                Ref
            };
        }

        struct Obj {
            size_t size;
            int8_t type;
            char *data;
            void write(void *data, size_t size, int8_t type);
            void read(void *target, size_t size);
            void clear();
            Obj();
            ~Obj();
            void copy(const cv::Obj &src);
            cv::Obj& operator= (const cv::Obj &src);
        };
    }

#endif