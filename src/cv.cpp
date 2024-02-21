#include <iostream>
#include <signal.h>
#include <cstring>
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

static const std::vector<std::string> RESERVED_WORDS = {
	"nil", "set", "proto", "fn",
	"ret", "skip", "stop",
	"if", "do", "iter"
};

namespace ItemTypes {
	enum ItemTypes : unsigned {
		NIL,
		NUMBER,
		STRING,
		LIST,
		PROTO,
		FUNCTION,
		INTERRUPT
	};
}


namespace InterruptTypes {
	enum InterruptTypes : unsigned {
        STOP,
        SKIP,
        RET		
	};
}



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
    return (s.find_first_not_of( "-.0123456789") == std::string::npos);
}

bool isValidVarName(const std::string &s){
    return (s.substr(0, 1).find_first_not_of( "-.0123456789" ) != std::string::npos);
}

bool isString(const std::string &s){
    return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
}

std::vector<std::string> split(const std::string &str, const char sep){
	std::vector<std::string> tokens;
	std::size_t start = 0, end = 0;
	while ((end = str.find(sep, start)) != std::string::npos) {
		tokens.push_back(str.substr(start, end - start));
		start = end + 1;
	}
	tokens.push_back(str.substr(start));
	return tokens;
}

bool isList(const std::string &s){

	if(s.length() < 3){
		return false;
	}

	bool str = false;

	for(unsigned i = 0; i < s.length(); ++i){
		char c = s[i];
        if(c == '\''){
            str = !str;
        }     
		if(!str && s[i] == ' '){
			return true;
		}
	}

	return false;

	// if()

    // return s.size() > 1 && s[0] == '[' && s[s.length()-1] == ']' && s.find(' ') == -1;
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
		std::cout << "Bad parenthesis" << std::endl;
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

struct Item {
	unsigned id;
	void *data;
	unsigned size;
	unsigned type;
	std::unordered_map<std::string,std::shared_ptr<Item>> members;
	Item(){
		type = ItemTypes::NIL;
		data = NULL;
		size = 0;
		id = genItemCountId();		
	}
	void clear(){	
		if(type != ItemTypes::NIL){
			free(data);
		}
		type = ItemTypes::NIL;
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

	void registerProperty(const std::string &name, const std::shared_ptr<Item> &v){
		this->members[name] = v;
	}

	std::shared_ptr<Item> getProperty(const std::string &name){
		auto it = this->members.find(name);
		if(it == this->members.end()){
			return std::make_shared<Item>(Item());
		}
		return it->second;
	}

	virtual std::string str(bool singleLine = false) const {
		switch(type){
			case ItemTypes::NUMBER: {
				std::ostringstream oss;
				oss << std::setprecision(8) << std::noshowpoint << *static_cast<double*>(data);
				return oss.str();
			} break;
			case ItemTypes::NIL: {
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

struct NumberType : Item {


	NumberType(){
		this->type = ItemTypes::NUMBER;
		this->data = new double(0);
	}

	~NumberType(){
		this->clear();
	}

	void set(int n){
		double _n = (double)n;
		memcpy(this->data, &_n, sizeof(_n));
	}

	void set(float n){
		double _n = (double)n;
		memcpy(this->data, &_n, sizeof(_n));
	}

	void set(double n){
		double _n = (double)n;
		memcpy(this->data, &_n, sizeof(_n));
	}

	double get(){
		double n;
		memcpy(&n, this->data, sizeof(n));
		return n;
	}

	std::shared_ptr<Item> copy(){
		auto copy = std::make_shared<NumberType>(NumberType());
		copy->set(get());
		return std::static_pointer_cast<Item>(copy);
	}	

	std::string str(bool singleLine = false) const {
		switch(type){
			case ItemTypes::NUMBER: {
				std::ostringstream oss;
				oss << std::setprecision(8) << std::noshowpoint << *static_cast<double*>(data);
				return oss.str();
			} break;
			case ItemTypes::NIL: {
				return "nil";
			} break;
			default: 
				// TODO: HANDLE ERROR
				std::exit(1);
		}		
	}

};

struct StringType : Item {
	std::string literal;
	int length;
	StringType(){
		this->type = ItemTypes::STRING;
		this->literal = "";
	}
	std::shared_ptr<Item> copy(){
		auto copy = std::shared_ptr<StringType>(new StringType());
		copy->literal = this->literal;
		return std::static_pointer_cast<Item>(copy);
	}
	
	void set(const std::string &v){
		this->literal = v;
		this->length = this->literal.length();
	}	

	std::string str(bool singleLine = false) const {
		switch(type){
			case ItemTypes::STRING: {
				return "'"+this->literal+"'";
			} break;
			case ItemTypes::NIL: {
				return "nil";
			} break;
			default: 
				// TODO: HANDLE ERROR
				std::exit(1);
		}		
	}
};

struct Context;
static std::shared_ptr<Item> create(int n);
static std::shared_ptr<Item> infer(const std::string &input, Context *ctx, Cursor *cursor);
static std::shared_ptr<Item> eval(const std::string &input, Context *ctx, Cursor *cursor);

struct ListType : Item {
	std::vector<std::shared_ptr<Item>> list;
	ListType(){
		type = ItemTypes::LIST;
		this->registerProperty("length", create(0));
	}
	void build(const std::vector<std::shared_ptr<Item>> &objects){
		this->list = objects;
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)this->list.size());
	}
	void add(const std::shared_ptr<Item> &item){
		this->list.push_back(item);
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)this->list.size());
	}
	std::shared_ptr<Item> copy(){
		return std::make_shared<Item>(Item());
	}
	std::string str(bool singleLine = false) const {
		switch(type){
			case ItemTypes::LIST: {
				std::string _params;
				std::string buff = "";
				for(int i = 0; i < this->list.size(); ++i){
					buff += list[i]->str();
					if(i < this->list.size()-1){
						buff += " ";
					}                
				}
				return "["+buff+"]";
			} break;
			case ItemTypes::NIL: {
				return "nil";
			} break;
			default: 
				std::exit(1);
		}
	}
};


struct FunctionType : Item {
	std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor)> lambda;
	std::vector<std::string> params;
	std::string body;
	bool inner;
	FunctionType(){
		this->type = ItemTypes::FUNCTION;
		this->body = "";
		this->inner = false;
		this->lambda = [](const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor){
			return std::make_shared<Item>(Item());
		};  
	}
	FunctionType(const std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor)> &lambda,
				const std::vector<std::string> &params = {}){
					this->type = ItemTypes::FUNCTION;
					this->lambda = lambda;
					this->params = params;
					this->inner = true;
				}
	
	void set(const std::string &body, const std::vector<std::string> &params = {}){
		this->lambda = [body](const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor){
			return eval(body, ctx, cursor);
		};    
		this->body = body;
		this->inner = false;
		this->params = params;
	}
	
	std::shared_ptr<Item> copy(){
		return std::make_shared<Item>(Item());
	}

	std::string str(bool singleLine = false) const {
		switch(type){
			case ItemTypes::FUNCTION: {
				if(this->inner){
					return "[COMPILED]";
				}
				std::string _params;
				for(int i = 0; i < params.size(); ++i){
					_params += params[i];
					if(i < params.size()-1){
						_params += " ";
					}
				}
				return "fn ["+_params+"] -> ["+body+"]";
			} break;
			case ItemTypes::NIL: {
				return "nil";
			} break;
			default:
				std::exit(1);
		}
	}
};

