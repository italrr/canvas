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

        static const CV_NUMBER VERSION[3] = { 1, 0, 0 };
        static const std::string RELEASE = "April 6th 2026"; 

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
            #define CV_PLATFORM_TYPE_WINDOWS
        #elif __linux__
            const static int PLATFORM = CV::SupportedPlatform::LINUX;
            #define CV_PLATFORM_TYPE_LINUX
        #else
            const static int PLATFORM = SupportedPlatform::UNSUPPORTED;
            #define CV_PLATFORM_TYPE_UNDEFINED
        #endif  


        struct Cursor;
        struct Data;
        struct Context;
        typedef std::shared_ptr<Cursor> CursorType;

        ////////////////////////////
        //// TYPES
        ///////////////////////////

        enum DataType {
            NIL,
            NUMBER,
            STRING,
            LIST,
            STORE,
            FUNCTION,
            CONTEXT, 
            PROXY
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
                case CV::DataType::CONTEXT: {
                    return "CONTEXT";
                };       
                case CV::DataType::PROXY: {
                    return "PROXY";
                };                                                                                                                                      
                default:
                case CV::DataType::NIL: {
                    return "NIL";
                };
            }
        }

        struct Cursor;
        struct Token;
        struct Data;
        struct Context;

        struct Data {
            CV::DataType type;
            Data();
            virtual std::shared_ptr<CV::Data> unwrap(){ return std::make_shared<CV::Data>(); };
        };

        struct DataNumber : Data, std::enable_shared_from_this<CV::DataNumber> {
            CV_NUMBER v;
            DataNumber();
            std::shared_ptr<CV::Data> unwrap() override;
        };      
        
        struct DataString : Data, std::enable_shared_from_this<CV::DataString> {
            std::string v;
            DataString();
            std::shared_ptr<CV::Data> unwrap() override;
        };   
        
        struct DataList : Data, std::enable_shared_from_this<CV::DataList> {
            std::vector<std::shared_ptr<CV::Data>> v;
            DataList();
            std::shared_ptr<CV::Data> unwrap() override;
        };    
        
        struct DataStore : Data, std::enable_shared_from_this<CV::DataStore> {
            std::unordered_map<std::string, std::shared_ptr<CV::Data>> v;
            DataStore();
            std::shared_ptr<CV::Data> unwrap() override;
        };   

        struct DataFunction : Data, std::enable_shared_from_this<CV::DataFunction> {
            std::vector<std::string> params;
            bool isLambda;
            bool isVariadic;
            std::shared_ptr<CV::Token> body;
            std::function<std::shared_ptr<CV::Data>(
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
                const std::shared_ptr<CV::Context> &ctx,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::Token> &token
            )> lambda;
            DataFunction();
            std::shared_ptr<CV::Data> unwrap() override;
        }; 
        
        struct DataProxy : Data, std::enable_shared_from_this<CV::DataProxy> {
            std::string pname;
            int ptype;
            std::shared_ptr<CV::Data> target;
            DataProxy();
            std::shared_ptr<CV::Data> unwrap() override;
        };         

        struct Context : Data, std::enable_shared_from_this<CV::Context> {
            std::shared_ptr<Context> head;
            std::unordered_map<std::string, std::shared_ptr<CV::Data>> data;
            std::set<std::string> namedNames;
            Context();
            std::pair<std::shared_ptr<CV::Context>, std::shared_ptr<CV::Data>> getNamed(const std::string &name);
            std::shared_ptr<CV::Context> buildContext(bool inherit = true);
            std::shared_ptr<CV::Data> buildNil();
            std::shared_ptr<CV::DataNumber> buildNumber(CV_NUMBER v = 0);
            std::shared_ptr<CV::DataString> buildString(const std::string &v = "");
            std::shared_ptr<CV::DataList> buildList();
            std::shared_ptr<CV::DataStore> buildStore();
            std::shared_ptr<CV::Data> copy(const std::shared_ptr<CV::Data> &target);
            std::shared_ptr<CV::Data> unwrap() override;
            void registerFunction(
                const std::string &name,
                const std::vector<std::string> &params,
                const std::function<std::shared_ptr<CV::Data>(
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
                    const std::shared_ptr<CV::Context> &ctx,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::Token> &token
                )> &lambda
            );

            void registerFunction(
                const std::string &name,
                const std::function<std::shared_ptr<CV::Data>(
                    const std::vector<std::pair<std::string, std::shared_ptr<CV::Data>>> &args,
                    const std::shared_ptr<CV::Context> &ctx,
                    const std::shared_ptr<CV::Cursor> &cursor,
                    const std::shared_ptr<CV::Token> &token
                )> &lambda
            );            

        };  
        typedef std::shared_ptr<CV::Context> ContextType;
        
        ////////////////////////////
        //// PARSING
        ///////////////////////////

        namespace Prefixer {
            enum Prefixer : int {
                UNDEFINED,
                NAMER,
                EXPANDER
            };
            static std::string name(int v){
                switch(v){
                    case CV::Prefixer::NAMER: {
                        return "NAMER";
                    };
                    case CV::Prefixer::EXPANDER: {
                        return "EXPANDER";
                    };
                    case CV::Prefixer::UNDEFINED:
                    default: {
                        return "UNDEFINED";
                    };
                }
            }
        }

        namespace ControlFlowState {
            enum ControlFlowState : int {
                CONTINUE,
                SKIP,
                YIELD,
                RETURN
            };
        }
        
        struct ControlFlow {
            int state;
            std::shared_ptr<CV::Data> payload;
            ControlFlow(){
                this->state = CV::ControlFlowState::CONTINUE;
            }
        };   
        
        typedef std::shared_ptr<CV::ControlFlow> ControlFlowType;

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
            Token();
            Token(const std::string &first, unsigned line);    
            std::shared_ptr<CV::Token> emptyCopy();
            std::shared_ptr<CV::Token> copy();
            std::string str() const;
            void refresh();
        };

        typedef std::shared_ptr<Token> TokenType;

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
        }

        void SetUseColor(bool v);
        std::string GetPrompt();  
        std::string DataToText(const std::shared_ptr<CV::Data> &t);      

        bool CoreSetup(
            const std::shared_ptr<CV::Context> &ctx
        );

        std::vector<CV::TokenType> BuildTree(
            const std::string &input,
            const CV::CursorType &cursor
        );        

        std::shared_ptr<CV::Data> Interpret(
            const CV::TokenType &token,
            const CV::CursorType &cursor,
            const CV::ControlFlowType &cf,
            const CV::ContextType &ctx
        );

        std::shared_ptr<CV::Data> Import(
            const std::string &fname,
            const CV::ContextType &ctx,
            const CV::CursorType &cursor
        );

        std::shared_ptr<CV::Data> ImportDynamicLibrary(
            const std::string &path,
            const std::string &fname,
            const CV::ContextType &ctx,
            const CV::CursorType &cursor
        );        

    }

#endif