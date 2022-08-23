#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <iomanip>
#include <cmath>

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

bool isNumber(const std::string &s){
    return (s.find_first_not_of( "-.0123456789" ) == std::string::npos);
}

bool isString (const std::string &s){
    return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
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

static unsigned genItemCountId(){
	static std::mutex genMutex;
	static unsigned itemCountId = 1;
	genMutex.lock();
	auto id = ++itemCountId;
	genMutex.unlock();
	return id;
}

namespace ItemType {
	enum ItemType : unsigned {
		NIL,
		NUMBER,
		STRING,
		LIST,
		PROTO,
		FUNCTION,
		INTERRUPT
	};
}


namespace InterruptType {
	enum InterruptType : unsigned {
        BREAK,
        CONTINUE,
        RETURN		
	};
}


struct Item {
	unsigned id;
	void *data;
	unsigned size;
	unsigned type;
	Item(){
		type = ItemType::NIL;
		data = NULL;
		size = 0;
		id = genItemCountId();		
	}
	void clear(){	
		if(type != ItemType::NIL){
			free(data);
		}
		type = ItemType::NIL;
		data = NULL;
		size = 0;
	}
	void write(void *data, size_t size, int type){
		if(this->data != NULL){
			clear();
		}
		this->data = malloc(size);
		this->size = size;
		this->type = type;
		memcpy(this->data, data, size);
	}
	virtual std::shared_ptr<Item> copy(){
		auto r = std::make_shared<Item>(Item());
		r->type = type;
		r->size = size;
		r->data = malloc(size);
		memcpy(r->data, data, size);
		return r;
	}

	virtual std::string str() const {
		switch(type){
			case ItemType::NUMBER: {
				std::ostringstream oss;
				oss << std::setprecision(8) << std::noshowpoint << *static_cast<double*>(data);
				return oss.str();
			} break;
			case ItemType::NIL: {
				return "nil";
			} break;
			default:
				//TODO: HANDLE ERROR
				std::exit(1);
		}
	}		

	virtual operator std::string() const {
		return str();
	}	
};

struct String : Item {
	std::string literal;
	String(){
		this->type = ItemType::STRING;
		this->literal = "";
	}
	std::shared_ptr<Item> copy(){
		auto copy = std::shared_ptr<String>(new String());
		copy->literal = this->literal;
		return std::static_pointer_cast<Item>(copy);
	}
	std::string str() const {
		switch(type){
			case ItemType::STRING: {
				return "'"+this->literal+"'";
			} break;
			case ItemType::NIL: {
				return "nil";
			} break;
			default: 
				// TODO: HANDLE ERROR
				std::exit(1);
		}		
	}
};

struct Context;
std::shared_ptr<Item> infer(const std::string &input, Context *ctx, Cursor *cursor);

struct Proto : Item {
	std::unordered_map<std::string, std::shared_ptr<Item>> items;

	void populate(const std::vector<std::string> &names, const std::vector<std::string> &values, Context *ctx, Cursor *cursor){
		for(unsigned i = 0; i < names.size(); ++i){
			this->items[names[i]] = infer(values[i], ctx, cursor);
		}
	}

	std::shared_ptr<Item> get(const std::string &key){
		auto it = this->items.find(key);
		if(it == this->items.end()){
			return std::shared_ptr<Item>(NULL);
		}
		return it->second;
	}

	// std::shared_ptr<Item> copy(){
	// 	auto copied = std::shared_ptr<Proto>(new Proto());
	// 	for(auto &it : this->items){
	// 		copied->items[it.first] = it.second;
	// 	}
	// 	return std::static_pointer_cast<Item>(copied);
	// }

	std::string str() const {
		auto addTab = [](unsigned n){
			std::string tabs;
			for(unsigned i = 0; i < n; ++i){
				tabs += "\t";
			}
			return tabs;
		};
		unsigned maxWidth = 0;
		std::vector<std::string> names;
		std::vector<std::string> values;
		for(auto &it : this->items){
			maxWidth = std::max((unsigned)it.first.length(), maxWidth);
			names.push_back(it.first);
			values.push_back(it.second->str());
		}		
		std::string out;
		for(unsigned i = 0; i < names.size(); ++i){
			out += names[i];
			out += addTab(maxWidth);
			out += std::string(": ") + values[i]+"\n";
		}
		return out;
	}
	

};

struct Context {
	Context *head;
	std::unordered_map<std::string, std::shared_ptr<Item>> items;
	Context(Context *head = NULL){
		this->head = head;
	}
	void add(const std::string &name, const std::shared_ptr<Item> &item){
		this->items[name] = item;
	}
	std::shared_ptr<Item> find(const std::string &name){
		auto it = items.find(name);
		if(it == items.end()){
			return head == NULL ? std::make_shared<Item>(Item()) : head->find(name);
		}
		return it->second;		
	}
	operator std::string() const {
		return str();
	}

	std::string str(bool ignoreInners = false) const {
		std::string output = "CONTEXT\n";

		int fcol = 5;
		int scol = 5;

		for(auto &it : items){
			// if(ignoreInners && it.second->innerType){
			// 	continue;
			// }
			while(it.first.length() > fcol){
				fcol += 5;
			}
		}

		// scol = 5;

		for(auto &it : items){
			// if(ignoreInners && it.second->innerType){
			// 	continue;
			// }    
			output += it.first;
			int toAdd = fcol - it.first.length();
			for(int i = 0; i < toAdd; ++i){
				output += " ";
			}
			output += "->";
			for(int i = 0; i < scol; ++i){
				output += " ";
			}    
			output += "'"+it.second->str()+"'\n";
		}

		return output;
	}
};


std::shared_ptr<Item> infer(const std::string &input, Context *ctx, Cursor *cursor){
    // auto v = ctx.find(input);
    // number
    if(isNumber(input) && input.find(' ') == -1){
        auto result = std::make_shared<Item>(Item());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ItemType::NUMBER);
        return result;
    }
    // string
    if(isString(input)){
        auto result = std::make_shared<String>(String());
        result->literal = input.substr(1, input.length() - 2);
        return result;        
    }
    // // variable
    // if(v->type != ItemType::NIL){
    //     return v;
    // }else{
    //     // consider it a list otherwise list
    //     auto result = std::make_shared<List>(List());
    //     auto tokens = parse(input, cursor);
    //     std::vector<std::shared_ptr<Object>> list;
    //     for(int i = 0; i < tokens.size(); ++i){
    //         list.push_back(eval(tokens[i], ctx, cursor));
    //     }
    //     result->build(list);
    //     return result;      
    // }

    return std::make_shared<Item>(Item());
}


