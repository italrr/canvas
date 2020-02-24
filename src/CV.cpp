#include <vector>
#include <string>
#include <stdio.h>

struct Result {
    bool scss;
    std::string msg;
};

static const std::string keywords[] = {
    "def", "if", "for", "return", "splice",
    "break", "cont", "->"
};
static const size_t totalKeywords = sizeof(keywords) / sizeof(std::string);
static const inline size_t isKeyword(const std::string &token){
    for(int i = 0; i < totalKeywords; ++i){
        if(keywords[i] == token){
            return i;
        }
    }
    return -1;
}
static const std::string operators[] = {
    "="
};
static const size_t totalOperators = sizeof(operators) / sizeof(std::string);
static const inline size_t isOperator(const std::string &token){
    for(int i = 0; i < totalOperators; ++i){
        if(operators[i] == token){
            return i;
        }
    }
    return -1;    
}

std::string removeExtraSpace(const std::string &in){
    std::string cpy;
    for(int i = 0; i < in.length(); ++i){
        if(i > 0 && in[i] == ' ' && in[i - 1] == ' '){
            continue;
        }
        cpy += in[i];
    }
    return cpy;
}

std::vector<std::string> parse(const std::string &input){
    std::vector<std::string> tokens;
    std::string in = removeExtraSpace(input);
    int j = 0;
    int lvpar = 0;
    int lvcb = 0;
    int lvstr = 0;
    auto add = [&](int i, int n){
        if(n <= 0){
            return;
        }
        tokens.push_back(removeExtraSpace(in.substr(j, n)));
    };
    for(int i = 0; i < in.length(); ++i){     
        if(in[i] == '{'){
            ++lvcb;
            if(lvcb == 1){
                j = i;
            }
        }
        if(in[i] == '}'){
            --lvcb;
            if(lvcb == 0){
                add(j, i - j + 1);
                j = i + 1;
                continue;            
            }
        }        
        if(in[i] == '(' && lvcb == 0){
            ++lvpar;
            if(lvpar == 1){
                j = i + 1;
            }
        }
        if(in[i] == ')' && lvcb == 0){
            --lvpar;
            if(lvpar == 0){
                add(j, i - j);
                j = i + 1;
                continue;            
            }
        }
        bool inp = lvcb == 0 && lvpar == 0;
        if(auto w = isKeyword(removeExtraSpace(in.substr(j, i - j))) != -1 && inp){
            add(j, i - j);
            j = i + 1;
        }        
        if(inp && (in[i] == ' ' || in[i] == ';' || in[i] == '\n' || i == in.length()-1)){
            add(j, i - j);
            j = i + 1;
            continue;
        }
        if(inp && in[i] == '='){
            add(j, i - j);
            tokens.push_back("=");
            j = i + 1;
            continue;
        }

    }
    return tokens;
}

void interpret(const std::string &input){
    auto tokens = parse(input);
}


int main(int argc, const char *argv[]){
    auto tokens = parse("def list = (1, 2, 3, 4);");
    for(int i = 0; i < tokens.size(); ++i){
        printf("'%s'\n", tokens[i].c_str());
    }
    return 0;
}