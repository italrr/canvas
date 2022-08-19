#include <iostream>
#include <vector>

/*
	
	[def a 5]
	[def b [
				fn [a b c]
				[
					return [+ a b c]
				]
			]
	]
	
    [def type
        [proto [
            a: 5,
            b: 10,
            c: 15
        ]]
    ]

*/

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

std::vector<std::string> parse(const std::string &input){
    auto clean = removeExtraSpace(input);
    std::vector<std::string> tokens;
    int open = 0;
    bool comm = false;
    bool str = false;
    std::string hold;
    for(int i = 0; i < input.length(); ++i){
        char c = input[i];
        // ++c;
        if(c == '[') ++open;
        if(c == ']') --open;
        if(c == '#'){
            comm = !comm;
            continue;
        }
        if(comm){
            continue;
        }
        if(c == '\''){
            str = !str;
        }           
        hold += c;     
        if(!str && open == 0){
            if(c == ']' || c == ' ' || i ==  input.length()-1){
                if(hold != " "){
                    tokens.push_back(removeExtraSpace(hold));
                }
                hold = "";
            }
        }
    }
    if(open != 0){
		std::cout << "bad parenthesis" << std::endl;
        // std::string error = "missing '"+std::string(open > 0 ? ")" : "(")+"' parenthesis";
        // printf("splitTokens: syntax error: %s\n", error.c_str());
    }
    return tokens;
}


namespace ItemType {
	enum ItemType : unsigned {
		
	}
};


struct Item {

};


int main(int argc, char* argv[]){
	auto printList = [](const std::vector<std::string> &in){
		for(int i = 0; i < in.size(); ++i){
			std::cout << "'" << in[i] << "'" << std::endl;
		}
	};


	printList(parse("proto [ a: 4, b: 5, c: [fn [][print 'lol']] ]"));



}