#include <string.h>
#include "CV.hpp"

#define __CV_DEBUG 1
#define SHARED std::shared_ptr
#define VECTOR std::vector

#if __CV_DEBUG == 1
    #include <iostream>
#endif


static const std::string GENERIC_UNDEFINED_SYMBOL_ERROR = "Undefined symbol or operator/name within this context or above";
static bool useColorOnText = true;


/* -------------------------------------------------------------------------------------------------------------------------------- */
// Tools
namespace CV {
    namespace Tools {
        
        #if __CV_DEBUG == 1
        static void printList(const VECTOR<std::string> &list){
            for(unsigned i = 0; i < list.size(); ++i){
                std::cout << "'" << list[i] << "'" << std::endl;
            }
        }
        #endif

        static bool isNumber(const std::string &s){
            return (s.find_first_not_of( "-.0123456789") == std::string::npos) && (s != "-" && s != "--" && s != "+" && s != "++");
        }      

        static bool isString(const std::string &s){
            return s.size() > 1 && s[0] == '\'' && s[s.length()-1] == '\'';
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
        }     

        static std::string removeTrailingZeros(double d){
            size_t len = std::snprintf(0, 0, "%.8f", d);
            std::string s(len+1, 0);
            std::snprintf(&s[0], len+1, "%.8f", d);
            s.pop_back();
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if(s.back() == '.') {
                s.pop_back();
            }
            return s;
        }


        namespace Color {
            enum Color : int {
                BLACK,
                RED,
                GREEN,
                YELLOW,
                BLUE,
                MAGENTA,
                CYAN,
                WHITE,
                RESET
            };
        }

        namespace UnixColor {

            static const std::unordered_map<int, std::string> TextColorCodes = {
                {0, "\e[0;30m"},
                {1, "\e[0;31m"},
                {2, "\e[0;32m"},
                {3, "\e[0;33m"},
                {4, "\e[0;34m"},
                {5, "\e[0;35m"},
                {6, "\e[0;36m"},
                {7, "\e[0;37m"},
                {8, "\e[0m"},
            };

            static const std::unordered_map<int, std::string> BoldTextColorCodes = {
                {0, "\e[1;30m"},
                {1, "\e[1;31m"},
                {2, "\e[1;32m"},
                {3, "\e[1;33m"},
                {4, "\e[1;34m"},
                {5, "\e[1;35m"},
                {6, "\e[1;36m"},
                {7, "\e[1;37m"},
                {8, "\e[0m"},
            };        

            static const std::unordered_map<int, std::string> BackgroundColorCodes = {
                {0, "\e[40m"},
                {1, "\e[41m"},
                {2, "\e[42m"},
                {3, "\e[43m"},
                {4, "\e[44m"},
                {5, "\e[45m"},
                {6, "\e[46m"},
                {7, "\e[47m"},
                {8, "\e[0m"},
            };     
        }   

        static std::string setTextColor(int color, bool bold = false){
            return useColorOnText ? (bold ? UnixColor::BoldTextColorCodes.find(color)->second : UnixColor::TextColorCodes.find(color)->second) : "";
        }

