#ifndef CANVAS_TOOLS_HPP
    #define CANVAS_TOOLS_HPP

    #include <string>
    #include <vector>
    #include <string.h>
    
    static const std::vector<std::string> KEYWORDLIST = {
        "def", "object", "func", "if",
        "for", "while", "nil", "print",
        "continue", "break", "return",
        "fn", "proto"
    };

    struct Cursor {
        int line;
        int column;
        bool error;
        std::string message;
        Cursor(){
            line = 0;
            column = 0;
            error = false;
            message = "";
        }
        void setError(const std::string &message, int line, int column){
            this->message = message;
            this->line = line;
            this->column = column;
            this->error = true;
        }
        void setError(const std::string &message){
            this->message = message;
            this->error = true;
        }
    };

    bool isKeyword(const std::string &input);
    std::string removeExtraSpace(const std::string &in);
    std::string removeEndOfLine(const std::string &in);
    std::vector<std::string> split(const std::string &str, const std::string &sep);
    bool isNumber(const std::string &s);
    bool isString (const std::string &s);
#endif