struct InterruptType : Item {
	int intype;
	std::shared_ptr<Item> payload;

	std::shared_ptr<Item> copy(){
		auto r = std::make_shared<InterruptType>();
		r->intype = intype;
		return std::static_pointer_cast<Item>(r);		
	}

	InterruptType(int intype = InterruptTypes::STOP, std::shared_ptr<Item> payload = std::make_shared<Item>(Item())){
		this->type = ItemTypes::INTERRUPT;
		this->intype = intype;
		this->payload = payload;
	}
	std::string str(bool singleLine = false) const {
		switch(type) {
			case ItemTypes::INTERRUPT: {
				switch(intype){
					case InterruptTypes::STOP: {
						return "[INTERRUPT-STOP]";
					} break;
					case InterruptTypes::SKIP: {
						return "[INTERRUPT-SKIP]";
					} break;
					case InterruptTypes::RET: {
						return "[INTERRUPT-RET]";
					} break;
					default: {
						return "[INTERRUPT]";
					} break;                             
				}
			} break;
			case ItemTypes::NIL: {
				return "nil";
			} break;
			default: 
				std::exit(1);
		}
	}
};

struct ProtoType : Item {

	ProtoType(){
		this->type = ItemTypes::PROTO;
		this->registerProperty("length", create(0));
	}

