#ifndef CANVAS_OBJECT_HPP
    #define CANVAS_OBJECT_HPP

    #include <sys/types.h>

    namespace cv {

        namespace ObjectType {
            enum ObjectType : int8_t {
                None = 0,
                Float,
                Int,
                String,
                Object
            };
        }

        struct Object {
            size_t size;
            int8_t type;
            char *data;
            void write(void *data, size_t size, int8_t type);
            void read(void *target, size_t size);
            void clear();
            Object();
            ~Object();
            void copy(const cv::Object &src);
            cv::Object& operator= (const cv::Object &src);
        };
    }

#endif