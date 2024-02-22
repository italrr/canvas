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

	void printList(const std::vector<std::string> &in){
		for(int i = 0; i < in.size(); ++i){
			std::cout << "'" << in[i] << "'" << std::endl;
		}
	};

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
		INTERRUPT,
		CONTEXT
	};
}


namespace InterruptTypes {
	enum InterruptTypes : unsigned {
        STOP,
        SKIP,
        RET		
	};
}

namespace Tools {
	static bool isNumber(const std::string &s){
		return (s.find_first_not_of( "-.0123456789") == std::string::npos);
	}

	static bool isValidVarName(const std::string &s){
		return (s.substr(0, 1).find_first_not_of( "-.0123456789" ) != std::string::npos);
	}

	static bool isString(const std::string &s){
		return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
	}

	static std::vector<std::string> split(const std::string &str, const char sep){
		std::vector<std::string> tokens;
		std::size_t start = 0, end = 0;
		while ((end = str.find(sep, start)) != std::string::npos) {
			tokens.push_back(str.substr(start, end - start));
			start = end + 1;
		}
		tokens.push_back(str.substr(start));
		return tokens;
	}

	static bool isList(const std::string &s){

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

	static std::string removeExtraSpace(const std::string &in){
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

	static std::vector<std::string> parse(const std::string &input){
		auto clean = Tools::removeExtraSpace(input);
		std::vector<std::string> tokens;
		int open = 0;
		bool comm = false;
		bool str = false;
		bool mod = false;
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
			
			mod = !str && open == 0 && c == ']' && i < input.length()-1 && (input[i+1] == '~' || input[i+1] == ':');
			
			if(!str && open == 0){
				if((c == ']' && !mod) || c == ' ' || i ==  input.length()-1){
					if(hold != " "){
						tokens.push_back(Tools::removeExtraSpace(hold));
					}
					mod = false;
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

	static int getModifiers(const std::string &input, std::unordered_map<std::string, std::string> &mods){

		int i = input.size()-1;
		int lastFound = i;
		std::string hold;
		while(i > 0){
			char c = input[i];
			
			if(c == ']'){
				break;
			}else
			if(c == ':' || c == '~'){
				mods[std::string()+c] = hold;
				hold = "";
				--i;
				lastFound = i;
				continue;
			}
			
			hold += c;
			--i;
		}

		return lastFound;

	}

	static std::vector<std::string> getCleanTokens(std::string &input){
		auto tokens = Tools::parse(input);

		if(tokens.size() == 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
			auto trimmed = tokens[0].substr(1, tokens[0].length() - 2);
			return getCleanTokens(trimmed);
		}	

		return tokens;
	}
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

static unsigned genItemCountId(){
	static std::mutex genMutex;
	static unsigned itemCountId = 1;
	genMutex.lock();
	auto id = ++itemCountId;
	genMutex.unlock();
	return id;
}

struct Item {
	std::string name;
	unsigned id;
	void *data;
	unsigned size;
	unsigned type;
	std::unordered_map<std::string,std::shared_ptr<Item>> members;
	std::shared_ptr<Item> head;
	Item(){
		type = ItemTypes::NIL;
		data = NULL;
		size = 0;
		id = genItemCountId();	
		name = ""; // anonymous	
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

	void deregisterProperty(const std::string &name){
		this->members.erase(name);
	}	

	std::shared_ptr<Item> findProperty(const std::string &name){
		auto it = this->members.find(name);
		if(it == this->members.end()){
			return this->head.get() == NULL ? std::make_shared<Item>(Item()) : head->findProperty(name);
		}
		return it->second;		
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
			case ItemTypes::CONTEXT: {
				return "CONTEXT";
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

static std::shared_ptr<Item> create(int n);
static std::shared_ptr<Item> infer(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);
static std::shared_ptr<Item> eval(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);




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
	StringType(){
		this->type = ItemTypes::STRING;
		this->literal = "";
		this->registerProperty("length", create(0));
	}
	std::shared_ptr<Item> copy(){
		auto copy = std::shared_ptr<StringType>(new StringType());
		copy->literal = this->literal;
		return std::static_pointer_cast<Item>(copy);
	}
	
	void set(const std::string &v){
		this->literal = v;
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)this->literal.size());
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

struct ListType : Item {
	std::vector<std::shared_ptr<Item>> list;
	ListType(int n = 0){
		type = ItemTypes::LIST;
		for(int i = 0; i < n; ++i){
			this->list.push_back(create(0));
		}
		this->registerProperty("length", create((int)this->list.size()));
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
	std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor)> lambda;
	std::vector<std::string> params;
	std::string body;
	bool inner;
	FunctionType(){
		this->type = ItemTypes::FUNCTION;
		this->body = "";
		this->inner = false;
		this->lambda = [](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			return std::make_shared<Item>(Item());
		};  
	}
	FunctionType(const std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor)> &lambda,
				const std::vector<std::string> &params = {}){
					this->type = ItemTypes::FUNCTION;
					this->lambda = lambda;
					this->params = params;
					this->inner = true;
				}
	
	void set(const std::string &body, const std::vector<std::string> &params = {}){
		this->lambda = [body](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
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

	void populate(const std::vector<std::string> &names, const std::vector<std::shared_ptr<Item>> &values, std::shared_ptr<Item> &ctx, Cursor *cursor){
		for(unsigned i = 0; i < names.size(); ++i){
			auto name = names[i];
			this->registerProperty(names[i], values[i]);
		}
		std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)members.size()-1);
	}


	void add(const std::string &name, std::shared_ptr<Item> &item, std::shared_ptr<Item> &ctx, Cursor *cursor){ 
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

static std::shared_ptr<Item> create(int n){
	auto result = std::make_shared<Item>(Item());
	double v = n;
	result->write(&v, sizeof(v), ItemTypes::NUMBER);
	return result;	
}

static std::shared_ptr<Item> createContext(const std::shared_ptr<Item> &head = std::shared_ptr<Item>(NULL)){
	auto result = std::make_shared<Item>(Item());
	result->type = ItemTypes::CONTEXT;
	result->head = head;
	return result;	
}


std::shared_ptr<Item> infer(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor){
    if(Tools::isNumber(input) && input.find(' ') == -1){
        auto result = std::make_shared<Item>(Item());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ItemTypes::NUMBER);
        return result;
    }else
    // String
    if(Tools::isString(input)){
        auto result = std::make_shared<StringType>(StringType());
        result->literal = input.substr(1, input.length() - 2);
        return result;        
    }else
	if(Tools::isList(input)){
		auto result = std::make_shared<ListType>(ListType());
		auto tokens = Tools::parse(input);
		std::vector<std::shared_ptr<Item>> list;
		for(int i = 0; i < tokens.size(); ++i){
			auto item = infer(tokens[i], ctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}				
			list.push_back(item);
		}
		result->build(list);
		return result;
	}else{
		return std::make_shared<Item>(Item());
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

static std::shared_ptr<Item> evalParams(std::vector<std::string> &tokens, int from, std::shared_ptr<Item> &ctx, Cursor *cursor){
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

static std::shared_ptr<Item> runFunction(std::shared_ptr<Item> &fn, const std::vector<std::string> &tokens, std::shared_ptr<Item> &ctx, Cursor *cursor){
	
	auto func = static_cast<FunctionType*>(fn.get());
	bool useParams = func->params.size() > 0;

	if(useParams && func->params.size() != tokens.size()){
		std::cout << "fn '" << "': expected [" << func->params.size() << "] arguments, received [" << tokens.size() << "]" << std::endl;
		return std::make_shared<Item>(Item());
	}

	std::vector<std::shared_ptr<Item>> operands;

	bool useFuncParams = func->params.size() >= tokens.size();
	for(int i = 0; i < tokens.size(); ++i){
		auto ev = eval(tokens[i], ctx, cursor);
		if(cursor->error){
			return std::make_shared<Item>(Item());
		}	
		if(useParams){
			// ev->name = func->params[i-1];
		}
		ctx->registerProperty(useFuncParams ?  func->params[i] : "__param"+std::to_string(i), ev);
		operands.push_back(ev);
	} 
	auto result = func->lambda(operands, ctx, cursor); 
	if(cursor->error){
		return std::make_shared<Item>(Item());
	}	
	return result;	
}

static std::shared_ptr<Item> eval(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor){
	auto tokens = Tools::parse(input);

	auto imp = tokens[0];

	// find modifiers if any
	std::string scopeMod = "";
	std::string namer = "";
	if(imp.rfind(":") != -1 || imp.rfind("~") != -1){
		std::unordered_map<std::string, std::string> mods;
		int cutOff = Tools::getModifiers(imp, mods);

		imp = imp.substr(0, cutOff+1);
		tokens[0] = imp;
		if(mods.count(":") > 0){
			scopeMod = mods[":"];
		}
		if(mods.count("~") > 0){
			namer = mods["~"];	
		}		
	}

    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);		
        auto obj = eval(imp, ctx, cursor);
		if(cursor->error){
			return std::make_shared<Item>(Item());
		}			
		if(namer.size() > 0){
			obj->name = namer;
		}
		if(scopeMod.size() > 0){
			auto inobj = obj->getProperty(scopeMod);
			if(inobj->type == ItemTypes::FUNCTION){
				auto params = compileParams(tokens, 1);
				auto tctx = createContext(ctx);
				tctx->registerProperty("_", obj);
				auto result = runFunction(obj, params, tctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}					
				return result;
			}else{
				return inobj;
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
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}			
			if(val->type == ItemTypes::INTERRUPT){
				auto interrupt = std::static_pointer_cast<InterruptType>(val);
				if(interrupt->intype == InterruptTypes::RET){
					val = interrupt->payload;
				}else{
					val = std::make_shared<Item>(Item());
				}
			}
			ctx->registerProperty(name, val);
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
				auto result = eval(varvalue, ctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}					
				values.push_back(result);				
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
            auto args = Tools::parse(largs);
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
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}				
            }
            return std::static_pointer_cast<Item>(interrupt);  
		}else
		// SKIP (skips current execution ie inside a loop, next iteration [do true [[std:print test][skip]]]))
		if(imp == "skip"){
            auto interrupt = std::make_shared<InterruptType>(InterruptType(InterruptTypes::SKIP));
			// eval parameters
            if(tokens.size() > 1){
				interrupt->payload = evalParams(tokens, 1, ctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}					
            }
            return std::static_pointer_cast<Item>(interrupt);  
		}else
		// STOP (stops current execution [do [cond] [[std:print test][stop]]])
		if(imp == "stop"){
            auto interrupt = std::make_shared<InterruptType>(InterruptType(InterruptTypes::STOP));
			// eval parameters
            if(tokens.size() > 1){
				interrupt->payload = evalParams(tokens, 1, ctx, cursor);	
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}				
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
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}

			int readFrom = 2;

			if(iteratable->type != ItemTypes::LIST && iteratable->type != ItemTypes::PROTO){
				cursor->setError("iter requires an iterable as first parameter");
				return std::make_shared<Item>(Item());
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
						auto fctx = createContext(ctx);
						if(iteratable->name.size() > 0){
							fctx->registerProperty(iteratable->name, current);
						}
						auto item = evalParams(tokens, readFrom, fctx, cursor);
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
							last->add(funct->lambda(params, fctx, cursor));
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
		auto var = ctx->findProperty(imp);	
		if(var->type == ItemTypes::NUMBER || var->type == ItemTypes::PROTO || var->type == ItemTypes::LIST || var->type == ItemTypes::STRING){
			if(scopeMod.length() > 0){
				auto obj = var->getProperty(scopeMod);
				if(obj->type == ItemTypes::FUNCTION){
					auto params = compileParams(tokens, 1);
					auto tctx = createContext(ctx);
					tctx->registerProperty("_", var);
					auto result = runFunction(obj, params, tctx, cursor);
					if(cursor->error){
						return std::make_shared<Item>(Item());
					}
					if(result->type == ItemTypes::INTERRUPT){
						auto interrupt = std::static_pointer_cast<InterruptType>(result);
						if(interrupt->intype == InterruptTypes::RET){
							return interrupt->payload;
						}
					}else{	
						if(namer.size() > 0){
							obj->name = namer;
						}										
						return result;
					}
				}else{
					if(namer.size() > 0){
						obj->name = namer;
					}							
					return obj;
				}
			}
			if(namer.size() > 0){
				var->name = namer;
			}				
			return var;

		}else
		if(var->type == ItemTypes::FUNCTION){
			auto params = compileParams(tokens, 1);
			auto tctx = createContext(ctx);
			auto result = runFunction(var, params, tctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}
			return result;			
		}else{
			auto v = infer(input, ctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}
			if(namer.size() > 0){
				v->name = namer;
			}
			return v;
		}
	}

	return std::make_shared<Item>(Item());


}



static bool doTypesMatch(const std::vector<std::shared_ptr<Item>> &items){
	int lastType = -1;
	for(int i = 0; i < items.size(); ++i){
		auto item = items[i];
		if(lastType != -1 && item->type != lastType){
			return false;
		}
		lastType = item->type;
	}
	return true;
}

static bool doSizesMatch(const std::vector<std::shared_ptr<Item>> &items){
	int lastSize = -1;
	for(int i = 0; i < items.size(); ++i){
		auto item = std::static_pointer_cast<ListType>(items[i]);
		if(lastSize != -1 && item->list.size() != lastSize){
			return false;
		}
		lastSize = item->list.size();
	}
	return true;
}

static int isThereNil(const std::vector<std::shared_ptr<Item>> &items){
	for(int i = 0; i < items.size(); ++i){
		auto item = items[i];
		if(item->type == ItemTypes::NIL){
			return i;
		}
	}
	return -1;
}

static std::string printContext(std::shared_ptr<Item> &ctx, bool ignoreInners = true){
	std::string output = "CONTEXT\n";

	int fcol = 5;
	int scol = 5;

	for(auto &it : ctx->members){
		const auto item = it.second;
		if(ignoreInners && item->type == ItemTypes::FUNCTION && std::static_pointer_cast<FunctionType>(item)->inner){
			continue;
		}
		while(it.first.length() > fcol){
			fcol += 5;
		}
	}

	for(auto &it : ctx->members){
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
		output += it.second->type == ItemTypes::STRING ?  it.second->str(true)+"\n" : "'"+it.second->str(true)+"'\n";
	}

	return output;
}


static bool isRunning = true;
Cursor cursor;
std::shared_ptr<Item> ctx = createContext();

static void onExit(){
	isRunning = false;
	std::cout << printContext(ctx) << std::endl;
}

static void catchCtrlC(int s){
	std::cout << "[CTRL+C]" << std::endl;
	onExit();
}

int main(int argc, char* argv[]){




	signal(SIGINT, catchCtrlC);

	// printList(parse("MOCK [ a:4 b:5 c:[fn [][print 'lol']] ]"));


	ctx->registerProperty("+", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator '+': no operands provided");
				return std::make_shared<Item>(Item());						
			}
			int nilInd = isThereNil(operands);
			if(nilInd != -1){
				cursor->setError("operator '+': argument("+std::to_string(nilInd+1)+") is nil");
				return std::make_shared<Item>(Item());				
			}
			if(!doTypesMatch(operands)){
				cursor->setError("operator '+': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::NUMBER){
				auto result = std::make_shared<Item>(Item());
				double v = 0;
				for(unsigned i = 0; i < operands.size(); ++i){
					v += *static_cast<double*>(operands[i]->data);
				}
				result->write(&v, sizeof(v), ItemTypes::NUMBER);
				return result;
			}else
			if(operands[0]->type == ItemTypes::LIST){
				if(!doSizesMatch(operands)){
					cursor->setError("operator '+': summing lists of mismatching sizes");
					return std::make_shared<Item>(Item());						
				}
				auto firstItem = std::static_pointer_cast<ListType>(operands[0]);

				if(firstItem->list.size() == 0){
					cursor->setError("operator '+': summing empty lists");
					return std::make_shared<Item>(Item());						
				}

				auto result = std::make_shared<ListType>(ListType( firstItem->list.size() ));
				for(unsigned i = 0; i < operands.size(); ++i){
					auto list = std::static_pointer_cast<ListType>(operands[i]);
					for(unsigned j = 0; j < list->list.size(); ++j){
						auto from = list->list[j];
						auto to = result->list[j];
						double v = *static_cast<double*>(from->data) + *static_cast<double*>(to->data);
						to->write(&v, sizeof(v), ItemTypes::NUMBER);
					}
				}
				return std::static_pointer_cast<Item>(result);
			}else
			if(operands[0]->type == ItemTypes::STRING){

				std::string str = "";

				for(unsigned i = 0; i < operands.size(); ++i){
					auto current = std::static_pointer_cast<StringType>(operands[i]);
					str += current->literal;
				}

				auto result = std::make_shared<StringType>(StringType());
				result->set(str);


				return std::static_pointer_cast<Item>(result);
			}else{
				cursor->setError("operator '+': invalid type");
				return std::make_shared<Item>(Item());				
			}		
			
			return std::make_shared<Item>(Item());				
		}, {}
	))); 



	// std::cout << eval("MOCK a:4 b:5", &ctx, &cursor)->str() << std::endl;
	// std::cout << eval("set a 5", &ctx, &cursor)->str() << std::endl;


	// while(isRunning){
	// 	std::cout << std::endl;
	// 	std::string input;
	// 	std::cout << "> ";
	// 	std::getline (std::cin, input);
	// 	// std::cout << std::endl;
	// 	std::cout << eval(input, &ctx, &cursor)->str() << std::endl;
		
		// std::cout << eval("set a [proto a:1 b:2]", &ctx, &cursor)->str() << std::endl;
		// std::cout << eval("a", &ctx, &cursor)->str() << std::endl;		

		// eval("set a 25", ctx, &cursor);
		// std::cout << eval("a:z", &ctx, &cursor)->str() << std::endl;

		// eval("set j [1]", ctx, &cursor)->str();
		std::cout << eval("+ 2 2", ctx, &cursor)->str() << std::endl;
		if(cursor.error){
			std::cout << cursor.message << std::endl;
		}

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
	// }

	onExit();

	return 0;

}