#include <set>
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

CV::Item *CV::Item::copy(){
    auto item = new Item();
    item->type = this->type;
    item->size = this->size;
    item->bsize = this->bsize;
    item->id = this->id;
    item->data = malloc(this->bsize);
    memcpy(item->data, this->data, this->bsize);
    return item;
}

void CV::Item::restore(CV::Item *item){
    this->type = item->type;
    this->size = item->size;
    this->bsize = item->bsize;
    this->id = item->id;
    memcpy(this->data, item->data, item->bsize);
}

// NUMBER
void CV::NumberType::set(__CV_DEFAULT_NUMBER_TYPE v){
    if(this->type == CV::NaturalType::NIL){
        clear();
        this->type = CV::NaturalType::NUMBER;
        this->bsize = sizeof(__CV_DEFAULT_NUMBER_TYPE);
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
    this->bsize = v.length();
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
    auto bsize = sizeof(unsigned) * 2 * n;
    this->bsize = bsize;
    this->type = CV::NaturalType::LIST;
    this->size = n;
    this->data = malloc(bsize);
    memset(this->data, '0', bsize);
}

void CV::ListType::set(unsigned index, unsigned ctxId, unsigned dataId){
    memcpy((void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 0), &ctxId, sizeof(unsigned));
    memcpy((void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 1), &dataId, sizeof(unsigned));
}

CV::Item *CV::ListType::get(CV::Stack *stack, unsigned index){
    unsigned ctxId, dataId;
    memcpy(&ctxId, (void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 0), sizeof(unsigned));
    memcpy(&dataId, (void*)((size_t)this->data + index * sizeof(unsigned) * 2 + sizeof(unsigned) * 1), sizeof(unsigned));    
    return stack->contexts[ctxId]->data[dataId];
}

CV::Item *CV::ListType::get(std::shared_ptr<CV::Stack> &stack, unsigned index){
    return this->get(stack.get(), index);
}

// FUNCTION
CV::Item *CV::FunctionType::copy(){
    return this;
}

void CV::FunctionType::restore(CV::Item *item){
    // Does nothing
}

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
    if(CV::Tools::isNumber(imp) && tokens.size() == 1){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto number = new CV::NumberType();
        number->set(std::stod(imp));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(number));
        return ins;
    }else
    if(CV::Tools::isString(imp) && tokens.size() == 1){
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
        auto string = new CV::StringType();
        string->set(imp.substr(1, imp.length() - 2));
        ins->data.push_back(ctx->id);
        ins->data.push_back(ctx->store(string));
        return ins;
    }else
    // Is it a constructor?
    if(CheckConstructor(stack, imp)){
        return stack->constructors[imp](imp, tokens, stack, ctx, cursor);
    }else{
        // Is it a name?
        if(ctx->check(imp)){
            auto pair = ctx->getIdByName(imp);
            if(pair.ctx == 0 || pair.id == 0){
                cursor->setError("Fatal Error referencing name '"+imp+"'", "Name was not found on this context or above", token.line);
                return stack->createInstruction(CV::InstructionType::NOOP, token);     
            }
            auto markedType = ctx->getMarked(pair.id);
            if(markedType == CV::NaturalType::FUNCTION){
                auto fn = static_cast<CV::FunctionType*>(stack->contexts[pair.ctx]->data[pair.id]);
                // Basic Error Checking
                if(!fn->variadic && tokens.size()-1 != fn->args.size()){
                    auto errmsg = "Expects "+std::to_string(fn->args.size())+" argument(s), "+std::to_string(tokens.size()-1)+" were provided";
                    cursor->setError("Function '"+imp+"'", errmsg, token.line);
                    return stack->createInstruction(CV::InstructionType::NOOP, token);     
                }
                // Create instruction
                auto ins = stack->createInstruction(CV::InstructionType::CF_INVOKE_FUNCTION, token);
                ins->data.push_back(fn->ctx);
                ins->data.push_back(fn->args.size());
                ins->data.push_back(fn->variadic);
                ins->parameter.push_back(fn->entry);
                // Pass parameter with its respective data
                for(int i = 0; i < fn->args.size(); ++i){
                    ins->data.push_back(fn->args[i]);
                }
                for(int i = 1; i < tokens.size(); ++i){
                    auto cins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
                    if(cursor->raise()){
                        return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                    }
                    ins->parameter.push_back(cins->id);
                }
                return ins;
            }else{
                auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, token);
                auto string = new CV::StringType();
                ins->data.push_back(pair.ctx);
                ins->data.push_back(pair.id);
                return ins;                  
            }   
        }else
        // Can it be a list?
        if(tokens.size() > 1){
            auto nins = stack->createInstruction(CV::InstructionType::CONSTRUCT_LIST, tokens[0]);
            auto list = new CV::ListType();        
            list->build(tokens.size());
            nins->data.push_back(ctx->id);
            nins->data.push_back(ctx->store(list));
            nins->data.push_back(tokens.size());
            for(int i = 0; i < tokens.size(); ++i){
                auto cins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
                if(cursor->raise()){
                    return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
                }
                nins->parameter.push_back(cins->id);
                nins->data.push_back(ctx->id);
            }
            return nins;           
        // I just don't know then
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
        cursor->setError("Failed to parse token", "Invalid syntax used or malformed token", tokens[0].line);
        return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
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
        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" NAME VALUE)", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        
        // TODO: Check for valid variable names

        // Interpret data instruction
        auto dataIns = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }

        // Allocate promise in current context
        auto dataId = ctx->promise();
        ctx->setName(tokens[1].first, dataId);

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::LET, tokens[1]);
        ins->data.push_back(ctx->id);
        ins->data.push_back(dataId);
        ins->parameter.push_back(dataIns->id);

        return ins;
    });


    /*
        MUT
    */
    DefConstructor(stack, "mut", [](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" NAME VALUE)", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process item to replace
        auto originIns = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }

        // Interpret replacement data instruction
        auto dataIns = interpretToken(tokens[2], {tokens[2]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::MUT, tokens[1]);
        ins->parameter.push_back(originIns->id);
        ins->parameter.push_back(dataIns->id);

        return ins;
    }); 


    /*
        FN
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "fn", [](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){

        if(tokens.size() != 3){
            cursor->setError(name, "Expects exactly 2 arguments ("+name+" [args] [code])", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        auto nctx = stack->createContext(ctx.get());

        bool variadic = false;
        std::vector<unsigned> argIds;
        
        // Get find arguments
        auto argTokens = parseTokens(tokens[1].first, ' ', cursor);
        if(argTokens.size() == 1 && argTokens[0].first == ".."){
            variadic = true;
            auto argId = nctx->promise();
            std::string name = "@";
            argIds.push_back(argId);            
            nctx->setName(name, argId);
        }else{
            for(int i = 0; i < argTokens.size(); ++i){
                auto argId = nctx->promise();
                auto &name =argTokens[i].first;
                argIds.push_back(argId);
                nctx->setName(name, argId);
            }
        }

        // Process function's body
        auto body = interpretToken(tokens[2], {tokens[2]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        } 

        // Create Item
        auto fn = new CV::FunctionType();
        fn->variadic = variadic;
        fn->ctx = nctx->id;
        fn->entry = body->id;
        for(int i = 0; i < argIds.size(); ++i){
            fn->args.push_back(argIds[i]);
        }
        auto fnId = ctx->store(fn);
        
        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::STATIC_PROXY, tokens[0]);
        ins->data.push_back(ctx->id);
        ins->data.push_back(fnId);
        ctx->markPromise(fnId, CV::NaturalType::FUNCTION);

        // Seal Context
        nctx->solidify();

        return ins;   
        
    });           

    /*
        NTH
    */
    DefConstructor(stack, "nth", [](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Process index
        auto index = interpretToken(tokens[1], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }       
        // Process 
        auto list = interpretToken(tokens[2], {tokens[1]}, stack, ctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }              

        // Create Context
        auto ins = stack->createInstruction(CV::InstructionType::NTH_PROXY, tokens[0]);

        ins->parameter.push_back(index->id);
        ins->parameter.push_back(list->id);
        
        return ins;
    });  

    /*
        IF
        [CC]  (Creates Context)
    */
    DefConstructor(stack, "if", [](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
        if(tokens.size() < 3){
            cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[0]);
        }
        // Create Context
        auto nctx = stack->createContext(ctx.get());

        // Process true branch
        auto condition = interpretToken(tokens[1], {tokens[1]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        } 

        // Process false branch
        auto trueBranch = interpretToken(tokens[2], {tokens[2]}, stack, nctx, cursor);
        if(cursor->raise()){
            return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
        }        

        // Create instruction
        auto ins = stack->createInstruction(CV::InstructionType::CF_COND_BINARY_BRANCH, tokens[0]);
        ins->data.push_back(nctx->id);
        ins->parameter.push_back(condition->id);
        ins->parameter.push_back(trueBranch->id);

        // Is there false branch?
        if(tokens.size() >= 4){
            auto falseBranch = interpretToken(tokens[3], {tokens[3]}, stack, nctx, cursor);
            if(cursor->raise()){
                return stack->createInstruction(CV::InstructionType::NOOP, tokens[2]);
            }
            ins->parameter.push_back(falseBranch->id);
        }

        // Seals it
        nctx->solidify();

        return ins;
    });     

    /*
        EMBEDDED OPERATORS (LOGIC & ARITHMETIC)
    */
    auto buildOp = [&](const std::string &name, unsigned TYPE){
        DefConstructor(stack, name, [TYPE](const std::string &name, const VECTOR<CV::Token> &tokens, SHARED<CV::Stack> &stack, SHARED<CV::Context> &ctx, SHARED<CV::Cursor> &cursor){
            if(tokens.size() < 3){
                cursor->setError(name, "Expects at least 2 arguments", tokens[0].line);
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
                auto ins = interpretToken(tokens[i], {tokens[i]}, stack, ctx, cursor);
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
    solid = false;
}

unsigned CV::Context::store(CV::Item *item){
    if(solid){
        fprintf(stderr, "Context %i: Tried storing new Item %i in a solid context\n", this->id, id);
        std::exit(1);           
    }
    item->id = generateId();
    this->data[item->id] = item;
    return item->id;
}

unsigned CV::Context::promise(){
    auto id = generateId();
    this->data[id] = NULL;
    return id;
}

void CV::Context::markPromise(unsigned id, unsigned type){
    this->markedItems[id] = type;
}

unsigned CV::Context::getMarked(unsigned id){
    auto it = this->markedItems.find(id);
    if(it == this->markedItems.end()){
        return this->top ? this->top->getMarked(id) : CV::NaturalType::UNDEFINED;
    }
    return it->second;
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
    std::set<CV::Item*> uniqueItems; // To avoid double free
    for(auto &it : this->data){
        auto item = it.second;
        item->clear();
        uniqueItems.insert(item);
    }
    for(auto &it : uniqueItems){
        free(it);
    }
    this->data.clear();
    this->dataIds.clear();
    // Clear originalData
    for(int i = 0; i < this->originalData.size(); ++i){
        auto item = this->originalData[i];
        item->clear();
        delete item;
    }
    this->originalData.clear();
}

CV::Item *CV::Context::buildNil(){
    auto nil = new CV::Item();
    this->store(nil);
    return nil;
}

void CV::Context::transferFrom(std::shared_ptr<CV::Context> &other, unsigned id){
    // Find and store it here
    auto data = other->data[id];
    this->store(data);
    // Remove from other
    auto it = other->data.find(id);
    other->data.erase(it);
}

void CV::Context::solidify(){
    if(this->solid){
        return;
    }
    for(auto &it : this->data){
        originalData.push_back(it.second->copy());
    }
    solid = true;
}

void CV::Context::revert(){
    for(int i = 0; i < this->originalData.size(); ++i){
        auto backup = this->originalData[i];
        auto local = this->data[backup->id];
        local->restore(backup);
    }
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

void CV::Stack::deleteContext(unsigned id){
    auto it = this->contexts.find(id);
    if(it == this->contexts.end()){
        fprintf(stderr, "Context %i: Failed to find such context\n", id);
        std::exit(1);
    }
    auto ctx = it->second;
    ctx->clear();
    this->contexts.erase(it);
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

            auto target = stack->execute(stack->instructions[ins->parameter[0]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }

            auto v = stack->execute(stack->instructions[ins->parameter[1]], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }

            if(target->type != v->type){
                cursor->setError("Illegal Mutation '"+ins->token.first+"'", "Can only mutate variables of the same type", ins->token.line);
                return ctx->buildNil();
            }else
            if(target->type != CV::NaturalType::NUMBER && target->type != CV::NaturalType::STRING){
                cursor->setError("Illegal Mutation '"+ins->token.first+"'", "Only natural types 'STRING' and 'NUMBER' may be mutated", ins->token.line);
                return ctx->buildNil();                
            }

            switch(target->type){
                case CV::NaturalType::NUMBER: {
                    auto original = static_cast<CV::NumberType*>(target);
                    auto newValue = static_cast<CV::NumberType*>(v);
                    original->set(newValue->get());
                } break;
                case CV::NaturalType::STRING: {
                    auto original = static_cast<CV::StringType*>(target);
                    auto newValue = static_cast<CV::StringType*>(v);                    
                    original->set(newValue->get());
                } break;                
            }

            return target;
        };
        case CV::InstructionType::CONSTRUCT_LIST: {
            auto &lsCtxId = ins->data[0];
            auto &lsDataId = ins->data[1];            
            auto list = static_cast<CV::ListType*>(stack->contexts[lsCtxId]->data[lsDataId]);

            unsigned nelements = ins->data[2];

            unsigned c = 0;
            
            for(int i = 0; i < nelements; ++i){
                auto elementV = stack->execute(stack->instructions[ins->parameter[i]], ctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                } 
                unsigned elCtxId = ins->data[3 +  i];
                list->set(i, ctx->id, elementV->id);
            }

            return static_cast<CV::Item*>(list);

        };        

        /*
            CONTROL FLOW
        */
        case CV::InstructionType::CF_COND_BINARY_BRANCH: { // CC

            auto nctx = stack->contexts[ins->data[0]];

            auto cond = stack->execute(stack->instructions[ins->parameter[0]], nctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }       

            bool isTrue = cond->type != CV::NaturalType::NIL && (cond->type == CV::NaturalType::NUMBER ? static_cast<CV::NumberType*>(cond)->get() != 0 : true);

            if(isTrue){
                auto trueBranch = stack->execute(stack->instructions[ins->parameter[1]], nctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }          
                ctx->transferFrom(nctx, trueBranch->id);
                return trueBranch;
            }else
            if(ins->parameter.size() > 2){
                auto falseBranch = stack->execute(stack->instructions[ins->parameter[2]], nctx, cursor);
                if(cursor->raise()){
                    return ctx->buildNil();
                }                    
                ctx->transferFrom(nctx, falseBranch->id);   
                return falseBranch;                
            }

            nctx->revert();

            return ctx->buildNil();
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
        case CV::InstructionType::NTH_PROXY: {
            // fetch
            auto &indexId = ins->parameter[0];
            auto &listId = ins->parameter[1];

            // run
            auto index = stack->execute(stack->instructions[indexId], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }
            auto list = stack->execute(stack->instructions[listId], ctx, cursor);
            if(cursor->raise()){
                return ctx->buildNil();
            }
            // Basic checks
            if(index->type != CV::NaturalType::NUMBER){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Expected a NUMBER at position 0 as index, "+CV::NaturalType::name(index->type)+" was provided", ins->token.line);
                return ctx->buildNil();                
            }

            if(list->type != CV::NaturalType::LIST){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Expected a LIST at position 1 as container, "+CV::NaturalType::name(list->type)+" was provided", ins->token.line);
                return ctx->buildNil();                
            }
            // fetch and return
            int n = static_cast<CV::NumberType*>(index)->get();
            auto ls = static_cast<CV::ListType*>(list);
            if(n < 0 || n >= ls->size){
                cursor->setError("Logic Error At '"+ins->token.first+"'", "Provided out-of-range index("+std::to_string(n)+"), expected from 0 to "+std::to_string(ls->size-1), ins->token.line);
                return ctx->buildNil();                 
            }

            return ls->get(stack, n);
        } break;
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
            if(cursor->raise()){
                return ctx->buildNil();
            }      

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

std::string CV::ItemToText(std::shared_ptr<CV::Stack> &stack, CV::Item *item){
    switch(item->type){
        
        default:
        case CV::NaturalType::NIL: {
            return Tools::setTextColor(Tools::Color::BLUE)+"nil"+Tools::setTextColor(Tools::Color::RESET);
        };

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

        case CV::NaturalType::LIST: {
            std::string output = Tools::setTextColor(Tools::Color::MAGENTA)+"["+Tools::setTextColor(Tools::Color::RESET);
            auto list = static_cast<CV::ListType*>(item);

            int total = list->size;
            int limit = total > 30 ? 10 : total;
            for(int i = 0; i < limit; ++i){
                auto item = list->get(stack, i);
                output += CV::ItemToText(stack, item);
                if(i < list->size-1){
                    output += " ";
                }
            }
            if(limit != total){
                output += Tools::setTextColor(Tools::Color::YELLOW)+"...("+std::to_string(total-limit)+" hidden)"+Tools::setTextColor(Tools::Color::RESET);
            }
                        
            return output + Tools::setTextColor(Tools::Color::MAGENTA)+"]"+Tools::setTextColor(Tools::Color::RESET);
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
    auto tokens = parseTokens("[let a [1 2 3]][mut [nth 0 a] 15][a]",  ' ', cursor);
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


    std::cout << CV::ItemToText(stack, result) << std::endl;

    stack->clear();

    return 0;
}