	void populate(const std::vector<std::string> &names, const std::vector<std::shared_ptr<Item>> &values, Context *ctx, Cursor *cursor){
		for(unsigned i = 0; i < names.size(); ++i){
			auto name = names[i];
			this->registerProperty(names[i], values[i]);
		}
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)members.size()-1);
	}


	void add(const std::string &name, std::shared_ptr<Item> &item, Context *ctx, Cursor *cursor){ 
		this->registerProperty(name, item);
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)members.size()-1);
	}

	// std::shared_ptr<Item> copy(){
	// 	auto copied = std::shared_ptr<MOCK>(new MOCK());
	// 	for(auto &it : this->items){
	// 		copied->items[it.first] = it.second;
	// 	}
	// 	return std::static_pointer_cast<Item>(copied);
	// }

	std::string str(bool singleLine = false) const {
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
		for(auto &it : this->members){
			maxWidth = std::max((unsigned)it.first.length(), maxWidth);
			names.push_back(it.first);
			values.push_back(it.second->str(true));
		}		
		std::string out = singleLine ? "PROTO " : "PROTO\n";
		for(unsigned i = 0; i < names.size(); ++i){
			if(singleLine){
				out += names[i]+std::string(": ")+values[i];
				if(i < names.size()-1){
					out += ", ";
				}
			}else{
				out += names[i];
				out += addTab(maxWidth);
				out += std::string(": ") + values[i]+"\n";
			}
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

	std::string str(bool ignoreInners = true) const {
		std::string output = "CONTEXT\n";

		int fcol = 5;
		int scol = 5;

		for(auto &it : items){
			const auto item = it.second;
			if(ignoreInners && item->type == ItemTypes::FUNCTION && std::static_pointer_cast<FunctionType>(item)->inner){
				continue;
			}
			while(it.first.length() > fcol){
				fcol += 5;
			}
		}

		// scol = 5;

		for(auto &it : items){
			const auto item = it.second;
			if(ignoreInners && item->type == ItemTypes::FUNCTION && std::static_pointer_cast<FunctionType>(item)->inner){
				continue;
			} 
			output += it.first;
			int toAdd = fcol - it.first.length();
			for(int i = 0; i < toAdd; ++i){
				output += " ";
			}
			output += "->";
			for(int i = 0; i < scol; ++i){
				output += " ";
			}    
			output += "'"+it.second->str(true)+"'\n";
		}

		return output;
	}
};


static std::shared_ptr<Item> create(int n){
	auto result = std::make_shared<Item>(Item());
	double v = n;
	result->write(&v, sizeof(v), ItemTypes::NUMBER);
	return result;	
}


std::shared_ptr<Item> infer(const std::string &input, Context *ctx, Cursor *cursor){
    // Number

    if(isNumber(input) && input.find(' ') == -1){

        auto result = std::make_shared<Item>(Item());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ItemTypes::NUMBER);
        return result;
    }else
    // String
    if(isString(input)){
        auto result = std::make_shared<StringType>(StringType());
        result->literal = input.substr(1, input.length() - 2);
        return result;        
    }else
	if(isList(input)){

		auto result = std::make_shared<ListType>(ListType());
		auto tokens = parse(input);
		std::vector<std::shared_ptr<Item>> list;
		for(int i = 0; i < tokens.size(); ++i){
			list.push_back(infer(tokens[i], ctx, cursor));
		}
		result->build(list);
		return result;
	}else{
		return eval(input, ctx, cursor);
	}
}

static bool isReserved(const std::string &word){
	for(unsigned i = 0; i < RESERVED_WORDS.size(); ++i){
		if(word == RESERVED_WORDS[i]){
			return true;
		}
	}
	return false;
}

static std::shared_ptr<Item> evalParams(std::vector<std::string> &tokens, int from, Context *ctx, Cursor *cursor){
	std::string input = "";
	for(int i = from; i < tokens.size(); ++i){
		input += tokens[i];
		if(i < tokens.size()-1){
			input += " ";
		}
	}
	return eval(input, ctx, cursor);
}