        static std::string setBackgroundColor(int color){
            return useColorOnText ? (UnixColor::BackgroundColorCodes.find(color)->second) : "";

        }
    }
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Cursor Stuff

CV::Cursor::Cursor(){
    this->error = false;
    this->shouldExit = true;
}

void CV::Cursor::setError(const std::string &title, const std::string &message, unsigned line){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = line;
    this->subject = NULL;
    this->error = true;
    accessMutex.unlock();
}

void CV::Cursor::setError(const std::string &title, const std::string &message, unsigned line, CV::Item *subject){
    accessMutex.lock();
    this->title = title;
    this->message = message;
    this->line = line;
    this->subject = subject;
    this->error = true;
    accessMutex.unlock();
}

bool CV::Cursor::raise(){
    accessMutex.lock();
    if(!this->error){
        accessMutex.unlock();
        return false;
    }
    fprintf(stderr, "%s[Line #%i]%s %s: %s%s%s.\n",
            (
                CV::Tools::setTextColor(CV::Tools::Color::BLACK, true) +
                CV::Tools::setBackgroundColor(CV::Tools::Color::WHITE)
            ).c_str(),
            this->line,
            CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str(),
            this->title.c_str(),
            CV::Tools::setTextColor(CV::Tools::Color::RED, false).c_str(),
            this->message.c_str(),
            CV::Tools::setTextColor(CV::Tools::Color::RESET, false).c_str()
            

    );
    if(this->shouldExit){
        std::exit(1);
    }
    accessMutex.unlock();
    return true;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Inner stuff

static std::mutex idGeneratorMutex;
static unsigned lastGenId = 1;
// Generates unique ID for each item
static unsigned generateId(){
    idGeneratorMutex.lock();
    unsigned id = ++lastGenId;
    idGeneratorMutex.unlock();
    return id;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Types

// ITEM
CV::Item::Item(){
    type = CV::NaturalType::NIL;
    data = NULL;
    size = 0;
}

void CV::Item::clear(){
    if(this->type == CV::NaturalType::NIL){
        return;
    }
    if(data){
        free(data);
    }
    this->data = NULL;
    this->size = 0;
    this->type = CV::NaturalType::NIL;
}

// NUMBER
void CV::NumberType::set(__CV_DEFAULT_NUMBER_TYPE v){
    if(this->type == CV::NaturalType::NIL){
        clear();
        this->type = CV::NaturalType::NUMBER;
        this->data = malloc(sizeof(__CV_DEFAULT_NUMBER_TYPE));
        this->size = sizeof(__CV_DEFAULT_NUMBER_TYPE);
    }
    memcpy(this->data, &v, sizeof(__CV_DEFAULT_NUMBER_TYPE));
}

__CV_DEFAULT_NUMBER_TYPE CV::NumberType::get(){
    if(this->type == CV::NaturalType::NIL){
        fprintf(stderr, "Dereferenced NIL Item %i\n", this->id);
        std::exit(1);
    }
    __CV_DEFAULT_NUMBER_TYPE r;
    memcpy(&r, this->data, sizeof(__CV_DEFAULT_NUMBER_TYPE));
    return r;
}

// STRING
void CV::StringType::set(const std::string &v){
    clear();
    this->type = CV::NaturalType::STRING;
    this->data = malloc(v.length());
    this->size = v.length();
    for(int i = 0; i < v.size(); ++i){
        auto *c = static_cast<char*>((void*)((size_t)(this->data) + i));
        *c = v[i]; 
    }
}

std::string CV::StringType::get(){
    if(this->type == CV::NaturalType::NIL){
        fprintf(stderr, "Dereferenced NIL Item %i\n", this->id);
        std::exit(1);
    }
    std::string r;
    for(int i = 0; i < this->size; ++i){
        auto *c = static_cast<char*>((void*)((size_t)(this->data) + i));
        r += *c;
    }
    return r;
}

// LIST
void CV::ListType::build(unsigned n){
    clear();
    auto bsize = sizeof(void*) * n;
    this->size = n;
    this->data = malloc(bsize);
    memset(this->data, 0, bsize);
}

void CV::ListType::set(unsigned index, CV::Item *item){
    size_t target = ((size_t)this->data + index * sizeof(void*));
    memcpy((void*)(target), &item, sizeof(void*));
}

CV::Item *CV::ListType::get(unsigned index){
    size_t target = ((size_t)this->data + index * sizeof(void*));
    return static_cast<Item*>((void*)(target));
}

// FUNCTION

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Parser

static VECTOR<CV::Token> parseTokens(std::string input, char sep, SHARED<CV::Cursor> &cursor){
    VECTOR<CV::Token> tokens; 
    std::string buffer;
    int open = 0;
    bool onString = false;
    bool onComment = false;
    int leftBrackets = 0;
    int rightBrackets = 0;
    int cline = 0;
    int quotes = 0;
    // Count outer brackets
    for(int i = 0; i < input.size(); ++i){
        char c = input[i];
        if(c == '\n'){
            ++cline;
        }else
        if(c == '\''){
            onString = !onString;
            if(onString){
                ++quotes;
            }else{
                --quotes;
            }
        }else
        if(c == '[' && !onString){
            if(open == 0) ++leftBrackets;
            ++open;
        }else
        if(c == ']' && !onString){
            if(open == 1) ++rightBrackets;
            --open;
        }
    }
    onString = false;
    open = 0;
    // Throw mismatching brackets
    if(leftBrackets != rightBrackets){
        cursor->setError("Syntax Error", "Mismatching brackets", cline);
        return {};
    }
    // Throw mismatching quotes
    if(quotes != 0){
        cursor->setError("Syntax Error", "Mismatching quotes", cline);
        return {};
    }
    cline = 1;
    // Add missing brackets for simple statements
    if(((leftBrackets+rightBrackets)/2 > 1) || (leftBrackets+rightBrackets) == 0 || input[0] != '[' || input[input.length()-1] != ']'){
        input = "[" + input + "]";
    }
    // For debugging:
    // std::cout << "To Parse '" << input << "' " << leftBrackets+rightBrackets << std::endl;
    // Parse
    for(int i = 0; i < input.size(); ++i){
        char c = input[i];
        if(i < input.size()-1 && c == '\\' && input[i+1] == '\'' && onString){
            buffer += '\'';
            ++i;
            continue;
        }else            
        if(c == '\'' && !onComment){
            onString = !onString;
            buffer += c;
        }else        
        if(c == '#' && !onString){
            onComment = !onComment;
        }else
        if(i < input.size()-1 && c == '\\' && input[i+1] == 'n' && onString){
            buffer += '\n';
            ++i;
            continue;
        }else                
        if(c == '\n' && !onString){
            ++cline;
            onComment = false;
            continue;
        }else
        if(onComment){
            continue;
        }else
        if(c == '[' && !onString){
            if(open > 0){
                buffer += c;
            }
            ++open;
        }else
        if(c == ']' && !onString){
            if(open > 1){
                buffer += c;
            }            
            --open;
            if(open == 1 || i == input.size()-1 && buffer.length() > 0){
                if(buffer != ""){
                    tokens.push_back(CV::Token(buffer, cline));
                }
                buffer = "";
            }
        }else
        if(i == input.size()-1){
            if(buffer != ""){
                tokens.push_back(CV::Token(buffer+c, cline));
            }
            buffer = "";
        }else
        if(c == sep && open == 1 && !onString){
            if(buffer != ""){
                tokens.push_back(CV::Token(buffer, cline));
            }
            buffer = "";
        }else{
            buffer += c;
        }
    }

    return tokens;
};

#if __CV_DEBUG == 1
    static void printTokenList(const VECTOR<CV::Token> &list){
        for(unsigned i = 0; i < list.size(); ++i){
            std::cout << "'" << list[i].first << "' " << (int)list[i].solved << std::endl;
        }
    }
#endif

/* -------------------------------------------------------------------------------------------------------------------------------- */
// JIT

static CV::Instruction *compileTokens(const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor);
static CV::Instruction *interpretToken(const CV::Token &token, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, std::shared_ptr<CV::Context> &ctx, SHARED<CV::Cursor> &cursor);

static void DefConstructor(     std::shared_ptr<CV::Stack> &stack,
                                const std::string &name,
                                const std::function<CV::Instruction*(   
                                                                        const std::string &name,
                                                                        const VECTOR<CV::Token> &tokens,
                                                                        SHARED<CV::Stack> &stack,
                                                                        SHARED<CV::Context> &ctx,
                                                                        SHARED<CV::Cursor> &cursor
                                                                    )> &method
                          ){
    stack->constructors[name] = method;
}

static bool CheckConstructor(std::shared_ptr<CV::Stack> &stack, const std::string &name){
    return stack->constructors.find(name) != stack->constructors.end();
}

static CV::Instruction *interpretToken(const CV::Token &token, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, std::shared_ptr<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
    if(!token.solved){
        auto tokens = parseTokens(token.first, ' ', cursor);
        // Update line number
        for(int i = 0; i < tokens.size(); ++i){
            tokens[i].line = token.line;
        }
        return compileTokens(tokens, stack, ctx, cursor);
    }

    auto &imp = token.first;

    // Infere for natural types
    if(CV::Tools::isNumber(imp)){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto number = new CV::NumberType();
        number->set(std::stod(imp));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(number));
        return ins;
    }else
    if(CV::Tools::isString(imp)){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto string = new CV::StringType();
        string->set(imp.substr(1, imp.length() - 2));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(string));
        return ins;
    }else
    if(CheckConstructor(stack, imp)){
        return stack->constructors[imp](imp, tokens, stack, ctx, cursor);
    }else{
        if(ctx->check(imp)){
            auto pair = ctx->getIdByName(imp);
            if(pair.ctx == 0 || pair.id == 0){
                cursor->setError("Fatal Error referencing name '"+imp+"'", "Name was not found on this context or above", token.line);
                return stack->createInstruction(CV::InstructionType::NOOP, token);     
            }
            auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
            auto string = new CV::StringType();
            ins->data.push_back(pair.ctx);
            ins->data.push_back(pair.id);
            return ins;            
        }else{
            cursor->setError("Unexpected Error on '"+imp+"'", GENERIC_UNDEFINED_SYMBOL_ERROR, token.line);
        }
    }
    
    return stack->createInstruction(CV::InstructionType::NOOP, token);
}

static CV::Instruction* compileTokens(const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
    VECTOR<CV::Instruction*> ins;

    // Single token or simple imperative token
    if(tokens.size() == 1 || tokens.size() > 1 && tokens[0].solved){
        auto nins = interpretToken(tokens[0], tokens, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        ins.push_back(nins);
    }else
    // Chained complex tokens
    if(tokens.size() > 1 && !tokens[0].solved){
        for(int i = 0; i < tokens.size(); ++i){
            auto nins = interpretToken(tokens[i], tokens, stack, ctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }            
            if(i > 0){
                nins->previous = ins[ins.size()-1]->id;
                ins[ins.size()-1]->next = nins->id;
            }
            ins.push_back(nins);
        }
    }else{
    // Otherwise, this must be a list
        
        std::cout << "list" << std::endl;
    }

    return ins[0];
}

static void AddStandardConstructors(std::shared_ptr<CV::Stack> &stack){
    /*
    
        STANDARD CONSTRUCTORS

        Constructors are embedded instructions that enable a variety of fundamental features such
        as the ability to store a variable into memory (current context) using a name, mutate already existing variables,
        invoke interruptors, flow control, defining functions, and of course, native operators such as sum, sub, mult and div.

    */


    /*
        LET
    */
    DefConstructor(stack, "let", [](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3 || tokens.size() > 3){
            cursor->setError(name, "Expects exactly 2 arguments (set NAME VALUE)", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        
        // TODO: Check for valid variable names

        // Interpret data instruction
        auto dataIns = interpretToken(tokens[2], tokens, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }

        // Allocate promise in current context
        auto dataId = ctx->promise();
        ctx->setName(tokens[1].first, dataId);


        auto ins = stack->createInstruction(CV::InstructionType::LET, tokens[1]);

        ins->data.push_back(ctx->id);
        ins->data.push_back(dataId);
        ins->parameter.push_back(dataIns->id);

        return ins;
    });


    /*
        EMBEDDED OPERATORS (LOGIC & ARITHMETIC)
    */
    auto buildOp = [&](const std::string &name, unsigned TYPE){
        DefConstructor(stack, name, [TYPE](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
            if(tokens.size() < 3){
                cursor->setError(name, "Expects exactly 2 arguments", tokens[0].line);
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
            }

            auto result = stack->createInstruction(CV::InstructionType::REFERRED_PROXY, tokens[0]);

            auto data = new CV::NumberType();
            data->set(0);
            auto dataId = ctx->store(data);
            result->data.push_back(ctx->id);
            result->data.push_back(dataId);

            auto actuator = stack->createInstruction(TYPE, tokens[0]);
            actuator->data.push_back(ctx->id);
            actuator->data.push_back(dataId);
            for(int i = 1; i < tokens.size(); ++i){
                auto ins = interpretToken(tokens[i], tokens, stack, ctx, cursor);
                if(cursor->raise()){
                    return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                }            
                actuator->parameter.push_back(ins->id);
            }

            result->parameter.push_back(actuator->id);

            return result;
        });
    };   
    buildOp("+", CV::InstructionType::OP_NUM_ADD);
    buildOp("-", CV::InstructionType::OP_NUM_SUB);
    buildOp("*", CV::InstructionType::OP_NUM_MULT);
    buildOp("/", CV::InstructionType::OP_NUM_DIV);
    buildOp("eq", CV::InstructionType::OP_COND_EQ);
    buildOp("neq", CV::InstructionType::OP_COND_NEQ);
    buildOp("lt", CV::InstructionType::OP_COND_LT);
    buildOp("lte", CV::InstructionType::OP_COND_LTE);
    buildOp("gt", CV::InstructionType::OP_COND_GT);
    buildOp("gte", CV::InstructionType::OP_COND_GTE);
    
}


/* -------------------------------------------------------------------------------------------------------------------------------- */
// Context

CV::Context::Context(){
    top = NULL;
}

unsigned CV::Context::store(CV::Item *item){
    item->id = generateId();
    this->data[item->id] = item;
    return item->id;
}

unsigned CV::Context::promise(){
    auto id = generateId();
    this->data[id] = NULL;
    return id;
}

void  CV::Context::setPromise(unsigned id, CV::Item *item){
    if(this->data.count(id) == 0){
        fprintf(stderr, "Context %i: Closing Promise using non-existing ID %i\n", this->id, id);
        std::exit(1);        
    }
    this->data[id] = item;
}

CV::ContextDataPair CV::Context::getIdByName(const std::string &name){
    auto it = this->dataIds.find(name);
    if(it == this->dataIds.end()){
        return this->top ? this->top->getIdByName(name) : CV::ContextDataPair();
    }
    return CV::ContextDataPair(this->id, it->second);
}

void CV::Context::setName(const std::string &name, unsigned id){
    auto it = this->data.find(id);
    if(it == this->data.end()){
        fprintf(stderr, "Context %i: Naming non-existing Item %i\n", this->id, id);
        std::exit(1);
    }
    auto item = it->second;
    this->dataIds[name] = id;
}

bool CV::Context::check(const std::string &name){
    auto v = this->dataIds.count(name);
    if(v == 0){
        return this->top && this->top->check(name);
    }
    return true;
}

CV::Item *CV::Context::getByName(const std::string &name){
    auto it = this->dataIds.find(name);
    if(it == this->dataIds.end()){
        return this->top ? this->top->getByName(name) : NULL;
    }
    return this->data[it->second];
}

void CV::Context::clear(){
    for(auto &it : this->data){
        auto item = it.second;
        item->clear();
        free(item);
    }
    this->data.clear();
    this->dataIds.clear();
}

CV::Item *CV::Context::buildNil(){
    auto nil = new CV::Item();
    this->store(nil);
    return nil;
}

/* -------------------------------------------------------------------------------------------------------------------------------- */
// Stack Management

CV::Stack::Stack(){

}

CV::Instruction *CV::Stack::createInstruction(unsigned type, const CV::Token &token){
    auto *ins = new CV::Instruction();
    ins->id = generateId();
    ins->type = type;
    ins->token = token;
    this->instructions[ins->id] = ins;
    return ins;
}

std::shared_ptr<CV::Context> CV::Stack::createContext(CV::Context *top){
    auto ctx = std::make_shared<CV::Context>();
    ctx->id = generateId();
    ctx->top = top;
    this->contexts[ctx->id] = ctx;
    return ctx;
}

static CV::Item *__execute(CV::Stack *stack, CV::Instruction *ins, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor){
    switch(ins->type){
        case CV::InstructionType::INVALID: {
            fprintf(stderr, "Instruction ID %i of invalid type\n", ins->id);
            std::exit(1);
        } break;
        case CV::InstructionType::NOOP: {
            return ctx->buildNil();
        };

        /*
            CONSTRUCTRORS 
        */
        case CV::InstructionType::LET: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];

            auto v = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }

            stack->contexts[ctxId]->setPromise(dataId, v);

            return v;
        };
        case CV::InstructionType::MUT: {

        };

        /*
            EMBEDDED OPERATORS (LOGIC & ARITHMETIC)
            TODO: Consider SIMD for extra fastiness 🥵
        */        
        case CV::InstructionType::OP_NUM_ADD: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) += static_cast<CV::NumberType*>(v)->get();
            }
            return NULL;
        };
        case CV::InstructionType::OP_NUM_SUB: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = static_cast<CV::NumberType*>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) -= static_cast<CV::NumberType*>(v)->get();
            }
            return NULL;
        };
        case CV::InstructionType::OP_NUM_MULT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = static_cast<CV::NumberType*>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) *= static_cast<CV::NumberType*>(v)->get();
            }
            return NULL;
        };
        case CV::InstructionType::OP_NUM_DIV: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = static_cast<CV::NumberType*>(firstv)->get();
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto cins = stack->instructions[ins->parameter[i]];
                auto v = stack->execute(cins, ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }
                auto operand = static_cast<CV::NumberType*>(v)->get();
                if(operand == 0){
                    cursor->setError("Logic Error At '"+ins->token.first+"'", "Division by zero", ins->token.line);
                    return ctx->buildNil();
                }
                *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) /= operand;
            }
            return NULL;
        };
        case CV::InstructionType::OP_COND_EQ: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            auto firstv = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }                 
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            for(int i = 1; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }
                if(static_cast<CV::NumberType*>(v)->get() != static_cast<CV::NumberType*>(firstv)->get()){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return NULL;
                }
            }
            return NULL;
        };      
        case CV::InstructionType::OP_COND_NEQ: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<CV::Item*> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                
                operands.push_back(v);
            }
            for(int i = 0; i < operands.size(); ++i){
                auto &v1 = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
                for(int j = 0; j < operands.size(); ++j){
                    if(j == i){
                        continue;
                    }
                    auto &v2 = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[j]->data);
                    if(v1 == v2){
                        *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                        return NULL;
                    }
                }
            }
            return NULL;
        };   
        case CV::InstructionType::OP_COND_LT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<CV::Item*> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last >= *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return NULL;
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return NULL;
        };       
        case CV::InstructionType::OP_COND_LTE: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<CV::Item*> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last > *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return NULL;
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return NULL;
        };      
        case CV::InstructionType::OP_COND_GT: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<CV::Item*> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last <= *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return NULL;
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return NULL;
        };       
        case CV::InstructionType::OP_COND_GTE: {
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 1;
            std::vector<CV::Item*> operands;
            for(int i = 0; i < ins->parameter.size(); ++i){
                auto v = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                
                operands.push_back(v);
            }
            auto last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[0]->data);
            for(int i = 1; i < operands.size(); ++i){
                if(last < *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data)){
                    *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(item->data) = 0;
                    return NULL;
                }
                last = *static_cast<__CV_DEFAULT_NUMBER_TYPE*>(operands[i]->data);
            }
            return NULL;
        };                          
                 

        /*
            PROXIES
        */
        case CV::InstructionType::STATIC_PROXY: {
            // fetch and return
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            return stack->contexts[ctxId]->data[dataId];
        };        
        case CV::InstructionType::REFERRED_PROXY: {
            // pre-fetch the item
            auto &ctxId = ins->data[0];
            auto &dataId = ins->data[1];
            auto item = stack->contexts[ctxId]->data[dataId];
            // run instruction
            stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            return item;
        };   
                    
    }

    return ctx->buildNil();
}

