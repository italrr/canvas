#include "cv.hpp"


namespace CV {
	namespace Tools {
		unsigned genItemCountId(){
			static std::mutex genMutex;
			static unsigned itemCountId = 1;
			genMutex.lock();
			auto id = ++itemCountId;
			genMutex.unlock();
			return id;
		}

		bool isNumber(const std::string &s){
			return (s.find_first_not_of( "-.0123456789") == std::string::npos);
		}

		double positive(double n){
			return n < 0 ? n * -1.0 : n;
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

		bool isLineComplete(const std::string &input){
			auto clean = Tools::removeExtraSpace(input);
			std::vector<std::string> tokens;
			int open = 0;
			bool comm = false;
			bool str = false;
			for(int i = 0; i < input.length(); ++i){
				char c = input[i];
				// ++c;
				if(c == '[' && !str) ++open;
				if(c == ']' && !str) --open;
				if(c == '#' && !str){
					comm = !comm;
					continue;
				}
				if(comm){
					continue;
				}     
				if(c == '\''){
					str = !str;
				}   				
			}	
			return open == 0;		
		}

		std::vector<std::string> parse(const std::string &input, std::string &error){
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
				if(c == '[' && !str) ++open;
				if(c == ']' && !str) --open;
				if(c == '#' && !str){
					comm = !comm;
					continue;
				}
				if(comm){
					continue;
				}        
				// escape characters
				if(str && c == '\\' && i < input.length()-1){
					char nc = input[i + 1];
					// new line
					if(nc == 'n'){
						i += 1;
						hold += "\n";
						continue;
					}
					// tab
					if(nc == 't'){
						i += 1;
						hold += "\t";
						continue;
					}	
					// slash
					if(nc == '\\'){
						i += 1;
						hold += "\\";
						continue;
					}
					// single quote
					if(nc == '\''){
						i += 1;
						hold += "'";
						continue;
					}														
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
				error = "Bad parenthesis";
				return {};
			}
			return tokens;
		}

		int getModifiers(const std::string &input, std::unordered_map<std::string, std::string> &mods){

			int i = input.size()-1;
			int lastFound = i;
			std::string hold;
			while(i > 0){
				char c = input[i];
				
				if(c == ']'){
					break;
				}else
				if(c == ':' || c == '~'){
					mods[std::string()+c] = std::string(hold.rbegin(), hold.rend());
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

		std::vector<std::string> getCleanTokens(std::string &input, std::string &error){
			auto tokens = Tools::parse(input, error);

			if(error.size() > 0){
				return {};
			}

			if(tokens.size() == 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
				auto trimmed = tokens[0].substr(1, tokens[0].length() - 2);
				return getCleanTokens(trimmed, error);
			}	

			return tokens;
		}
	}
}

CV::Item::Item(){
	type = ItemTypes::NIL;
	data = NULL;
	size = 0;
	id = Tools::genItemCountId();	
	name = ""; // anonymous	
}

void CV::Item::clear(){	
	if(type != ItemTypes::NIL){
		free(data);
	}
	type = ItemTypes::NIL;
	data = NULL;
	size = 0;
}

void CV::Item::write(void *data, size_t size, int type){
	if(this->data != NULL){
		clear();
	}
	this->data = malloc(size);
	this->size = size;
	this->type = type;
	memcpy(this->data, data, size);
}

std::shared_ptr<CV::Item> CV::Item::copy(){
	auto r = std::make_shared<Item>(Item());
	r->type = type;
	r->size = size;
	r->data = malloc(size);
	memcpy(r->data, data, size);
	return r;
}

void CV::Item::registerProperty(const std::string &name, const std::shared_ptr<Item> &v){
	this->members[name] = v;
}

void CV::Item::deregisterProperty(const std::string &name){
	this->members.erase(name);
}	

std::shared_ptr<CV::Item> CV::Item::findProperty(const std::string &name){
	auto it = this->members.find(name);
	if(it == this->members.end()){
		return this->head.get() == NULL ? std::make_shared<CV::Item>(CV::Item()) : head->findProperty(name);
	}
	return it->second;		
}	

CV::Item *CV::Item::findContext(const std::string &name){
	auto it = this->members.find(name);
	if(it == this->members.end()){
		return this->head.get() == NULL ? NULL : this->head->findContext(name);
	}
	return this;
}

std::shared_ptr<CV::Item> CV::Item::findAndSetProperty(const std::string &name, const std::shared_ptr<CV::Item> &v){
	auto ctx = this->findContext(name);
	if(ctx == this || ctx == NULL) return std::make_shared<CV::Item>(CV::Item());
	ctx->members[name] = v;
	v->name = name;
	return ctx->members[name];	
}		

std::shared_ptr<CV::Item> CV::Item::getProperty(const std::string &name){
	auto it = this->members.find(name);
	if(it == this->members.end()){
		return std::make_shared<Item>(Item());
	}
	return it->second;
}

std::string CV::Item::str(bool singleLine) const {
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

CV::Item::operator std::string() const {
	return str();
}	

CV::NumberType::NumberType(){
	this->type = ItemTypes::NUMBER;
	this->data = new double(0);
}

CV::NumberType::~NumberType(){
	this->clear();
}

void CV::NumberType::set(int n){
	double _n = (double)n;
	memcpy(this->data, &_n, sizeof(_n));
}

void CV::NumberType::set(float n){
	double _n = (double)n;
	memcpy(this->data, &_n, sizeof(_n));
}

void CV::NumberType::set(double n){
	double _n = (double)n;
	memcpy(this->data, &_n, sizeof(_n));
}

double CV::NumberType::get(){
	double n;
	memcpy(&n, this->data, sizeof(n));
	return n;
}

std::shared_ptr<CV::Item> CV::NumberType::copy(){
	auto copy = std::make_shared<NumberType>(NumberType());
	copy->set(get());
	return std::static_pointer_cast<Item>(copy);
}	

std::string CV::NumberType::str(bool singleLine) const {
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


CV::StringType::StringType(){
	this->type = ItemTypes::STRING;
	this->literal = "";
	this->registerProperty("length", create(0));
}
std::shared_ptr<CV::Item> CV::StringType::copy(){
	auto copy = std::shared_ptr<CV::StringType>(new CV::StringType());
	copy->literal = this->literal;
	return std::static_pointer_cast<Item>(copy);
}

std::shared_ptr<CV::Item> CV::StringType::get(int index){
	if(index < 0 || index >= this->literal.size()){
		return std::make_shared<CV::Item>(CV::Item());
	}
	auto result = std::make_shared<CV::StringType>(CV::StringType());	
	result->set(std::string()+this->literal[index]);
	return result;
}

void CV::StringType::set(const std::string &v){
	this->literal = v;
	std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)this->literal.size());
}	

std::string CV::StringType::str(bool singleLine) const {
	switch(type){
		case ItemTypes::STRING: {
			return this->literal;
		} break;
		case ItemTypes::NIL: {
			return "nil";
		} break;
		default: 
			// TODO: HANDLE ERROR
			std::exit(1);
	}		
}


CV::ListType::ListType(int n){
	type = ItemTypes::LIST;
	for(int i = 0; i < n; ++i){
		this->list.push_back(create(0));
	}
	this->registerProperty("length", create((int)this->list.size()));
}
void CV::ListType::build(const std::vector<std::shared_ptr<CV::Item>> &objects){
	this->list = objects;
	std::static_pointer_cast<CV::NumberType>(this->getProperty("length"))->set((int)this->list.size());
}
void CV::ListType::add(const std::shared_ptr<CV::Item> &item){
	this->list.push_back(item);
	std::static_pointer_cast<CV::NumberType>(this->getProperty("length"))->set((int)this->list.size());
}
std::shared_ptr<CV::Item> CV::ListType::get(int index){
	if(index < 0 || index >= this->list.size()){
		return std::make_shared<Item>(Item());
	}
	return this->list[index];
}

std::shared_ptr<CV::Item> CV::ListType::copy(){
	return std::make_shared<CV::Item>(CV::Item());
}
std::string CV::ListType::str(bool singleLine) const {
	switch(type){
		case ItemTypes::LIST: {
			std::string _params;
			std::string buff = "";
			for(int i = 0; i < this->list.size(); ++i){
				buff += list[i]->str(false);
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



CV::FunctionType::FunctionType(){
	this->type = ItemTypes::FUNCTION;
	this->body = "";
	this->inner = false;
	this->lambda = [](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
		return std::make_shared<Item>(Item());
	};  
}
CV::FunctionType::FunctionType(const std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor)> &lambda,
			const std::vector<std::string> &params){
				this->type = ItemTypes::FUNCTION;
				this->lambda = lambda;
				this->params = params;
				this->inner = true;
			}

void CV::FunctionType::set(const std::string &body, const std::vector<std::string> &params){
	this->lambda = [body](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
		return eval(body, ctx, cursor);
	};    
	this->body = body;
	this->inner = false;
	this->params = params;
}

std::shared_ptr<CV::Item> CV::FunctionType::copy(){
	return std::make_shared<CV::Item>(CV::Item());
}

std::string CV::FunctionType::str(bool singleLine) const {
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




std::shared_ptr<CV::Item> CV::InterruptType::copy(){
	auto r = std::make_shared<InterruptType>();
	r->intype = intype;
	return std::static_pointer_cast<Item>(r);		
}

CV::InterruptType::InterruptType(int intype, std::shared_ptr<CV::Item> payload){
	this->type = ItemTypes::INTERRUPT;
	this->intype = intype;
	this->payload = payload;
}

std::string CV::InterruptType::str(bool singleLine) const {
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



CV::ProtoType::ProtoType(){
	this->type = ItemTypes::PROTO;
	this->registerProperty("length", create(0));
}

void CV::ProtoType::populate(const std::vector<std::string> &names, const std::vector<std::shared_ptr<Item>> &values, std::shared_ptr<Item> &ctx, Cursor *cursor){
	for(unsigned i = 0; i < names.size(); ++i){
		auto name = names[i];
		this->registerProperty(names[i], values[i]);
	}
	std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)members.size()-1);
}


void CV::ProtoType::add(const std::string &name, std::shared_ptr<Item> &item, std::shared_ptr<Item> &ctx, Cursor *cursor){ 
	this->registerProperty(name, item);
	std::static_pointer_cast<NumberType>(this->getProperty("length"))->set((int)members.size()-1);
}

std::vector<std::string> CV::ProtoType::getKeys(){
	std::vector<std::string> names;
	for(auto &it : this->members){
		if(it.first == "length"){
			continue;
		}
		names.push_back(it.first);
	}
	return names;
}


std::string CV::ProtoType::str(bool singleLine) const {
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


std::shared_ptr<CV::Item> CV::create(double n){
	auto result = std::make_shared<CV::Item>(CV::Item());
	double v = n;
	result->write(&v, sizeof(v), ItemTypes::NUMBER);
	return result;	
}

std::shared_ptr<CV::Item> CV::createContext(const std::shared_ptr<CV::Item> &head){
	auto result = std::make_shared<CV::Item>(CV::Item());
	result->type = CV::ItemTypes::CONTEXT;
	result->head = head;
	return result;	
}

std::shared_ptr<CV::ListType> CV::createList(int n){
	return std::make_shared<CV::ListType>(CV::ListType(n));	
}

std::shared_ptr<CV::ProtoType> CV::createProto(){
	return std::make_shared<CV::ProtoType>(CV::ProtoType());	
}

std::shared_ptr<CV::StringType> CV::createString(const std::string &str){
	auto result = std::make_shared<CV::StringType>(CV::StringType());	
	result->set(str);
	return result;
}




std::shared_ptr<CV::Item> CV::infer(const std::string &input, std::shared_ptr<CV::Item> &ctx, Cursor *cursor){
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
		auto result = createList(0);
		std::string _input = input;
		if(_input.length() > 1 && _input[0] == '[' && _input[_input.length()-1] == ']'){
			_input = _input.substr(1, _input.length() - 2);		
		}		
		std::string error;
		auto tokens = Tools::parse(_input, error);
		if(error.length() > 0){
			cursor->setError(error);
			return std::make_shared<Item>(Item());
		}
		for(int i = 0; i < tokens.size(); ++i){			
			auto item = eval(tokens[i], ctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}				
			result->add(item);
		}
		return result;
	}else{
		return std::make_shared<Item>(Item());
	}
}



static std::shared_ptr<CV::Item> evalParams(std::vector<std::string> &tokens, int from, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
	std::string input = "";
	for(int i = from; i < tokens.size(); ++i){
		input += tokens[i];
		if(i < tokens.size()-1){
			input += " ";
		}
	}
	return CV::eval(input, ctx, cursor);
}

static std::vector<std::string> compileParams(const std::vector<std::string> &tokens, int from){
	std::vector<std::string> compiled;
	for(int i = from; i < tokens.size(); ++i){
		compiled.push_back(tokens[i]);
	}
	return compiled;
}

static std::string compileParams(const std::vector<std::string> &tokens){
	std::string compiled;
	for(int i = 0; i < tokens.size(); ++i){
		compiled += tokens[i];
		if(i < tokens.size()-1) compiled += " ";
	}
	return compiled;
}

static std::shared_ptr<CV::Item> runFunction(std::shared_ptr<CV::Item> &fn, const std::vector<std::string> &tokens, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
	
	auto func = static_cast<CV::FunctionType*>(fn.get());
	bool useParams = func->params.size() > 0;

	if(useParams && func->params.size() != tokens.size()){
		// std::cout << "fn '" << "': expected [" << func->params.size() << "] operands, received [" << tokens.size() << "]" << std::endl;
		return std::make_shared<CV::Item>(CV::Item());
	}

	std::vector<std::shared_ptr<CV::Item>> operands;

	bool useFuncParams = func->params.size() >= tokens.size();
	for(int i = 0; i < tokens.size(); ++i){

		auto ev = eval(tokens[i], ctx, cursor);

		if(cursor->error){
			return std::make_shared<CV::Item>(CV::Item());
		}	
		if(useParams){
			// ev->name = func->params[i-1];
		}
		ctx->registerProperty(useFuncParams ?  func->params[i] : "__param"+std::to_string(i), ev);
		operands.push_back(ev);
	} 
	auto result = func->lambda(operands, ctx, cursor); 
	if(cursor->error){
		return std::make_shared<CV::Item>(CV::Item());
	}	
	return result;	
}


std::shared_ptr<CV::Item> CV::eval(const std::string &input, std::shared_ptr<CV::Item> &ctx, CV::Cursor *cursor){
	if(input.length() == 0){
		cursor->setError("There's nothing here...");
		return std::make_shared<CV::Item>(Item()); 
	}
	std::string error;
	auto tokens = Tools::parse(input, error);
	if(error.size() > 0){
		cursor->setError(error);
		return std::make_shared<CV::Item>(Item());
	}

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

	// chained
	if(tokens.size() > 1 && tokens[0].length() > 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
		auto last = CV::createList(0);
		// chained are evaluated and results compiled into a single list
		for(int i = 0; i < tokens.size(); ++i){
			auto imp = tokens[i];
			auto obj = eval(imp, ctx, cursor);
			last->add(obj);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}				
		}
		return last;
	}else
	// single
	if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '[' && tokens[0][tokens[0].length()-1] == ']'){
		imp = tokens[0].substr(1, tokens[0].length() - 2);		
		auto obj = eval(imp, ctx, cursor);
		if(namer.size() > 0){
			obj->name = namer;
		}			
		if(cursor->error){
			return std::make_shared<Item>(Item());
		}			
		if(obj->type == ItemTypes::LIST && scopeMod.size() > 0 && Tools::isNumber(scopeMod)){
			auto list = std::static_pointer_cast<ListType>(obj);
			auto item = list->get(std::stod(scopeMod));
			if(namer.size() > 0){
				item->name = namer;
			}
			return item;
		}else
		if(obj->type == ItemTypes::STRING && scopeMod.size() > 0 && Tools::isNumber(scopeMod)){
			auto str = std::static_pointer_cast<StringType>(obj);
			auto item = str->get(std::stod(scopeMod));
			if(namer.size() > 0){
				item->name = namer;
			}					
			return item;
		}else
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
				if(namer.size() > 0){
					inobj->name = namer;
				}				
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
			val->name = name;
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
			std::string error;
			auto args = Tools::parse(largs, error);
			if(error.size() > 0){
				cursor->setError(error);
				return std::make_shared<Item>(Item());
			}
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
			if(tokens.size() < 3){
				cursor->setError("operator 'iter': expects at least 2 operands");
				return std::make_shared<Item>(Item());
			}
			if(tokens.size() > 4){
				cursor->setError("operator 'iter': no more than 3 operands");
				return std::make_shared<Item>(Item());
			}
			auto cond = eval(tokens[1], ctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}			
			bool t = cond->type == ItemTypes::NUMBER && *static_cast<double*>(cond->data) > 0;
			if(t){
				auto tctx = createContext(ctx);
				auto trueBranch = eval(tokens[2], tctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}
				return trueBranch->type == ItemTypes::INTERRUPT ? std::static_pointer_cast<InterruptType>(trueBranch)->payload : trueBranch;
			}else
			if(tokens.size() > 3){
				auto tctx = createContext(ctx);
				auto falseBranch = eval(tokens[3], tctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}				
				return falseBranch->type == ItemTypes::INTERRUPT ? std::static_pointer_cast<InterruptType>(falseBranch)->payload : falseBranch;
			}
		}else
		// for loop procedure ([for [start][cond][modifier] [code]...[code]])
		if(imp == "for"){
			if(tokens.size() < 5){
				cursor->setError("operator 'for': expects at least 5 operands");
				return std::make_shared<Item>(Item());
			}	
			bool t = false;
			auto ctctx = createContext(ctx);
			auto last = std::make_shared<Item>(Item());;

			// start
			auto part = eval(tokens[1], ctctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}				

			do {
				auto cond = eval(tokens[2], ctctx, cursor);
				if(cursor->error){
					return last;
				}		
				t = cond->type == ItemTypes::NUMBER && *static_cast<double*>(cond->data) > 0;
				if(t){
					auto ntokens = compileParams(compileParams(tokens, 4));
					auto run = eval(ntokens, ctctx, cursor);
					last = run;
					if(cursor->error){
						return last;
					}
					if(run->type == ItemTypes::INTERRUPT){
						return std::static_pointer_cast<InterruptType>(run)->payload;
					}
					auto modifier = eval(tokens[3], ctctx, cursor);
					if(cursor->error){
						return last;
					}							
				}
			} while(t);
			return last;
		}else		
		// DO loop procedure ([do [cond] [code]...[code]])
		if(imp == "do"){
			if(tokens.size() < 3){
				cursor->setError("operator 'do': expects at least 2 operands");
				return std::make_shared<Item>(Item());
			}	
			bool t = false;
			auto ctctx = createContext(ctx);
			auto last = std::make_shared<Item>(Item());;
			do {
				auto cond = eval(tokens[1], ctctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}		
				t = cond->type == ItemTypes::NUMBER && *static_cast<double*>(cond->data) > 0;
				if(t){
					auto tctx = createContext(ctctx);
					auto ntokens = compileParams(compileParams(tokens, 2));
					auto run = eval(ntokens, tctx, cursor);
					last = run;
					if(cursor->error){
						return last;
					}
					if(run->type == ItemTypes::INTERRUPT){
						return std::static_pointer_cast<InterruptType>(run)->payload;
					}
				}
			} while(t);
			return last;
		}else
		// ITER goes through a list ([iter [1 2 3 4] [code]...[code]]) ([iter [proto a:1 b:2 c:3] [code]...[code]])
		if(imp == "iter"){
			if(tokens.size() < 3){
				cursor->setError("operator 'iter': expects at least 2 operands");
				return std::make_shared<Item>(Item());
			}
			auto iteratable = eval(tokens[1], ctx, cursor);

			// std::cout << "XD " << tokens[1] << " " << iteratable->type << " " << iteratable->str() << " '" << iteratable->name << "'" << std::endl;

			if(cursor->error){
				return std::make_shared<Item>(Item());
			}

			int readFrom = 2;

			if(iteratable->type != ItemTypes::LIST && iteratable->type != ItemTypes::PROTO){
				cursor->setError("operator 'iter': expects an iterable as first operands");
				return std::make_shared<Item>(Item());
			}
	
			switch(iteratable->type){
				case ItemTypes::PROTO: {
					auto proto = static_cast<ProtoType*>(iteratable.get());
					std::shared_ptr<ListType> last = std::make_shared<ListType>(ListType());
					auto names = proto->getKeys();
					for(int i = 0; i < names.size(); ++i){
						auto current = proto->members[names[i]];
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
							}else
							if(interrupt->intype == InterruptTypes::SKIP){
								last->add(interrupt->payload);
								continue;
							}else
							if(interrupt->intype == InterruptTypes::STOP){
								last->add(interrupt->payload);
								return last;
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
							}else
							if(interrupt->intype == InterruptTypes::SKIP){
								last->add(interrupt->payload);
								continue;
							}else
							if(interrupt->intype == InterruptTypes::STOP){
								last->add(interrupt->payload);
								return last;
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
		if(var->type == ItemTypes::LIST && scopeMod.length() > 0 && Tools::isNumber(scopeMod)){
			auto list = std::static_pointer_cast<ListType>(var);
			auto item = list->get(std::stod(scopeMod));
			if(namer.size() > 0){
				item->name = namer;
			}					
			return item;
		}else
		if(var->type == ItemTypes::STRING && scopeMod.length() > 0 && Tools::isNumber(scopeMod)){
			auto str = std::static_pointer_cast<StringType>(var);
			auto item = str->get(std::stod(scopeMod));
			if(namer.size() > 0){
				item->name = namer;
			}					
			return item;
		}else		
		if(var->type == ItemTypes::NUMBER || var->type == ItemTypes::LIST || var->type == ItemTypes::PROTO || var->type == ItemTypes::STRING){
			if(scopeMod.length() == 0 && tokens.size() > 1){ // perhaps this is part of a list
				auto params = compileParams(tokens);
				auto v = infer(params, ctx, cursor);
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}
				return v;
			}
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

			if(namer.size() > 0){
				result->name = namer;
			}				
			return result;			
		}else{
			auto params = compileParams(tokens);
			auto v = infer(params, ctx, cursor);
			if(cursor->error){
				return std::make_shared<Item>(Item());
			}else
			if(v->type == ItemTypes::LIST && scopeMod.length() > 0 && Tools::isNumber(scopeMod)){
				auto list = std::static_pointer_cast<ListType>(v);
				auto item = list->get(std::stod(scopeMod));
				if(namer.size() > 0){
					item->name = namer;
				}					
				return item;
			}else
			if(v->type == ItemTypes::STRING && scopeMod.length() > 0 && Tools::isNumber(scopeMod)){
				auto str = std::static_pointer_cast<StringType>(v);
				auto item = str->get(std::stod(scopeMod));
				if(namer.size() > 0){
					item->name = namer;
				}					
				return item;
			}else{
				if(cursor->error){
					return std::make_shared<Item>(Item());
				}
				if(namer.size() > 0){
					v->name = namer;
				}
				return v;
			}
		}
	}


	return std::make_shared<Item>(Item());


}




static bool doTypesMatch(const std::vector<std::shared_ptr<CV::Item>> &items){
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

static bool doSizesMatch(const std::vector<std::shared_ptr<CV::Item>> &items){
	int lastSize = -1;
	for(int i = 0; i < items.size(); ++i){
		auto item = std::static_pointer_cast<CV::ListType>(items[i]);
		if(lastSize != -1 && item->list.size() != lastSize){
			return false;
		}
		lastSize = item->list.size();
	}
	return true;
}

static int isThereNil(const std::vector<std::shared_ptr<CV::Item>> &items){
	for(int i = 0; i < items.size(); ++i){
		auto item = items[i];
		if(item->type == CV::ItemTypes::NIL){
			return i;
		}
	}
	return -1;
}

std::string CV::printContext(std::shared_ptr<CV::Item> &ctx, bool ignoreInners){
	std::string output = "CONTEXT\n";

	int fcol = 5;
	int scol = 5;

	for(auto &it : ctx->members){
		const auto item = it.second;
		if(ignoreInners && item->type == CV::ItemTypes::FUNCTION && std::static_pointer_cast<CV::FunctionType>(item)->inner){
			continue;
		}
		while(it.first.length() > fcol){
			fcol += 5;
		}
	}

	for(auto &it : ctx->members){
		const auto item = it.second;
		if(ignoreInners && item->type == CV::ItemTypes::FUNCTION && std::static_pointer_cast<CV::FunctionType>(item)->inner){
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
		output += it.second->type == CV::ItemTypes::STRING ?  it.second->str(true)+"\n" : "'"+it.second->str(true)+"'\n";
	}

	return output;
}



void CV::registerEmbeddedOperators(std::shared_ptr<CV::Item> &ctx){
	/* 

		ADDITION

	*/
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
				double v = *static_cast<double*>(operands[0]->data);
				for(unsigned i = 1; i < operands.size(); ++i){
					v += *static_cast<double*>(operands[i]->data);
				}
				result->write(&v, sizeof(v), ItemTypes::NUMBER);
				return result;
			}else
			if(operands[0]->type == ItemTypes::LIST){
				if(!doSizesMatch(operands)){
					cursor->setError("operator '+': lists of mismatching sizes");
					return std::make_shared<Item>(Item());						
				}
				auto firstItem = std::static_pointer_cast<ListType>(operands[0]);

				if(firstItem->list.size() == 0){
					cursor->setError("operator '+': empty lists");
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

	/*
	
		SUBSTRACTION

	*/
	ctx->registerProperty("-", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator '-': no operands provided");
				return std::make_shared<Item>(Item());						
			}
			int nilInd = isThereNil(operands);
			if(nilInd != -1){
				cursor->setError("operator '-': argument("+std::to_string(nilInd+1)+") is nil");
				return std::make_shared<Item>(Item());				
			}
			if(!doTypesMatch(operands)){
				cursor->setError("operator '-': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::NUMBER){
				auto result = std::make_shared<Item>(Item());
				double v = *static_cast<double*>(operands[0]->data);
				for(unsigned i = 1; i < operands.size(); ++i){
					v -= *static_cast<double*>(operands[i]->data);
				}
				result->write(&v, sizeof(v), ItemTypes::NUMBER);
				return result;
			}else
			if(operands[0]->type == ItemTypes::LIST){
				if(!doSizesMatch(operands)){
					cursor->setError("operator '-': lists of mismatching sizes");
					return std::make_shared<Item>(Item());						
				}
				auto firstItem = std::static_pointer_cast<ListType>(operands[0]);

				if(firstItem->list.size() == 0){
					cursor->setError("operator '-': empty lists");
					return std::make_shared<Item>(Item());						
				}

				auto result = std::make_shared<ListType>(ListType( firstItem->list.size() ));
				for(unsigned i = 0; i < operands.size(); ++i){
					auto list = std::static_pointer_cast<ListType>(operands[i]);
					for(unsigned j = 0; j < list->list.size(); ++j){
						auto from = list->list[j];
						auto to = result->list[j];
						double v = *static_cast<double*>(from->data) - *static_cast<double*>(to->data);
						to->write(&v, sizeof(v), ItemTypes::NUMBER);
					}
				}
				return std::static_pointer_cast<Item>(result);
			}else{
				cursor->setError("operator '-': invalid type");
				return std::make_shared<Item>(Item());				
			}		
			
			return std::make_shared<Item>(Item());				
		}, {}
	))); 	

	/*
	
		MULTIPLICATION

	*/
	ctx->registerProperty("*", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator '*': no operands provided");
				return std::make_shared<Item>(Item());						
			}
			int nilInd = isThereNil(operands);
			if(nilInd != -1){
				cursor->setError("operator '*': argument("+std::to_string(nilInd+1)+") is nil");
				return std::make_shared<Item>(Item());				
			}
			if(!doTypesMatch(operands)){
				cursor->setError("operator '*': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::NUMBER){
				auto result = std::make_shared<Item>(Item());
				double v = *static_cast<double*>(operands[0]->data);
				for(unsigned i = 1; i < operands.size(); ++i){
					v *= *static_cast<double*>(operands[i]->data);
				}
				result->write(&v, sizeof(v), ItemTypes::NUMBER);
				return result;
			}else
			if(operands[0]->type == ItemTypes::LIST){
				if(!doSizesMatch(operands)){
					cursor->setError("operator '*': lists of mismatching sizes");
					return std::make_shared<Item>(Item());						
				}
				auto firstItem = std::static_pointer_cast<ListType>(operands[0]);

				if(firstItem->list.size() == 0){
					cursor->setError("operator '*': empty lists");
					return std::make_shared<Item>(Item());						
				}

				auto result = std::make_shared<ListType>(ListType( firstItem->list.size() ));
				for(unsigned j = 0; j < firstItem->list.size(); ++j){
					double v = *static_cast<double*>(firstItem->list[j]->data);
					auto to = result->list[j];
					to->write(&v, sizeof(v), ItemTypes::NUMBER);
				}

				for(unsigned i = 1; i < operands.size(); ++i){
					auto list = std::static_pointer_cast<ListType>(operands[i]);
					for(unsigned j = 0; j < list->list.size(); ++j){
						auto from = list->list[j];
						auto to = result->list[j];
						double v = *static_cast<double*>(from->data) * *static_cast<double*>(to->data);
						to->write(&v, sizeof(v), ItemTypes::NUMBER);
					}
				}
				return std::static_pointer_cast<Item>(result);
			}else{
				cursor->setError("operator '*': invalid type");
				return std::make_shared<Item>(Item());				
			}		
			
			return std::make_shared<Item>(Item());				
		}, {}
	))); 

	/*
	
		DIVISION

	*/
	ctx->registerProperty("/", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator '/': no operands provided");
				return std::make_shared<Item>(Item());						
			}
			int nilInd = isThereNil(operands);
			if(nilInd != -1){
				cursor->setError("operator '/': argument("+std::to_string(nilInd+1)+") is nil");
				return std::make_shared<Item>(Item());				
			}
			if(!doTypesMatch(operands)){
				cursor->setError("operator '/': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::NUMBER){
				auto result = std::make_shared<Item>(Item());
				double v = *static_cast<double*>(operands[0]->data);
				for(unsigned i = 1; i < operands.size(); ++i){
					v /= *static_cast<double*>(operands[i]->data);
				}
				result->write(&v, sizeof(v), ItemTypes::NUMBER);
				return result;
			}else
			if(operands[0]->type == ItemTypes::LIST){
				if(!doSizesMatch(operands)){
					cursor->setError("operator '/': lists of mismatching sizes");
					return std::make_shared<Item>(Item());						
				}
				auto firstItem = std::static_pointer_cast<ListType>(operands[0]);

				auto result = std::make_shared<ListType>(ListType( firstItem->list.size() ));
				for(unsigned j = 0; j < firstItem->list.size(); ++j){
					double v = *static_cast<double*>(firstItem->list[j]->data);
					auto to = result->list[j];
					to->write(&v, sizeof(v), ItemTypes::NUMBER);
				}

				for(unsigned i = 1; i < operands.size(); ++i){
					auto list = std::static_pointer_cast<ListType>(operands[i]);
					for(unsigned j = 0; j < list->list.size(); ++j){
						auto from = list->list[j];
						auto to = result->list[j];
						double v = *static_cast<double*>(from->data) / *static_cast<double*>(to->data);
						to->write(&v, sizeof(v), ItemTypes::NUMBER);
					}
				}
				return std::static_pointer_cast<Item>(result);
			}else{
				cursor->setError("operator '/': invalid type");
				return std::make_shared<Item>(Item());				
			}		
			
			return std::make_shared<Item>(Item());				
		}, {}
	))); 	

