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
            void setError(const std::string &v, int line = -1);
        };


        namespace ModifierTypes {
            enum ModifierTypes : uint8_t {
                UNDEFINED,
                NAMER,
                SCOPE,
                LINKER,
                EXPAND,
                UNTETHERED,
                TRAIT
            };
            static uint8_t getToken(char mod){
                if(mod == '~'){
                    return ModifierTypes::NAMER;
                }else
                if(mod == ':'){
                    return ModifierTypes::SCOPE;
                }else
                if(mod == '<'){
                    return ModifierTypes::LINKER;
                }else
                if(mod == '!'){
                    return ModifierTypes::UNTETHERED;
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
                    case ModifierTypes::UNTETHERED: {
                        return "!";
                    };                       
                    case ModifierTypes::TRAIT: {
                        return "|";
                    };              
                    case ModifierTypes::LINKER: {
                        return "<";
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
                INTERRUPT
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
                    default: {
                        return "UNDEFINED";
                    };         
                }
            }
        }

        namespace InterruptTypes {
            enum InterruptTypes : uint8_t {
                SKIP,
                STOP
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

        struct Item;
        struct Context;
        struct ItemContextPair;

        struct ModifierPair {
            uint8_t type;
            std::string subject;
            ModifierPair(){
                type = ModifierTypes::UNDEFINED;
            }
            ModifierPair(uint8_t type, const std::string &subject){
                this->type = type;
                this->subject = subject;

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
            std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects)> action;
            Trait();
        };


        struct Item {
            uint8_t type;
            std::unordered_map<std::string, Trait> traits;
            std::unordered_map<uint8_t, Trait> traitsAny;
            bool copyable;
            std::mutex accessMutex; 

            Item(const Item &other){

            };
            Item& operator=(const Item& other){
                return *this;
            };

            Item();

            void registerTrait(uint8_t type, const std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects)> &action);
            
            void registerTrait(const std::string &name, const std::function<std::shared_ptr<Item>(Item *subject, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects)> &action);
            
            std::shared_ptr<Item> runTrait(const std::string &name, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects);

            std::shared_ptr<Item> runTrait(uint8_t type, const std::string &value, Cursor *cursor, std::shared_ptr<Context> &ctx, CV::ModifierEffect &effects);   

            bool hasTrait(const std::string &name);

            bool hasTrait(uint8_t type);
                
            virtual void registerTraits();

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

            void registerTraits();

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

            void registerTraits();

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

            void registerTraits();

            bool isEq(std::shared_ptr<CV::Item> &item);

            std::shared_ptr<CV::Item> copy(bool deep = true);

            void set(const std::string &str);

            std::string get();

        };



        struct Function : Item {
            std::string body;
            std::vector<std::string> params;
            bool binary;
            bool threaded;
            bool async;
            bool variadic;


            std::vector<std::string> getParams();
            std::string getBody();

            Function(const Function &other){
            };

            Function& operator=(const Function& other){
                return *this;
            };

            std::function< std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx) > fn;
            std::shared_ptr<CV::Item> copy(bool deep = true);
            Function();
            Function(const std::vector<std::string> &params, const std::string &body, bool variadic = false);
            Function(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic = false);
            void registerTraits();
            void set(const std::vector<std::string> &params, const std::string &body, bool variadic);
            void set(const std::vector<std::string> &params, const std::function<std::shared_ptr<CV::Item> (const std::vector<std::shared_ptr<CV::Item>> &params, Cursor *cursor, std::shared_ptr<Context> &ctx)> &fn, bool variadic);
            
        };    

        struct JobHandle {
            std::vector<std::shared_ptr<CV::Item>> params;
            std::shared_ptr<CV::Function> func;
            bool async;
            bool threaded;
            bool running;
            std::thread thread;

            JobHandle(const JobHandle &other){
                this->params = other.params;
                this->func = other.func;
                this->async = other.async;
                this->threaded = other.threaded;
                this->running = other.running;
            };
            JobHandle& operator=(const JobHandle& other){
                this->params = other.params;
                this->func = other.func;
                this->async = other.async;
                this->threaded = other.threaded;
                this->running = other.running;     
                return *this;
            };

        };


        struct Context : Item {

            std::unordered_map<std::string, std::shared_ptr<CV::Item>> vars;
            std::shared_ptr<Context> head;
            bool readOnly;

            std::vector<CV::JobHandle> jobs;

            Context(const Context &other){
                this->type = CV::ItemTypes::CONTEXT;        
             };
            Context& operator=(const Context& other){
                this->type = CV::ItemTypes::CONTEXT;        
                return *this;
            };

            bool isDone();

            std::shared_ptr<CV::Item> get(const std::string &name);

            std::shared_ptr<CV::Item> copy(bool deep = true);

            void setTop(std::shared_ptr<Context> &nctx);
            ItemContextPair getWithContext(const std::string &name);
            ItemContextPair getWithContext(std::shared_ptr<CV::Item> &item);
            std::shared_ptr<CV::Item> set(const std::string &name, const std::shared_ptr<CV::Item> &item);
            Context();
            Context(std::shared_ptr<Context> &ctx);


            void debug();

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

        };        


        struct Token {
            std::string first;
            std::vector<CV::ModifierPair> modifiers;
            CV::ModifierPair getModifier(uint8_t type) const {
                for(int i = 0; i < modifiers.size(); ++i){
                    if(modifiers[i].type == type){
                        return modifiers[i];
                    }
                }
                return CV::ModifierPair();
            }
            bool hasModifier(uint8_t type){
                for(int i = 0; i < modifiers.size(); ++i){
                    if(modifiers[i].type == type){
                        return true;
                    }
                }
                return false;
            }
            std::string literal() const {
                std::string part = "<"+this->first+">";
                for(int i = 0; i < modifiers.size(); ++i){
                    auto &mod = modifiers[i];
                    part += "("+CV::ModifierTypes::str(mod.type)+mod.subject+")";
                }
                return part;
            }
        };


        void AddStandardOperators(std::shared_ptr<CV::Context> &ctx);
        std::shared_ptr<CV::Item> interpret(const std::string &input, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx);
        std::shared_ptr<CV::Item>  interpret(const CV::Token &token, CV::Cursor *cursor, std::shared_ptr<CV::Context> &ctx);
        std::string ItemToText(CV::Item *item, bool useColors = false);


    }

#endif