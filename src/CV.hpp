#ifndef CANVAS_CORE_HPP
    #define CANVAS_CORE_HPP

    #include <vector>
    #include <unordered_map>
    #include <functional>
    #include <memory>
    #include <string>
    #include <mutex>

    #define __CV_DEFAULT_NUMBER_TYPE float  
    static const __CV_DEFAULT_NUMBER_TYPE CANVAS_LANG_VERSION[3] = { 0, 3, 0 };

    namespace CV {

        namespace Tools {
            std::string readFile(const std::string &path);
        }

        namespace SupportedPlatform {
            enum SupportedPlatform : int {
                WINDOWS,
                LINUX,
                OSX
            };
            static std::string name(int plat){
                switch(plat){
                    case SupportedPlatform::WINDOWS: 
                        return "WINDOWS";
                    case SupportedPlatform::LINUX:
                        return "LINUX";
                    default:
                        return "UNKNOWN";
                }
            }
        }

        namespace SupportedArchitecture {
            enum SupportedArchitecture : int {
                X86,
                X86_64,
                ARM,
                ARM64,
                UNKNOWN
            };
            static std::string name(int plat){
                switch(plat){
                    case SupportedArchitecture::X86: 
                        return "X86";
                    case SupportedArchitecture::X86_64: 
                        return "X86_64";
                    case SupportedArchitecture::ARM: 
                        return "ARM";
                    case SupportedArchitecture::ARM64: 
                        return "ARM64";                                                                                    
                    default:
                        return "UNKNOWN";
                }
            }
        }

        #if defined(__i386__) || defined(__i686__)
            const static int ARCH = SupportedArchitecture::X86;
        #elif defined(__x86_64__) || defined(__amd64__)
            const static int ARCH = SupportedArchitecture::X86_64;
        #elif defined(__arm__)
            const static int ARCH = SupportedArchitecture::ARM;
        #elif defined(__aarch64__)
            const static int ARCH = SupportedArchitecture::ARM64;
        #else
            const static int ARCH = SupportedArchitecture::UNKNOWN;
        #endif

        #ifdef _WIN32
            const static int PLATFORM = SupportedPlatform::WINDOWS;
        #elif __linux__
            const static int PLATFORM = SupportedPlatform::LINUX;
        #else
            const static int PLATFORM = SupportedPlatform::UNSUPPORTED;
        #endif    


        namespace NaturalType {
            enum NaturalType : unsigned {
                UNDEFINED = 0,
                NUMBER = 1000,
                STRING,
                LIST,
                FUNCTION,
                NIL,
            };
            static std::string name(unsigned type){
                switch(type){
                    case CV::NaturalType::NUMBER: {
                        return "NUMBER";
                    };
                    case CV::NaturalType::STRING: {
                        return "STRING";
                    };
                    case CV::NaturalType::LIST: {
                        return "LIST";
                    };
                    case CV::NaturalType::FUNCTION: {
                        return "FUNCTION";
                    };  
                    case CV::NaturalType::NIL: {
                        return "NIL";
                    };                                                                               
                    default:
                    case CV::NaturalType::UNDEFINED: {
                        return "UNDEFINED";
                    };
                }
            }
        }

        namespace InstructionType {
            enum InstructionType : unsigned {
                INVALID = 0,
                NOOP,
                // CONSTRUCTORS
                LET = 100,
                MUT,                    // Mutator
                CONSTRUCT_LIST,
                CONSTRUCT_FUNCTION,
                // CONTROL FLOW
                CF_INVOKE_FUNCTION,
                CF_INVOKE_BINARY_FUNCTION,
                CF_COND_BINARY_BRANCH, 
                CF_LOOP_DO,
                CF_LOOP_FOR,
                CF_INT_YIELD,
                CF_INT_SKIP,
                CF_INT_RETURN,
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
                OP_LOGIC_NOT,
                OP_LOGIC_AND,
                OP_LOGIC_OR,
                // LIST
                LS_NTH_ELEMENT,
                // PROXIES
                STATIC_PROXY = 200,
                REFERRED_PROXY,
                NTH_PROXY
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


        /* -------------------------------------------------------------------------------------------------------------------------------- */
        // Types

        struct Item {
            unsigned id;
            unsigned ctx;
            unsigned type;
            unsigned size;
            unsigned bsize;
            void *data;
            Item();
            void clear();
            virtual CV::Item *copy();
            virtual void restore(CV::Item *item);
        };

        struct NumberType : Item {
            void set(__CV_DEFAULT_NUMBER_TYPE v);
            __CV_DEFAULT_NUMBER_TYPE get();
        };

        struct StringType : Item {
            void set(const std::string &v);
            std::string get();
        };        

        struct Stack;
        struct ListType : Item {
            void build(unsigned n);
            void set(unsigned index, unsigned ctxId, unsigned dataId);
            CV::Item *get(const std::shared_ptr<CV::Stack> &stack, unsigned index);
            CV::Item *get(const CV::Stack *stack, unsigned index);
        };

        struct FunctionType : Item {
            bool variadic;
            std::vector<std::string> args;            
            CV::Token body;
            CV::Item *copy();
            void restore(CV::Item *item);            
        };

        /* -------------------------------------------------------------------------------------------------------------------------------- */
        // JIT Stuff

        namespace ControlFlowType {
            enum ControlFlowType : unsigned {
                CONTINUE,
                SKIP,
                YIELD,
                RETURN
            };
            static unsigned type(unsigned ins){
                switch(ins){
                    case CV::InstructionType::CF_INT_SKIP: {
                        return CV::ControlFlowType::SKIP;
                    };
                    case CV::InstructionType::CF_INT_RETURN: {
                        return CV::ControlFlowType::RETURN;
                    };
                    case CV::InstructionType::CF_INT_YIELD: {
                        return CV::ControlFlowType::YIELD;
                    };
                    default: {
                        return CV::ControlFlowType::CONTINUE;
                    };                                                            
                }
            }
        }

        struct ControlFlow {
            CV::Item *value;
            unsigned type;
            ControlFlow(){
                this->value = NULL;
            }
            ControlFlow(CV::Item *v, unsigned type = CV::ControlFlowType::CONTINUE){
                this->value = v;
                this->type = type;
            }
        };

        struct ContextDataPair {
            unsigned ctx;
            unsigned id;
            ContextDataPair(){
                ctx = 0;
                id = 0;
            }
            ContextDataPair(unsigned ctx, unsigned id){
                this->ctx = ctx;
                this->id = id;
            }
        };
        struct Context;
        struct BinaryFunction {
            bool preprocessor;
            std::string name;
            unsigned id;  
            std::function<CV::Item*(std::vector<CV::Item*>&, std::shared_ptr<CV::Context>&, std::shared_ptr<CV::Cursor> &cursor)> fn;
            BinaryFunction(){
                preprocessor = false;
            }
        };

        struct Context {
            Context *top;
            unsigned id; 
            bool solid; // Solid contexts won't accept new data
            std::unordered_map<unsigned, CV::Item*> data;
            std::unordered_map<std::string, unsigned> dataIds;
            std::unordered_map<unsigned, unsigned> markedItems;
            void deleteData(unsigned id);
            unsigned store(CV::Item *item);
            unsigned promise();
            void markPromise(unsigned id, unsigned type);
            unsigned getMarked(unsigned id);
            void setPromise(unsigned id, CV::Item *item);
            void setName(const std::string &name, unsigned id);
            void deleteName(unsigned id);
            ContextDataPair getIdByName(const std::string &name);
            CV::Item *getByName(const std::string &name);
            bool check(const std::string &name);
            void clear();
            CV::Item *buildNil();
            Context();
            void transferFrom(CV::Stack *stack, CV::Item *item);
            void transferFrom(std::shared_ptr<CV::Stack> &stack, CV::Item *item);
            std::vector<CV::Item*> originalData;
            void solidify();
            void revert();
        };
        
        struct Instruction {
            unsigned type;
            unsigned id;
            unsigned next;
            unsigned previous;
            std::vector<std::string> literal;
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
                                            const std::shared_ptr<CV::Stack> &stack,
                                            std::shared_ptr<CV::Context> &ctx,
                                            std::shared_ptr<CV::Cursor> &cursor
                                           )>> constructors;
            std::unordered_map<unsigned, CV::Instruction*> instructions;
            std::unordered_map<unsigned, std::shared_ptr<CV::Context>> contexts;
            std::unordered_map<unsigned, CV::BinaryFunction> binaryFunctions;
            Stack();
            void clear();
            CV::Instruction *createInstruction(unsigned type, const CV::Token &token);
            std::shared_ptr<CV::Context> createContext(CV::Context *top = NULL);
            std::shared_ptr<CV::Context> getContext(unsigned id) const;
            void deleteContext(unsigned id);
            CV::ControlFlow execute(CV::Instruction *ins, std::shared_ptr<Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);
            void registerFunction(const std::string &name, const std::function<CV::Item*(std::vector<CV::Item*>&, std::shared_ptr<CV::Context>&, std::shared_ptr<CV::Cursor> &cursor)> &fn, bool preprocessor = false);
            unsigned getRegisteredFunctionId(const std::string &name);
        };

        void AddStandardConstructors(std::shared_ptr<CV::Stack> &stack);
        std::string ItemToText(const std::shared_ptr<CV::Stack> &stack, CV::Item *item);
        void SetUseColor(bool v);
        std::string QuickInterpret(const std::string &input, std::shared_ptr<CV::Stack> &stack, std::shared_ptr<Context> &ctx, std::shared_ptr<CV::Cursor> &cursor);

    }


#endif