	/*
	
		eq (==)

	*/
	ctx->registerProperty("eq", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'eq': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'eq': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				cursor->setError("operator 'eq': can't compare LIST or PROTOTYPE");
				return std::make_shared<Item>(Item());
			}else
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last !=  *static_cast<double*>(operands[i]->data)){
						return create(0);
					}
				}				
				return create(1);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last != static_cast<StringType*>(operands[i].get())->literal){
						return create(0);
					}
				}				
				return create(1);
			}else{
				cursor->setError("operator 'eq': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));	

	/*
	
		neq (==)

	*/
	ctx->registerProperty("neq", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'neq': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'neq': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				cursor->setError("operator 'neq': can't compare LIST or PROTOTYPE");
				return std::make_shared<Item>(Item());
			}else
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last !=  *static_cast<double*>(operands[i]->data)){
						return create(1);
					}
				}				
				return create(0);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last != static_cast<StringType*>(operands[i].get())->literal){
						return create(1);
					}
				}				
				return create(0);
			}else{
				cursor->setError("operator 'neq': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));	

	/*
	
		gt (>)

	*/
	ctx->registerProperty("gt", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'gt': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'gt': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				int last = *static_cast<double*>(operands[0]->getProperty("length")->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last <= *static_cast<double*>(operands[i]->getProperty("length")->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->getProperty("length")->data);
				}				
				return create(1);
			}else		
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last <=  *static_cast<double*>(operands[i]->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->data);
				}				
				return create(1);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last.size() <= static_cast<StringType*>(operands[i].get())->literal.size()){
						return create(0);
					}
					last = static_cast<StringType*>(operands[i].get())->literal;
				}				
				return create(1);
			}else{
				cursor->setError("operator 'gt': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));		

	/*
	
		gte (>=)

	*/
	ctx->registerProperty("gte", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'gte': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'gte': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				int last = *static_cast<double*>(operands[0]->getProperty("length")->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last < *static_cast<double*>(operands[i]->getProperty("length")->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->getProperty("length")->data);
				}				
				return create(1);
			}else		
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last <  *static_cast<double*>(operands[i]->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->data);
				}				
				return create(1);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last.size() < static_cast<StringType*>(operands[i].get())->literal.size()){
						return create(0);
					}
					last = static_cast<StringType*>(operands[i].get())->literal;
				}				
				return create(1);
			}else{
				cursor->setError("operator 'gte': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));	

	/*
	
		lt (<)

	*/
	ctx->registerProperty("lt", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'lt': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'lt': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				int last = *static_cast<double*>(operands[0]->getProperty("length")->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last >= *static_cast<double*>(operands[i]->getProperty("length")->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->getProperty("length")->data);
				}				
				return create(1);
			}else		
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last >=  *static_cast<double*>(operands[i]->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->data);
				}				
				return create(1);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last.size() >= static_cast<StringType*>(operands[i].get())->literal.size()){
						return create(0);
					}
					last = static_cast<StringType*>(operands[i].get())->literal;
				}				
				return create(1);
			}else{
				cursor->setError("operator 'lt': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));		

	/*
	
		lte (<=)

	*/
	ctx->registerProperty("lte", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'lte': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'lte': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::LIST || operands[0]->type == ItemTypes::PROTO){
				int last = *static_cast<double*>(operands[0]->getProperty("length")->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last > *static_cast<double*>(operands[i]->getProperty("length")->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->getProperty("length")->data);
				}				
				return create(1);
			}else		
			if(operands[0]->type == ItemTypes::NUMBER){
				double last = *static_cast<double*>(operands[0]->data);
				for(int i = 1; i < operands.size(); ++i){
					if(last >  *static_cast<double*>(operands[i]->data)){
						return create(0);
					}
					last = *static_cast<double*>(operands[i]->data);
				}				
				return create(1);
			}else
			if(operands[0]->type == ItemTypes::STRING){
				std::string last = static_cast<StringType*>(operands[0].get())->literal;
				for(int i = 1; i < operands.size(); ++i){
					if(last.size() > static_cast<StringType*>(operands[i].get())->literal.size()){
						return create(0);
					}
					last = static_cast<StringType*>(operands[i].get())->literal;
				}				
				return create(1);
			}else{
				cursor->setError("operator 'lte': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));	

	/*
	
		..

	*/
	ctx->registerProperty("..", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator '..': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			if(operands.size() > 3){
				cursor->setError("operator '..': expects no more than 3 operands");
				return std::make_shared<Item>(Item());						
			}			

			// 

			if(!doTypesMatch(operands)){
				cursor->setError("operator '..': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type == ItemTypes::NUMBER){
				double s = *static_cast<double*>(operands[0]->data);
				double e = *static_cast<double*>(operands[1]->data);
				auto result = createList(0);
				if(s == e){
					return std::static_pointer_cast<Item>(result);
				}
				bool inc = e > s;
				double step = Tools::positive(operands.size() == 3 ? *static_cast<double*>(operands[2]->data) : 1);

				for(double i = s; (inc ? i <= e : i >= e); i += (inc ? step : -step)){
					result->add(create(i));
				}

				return std::static_pointer_cast<Item>(result);
			}else{
				cursor->setError("operator '..': unsupported operand type");
				return std::make_shared<Item>(Item());				
			}
			
			return std::make_shared<Item>(Item());				
		}, {}
	)));	

	/*
	
		splice

	*/
	ctx->registerProperty("splice", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			
			if(operands.size() < 2){
				cursor->setError("operator 'splice': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}			

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'splice': mismatching types");
				return std::make_shared<Item>(Item());
			}		

			if(operands[0]->type == ItemTypes::LIST){
				auto list = createList(0);
				for(int i = 0; i < operands.size(); ++i){
					auto operand = std::static_pointer_cast<ListType>(operands[i]);
					for(int j = 0; j < operand->list.size(); ++j){
						list->add(operand->list[j]);
					}
				}
				return std::static_pointer_cast<Item>(list);
			}else
			if(operands[0]->type == ItemTypes::PROTO){
				auto proto = createProto();
				for(int i = 0; i < operands.size(); ++i){
					auto operand = std::static_pointer_cast<ProtoType>(operands[i]);
					for(auto &it : operand->members){
						std::string name = it.first;
						proto->add(name, it.second, ctx, cursor);
					}
				}
				return std::static_pointer_cast<Item>(proto);
			}else{
				cursor->setError("operator 'splice': unsupported operand type");
				return std::make_shared<Item>(Item());					
			}

		}, {}

	)));	

	/*
	
		l-flatten

	*/
	ctx->registerProperty("l-flatten", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			
			if(operands.size() < 1){
				cursor->setError("operator 'l-flatten': expects at least 1 operand");
				return std::make_shared<Item>(Item());						
			}			

			if(!doTypesMatch(operands)){
				cursor->setError("operator 'l-flatten': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type != ItemTypes::LIST){
				cursor->setError("operator 'l-flatten': expects only lists");
				return std::make_shared<Item>(Item());
			}

			auto out = createList(0);


			std::function<void(const std::shared_ptr<Item> &obj)> recursive = [&](const std::shared_ptr<Item> &obj){
				if(obj->type == ItemTypes::LIST){
					auto list = std::static_pointer_cast<ListType>(obj);
					for(int i = 0; i < list->list.size(); ++i){
						recursive(list->list[i]);
					}
				}else{
					out->add(obj);
				}
			};

			for(int i = 0; i < operands.size(); ++i){
				recursive(operands[i]);
			}


			return std::static_pointer_cast<Item>(out);


		}, {}

	)));




	/*
	
		rset

	*/
	ctx->registerProperty("rset", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){

			if(operands.size() < 2){
				cursor->setError("operator 'rset': expects 2 operand");
				return std::make_shared<Item>(Item());						
			}			

			if(operands.size() > 2){
				cursor->setError("operator 'rset': expects no more than 2 operand");
				return std::make_shared<Item>(Item());						
			}	

			if(operands[0]->name.size() == 0){
				cursor->setError("operator 'rset': the provided variable doesn't have a name");
				return std::make_shared<Item>(Item());					
			}

			auto mutated = ctx->findAndSetProperty(operands[0]->name, operands[1]);


			if(mutated->type != ItemTypes::NIL){
				return mutated;				
			}else{
				return std::make_shared<Item>(Item());					
			}

		}, {}
	)));


	/*
	
		++

	*/
	ctx->registerProperty("++", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			
			if(operands.size() < 1){
				cursor->setError("operator '++': expects at least 1 operand");
				return std::make_shared<Item>(Item());						
			}			

			if(!doTypesMatch(operands)){
				cursor->setError("operator '++': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type != ItemTypes::NUMBER){
				cursor->setError("operator '++': expects only lists");
				return std::make_shared<Item>(Item());
			}

			for(int i = 0; i < operands.size(); ++i){
				auto item = operands[i];
				double v = *static_cast<double*>(item->data);
				v += 1;
				item->write(&v, sizeof(v), ItemTypes::NUMBER);
			}

			return operands[0];
		}, {}
	)));	

	/*
	
		--

	*/
	ctx->registerProperty("--", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			
			if(operands.size() < 1){
				cursor->setError("operator '--': expects at least 1 operand");
				return std::make_shared<Item>(Item());						
			}			

			if(!doTypesMatch(operands)){
				cursor->setError("operator '--': mismatching types");
				return std::make_shared<Item>(Item());
			}

			if(operands[0]->type != ItemTypes::NUMBER){
				cursor->setError("operator '--': expects only lists");
				return std::make_shared<Item>(Item());
			}

			for(int i = 0; i < operands.size(); ++i){
				auto item = operands[i];
				double v = *static_cast<double*>(item->data);
				v -= 1;
				item->write(&v, sizeof(v), ItemTypes::NUMBER);
			}

			return operands[0];
		}, {}
	)));

	/*
	
		noop

	*/
	ctx->registerProperty("noop", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			
			return std::make_shared<Item>(Item());

		}, {}
	)));		

	/*

		eq-ref
	
	*/
	ctx->registerProperty("eq-ref", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			if(operands.size() < 2){
				cursor->setError("operator 'eq-ref': expects at least 2 operands");
				return std::make_shared<Item>(Item());						
			}

			void *last = operands[0].get();
			for(int i = 1; i < operands.size(); ++i){
				if(static_cast<Item*>(last)->type != operands[i]->type || operands[i].get() != last){
					return create(0);
				}
			}

			return create(1);
			
		}, {}
	)));	


	/*

		quit
	
	*/
	ctx->registerProperty("quit", std::make_shared<FunctionType>(FunctionType([](const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor){
			std::exit(0);
			return std::make_shared<Item>(Item());				
		}, {}
	)));		

}        