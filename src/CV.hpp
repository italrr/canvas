#ifndef CANVAS_HPP
    #define CANVAS_HPP

    #include <vector>
    #include <cctype>
    #include <unordered_map>
    #include <memory>
    #include <thread>
    #include <set>
    #include <string>
    #include <functional>
    #include <mutex>

    #define CV_DEFAULT_NUMBER_TYPE double
    typedef CV_DEFAULT_NUMBER_TYPE CV_NUMBER;

    #define CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS "Provided no instructions"
    #define CV_ERROR_MSG_WRONG_TYPE "Provided wrong types"
    #define CV_ERROR_MSG_WRONG_OPERANDS "Provided wrong operands"
    #define CV_ERROR_MSG_INVALID_INSTRUCTION "Invalid Instruction Type"
    #define CV_ERROR_MSG_MISUSED_IMPERATIVE "Misused Imperative"
    #define CV_ERROR_MSG_UNDEFINED_IMPERATIVE "Undefined imperative, name or constructor within this context or above"
    #define CV_ERROR_MSG_MISUSED_CONSTRUCTOR "Misused Constructor"
    #define CV_ERROR_MSG_INVALID_ACCESOR "Invalid Accessor"
    #define CV_ERROR_MSG_INVALID_INDEX "Invalid Index"
    #define CV_ERROR_MSG_MISUSED_PREFIX "Misused Prefix" 
    #define CV_ERROR_MSG_MISUSED_FUNCTION "Misused Operator/Function" 
    #define CV_ERROR_MSG_ILLEGAL_PREFIXER "Illegal Prefixer"
    #define CV_ERROR_MSG_ILLEGAL_ITERATOR "Illegal Iterator"
    #define CV_ERROR_MSG_INVALID_SYNTAX "Invalid Syntax"
    #define CV_ERROR_MSG_LIBRARY_NOT_VALID "Invalid Library Import"
    #define CV_ERROR_MSG_STORE_UNDEFINED_MEMBER "Undefined Named Type"


    namespace CV {

        static const CV_NUMBER VERSION[3] = { 0, 9, 1 };
        static const std::string RELEASE = "Dec. 20th 2025"; 

        ////////////////////////////
        //// PLATFORM
        ///////////////////////////

        namespace SupportedPlatform {
            enum SupportedPlatform : int {
                WINDOWS,
                LINUX,
                OSX,
                UNKNOWN
            };
            static std::string name(int plat){
                switch(plat){
                    case SupportedPlatform::WINDOWS: 
                        return "WINDOWS";
                    case SupportedPlatform::LINUX:
                        return "LINUX";
                    case SupportedPlatform::OSX:
                        return "OSX";                        
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
            const static int ARCH = CV::SupportedArchitecture::X86_64;
        #elif defined(__arm__)
            const static int ARCH = SupportedArchitecture::ARM;
        #elif defined(__aarch64__)
            const static int ARCH = SupportedArchitecture::ARM64;
        #else
            const static int ARCH = SupportedArchitecture::UNKNOWN;
        #endif
        
        #ifdef _WIN32
            const static int PLATFORM = SupportedPlatform::WINDOWS;
            #define _CV_PLATFORM _CV_PLATFORM_TYPE_WINDOWS
        #elif __linux__
            const static int PLATFORM = CV::SupportedPlatform::LINUX;
            #define _CV_PLATFORM _CV_PLATFORM_TYPE_LINUX
        #else
            const static int PLATFORM = SupportedPlatform::UNSUPPORTED;
            #define _CV_PLATFORM _CV_PLATFORM_TYPE_UNDEFINED
        #endif  

        ////////////////////////////
        //// TYPES
        ///////////////////////////

        enum DataType {
            NIL,
            NUMBER,
            STRING,
            LIST,
            STORE,
            FUNCTION
        };

        static std::string DataTypeName(int v){
            switch(v){
                case CV::DataType::NUMBER: {
                    return "NUMBER";
                };
                case CV::DataType::STRING: {
                    return "STRING";
                };
                case CV::DataType::LIST: {
                    return "LIST";
                };  
                case CV::DataType::STORE: {
                    return "STORE";
                };
                case CV::DataType::FUNCTION: {
                    return "FUNCTION";
                };                                                                                                  
                default:
                case CV::DataType::NIL: {
                    return "NIL";
                };
            }
        }

        namespace ThreadState {
            enum ThreadState : int {
                UNDEFINED = 0,
                CREATED = 10,
                STARTED,
                FINISHED
            };
            static std::string name(int state){
                switch(state){
                    case CV::ThreadState::CREATED: {
                        return "CREATED";
                    };
                    case CV::ThreadState::STARTED: {
                        return "STARTED";
                    };
                    case CV::ThreadState::FINISHED: {
                        return "FINISHED";
                    };
                    default: {
                        return "UNDEFINED";
                    };
                }
            } 
        }

        struct Program;
        struct Context;
        struct Cursor;
        struct Token;
        struct ControlFlow;        
        struct Data;
        struct Instruction;

        typedef std::function<CV::Data*(
            const std::shared_ptr<CV::Program> &prog,
            const std::string &name,
            const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &params,
            const std::shared_ptr<CV::Token> &token,
            const std::shared_ptr<CV::Cursor> &cursor,
            const std::shared_ptr<CV::Context> &ctx,
            const std::shared_ptr<CV::ControlFlow> &st            
        )> Lambda;

        struct Data {
            int id;
            int ref;
            CV::DataType type;
            std::mutex mutexRefCount;
            Data();
            virtual ~Data() = default;
            virtual void clear(const std::shared_ptr<CV::Program> &prog);
            virtual void build(const std::shared_ptr<CV::Program> &prog);
            int getRefCount();
            void incRefCount();
            void decRefCount();          
        };

        struct DataNumber : Data {
            CV_NUMBER value;
            DataNumber(CV_NUMBER n = 0);
        };

        struct DataString : Data {
            std::string value;
            DataString(const std::string &value = "");
        };        

        struct DataList : Data {
            std::vector<int> value;
            DataList();
            void clear(const std::shared_ptr<CV::Program> &prog);
        };       

        struct DataStore : Data {
            std::unordered_map<std::string, int> value;
            DataStore();
            void clear(const std::shared_ptr<CV::Program> &prog);
        };           
        
        struct DataFunction : Data {
            bool isBinary;
            bool isVariadic;
            CV::Lambda lambda;
            int closureCtxId;
            std::shared_ptr<CV::Token> body;
            std::vector<std::string> params;
            std::vector<std::string> mandatory;
            DataFunction();
            void clear(const std::shared_ptr<CV::Program> &prog);
        };            
        
        ////////////////////////////
        //// PARSING
        ///////////////////////////

        namespace Prefixer {
            enum Prefixer : int {
                UNDEFINED,
                NAMER,
                EXPANDER,
                PARALELLER
            };
            static std::string name(int v){
                switch(v){
                    case CV::Prefixer::NAMER: {
                        return "NAMER";
                    };
                    case CV::Prefixer::EXPANDER: {
                        return "EXPANDER";
                    };
                    case CV::Prefixer::PARALELLER: {
                        return "PARALELLER";
                    };
                    case CV::Prefixer::UNDEFINED:
                    default: {
                        return "UNDEFINED";
                    };
                }
            }
        }

        struct Token;
        struct Cursor {
            std::mutex accessMutex;
            std::string message;
            std::string title;
            bool autoprint;
            bool error;
            unsigned line;
            std::shared_ptr<CV::Token> subject;
            bool shouldExit;
            bool used;
            Cursor();
            std::string getRaised();
            bool raise();
            void reset();
            void clear();
            void setError(const std::string &title, const std::string &message, const std::shared_ptr<CV::Token> &subject = NULL);
            void setError(const std::string &title, const std::string &message, int line);
        };

        typedef std::shared_ptr<Cursor> CursorType;

        struct Token {
            std::string first;
            unsigned line;
            bool solved;
            bool complex;
            std::vector<std::shared_ptr<Token>> inner;
            Token();
            Token(const std::string &first, unsigned line);    
            std::shared_ptr<CV::Token> emptyCopy();
            std::shared_ptr<CV::Token> copy();
            std::string str() const;
            void refresh();
        };

        typedef std::shared_ptr<Token> TokenType;

        ////////////////////////////
        //// JIT
        ///////////////////////////

        #define CV_INS_RANGE_TYPE_CONSTRUCTORS 100
        #define CV_INS_RANGE_TYPE_CONTROL_FLOW 200
        #define CV_INS_RANGE_TYPE_PROXY 1000
        #define CV_INS_PREFIXER_IDENTIFIER_INSTRUCTION 4000000

        namespace InstructionType {
            enum InstructionType : int {
                
                INVALID = 0,
                NOOP = 10,
                LIBRAY_IMPORT,
                
                // CONSTRUCTORS
                LET = CV_INS_RANGE_TYPE_CONSTRUCTORS,
                MUT,
                CARBON_COPY,
                CONSTRUCT_LIST,
                CONSTRUCT_STORE,

                // CONTROL FLOW
                CF_INTERRUPT = CV_INS_RANGE_TYPE_CONTROL_FLOW,
                CF_LOOP_DO,
                CF_LOOP_FOR,
                CF_LOOP_ITER,            
                CF_INVOKE_FUNCTION,
                CF_INVOKE_BINARY_FUNCTION,
                CF_RAW_TOKEN_REFERENCE,

                // PROXIES
                PROXY_STATIC = CV_INS_RANGE_TYPE_PROXY,
                PROXY_PROMISE,
                PROXY_DYNAMIC,
                PROXY_NAMER,
                PROXY_EXPANDER,
                PROXY_PARALELER,
                PROXY_ACCESS,
            };
        }

        namespace InterruptType {
            enum InterruptType : int {
                UNDEFINED = 0,
                RETURN = 10,
                YIELD,
                SKIP
            };
            static std::string name(int t){
                switch(t){
                    case CV::InterruptType::RETURN: {
                        return "RETURN";
                    };
                    case CV::InterruptType::YIELD: {
                        return "YIELD";
                    };
                    case CV::InterruptType::SKIP: {
                        return "SKIP";
                    };
                    default: {
                        return "UNDEFINED";
                    };

                }
            }
            static int type(const std::string &name){
                if(name == "YIELD"){
                    return CV::InterruptType::YIELD;
                }else
                if(name == "RETURN"){
                    return CV::InterruptType::RETURN;
                }else
                if(name == "SKIP"){
                    return CV::InterruptType::SKIP;
                }else{
                    return CV::InterruptType::UNDEFINED;
                }
            }
        }

        namespace ControlFlowState {
            enum ControlFlowState : int {
                CONTINUE,
                SKIP,
                YIELD,
                RETURN,
                CRASH
            };
        }

        struct Instruction {
            int id;
            int type;
            std::vector<int> params;
            std::vector<int> data;
            std::vector<std::string> literal;
            std::shared_ptr<Token> token;
            int next;
            int prev;
            Instruction(){
                this->next = CV::InstructionType::INVALID;
                this->prev = CV::InstructionType::INVALID;
                this->type = CV::InstructionType::INVALID;
                this->id = CV::InstructionType::INVALID;
            }
        };

        typedef std::shared_ptr<CV::Instruction> InsType;

        struct ControlFlow {
            int state;
            int ctx;
            int ref;
            int next;
            int current;
            int prev;
            CV::Data* payload;
            ControlFlow(){
                this->state = CV::ControlFlowState::CONTINUE;
                this->ref = 0;
                this->ctx = 0;
                this->next = CV::InstructionType::INVALID;
                this->current = CV::InstructionType::INVALID;
                this->prev = CV::InstructionType::INVALID;                
            }
        };
        
        typedef std::shared_ptr<CV::ControlFlow> CFType;

        ////////////////////////////
        //// Context
        ///////////////////////////        
        struct Context {
            int id;
            int top;
            std::set<int> dependants;
            std::mutex mutexData;
            std::mutex mutexNames;
            std::mutex mutexDependants;
            std::unordered_map<std::string, int> names;
            std::set<int> data;
            Context(int root = -1);
            void insertDependant(int ctxId);
            bool removeDependant(int id);
            bool isNamed(int dataId);
            std::pair<int,int> getNamed(const std::shared_ptr<CV::Program> &prog, const std::string &name, bool localOnly = false);
            bool isName(const std::string &name);
            void setName(const std::string &name, int id);
            bool removeName(int id);
            bool isDataIn(int id);
            bool setData(const std::shared_ptr<CV::Program> &prog, int dataId);
            bool removeData(const std::shared_ptr<CV::Program> &prog, int dataId);
            void clear(const std::shared_ptr<CV::Program> &prog);
            void registerFunction(
                const std::shared_ptr<CV::Program> &prog,
                const std::string &name,
                const std::vector<std::string> &params,
                const CV::Lambda &lambda
            );
            void registerFunction(
                const std::shared_ptr<CV::Program> &prog,
                const std::string &name,
                const std::vector<std::string> &params,
                const std::vector<std::string> &mandatory,
                const CV::Lambda &lambda
            );            
            void registerFunction(
                const std::shared_ptr<CV::Program> &prog,
                const std::string &name,
                const CV::Lambda &lambda
            );            
        };
        typedef std::shared_ptr<CV::Context> ContextType;
        
        ////////////////////////////
        //// Program
        ///////////////////////////
        struct Program : std::enable_shared_from_this<Program> {
            std::mutex mutexContext;
            std::mutex mutexStack;
            std::mutex mutexIns;

            std::unordered_map<int, std::shared_ptr<CV::Context>> contexts;
            std::unordered_map<int, CV::Data*> stack;
            std::unordered_map<int, CV::InsType> instructions;

            std::mutex prefetchMutex;
            std::unordered_map<int, int> prefetched;

            CV::ContextType root;

            Program();
            ~Program();

            void setPrefetch(int insId, int v);
            bool removePrefetch(int insId);
            int getPrefetch(int id);
            bool isPrefetched(int id);
            void clearPrefetch();            

            void end();
            CV::InsType createInstruction(unsigned type, const std::shared_ptr<CV::Token> &token);
            std::shared_ptr<CV::Instruction> &getIns(int id);

            bool swapId(int fdataId, int tdataId);
            bool placeData(int dataId, int ctxId);
            bool moveData(int dataId, int fctxId, int tctxId);
            
            CV::ContextType getContext(int id);
            CV::ContextType createContext(int root = -1);
            bool deleteContext(int id);

            CV::Data *getData(int dataId);

            int allocateData(CV::Data *ref);
            bool deallocateData(int id);

            CV::Data* buildNil();

            void quickGC();
        };

        typedef std::shared_ptr<CV::Program> ProgramType;

        ////////////////////////////
        //// TOOLS
        ///////////////////////////
        namespace Tools {
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
            void sleep(uint64_t t);
            std::string setTextColor(int color, bool bold = false);
            std::string setBackgroundColor(int color);
            bool fileExists(const std::string &path);
            std::string readFile(const std::string &path);
            bool isReservedWord(const std::string &name);
            bool isValidVarName(const std::string &name);
            std::string lower(const std::string &in);
            std::string upper(const std::string &in);
            std::string fileExtension(const std::string &filename);
            bool isInList(const std::string &v, const std::vector<std::string> &list);
            std::shared_ptr<CV::Instruction> fetchInstruction(
                const std::string &name,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins
            );
            namespace ErrorCheck {
                bool IsType(
                    const std::string &fname,
                    const std::string &vname,
                    CV::Data* v,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::ControlFlow> &st,
                    const CV::TokenType &token,
                    int expType
                );
                bool AreAllType(
                    const std::string &name,
                    const std::vector<CV::Data*> &params,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::ControlFlow> &st,
                    const CV::TokenType &token,
                    int expType
                );
                std::shared_ptr<CV::Instruction> FetchInstruction(
                    const std::string &fname,
                    const std::string &vname,
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::ControlFlow> &st,
                    const CV::TokenType &token
                );   
                
                CV::Data *SolveInstruction(
                    const std::shared_ptr<CV::Program> &prog,
                    const std::string &fname,
                    const std::string &vname,
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                    const std::shared_ptr<CV::Context> &ctx,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::ControlFlow> &st,
                    const CV::TokenType &token,
                    int expType
                );        
                std::shared_ptr<CV::Instruction> FetchInstructionIfExists(
                    const std::string &vname,
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins
                );
                CV::Data *SolveInstructionIfExists(
                    const std::shared_ptr<CV::Program> &prog,
                    const std::string &fname,
                    const std::string &vname,
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &ins,
                    const std::shared_ptr<CV::Context> &ctx,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::ControlFlow> &st,
                    const std::shared_ptr<CV::Token> &token,
                    int expType
                );                
            }

        }        


        ////////////////////////////
        //// Data To Text
        ///////////////////////////
        std::string DataToText(const std::shared_ptr<CV::Program> &prog, CV::Data *t);        


        ////////////////////////////
        //// API
        ///////////////////////////        
        CV::InsType Compile(const std::string &input, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx = NULL);
        CV::InsType Compile(const CV::TokenType &input, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx);
        CV::InsType Translate(const CV::TokenType &token, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);
        CV::Data *Execute(const CV::InsType &entry, const CV::ProgramType &prog, const CV::CursorType &cursor, const CV::ContextType &ctx, CFType cf);
        void SetupCore(const CV::ProgramType &prog);
        void SetUseColor(bool v);
        std::string GetPrompt();
        bool GetBooleanValue(CV::Data *input);
        CV::Data *Copy(
            const std::shared_ptr<CV::Program> &prog,
            CV::Data *subject            
        );
     
    }

#endif