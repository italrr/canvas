#include "Tools.hpp"
#include <string.h>

std::vector<std::string> cv::split(const std::string &str, const std::string &sep){
    std::vector<std::string> tokens;
    char *token = strtok((char*)str.c_str(), sep.c_str()); 
    while(token != NULL){ 
        tokens.push_back(std::string(token));
        token = strtok(NULL, "-"); 
    } 
    return tokens;
}