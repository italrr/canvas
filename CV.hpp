#ifndef CANVAS_HPP
    #define CANVAS_HPP

    #include <vector>
    #include <unordered_map>
    #include <memory>
    #include <string>
    #include <mutex>

    #define CV_DEFAULT_NUMBER_TYPE double
    typedef CV_DEFAULT_NUMBER_TYPE number;
    #define CV_RELEASE_VERSION number[3] = { 1, 0, 0 }
    #define CV_RELEASE_DATE "Oct. 1st 2025"; 

    #define CV_ERROR_MSG_NOOP_NO_INSTRUCTIONS "Provided no instructions"

    namespace CV {
        ////////////////////////////
        //// TOOLS
        ///////////////////////////

        ////////////////////////////
        //// TYPES
        ///////////////////////////
        namespace QuantType {
            enum QuantType : int {
                NIL,
                NUMBER,
                STRING,
                LIST,
                DICTIONARY,
                PROTOTYPE
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
                    case QuantType::DICTIONARY: {
                        return "DICTIONARY";
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
        };

        struct TypeNumber : CV::Quant {
            number v;
            TypeNumber();
            virtual bool clear();
        };

        struct TypeString : CV::Quant {
            std::string v;
            TypeString();
            virtual bool clear();
        };

        struct TypeList : CV::Quant {
            std::vector<std::shared_ptr<CV::Quant>> v;
            TypeList();
            virtual bool clear();
        };

        ////////////////////////////
        //// PARSING
        ///////////////////////////

        struct Cursor {
            std::mutex accessMutex;
            std::string message;
            std::string title;
            bool autoprint;
            bool error;
            unsigned line;
            std::shared_ptr<Quant> subject;
            bool shouldExit;
            bool used;
            Cursor();
            std::string getRaised();
            bool raise();
            void reset();
            void setError(const std::string &title, const std::string &message, unsigned line);
            void setError(const std::string &title, const std::string &message, unsigned line, std::shared_ptr<Quant> subject); 
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
            std::string str(int c = 0) const {
                std::string out = "c"+std::to_string(c)+this->first + (this->inner.size() > 0  ? " " : "");
                for(int i = 0; i < this->inner.size(); ++i){
                    out += inner[i]->inner.size() == 0 ? inner[i]->first : "["+inner[i]->str(c+1)+"]";
                    if(i < this->inner.size()-1){
                        out += " ";
                    }
                }
                return out;
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
        #define CV_INS_RANGE_TYPE_PROXY 1000

        namespace InstructionType {
            enum InstructionType : int {
                INVALID = 0,
                NOOP,
                // CONSTRUCTORS
                LET = CV_INS_RANGE_TYPE_CONSTRUCTORS,
                MUT,
                CONSTRUCT_LIST,

                // PROXIES
                STATIC_PROXY = CV_INS_RANGE_TYPE_PROXY,  // DATA[0] -> CTX_ID, DATA[1] -> DATA_ID

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
            std::shared_ptr<CV::Context> head;
            Context();
            Context(const std::shared_ptr<CV::Context> &head);
            std::shared_ptr<CV::TypeNumber> buildNumber(number n);
            std::shared_ptr<CV::Quant> buildNil();
            std::shared_ptr<CV::TypeList> buildList(const std::vector<std::shared_ptr<CV::Quant>> &list = {});
            std::shared_ptr<CV::TypeString> buildString(const std::string &s);
            std::shared_ptr<CV::Quant> get(int id);
        };

        struct Program {
            unsigned entrypointIns;
            std::shared_ptr<Context> rootContext;
            std::unordered_map<unsigned, std::shared_ptr<CV::Context>> ctx; 
            std::unordered_map<unsigned, std::shared_ptr<CV::Instruction>> instructions;
            std::shared_ptr<CV::Instruction> createInstruction(unsigned type, const std::shared_ptr<CV::Token> &token);
            std::shared_ptr<Context> createContext(const std::shared_ptr<CV::Context> &head = std::shared_ptr<CV::Context>(NULL));
            bool deleteContext(int id);
        };

        struct ControlFlow {
            int state;
            std::shared_ptr<CV::Quant> payload;
        };

        typedef std::shared_ptr<CV::Instruction> InsType;
        typedef std::shared_ptr<CV::Cursor> CursorType;
        typedef std::shared_ptr<CV::Program> ProgramType;
        typedef std::shared_ptr<CV::Token> TokenType;
        typedef std::shared_ptr<CV::Context> ContextType;


        std::string QuantToText(const std::shared_ptr<CV::Quant> &t);
        std::shared_ptr<CV::Quant> Execute(const CV::InsType &entry, const CV::ContextType &ctx, const CV::ProgramType &prog, const CV::CursorType &cursor);
        CV::ProgramType Compile(const std::string &input, CV::CursorType &cursor);
        CV::InsType Translate(const CV::TokenType &token, const CV::ProgramType &prog, const CV::ContextType &ctx, const CV::CursorType &cursor);

    }

#endif