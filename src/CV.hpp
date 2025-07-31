#ifndef CANVAS_HPP
    #define CANVAS_HPP

    #include <vector>
    #include <cctype>
    #include <unordered_map>
    #include <memory>
    #include <string>
    #include <functional>
    #include <mutex>

    #define CV_DEFAULT_NUMBER_TYPE double
    typedef CV_DEFAULT_NUMBER_TYPE number;
    static const number CV_RELEASE_VERSION[3] = { 1, 0, 0 };
    static const std::string CV_RELEASE_DATE = "Oct. 1st 2025"; 

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

    #define _CV_PLATFORM_TYPE_LINUX 0
    #define _CV_PLATFORM_TYPE_WINDOWS 1
    #define _CV_PLATFORM_TYPE_OSX 2
    #define _CV_PLATFORM_TYPE_UNDEFINED 4

    #define CV_BINARY_FN_PARAMS const std::vector<std::shared_ptr<CV::Instruction>> &, const std::string&, const CV::TokenType &, const CV::CursorType &, int, int, int, const std::shared_ptr<CV::Program> &, const CV::CFType &

    namespace CV {

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

        namespace QuantType {
            enum QuantType : int {
                NIL,
                NUMBER,
                STRING,
                LIST,
                STORE,
                PROTOTYPE,
                FUNCTION,
                BINARY_FUNCTION
            };
            static std::string name(int t){
                switch(t){
                    case QuantType::NIL: {
                        return "NIL";
                    };
                    case QuantType::NUMBER: {
                        return "NUMBER";
                    };
                    case QuantType::LIST: {
                        return "LIST";
                    };
                    case QuantType::STORE: {
                        return "STORE";
                    };
                    case QuantType::PROTOTYPE: {
                        return "PROTOTYPE";
                    };
                    default:
                        return "UNDEFINED";
                }
            }
        }

        struct Quant {
            int id;
            int type;
            Quant();
            virtual bool clear(){
                return true;
            }
            virtual bool isInit(){
                return true;
            }
            virtual std::shared_ptr<Quant> copy();
        };

        struct TypeNumber : CV::Quant {
            number v;
            TypeNumber();
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };

        struct TypeString : CV::Quant {
            std::string v;
            TypeString();
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };

        struct TypeList : CV::Quant {
            std::vector<std::shared_ptr<CV::Quant>> v;
            TypeList();
            virtual bool isInit();        
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };

        struct TypeStore : CV::Quant {
            std::unordered_map<std::string, std::shared_ptr<CV::Quant>> v;
            TypeStore();
            virtual bool isInit();    
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };        
        struct Token;
        struct TypeFunction : CV::Quant {
            int entrypoint;
            int ctxId;
            std::string name;
            std::vector<std::string> params;
            std::shared_ptr<CV::Token> body;
            TypeFunction();
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };        

        struct Program;
        struct TypeFunctionBinary : TypeFunction {
            void *ref;
            std::unordered_map<int, bool> multiParam;
            TypeFunctionBinary();
            virtual bool clear();
            virtual std::shared_ptr<CV::Quant> copy();
        };          

        ////////////////////////////
        //// PARSING
        ///////////////////////////

        namespace Prefixer {
            enum Prefixer : int {
                UNDEFINED,
                NAMER
            };
            static std::string name(int v){
                switch(v){
                    case CV::Prefixer::NAMER: {
                        return "NAMER";
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

        struct Token {
            std::string first;
            unsigned line;
            bool solved;
            bool complex;
            std::vector<std::shared_ptr<Token>> inner;
            Token(){
                solved = false;
                complex = false;
            }
            Token(const std::string &first, unsigned line){
                this->first = first;
                this->line = line;
                refresh();
            }    
            std::shared_ptr<CV::Token> emptyCopy(){
                auto c = std::make_shared<CV::Token>();
                c->first = this->first;
                c->line = this->line;
                c->refresh();
                return c;
            }
            std::shared_ptr<CV::Token> copy(){
                auto c = std::make_shared<CV::Token>();
                c->first = this->first;
                c->line = this->line;
                c->inner = this->inner;
                c->refresh();
                return c;
            }
            std::string str() const {
                auto f = this->first;
                std::string out = f + (this->first.size() > 0 && this->inner.size() > 0  ? " " : "");
                for(int i = 0; i < this->inner.size(); ++i){
                    out += inner[i]->inner.size() == 0 ? inner[i]->first : inner[i]->str();
                    if(i < this->inner.size()-1){
                        out += " ";
                    }
                }
                return this->inner.size() > 0 ? "[" + out + "]" : out;
            }
            void refresh(){
                complex = this->inner.size() > 0;
                solved = !(first.length() >= 3 && first[0] == '[' && first[first.size()-1] == ']');
            }
        };

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
                CF_INVOKE_BINARY_FUNCTION,                  // DATA[0] -> CTX_ID, DATA[1] -> DATA_ID(owner), DATA[2]-> N_ARGS DATA[3] -> TARGET_DATA_ID | PARAMS... -> ARG_INS...
                CF_RAW_TOKEN_REFERENCE,

                // PROXIES
                PROXY_STATIC = CV_INS_RANGE_TYPE_PROXY,     // DATA[0] -> CTX_ID, DATA[1] -> DATA_ID
                PROXY_PROMISE,                              // DATA[0] -> CTX_ID, DATA[1] -> PROMISED_DATA_ID |  PARAM[0] -> TARGET_INS, 
                PROXY_DYNAMIC,                              // DATA[0] -> CTX_ID, DATA[1] -> DATA_ID, DATA[n] -> ...LITERAL, OPTIONAL: PARAM[0] -> preprocess
                PROXY_NAMER,                                // PARAM[0] -> TARGET_INS | LITERAL[0] -> NAME                            
                PROXY_ACCESS,                               // DATA[0] -> CTX_ID, DATA[1] -> DATA_ID, LITERAL[0] -> NAME
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

        struct Context {
            int id;
            std::unordered_map<unsigned, std::shared_ptr<CV::Quant>> memory;
            std::unordered_map<std::string, unsigned> names;
            std::unordered_map<unsigned, std::vector<unsigned>> prefetched;
            std::shared_ptr<CV::Context> head;
            Context();
            Context(const std::shared_ptr<CV::Context> &head);
            std::shared_ptr<CV::TypeNumber> buildNumber(number n = 0);
            std::shared_ptr<CV::Quant> buildCopy(const std::shared_ptr<CV::Quant> &subject);
            std::shared_ptr<CV::Quant> buildNil();
            std::shared_ptr<CV::Quant> buildType(int type);
            std::shared_ptr<CV::TypeList> buildList(const std::vector<std::shared_ptr<CV::Quant>> &list = {});
            std::shared_ptr<CV::TypeStore> buildStore(const std::unordered_map<std::string, std::shared_ptr<CV::Quant>> &list = {});
            std::shared_ptr<CV::TypeString> buildString(const std::string &s = "");
            std::shared_ptr<CV::Quant> get(int id);
            std::shared_ptr<TypeFunctionBinary> registerBinaryFuntion(const std::string &name, void *ref);
            std::vector<int> getName(const std::string &name);
        };

        struct Program {
            unsigned entrypointIns;
            std::shared_ptr<Context> rootContext;
            std::unordered_map<unsigned, void*> loadedDynamicLibs;
            std::unordered_map<unsigned, std::shared_ptr<CV::Context>> ctx; 
            std::unordered_map<unsigned, std::shared_ptr<CV::Instruction>> instructions;
            std::shared_ptr<CV::Instruction> createInstruction(unsigned type, const std::shared_ptr<CV::Token> &token);
            std::shared_ptr<Context> createContext(const std::shared_ptr<CV::Context> &head = std::shared_ptr<CV::Context>(NULL));
            bool deleteContext(int id);
        };

        struct ControlFlow {
            int state;
            int ctx;
            int ref;
            int next;
            int current;
            int prev;
            std::shared_ptr<CV::Quant> payload;
            ControlFlow(){
                this->state = CV::ControlFlowState::CONTINUE;
                this->ref = 0;
                this->ctx = 0;
                this->next = CV::InstructionType::INVALID;
                this->current = CV::InstructionType::INVALID;
                this->prev = CV::InstructionType::INVALID;                
            }
        };

        typedef std::shared_ptr<CV::Instruction> InsType;
        typedef std::shared_ptr<CV::Cursor> CursorType;
        typedef std::shared_ptr<CV::Program> ProgramType;
        typedef std::shared_ptr<CV::Token> TokenType;
        typedef std::shared_ptr<CV::Context> ContextType;
        typedef std::shared_ptr<CV::ControlFlow> CFType;

        ////////////////////////////
        //// TOOLS
        ///////////////////////////

        namespace Test {
            bool IsItPrefixInstruction(const std::shared_ptr<CV::Instruction> &ins);
        }
        
        namespace ErrorCheck {
            bool AllNumbers(const std::vector<std::shared_ptr<CV::Quant>> &args, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectsOperands(int prov, int exp, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectsTypeAt(int type, int exp, int at, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectsNoMoreOperands(int prov, int max, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectsExactlyOperands(int prov, int exp, const std::string &name, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectsExactlyOperands(int prov, int exp, const std::string &name, const std::vector<std::string> &names, const CV::TokenType &token, const CV::CursorType &cursor);
            bool ExpectNoPrefixer(const std::string &name, const std::vector<std::shared_ptr<CV::Instruction>> &args, const CV::TokenType &token, const CV::CursorType &cursor);
        }

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
            std::string setTextColor(int color, bool bold = false);
            std::string setBackgroundColor(int color);
            bool fileExists(const std::string &path);
            std::string readFile(const std::string &path);
            bool isReservedWord(const std::string &name);
            bool isValidVarName(const std::string &name);
        }

        ////////////////////////////
        //// API
        ///////////////////////////

        int ImportDynamicLibrary(const std::string &path, const std::string &fname, const std::shared_ptr<CV::Program> &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);
        int Import(const std::string &name, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);
        bool Unimport(int id);
        bool GetBooleanValue(const std::shared_ptr<CV::Quant> &data);
        std::string QuantToText(const std::shared_ptr<CV::Quant> &t);
        void SetUseColor(bool v);
        std::string GetLogo();
        void InitializeCore(const CV::ProgramType &prog);
        std::shared_ptr<CV::Quant> Execute(const CV::InsType &entry, const CV::ContextType &ctx, const CV::ProgramType &prog, const CV::CursorType &cursor, CFType cf = CFType(NULL));
        CV::InsType Compile(const std::string &input, const CV::ProgramType &prog, const CV::CursorType &cursor);
        CV::InsType Compile(const CV::TokenType &input, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);
        CV::InsType Translate(const CV::TokenType &token, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);

    }

#endif