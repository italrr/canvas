#include "Tools.hpp"

bool isKeyword(const std::string &input){
    for(int i = 0; i < KEYWORDLIST.size(); ++i){
        if(input == KEYWORDLIST[i]){
            return true;
        }
    }
    return false;
}

std::string removeExtraSpace(const std::string &in){
    std::string cpy;
    for(int i = 0; i < in.length(); ++i){
        if(i > 0 && in[i] == ' ' && in[i - 1] == ' '){
            continue;
        }
        if((i == 0 || i == in.length() - 1) && in[i] == ' '){
            continue;
        }
        cpy += in[i];
    }
    return cpy;
}

std::string removeEndOfLine(const std::string &in){
    std::string cpy;
    for(int i = 0; i < in.length(); ++i){
        if(in[i] == '\n' || in[i] == '\r'){
            continue;
        }
        cpy += in[i];
    }
    return cpy;
}

std::vector<std::string> split(const std::string &str, const std::string &sep){
    std::vector<std::string> tokens;
    char *token = strtok((char*)str.c_str(), sep.c_str()); 
    while(token != NULL){ 
        tokens.push_back(std::string(token));
        token = strtok(NULL, "-"); 
    } 
    return tokens;
}

bool isNumber(const std::string &s){
    return (s.find_first_not_of( "-.0123456789" ) == std::string::npos);
}

bool isString (const std::string &s){
    return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
}