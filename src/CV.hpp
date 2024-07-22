#ifndef CANVAS_CORE_HPP
    #define CANVAS_CORE_HPP

    #include <vector>
    #include <unordered_map>
    #include <functional>
    #include <memory>
    #include <string>
    #include <mutex>

    #define __CV_DEFAULT_NUMBER_TYPE float  

    namespace CV {

        namespace NaturalType {
            enum NaturalType : unsigned {
                NUMBER,
                STRING,
                LIST,
                LAMBDA,
                NIL
            };
        }

        namespace InstructionType {
            enum InstructionType : unsigned {
                INVALID = 0,
                NOOP,
                // CONSTRUCTORS
                LET = 100,
                MUT,
                // OPERATORS
                OP_NUM_ADD = 150,
                OP_NUM_SUB,
                OP_NUM_MULT,
                OP_NUM_DIV,
                OP_COND_EQ,
                OP_COND_NEQ,
                OP_COND_LT,
                OP_COND_LTE,
                OP_COND_GT,
                OP_COND_GTE,
                // PROXIES
                STATIC_PROXY = 200,
                REFERRED_PROXY,
                PROMISE_PROXY
            };
        };     

        struct Item;
        struct Cursor {
            std::mutex accessMutex;
            std::string message;
            std::string title;
            bool error;
            unsigned line;
            Item *subject;
            bool shouldExit;
            Cursor();
            bool raise();
            void setError(const std::string &title, const std::string &message, unsigned line);
            void setError(const std::string &title, const std::string &message, unsigned line, Item *subject);            
        };

        /* -------------------------------------------------------------------------------------------------------------------------------- */
        // Types

        struct Item {
            unsigned id;
            unsigned type;
            unsigned size;
            void *data;
            Item();
            void clear();
        };

        struct NumberType : Item {
            void set(__CV_DEFAULT_NUMBER_TYPE v);
            __CV_DEFAULT_NUMBER_TYPE get();
        };

        struct StringType : Item {
            void set(const std::string &v);
            std::string get();
        };        

        struct ListType : Item {
            void build(unsigned n);
            void set(unsigned index, Item *item);
            Item *get(unsigned index);
        };

        struct FunctionType : Item {
            bool binary;
            bool variadic;
            unsigned nargs;
            unsigned entry;
            void set(bool variadic, unsigned nargs, unsigned entry);
        };

        /* -------------------------------------------------------------------------------------------------------------------------------- */
        // JIT Stuff

        struct Context {
            Context *top;
            unsigned id;
            std::unordered_map<unsigned, CV::Item*> data;
            std::unordered_map<std::string, unsigned> dataIds;
            unsigned store(CV::Item *item);
            unsigned promise();
            void setName(const std::string &name, unsigned id);
            Item *getByName(const std::string &name);
            void clear();
            CV::Item *buildNil();
            Context();
        };
        
        struct Constructor {
            unsigned id;
            std::string name;

        };
        struct Token {
            std::string first;
            unsigned line;
            bool solved;
            Token(){
                solved = false;
            }
            Token(const std::string &first, unsigned line){
                this->first = first;
                this->line = line;
                parse();
            }    
            void parse(){
                solved = !(first.length() >= 3 && first[0] == '[' && first[first.size()-1] == ']');
            }
        };    

        struct Instruction {
            unsigned type;
            unsigned id;
            unsigned next;
            unsigned previous;
            std::string literal;
            std::vector<unsigned> data;
            std::vector<unsigned> parameter;
            CV::Token token;
            Instruction(){
                this->next = CV::InstructionType::INVALID;
                this->previous = CV::InstructionType::INVALID;
                this->type = CV::InstructionType::INVALID;
                this->id = CV::InstructionType::INVALID;
            }
        };

    

        struct Stack {
            std::unordered_map<std::string, 
            std::function<CV::Instruction*(
                                            const std::string &name,
                                            const std::vector<CV::Token> &tokens,
                                            std::shared_ptr<CV::Stack> &stack,
                                            std::shared_ptr<CV::Context> &ctx,
                                            std::shared_ptr<CV::Cursor> &cursor
                                           )>> constructors;
            std::unordered_map<unsigned, CV::Instruction*> instructions;
            std::unordered_map<unsigned, std::shared_ptr<CV::Context>> contexts;
            Stack();
            void clear();
            CV::Instruction *createInstruction(unsigned type, const CV::Token &token);
            std::shared_ptr<CV::Context> createContext(CV::Context *top = NULL);
            CV::Item *execute(CV::Instruction *ins, std::shared_ptr<Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);
        };


        std::string ItemToText(CV::Item *item);

    }


#endif