CV::Item *CV::Stack::execute(CV::Instruction *ins, std::shared_ptr<Context> &ctx, SHARED<CV::Cursor> &cursor){
    CV::Item *result = NULL;
    bool valid = false;
    do {
        result = __execute(this, ins, ctx, cursor);
        if(cursor->raise()){
            return ctx->buildNil();
        }        
        valid = ins->next != CV::InstructionType::INVALID;    
        ins = valid ? this->instructions[ins->next] : NULL;
    } while(valid);

    return result;
}

void CV::Stack::clear(){
    // Clear Instructions
    for(auto &it : this->instructions){
        delete it.second;
    }
    this->instructions.clear();
    // Clear contexts
    for(auto &it : this->contexts){
        it.second->clear();
    }
    this->contexts.clear();
}


/* -------------------------------------------------------------------------------------------------------------------------------- */

std::string CV::ItemToText(CV::Item *item){
    switch(item->type){
        case CV::NaturalType::NUMBER: {
            return  CV::Tools::setTextColor(Tools::Color::CYAN) +
                    CV::Tools::removeTrailingZeros(static_cast<CV::NumberType*>(item)->get())+
                    CV::Tools::setTextColor(Tools::Color::RESET);
        };

        case CV::NaturalType::STRING: {
            return Tools::setTextColor(Tools::Color::GREEN)+
                    "'"+static_cast<CV::StringType*>(item)->get()+"'"+
                    Tools::setTextColor(Tools::Color::RESET);
        };        

        default:
        case CV::NaturalType::NIL: {
            return "NIL";
        };
    }
}

/* -------------------------------------------------------------------------------------------------------------------------------- */


int main(){

    auto cursor = std::make_shared<CV::Cursor>();
    auto stack = std::make_shared<CV::Stack>();
    auto context = stack->createContext();
    AddStandardConstructors(stack);

    // Get text tokens
    auto tokens = parseTokens("[let a [+ 1 1 1]][let b 5][+ a b]",  ' ', cursor);
    if(cursor->raise()){
        return 1;
    }

    // Compile tokens into its bytecode
    auto entry = compileTokens(tokens, stack, context, cursor);
    if(cursor->raise()){
        return 1;
    }

    // Execute
    auto result = stack->execute(entry, context, cursor);
    if(cursor->raise()){
        return 1;
    }


    std::cout << CV::ItemToText(result) << std::endl;

    // stack->clear();

    return 0;
}