static std::vector<std::string> compileParams(std::vector<std::string> &tokens, int from){
	std::vector<std::string> compiled;
	for(int i = from; i < tokens.size(); ++i){
		compiled.push_back(tokens[i]);
	}
	return compiled;
}

static std::vector<std::string> getCleanTokens(std::string &input){
	auto tokens = parse(input);

    if(tokens.size() == 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
		auto trimmed = tokens[0].substr(1, tokens[0].length() - 2);
        return getCleanTokens(trimmed);
    }	

	return tokens;
}

static std::shared_ptr<Item> runFunction(std::shared_ptr<Item> &fn, const std::vector<std::string> &tokens, Context *ctx, Cursor *cursor){
	Context lctx(ctx);

	auto func = static_cast<FunctionType*>(fn.get());
	bool useParams = func->params.size() > 0;

	if(useParams && func->params.size() != tokens.size()){
		std::cout << "fn '" << "': expected [" << func->params.size() << "] arguments, received [" << tokens.size() << "]" << std::endl;
		return std::make_shared<Item>(Item());
	}

	std::vector<std::shared_ptr<Item>> operands;
	bool useFuncParams = func->params.size() >= tokens.size();
	for(int i = 0; i < tokens.size(); ++i){
		auto ev = infer(tokens[i], &lctx, cursor);
		// ev->name = std::to_string(i-1);
		if(useParams){
			// ev->name = func->params[i-1];
		}
		lctx.add(useFuncParams ?  func->params[i] : "__param"+std::to_string(i), ev);
		operands.push_back(ev);
	} 
	return func->lambda(operands, &lctx, cursor); 
}

