#ifndef CANVAS_HPP
    #define CANVAS_HPP

    #include <string>
    #include <functional>
    #include <memory>
    #include <mutex>     
    #include <thread>
    #include <unordered_map>
    #include <inttypes.h>    
    #include <vector>

    #define __CV_NUMBER_NATIVE_TYPE double

    static const __CV_NUMBER_NATIVE_TYPE CANVAS_LANG_VERSION[3] = { 1, 2, 0 };

    namespace CV {

        namespace Tools {
            bool isLineComplete(const std::string &input);
            void sleep(uint64_t t);
            uint64_t ticks();
            bool isReservedWord(const std::string &input);
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


        struct Cursor {
            bool error;
            int line;
            std::string message;
            Cursor();
            void clear();
            std::string getOrigin(const std::string &origin);
            void setError(const std::string &origin, const std::string &v, int line = -1);
        };


        namespace ModifierTypes {
            enum ModifierTypes : uint8_t {
                UNDEFINED,
                NAMER,
                SCOPE,
                EXPAND,
                TRAIT
            };
            static uint8_t getToken(char mod){
                if(mod == '~'){
                    return ModifierTypes::NAMER;
                }else
                if(mod == ':'){
                    return ModifierTypes::SCOPE;
                }else         
                if(mod == '|'){
                    return ModifierTypes::TRAIT;
                }else        
                if(mod == '^'){
                    return ModifierTypes::EXPAND;            
                }else{
                    return ModifierTypes::UNDEFINED;
                }
            }
            static std::string str(uint8_t type){
                switch(type){
                    case ModifierTypes::NAMER: {
                        return "~";
                    };
                    case ModifierTypes::SCOPE: {
                        return ":";
                    };   
                    case ModifierTypes::TRAIT: {
                        return "|";
                    };
                    case ModifierTypes::EXPAND: {
                        return "^";
                    };                          
                    default: {
                        return "";
                    };       
                }
            }
        }

        namespace ItemTypes {
            enum ItemTypes : uint8_t {
                NIL,
                NUMBER,
                STRING,
                LIST,
                CONTEXT,
                FUNCTION,
                INTERRUPT,
                JOB
            };
            static std::string str(uint8_t type){
                switch(type){
                    case NIL: {
                        return "NIL";
                    };
                    case NUMBER: {
                        return "NUMBER";
                    };  
                    case STRING: {
                        return "STRING";
                    };  
                    case LIST: {
                        return "LIST";
                    }; 
                    case CONTEXT: {
                        return "CONTEXT";
                    };    
                    case FUNCTION: {
                        return "FUNCTION";
                    }; 
                    case INTERRUPT: {
                        return "INTERRUPT";
                    };    
                    case JOB: {
                        return "JOB";
                    };                      
                    default: {
                        return "UNDEFINED";
                    };         
                }
            }
        }

        namespace InterruptTypes {
            enum InterruptTypes : uint8_t {
                SKIP,
                STOP,
                CONTINUE
            };
            static std::string str(uint8_t type){
                switch(type){
                    case InterruptTypes::SKIP:{
                        return "SKIP";
                    };
                    case InterruptTypes::STOP:{
                        return "STOP";
                    };    
                    default: {
                        return "UNDEFINED";    
                    }        
                }
            }
        }

        namespace TraitType {
            enum TraitType : uint8_t {
                ANY_NUMBER,
                ANY_STRING,
                SPECIFIC
            };
        }

        namespace ByteCodeType {
            enum ByteCodeType : unsigned {
                JMP_FUNCTION = 91,          // Refers to CV Function (Can be another CompiledToken or a binary defined function)
                FORWARD_INS,                
                PROXY,                      // Links to an already existing item
                CONSTRUCT_PROXY,
                CONSTRUCT_LIST,             // Used to build lists
                CONSTRUCT_CTX,              // Used to build contexts
                CONSTRUCT_FN,               // Used to build functions
                CONSTRUCT_CALLBACK,
                LITERAL,                    // Normally instructions in text that couldn't be compiled into a real BC token
                SET,                        // 1) Can define a variable within a Context 2) Can replace an existing variable with a new one using the same name
                MUT,                        // Can modify primitive types (Numbers and Strings)
                NOOP,
                REFERRED_PROXY,             // It's a proxy for a data type doesn't exist when compiling. It only points to the constructor
                REFERRED_FN,
                SUMMON_ASYNC,
                SUMMON_UNTETHER,
                BINARY_BRANCH_COND,
                COND_LOOP,
                ITERATION_LOOP,
                INTERRUPTOR,
                MODIFIER_DIG_PROXY,       
                UNDEFINED
            };
            static std::string str(unsigned type){
                switch(type){
                    case SUMMON_ASYNC: {
                        return "SUMMON_ASYNC";
                    } break;
                    
                    case SUMMON_UNTETHER: {
                        return "SUMMON_UNTETHER";
                    } break;

                    case JMP_FUNCTION: {
                        return "JMP_FUNCTION";
                    } break;

                    case FORWARD_INS: {
                        return "FORWARD_INS";
                    } break;    

                    case CONSTRUCT_PROXY: {
                        return "CONSTRUCT_PROXY";
                    } break;
                    
                    case CONSTRUCT_CALLBACK: {
                        return "CONSTRUCT_CALLBACK";
                    } break;

                    case CONSTRUCT_CTX: {
                        return "CONSTRUCT_CTX";
                    } break;   

                    case CONSTRUCT_LIST: {
                        return "CONSTRUCT_LIST";
                    } break;   

                    case CONSTRUCT_FN: {
                        return "CONSTRUCT_FN";
                    } break; 

                    case PROXY: {
                        return "PROXY";
                    } break;              

                    case REFERRED_PROXY: {
                        return "REFERRED_PROXY";
                    } break;              

                    case REFERRED_FN: {
                        return "REFERRED_FN";
                    } break;          
                    
                    case SET: {
                        return "SET";
                    } break; 

                    case COND_LOOP: {
                        return "COND_LOOP";
                    } break;     
                    
                    case MODIFIER_DIG_PROXY: {
                        return "MODIFIER_DIG_PROXY";
                    } break;

                    case BINARY_BRANCH_COND: {
                        return "BINARY_BRANCH_COND";
                    } break;                     

                    case ITERATION_LOOP: {
                        return "ITERATION_LOOP";
                    } break; 

                    case INTERRUPTOR: {
                        return "INTERRUPTOR";
                    } break;                     

                    case MUT: {
                        return "MUT";
                    } break;    

                    case NOOP: {
                        return "NOOP";
                    } break;     

                    case UNDEFINED:
                    default: {
                        return "UNDEFINED";
                    }                                            
                }
            }
        }


        namespace JobStatus {
            enum JobStatus : int {
                IDLE,
                RUNNING,
                DONE
            };
            static std::string str(int t){
                switch(t){
                    case CV::JobStatus::IDLE:{
                        return "IDLE";
                    };
                    case CV::JobStatus::RUNNING:{
                        return "RUNNING";
                    };
                    default: {
                        return "DONE";
                    }
                }
            }            
        }
        namespace JobType {
            enum JobType : int {
                ASYNC,
                THREAD
            };
            static std::string str(int t){
                switch(t){
                    case JobType::ASYNC:{
                        return "ASYNC";
                    };
                    case JobType::THREAD: {
                        return "THREAD";
                    };                  
                    default: {
                        return "UNDEFINED";
                    }
                }
            }
        }        

        struct Item;
        struct Context;
        struct ItemContextPair;
       

        struct ModifierPair {
            uint8_t type;
            std::string subject;
            bool used;
            ModifierPair(){
                type = ModifierTypes::UNDEFINED;
                used = false;
            }
            ModifierPair(uint8_t type, const std::string &subject){
                this->type = type;
                this->subject = subject;
                this->used = false;

            }
        };

        struct ModifierEffect {
            bool expand;
            bool ctxSwitch;
            bool postEval;
            std::shared_ptr<Context> toCtx;
            std::string ctxTargetName;
            std::string named;
            ModifierEffect(){
                this->reset();
            }
            void reset(){
                this->toCtx = NULL;
                this->ctxTargetName = "";
                this->postEval = true;
                this->expand = false;
                this->ctxSwitch = false;
                this->named = "";
            }
        };

        struct Trait {
            std::string name;
            uint8_t type;
            std::function<std::shared_ptr<Item>(std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects)> action;
            Trait();
            Trait(const std::string &name, uint8_t type, const std::function<std::shared_ptr<Item>(std::shared_ptr<Item> &subject, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects)> &fn){
                this->name = name;
                this->type = type;
                this->action = fn;
            }
        };


        struct Item {
            unsigned id;
            uint8_t type;
            bool copyable;
            bool solid;
            std::mutex accessMutex; 

            Item(const Item &other){

            };
            Item& operator=(const Item& other){
                return *this;
            };

            Item();

            virtual bool isEq(std::shared_ptr<Item> &item);
            virtual std::shared_ptr<Item> copy(bool deep = true);

        };

        struct Interrupt : Item {
            uint8_t intType;
            std::shared_ptr<Item> payload;


            Interrupt(const Interrupt &other){
            };
            Interrupt& operator=(const Interrupt& other){
                return *this;                   
            };

            Interrupt(uint8_t intType);

            std::shared_ptr<Item> copy(bool deep = true); 

            bool hasPayload();

            void setPayload(std::shared_ptr<Item> &item);

            std::shared_ptr<Item> getPayload();

        };


        struct Number : Item {
            __CV_NUMBER_NATIVE_TYPE n;

            Number(const Number &other){
                // this->type = other.type;
                // this->n = other.n;
            };
            Number& operator=(const Number& other){
                // this->type = other.type;
                // this->n = other.n;                
                return *this;
            };

            Number();
            Number(__CV_NUMBER_NATIVE_TYPE v);

            bool isEq(std::shared_ptr<Item> &item);


            std::shared_ptr<Item> copy(bool deep = true);

            void set(const __CV_NUMBER_NATIVE_TYPE v);
            
            __CV_NUMBER_NATIVE_TYPE get();

        };



        struct List : Item {

            std::vector<std::shared_ptr<Item>> data;

            List(const List &other){
            };
            List& operator=(const List& other){
                return *this;
            };

            List();

            List(const std::vector<std::shared_ptr<Item>> &list, bool toCopy = true);


            std::shared_ptr<Item> copy(bool deep = true);

            void set(const std::vector<std::shared_ptr<Item>> &list, bool toCopy = true);

            std::shared_ptr<Item> get(size_t index);

            size_t size();


        };



        struct String : Item {
            std::string data;
            

            String(const String &other){
            };
            String& operator=(const String& other){
                return *this;
            };


            size_t size();

            String();
            String(const std::string &str);  


            bool isEq(std::shared_ptr<CV::Item> &item);

            std::shared_ptr<CV::Item> copy(bool deep = true);

            void set(const std::string &str);

            std::string get();

        };


        struct TokenByteCode;
        struct Stack;
        struct Function : Item {
            std::shared_ptr<CV::TokenByteCode> entry;
            std::vector<std::string> params;
            std::vector<unsigned> paramId;
            bool binary;
            bool threaded;
            bool async;
            bool variadic;

            std::vector<std::string> getParams();

            Function(const Function &other){

            };

            Function& operator=(const Function& other){
                return *this;
            };

            std::function< std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx) > fn;
            std::shared_ptr<CV::Item> copy(bool deep = true);
            Function();
            Function(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic = false);
            void set(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic);
            void set(std::shared_ptr<CV::TokenByteCode> &entry, bool variadic);
            
        };    

        struct Job : Item {
            __CV_NUMBER_NATIVE_TYPE id;
            std::shared_ptr<CV::Item> payload;
            std::shared_ptr<CV::TokenByteCode> entry;
            std::shared_ptr<CV::Job> callback;
            std::shared_ptr<Cursor> cursor;
            std::shared_ptr<CV::Stack> program;

            unsigned inherited;
            bool isCallback;
            bool ready;
            int jobType;
            int status;
            std::thread thread;
            

            void setStatus(int nstatus);
            int getStatus();

            void setCallback(std::shared_ptr<CV::Job> &cb);
            std::shared_ptr<CV::Item> getPayload();
            void setPayload(std::shared_ptr<CV::Item> &item);
            bool hasPayload();

            std::shared_ptr<CV::Item> copy(bool deep = true);

            Job();

            void set(int type, std::shared_ptr<CV::TokenByteCode> &entry, std::shared_ptr<CV::Stack> &program);

            Job(const Job &other){
                this->id = other.id;
                this->entry = other.entry;
                this->type = other.type;
                this->status = other.status;
            };
            Job& operator=(const Job& other){
                this->id = other.id;                
                this->entry = other.entry;
                this->type = other.type;                
                this->status = other.status;
                return *this;
            };

        };

        // Used to build the Stack
        // TODO: should probably add some mutex here
        struct DisplayItem {
            std::string name;

            unsigned insId;
            unsigned insType;
            unsigned itemId;
            unsigned itemType;
            unsigned etc;
            unsigned origin;
            
            DisplayItem(){

            }

            void Function(unsigned insId, unsigned argN, unsigned origin = 0){ 
                this->insId = insId;
                this->itemType = CV::ItemTypes::FUNCTION;
                this->etc = argN;
                this->origin = origin;
            }
            
            void set(unsigned itemType, unsigned insId, unsigned origin = 0){
                this->insId = insId;
                this->itemType = itemType;
                this->origin = origin;
            }

        };

        struct Context : Item {

            std::unordered_map<unsigned, std::shared_ptr<CV::Item>> staticValues;
            std::shared_ptr<CV::Item> setStaticValue(const std::shared_ptr<CV::Item> &item);
            std::shared_ptr<CV::Item> getStaticValue(unsigned id);
            void flushStaticValue();

            // For helping the stack finding items (existing or not existing ones)
            std::shared_ptr<CV::DisplayItem> addDisplayItem(const std::string &name, std::shared_ptr<CV::DisplayItem> &ditem);
            std::shared_ptr<CV::DisplayItem> addDisplayItem(const std::string &name, unsigned insId, unsigned origin = 0); // generic
            std::shared_ptr<CV::DisplayItem> addDisplayItemFunction(const std::string &name, unsigned insId, unsigned argN, unsigned origin = 0);
            bool removeDisplayItem(unsigned id); // by insId
            void flushDisplayItems();
            std::shared_ptr<CV::DisplayItem> findDisplayItem(const std::string &name);
            std::unordered_map<std::string, std::shared_ptr<CV::DisplayItem>> displayItems;
            

            std::unordered_map<std::string, std::shared_ptr<CV::Item>> vars;
            std::shared_ptr<Context> upper;
            std::shared_ptr<Context> top;
            bool temporary;
            bool readOnly;
            uint64_t createdAt;

            std::vector<std::shared_ptr<CV::Job>> jobs;

            std::shared_ptr<CV::Job> getJobById(__CV_NUMBER_NATIVE_TYPE id);
            void addJob(std::shared_ptr<Job> &job);
            

            unsigned getJobNumber();

            Context(const Context &other){
                this->type = CV::ItemTypes::CONTEXT;        
             };
            Context& operator=(const Context& other){
                this->type = CV::ItemTypes::CONTEXT;        
                return *this;
            };

            void solidify(bool downstream);

            std::shared_ptr<CV::Item> get(const std::string &name);
            std::string getNameById(unsigned id);
            void reset(bool downstream);

            std::shared_ptr<CV::Item> copy(bool deep = true);

            void setTop(std::shared_ptr<Context> &nctx);
            ItemContextPair getWithContext(const std::string &name);
            ItemContextPair getWithContext(std::shared_ptr<CV::Item> &item);
            std::shared_ptr<CV::Item> set(const std::string &name, const std::shared_ptr<CV::Item> &item);
            Context();
            Context(std::shared_ptr<Context> &ctx);


            void debug();

        };        

        struct TokenBase {
            mutable std::mutex modMutex;
            std::vector<std::shared_ptr<CV::ModifierPair>> modifiers;
            void resetModifiers(){
                modMutex.lock();
                for(int i = 0; i < this->modifiers.size(); ++i){
                    this->modifiers[i]->used = false;
                }
                modMutex.unlock();
            }

            TokenBase(TokenBase &other){
                this->modifiers = other.modifiers;
            }

            TokenBase(const TokenBase &other){
                this->modifiers = other.modifiers;
            }

            TokenBase(){
            }            
            
            TokenBase& operator=(TokenBase& other){
                return *this;
            }      

            TokenBase& operator=(const TokenBase& other){
                return *this;
            }                   

            std::shared_ptr<CV::ModifierPair> getModifier(uint8_t type) const {
                modMutex.lock();
                for(int i = 0; i < modifiers.size(); ++i){
                    if(modifiers[i]->type == type){
                        auto v = modifiers[i]; 
                        modMutex.unlock();
                        return v;
                    }
                }
                modMutex.unlock();
                return std::shared_ptr<CV::ModifierPair>(NULL);
            }
            unsigned getModifierNumber(){
                modMutex.lock();
                auto n = modifiers.size();
                modMutex.unlock();
                return n;
            }
            
            void clearModifiers(){
                modMutex.lock();
                modifiers.clear();
                modMutex.unlock();
            }
            bool hasModifier(uint8_t type){
                modMutex.lock();
                for(int i = 0; i < modifiers.size(); ++i){
                    if(modifiers[i]->type == type){
                        modMutex.unlock();
                        return true;
                    }
                }
                modMutex.unlock();
                return false;
            }
        }; 

        // Text Token
        struct Token : TokenBase {
            bool complex;
            std::string first;
            std::string literal() const {
                std::string part = std::string("<")+(this->complex ? "(C) " : "")+this->first+">";
                for(int i = 0; i < modifiers.size(); ++i){
                    auto &mod = modifiers[i];
                    part += "("+CV::ModifierTypes::str(mod->type)+mod->subject+")";
                }
                return part;
            }
            Token(){
                complex = false;
            }
        };

        // Byte Code Token
        struct TokenByteCode : TokenBase {
            unsigned id;
            unsigned type;
            unsigned origin;
            bool expirable;
            std::string str;
            std::vector<unsigned> data;         // For Items
            std::vector<unsigned> parameter;    // For instructions only
            unsigned next;                      // NEXT INSTRUCTION ID
            TokenByteCode(unsigned type);
            TokenByteCode();
            unsigned first();
            void inheritModifiers(CV::Token &token);
            void inheritModifiers(std::vector<std::shared_ptr<CV::ModifierPair>> &mods);
            std::string literal(CV::Context *ctx = NULL, CV::Stack *stack = NULL);
        };


        struct Stack {
            std::mutex accessMutex;
            std::unordered_map<unsigned, std::shared_ptr<CV::TokenByteCode>> instructions;
            std::shared_ptr<CV::TokenByteCode> create(unsigned type){
                this->accessMutex.lock();
                auto code = std::make_shared<CV::TokenByteCode>(type);
                this->instructions[code->id] = code;
                this->accessMutex.unlock();
                return code;
            }
        };        


        struct ItemContextPair {
            Item *item;
            Context *context;
            std::string name;
            bool error;
            ItemContextPair(){
                item = NULL;   
                context = NULL;
                error = false;
            }
            ItemContextPair(Item *item, Context *ctx, const std::string &name, bool error = false){
                this->item = item;
                this->context = ctx;
                this->name = name;
                this->error = error;
            }
        };         

        struct FunctionConstraints {
            bool enabled;

            bool useMinParams;
            int minParams;

            bool useMaxParams;
            int maxParams;

            bool allowMisMatchingTypes;
            bool allowNil;

            std::vector<uint8_t> expectedTypes;
            std::unordered_map<int, uint8_t> expectedTypeAt;

            FunctionConstraints();

            void setExpectType(uint8_t type);

            void setExpectedTypeAt(int pos, uint8_t type);

            void clearExpectedTypes();

            void setMaxParams(unsigned n);
            void setMinParams(unsigned n);

            bool test(List *list, std::string &errormsg);
            bool test(const std::vector<std::shared_ptr<CV::Item>> &items, std::string &errormsg);
            bool testAgainst(std::shared_ptr<CV::String> &item, const std::vector<std::string> &opts, std::string &errormsg);
            bool testRange(std::shared_ptr<CV::Number> &item, __CV_NUMBER_NATIVE_TYPE min, __CV_NUMBER_NATIVE_TYPE max, std::string &errormsg);
            bool testRange(std::shared_ptr<CV::List> &item, __CV_NUMBER_NATIVE_TYPE min, __CV_NUMBER_NATIVE_TYPE max, std::string &errormsg);
            bool testListUniformType(std::shared_ptr<CV::List> &item, uint8_t type, std::string &errormsg);

        };        

        std::shared_ptr<Item> runTrait(std::shared_ptr<Item> &item, const std::string &name, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects);
        std::shared_ptr<Item> runTrait(std::shared_ptr<Item> &item, uint8_t type, const std::string &value, std::shared_ptr<CV::Cursor> &cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects);   
        void AddStandardOperators(std::shared_ptr<CV::Context> &ctx);
        bool ContextStep(std::shared_ptr<CV::Context> &cv);
        void setUseColor(bool v);
        std::string ItemToText(CV::Item *item);
        std::string getPrompt();

        std::shared_ptr<CV::Item> interpret(const std::string &input, std::shared_ptr<CV::Context> &ctx, std::shared_ptr<CV::Cursor> &cursor, bool flushTemps = true);
        void flushContextTemps(std::shared_ptr<CV::Context> &ctx);


        static std::vector<std::shared_ptr<CV::Item>> toList(const std::vector<std::string> &strings){
            std::vector<std::shared_ptr<CV::Item>> result;
            auto toStr = [](const std::string &str){
                return std::make_shared<CV::String>(str);
            };
            for(int i = 0; i < strings.size(); ++i){
                result.push_back(toStr(strings[i]));
            }
            return result;
        }

        static std::vector<std::shared_ptr<CV::Item>> toList(const std::vector<__CV_NUMBER_NATIVE_TYPE> &numbers){
            std::vector<std::shared_ptr<CV::Item>> result;

            auto toNumber = [](__CV_NUMBER_NATIVE_TYPE n){
                return std::make_shared<CV::Number>(n);
            };
            for(int i = 0; i < numbers.size(); ++i){
                result.push_back(toNumber(numbers[i]));
            }
            return result;
        }


    }

#endif