static std::shared_ptr<Item> eval(const std::string &input, Context *ctx, Cursor *cursor){
	auto tokens = parse(input);


	auto imp = tokens[0];

    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);
        return eval(imp, ctx, cursor);
    }

	// DECLARE VARIABLE IN CURRENT CONTEXT
	if(imp == "def"){

	}else
	// FUNCTION
	if(imp == "fn"){

	}else
	// PROTOTYPE OBJECT
	if(imp == "proto"){
		auto proto = std::make_shared<Proto>(Proto());

		std::vector<std::string> names;
		std::vector<std::string> values;

		for(unsigned i = 1; i < tokens.size(); ++i){
			auto &token = tokens[i];

			auto fc = token.find(":");

			std::string varname = token.substr(0, fc);
			std::string varvalue = token.substr(fc+1, token.length()-fc-1);

			names.push_back(varname);
			values.push_back(varvalue);
		}

		proto->populate(names, values, ctx, cursor);	

		return proto;		
	}


	return std::make_shared<Item>(Item());


}




int main(int argc, char* argv[]){
	auto printList = [](const std::vector<std::string> &in){
		for(int i = 0; i < in.size(); ++i){
			std::cout << "'" << in[i] << "'" << std::endl;
		}
	};


	// printList(parse("proto [ a:4 b:5 c:[fn [][print 'lol']] ]"));

	Cursor cursor;
	Context ctx;
	std::cout << eval("proto a:4 b:5", &ctx, &cursor)->str() << std::endl;



}