static std::shared_ptr<Item> eval(const std::string &input, Context *ctx, Cursor *cursor){
	auto tokens = parse(input);


	// scope modifier
	std::string scopeMod = "";
	if(tokens[0].find(':') != -1 && tokens[0][0] != '[' && tokens[0][tokens[0].length()-1] != ']'){



		int fc = tokens[0].find(':');
		std::string varname = tokens[0].substr(0, fc);
		scopeMod = tokens[0].substr(fc+1, tokens[0].length()-fc-1);
		tokens[0] = varname;


	}

	auto imp = tokens[0];


    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);		
        auto obj = eval(imp, ctx, cursor);
		if(scopeMod.size() > 0){

			obj = obj->getProperty(scopeMod);

			if(obj->type == ItemTypes::FUNCTION){
				auto params = compileParams(tokens, 1);
				return runFunction(obj, params, ctx, cursor);
			}else{
				return obj;
			}
		}
		return obj;
    }
	


	// Reserved imperative
	if(isReserved(imp)){
		// NIL
		if(imp == "nil"){
			return std::make_shared<Item>(Item());
		}else
		// SET declares a variable in the curret scope
		if(imp == "set"){
			auto name =  tokens[1];
			auto val = eval(tokens[2], ctx, cursor);
			if(val->type == ItemTypes::INTERRUPT){
				auto interrupt = std::static_pointer_cast<InterruptType>(val);
				if(interrupt->intype == InterruptTypes::RET){
					val = interrupt->payload;
				}else{
					val = std::make_shared<Item>(Item());
				}
			}
			ctx->add(name, val);
			return val;
		}else
		// PROTO builds a prototype item: the other first class citizen of canvas (other one being list)
		if(imp == "proto"){
			auto proto = std::make_shared<ProtoType>(ProtoType());



			std::vector<std::string> names;
			std::vector<std::shared_ptr<Item>> values;


			for(unsigned i = 1; i < tokens.size(); ++i){

				auto &token = tokens[i];

				auto fc = token.find(":");

				std::string varname = token.substr(0, fc);
				std::string varvalue = token.substr(fc+1, token.length()-fc-1);


				names.push_back(varname);
				values.push_back(eval(varvalue, ctx, cursor));
			}

			proto->populate(names, values, ctx, cursor);	

			return proto;		
		}else
		// FUNCTION
		if(imp == "fn"){
            auto largs = tokens[1];
            if(largs.length() > 1 && largs[0] == '[' && largs[largs.length()-1] == ']'){
                largs = largs.substr(1, largs.length() - 2);
            }
            auto args = parse(largs);
            auto fn = std::make_shared<FunctionType>(FunctionType());
            fn->set(tokens[2], args);
            return fn;
		}else
		// RET
		if(imp == "ret"){
            auto interrupt = std::make_shared<InterruptType>(InterruptType(InterruptTypes::RET));
			// eval parameters
            if(tokens.size() > 1){
                interrupt->payload = evalParams(tokens, 1, ctx, cursor);
            }
            return std::static_pointer_cast<Item>(interrupt);  
		}else
		// SKIP (skips current execution ie inside a loop, next iteration [do true [[std:print test][skip]]]))
		if(imp == "skip"){
            auto interrupt = std::make_shared<InterruptType>(InterruptType(InterruptTypes::SKIP));
			// eval parameters
            if(tokens.size() > 1){
				interrupt->payload = evalParams(tokens, 1, ctx, cursor);
            }
            return std::static_pointer_cast<Item>(interrupt);  
		}else
		// STOP (stops current execution [do [cond] [[std:print test][stop]]])
		if(imp == "stop"){
            auto interrupt = std::make_shared<InterruptType>(InterruptType(InterruptTypes::STOP));
			// eval parameters
            if(tokens.size() > 1){
				interrupt->payload = evalParams(tokens, 1, ctx, cursor);	
            }
            return std::static_pointer_cast<Item>(interrupt);  
		}else
		// IF conditional statement ([if [cond] [branch-true][branch-false]])
		if(imp == "if"){

		}else
		// DO loop procedure ([do [cond] [code]...[code]])
		if(imp == "do"){

		}else
		// ITER goes through a list ([iter [1 2 3 4] [code]...[code]]) ([iter [proto a:1 b:2 c:3] [code]...[code]])
		if(imp == "iter"){
			if(tokens.size() < 3){
				return std::make_shared<Item>(Item());
			}
			auto iteratable = eval(tokens[1], ctx, cursor);
			int readFrom = 2;
			std::vector<std::string> names; // first is the name that one current item receives. for dictionaries, the second is always the name of the key

			if(tokens[2] == "as"){
				if(tokens.size() < 4){
					cursor->setError("iter: invalid number of parameters. Example: iter [1 2 3] as [i] [CODE]\nor iter [1 2 3] as [v, k] [CODE]");
					return std::make_shared<Item>(Item());					
				}
				names = getCleanTokens(tokens[3]);
				readFrom += 2;
			}


			if(iteratable->type != ItemTypes::LIST && iteratable->type != ItemTypes::PROTO){
				cursor->setError("iter requires an iterable as first parameter");
				return std::make_shared<Item>(Item());
			}

			std::string varName = names.size() > 0 ? names[0] : "";
			if(iteratable->type == ItemTypes::LIST && names.size() > 0){
				if(!isValidVarName(varName)){
					cursor->setError("iter: invalid name for iteration variable "+varName);
					return std::make_shared<Item>(Item());					
				}
			}

			std::string keyName = names.size() > 1 ? names[1] : ""; 
			if(iteratable->type == ItemTypes::PROTO && names.size() > 1){
				if(!isValidVarName(keyName)){
					cursor->setError("iter: invalid name for iteration key "+keyName);
					return std::make_shared<Item>(Item());					
				}
			}			

			switch(iteratable->type){
				case ItemTypes::PROTO: {
					auto proto = static_cast<ProtoType*>(iteratable.get());
				} break;
				case ItemTypes::LIST: {
					auto list = static_cast<ListType*>(iteratable.get());
					std::shared_ptr<ListType> last = std::make_shared<ListType>(ListType());
					for(int i = 0; i < list->list.size(); ++i){
						auto current = list->list[i];
						Context fctx(ctx);
						if(varName.size() > 0){
							fctx.add(varName, current);
						}
						auto item = evalParams(tokens, readFrom, &fctx, cursor);
						if(cursor->error){
							return std::make_shared<Item>(Item());
						}
						if(item->type == ItemTypes::INTERRUPT){
							auto interrupt = std::static_pointer_cast<InterruptType>(item);
							if(interrupt->intype == InterruptTypes::RET){
								last->add(interrupt->payload);
							}
						}else
						if(item->type == ItemTypes::FUNCTION){
							auto funct = std::static_pointer_cast<FunctionType>(item);
							std::vector<std::shared_ptr<Item>> params = { current };
							last->add(funct->lambda(params, &fctx, cursor));
						}else{
							last->add(item);
						}
					}
					return last;
				};				
			}

		}
	}else{
	// Defined symbol
		auto var = ctx->find(imp);
		if(var->type == ItemTypes::NUMBER || var->type == ItemTypes::PROTO || var->type == ItemTypes::LIST || var->type == ItemTypes::STRING){
			if(scopeMod.length() > 0){
				auto obj = var->getProperty(scopeMod);
				if(obj->type == ItemTypes::FUNCTION){
					auto params = compileParams(tokens, 1);
					auto result = runFunction(obj, params, ctx, cursor);
					if(result->type == ItemTypes::INTERRUPT){
						auto interrupt = std::static_pointer_cast<InterruptType>(result);
						if(interrupt->intype == InterruptTypes::RET){
							return interrupt->payload;
						}
					}else{
						return obj;
					}
				}else{
					return obj;
				}
			}
			return var;

		}else
		if(var->type == ItemTypes::FUNCTION){

			auto params = compileParams(tokens, 1);
			return runFunction(var, params, ctx, cursor);
		}else{
			return infer(input, ctx, cursor);
		}
	}

	return std::make_shared<Item>(Item());


}

static bool isRunning = true;
static Cursor cursor;
static Context ctx;

static void onExit(){
	isRunning = false;
	std::cout << ctx.str() << std::endl;
}

static void catchCtrlC(int s){
	std::cout << "[CTRL+C]" << std::endl;
	onExit();
}




int main(int argc, char* argv[]){
	auto printList = [](const std::vector<std::string> &in){
		for(int i = 0; i < in.size(); ++i){
			std::cout << "'" << in[i] << "'" << std::endl;
		}
	};


	signal(SIGINT, catchCtrlC);

	// printList(parse("MOCK [ a:4 b:5 c:[fn [][print 'lol']] ]"));


	ctx.add("+", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor){
			auto result = std::make_shared<Item>(Item());
			double v = 0;
			for(unsigned i = 0; i < operands.size(); ++i){
				if(operands[i]->type == ItemTypes::NUMBER){
					v += *static_cast<double*>(operands[i]->data);
				}
			}
			result->write(&v, sizeof(v), ItemTypes::NUMBER);
			return result;
		}, {}
	))); 

	ctx.add("-", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, Context *ctx, Cursor *cursor){
			auto result = std::make_shared<Item>(Item());
			double v = 0;
			for(unsigned i = 0; i < operands.size(); ++i){
				if(operands[i]->type == ItemTypes::NUMBER){
					v -= *static_cast<double*>(operands[i]->data);
				}
			}
			result->write(&v, sizeof(v), ItemTypes::NUMBER);
			return result;
		}, {}
	))); 

	// std::cout << eval("MOCK a:4 b:5", &ctx, &cursor)->str() << std::endl;
	// std::cout << eval("set a 5", &ctx, &cursor)->str() << std::endl;


	while(isRunning){
		std::cout << std::endl;
		std::string input;
		std::cout << "> ";
		std::getline (std::cin, input);
		// std::cout << std::endl;
		std::cout << eval(input, &ctx, &cursor)->str() << std::endl;
		
		// std::cout << eval("[1 2 3 4]", &ctx, &cursor)->str() << std::endl;
		// if(cursor.error){
		// 	std::cout << cursor.message << std::endl;
		// }

		// std::cout << eval("set b [fn [a][ret + a 1]]", &ctx, &cursor)->str() << std::endl;
		// std::cout << eval("set a [proto a:1 b:2]", &ctx, &cursor)->str() << std::endl;
		// std::cout << eval("a", &ctx, &cursor)->str() << std::endl;
		//std::cout <<
		//  eval("set a [proto z:15 b:[fn [a][ret + 25 a]]]", &ctx, &cursor);// << std::endl;
		// std::cout << eval("a:b 1", &ctx, &cursor)->str() << std::endl;
		// std::cout << eval("a 1", &ctx, &cursor)->str() << std::endl;


		// if(cursor.error){
		// 	std::cout << cursor.message << std::endl;
		// }

		// std::cout << eval("iter [1 2 3 4 5] [fn [set a [+ a 1]]]", &ctx, &cursor)->str() << std::endl;

		// std::cout << eval("+ 1 1", &ctx, &cursor)->str() << std::endl;
	}

	onExit();

	return 0;

}