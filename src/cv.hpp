#ifndef CANVAS_INTERPRETER_HPP
    #define CANVAS_INTERPRETER_HPP

    #include <cstring>
    #include <vector>
    #include <memory>
    #include <mutex>
    #include <thread>
    #include <functional>
    #include <sstream>
    #include <unordered_map>
    #include <iomanip>
    #include <cmath>

    namespace CV {
        
        static const std::vector<std::string> RESERVED_WORDS = {
            "nil", "set", "proto", "fn",
            "ret", "skip", "stop",
            "if", "do", "iter",
            "for", "mut"
        };

        static bool isReserved(const std::string &word){
            for(unsigned i = 0; i < RESERVED_WORDS.size(); ++i){
                if(word == RESERVED_WORDS[i]){
                    return true;
                }
            }
            return false;
        }

        namespace ItemTypes {
            enum ItemTypes : unsigned {
                NIL,
                NUMBER,
                STRING,
                LIST,
                PROTO,
                FUNCTION,
                INTERRUPT,
                CONTEXT
            };
        }
        namespace InterruptTypes {
            enum InterruptTypes : unsigned {
                STOP,
                SKIP,
                RET		
            };
        }
        namespace Tools {
            unsigned genItemCountId();
            bool isNumber(const std::string &s);
            double positive(double n);
            bool isValidVarName(const std::string &s);
            bool isString(const std::string &s);
            std::vector<std::string> split(const std::string &str, const char sep);
            bool isList(const std::string &s);
            std::string removeExtraSpace(const std::string &in);
            std::vector<std::string> parse(const std::string &input, std::string &error);
            bool isLineComplete(const std::string &input);
            int getModifiers(const std::string &input, std::unordered_map<std::string, std::string> &mods);
            std::vector<std::string> getCleanTokens(std::string &input, std::string &error);
        }

        struct Cursor {
            int line;
            int column;
            bool error;
            std::string message;
            Cursor(){
                line = 0;
                column = 0;
                error = false;
                message = "";
            }
            void setError(const std::string &message, int line, int column){
                this->message = message;
                this->line = line;
                this->column = column;
                this->error = true;
            }
            void setError(const std::string &message){
                this->message = message;
                this->error = true;
            }
        };        


        struct Item {
            std::string name;
            unsigned id;
            void *data;
            unsigned size;
            unsigned type;
            std::unordered_map<std::string,std::shared_ptr<Item>> members;
            std::shared_ptr<Item> head;
            Item();
            void clear();
            void write(void *data, size_t size, int type);
            virtual std::shared_ptr<Item> copy();

            void registerProperty(const std::string &name, const std::shared_ptr<Item> &v);

            void deregisterProperty(const std::string &name);

            std::shared_ptr<Item> findProperty(const std::string &name);
            Item *findContext(const std::string &name);

            std::shared_ptr<Item> findAndSetProperty(const std::string &name, const std::shared_ptr<Item> &v);
            std::shared_ptr<Item> getProperty(const std::string &name);
            virtual std::string str(bool singleLine = false) const;

            virtual operator std::string() const;
        };

        std::shared_ptr<Item> create(double n);
        std::shared_ptr<Item> infer(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);
        std::shared_ptr<Item> eval(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);
        


        struct NumberType : Item {


            NumberType();
            ~NumberType();
            void set(int n);
            void set(float n);
            void set(double n);
            double get();

            std::shared_ptr<Item> copy();

            std::string str(bool singleLine = false) const;

        };

        struct StringType : Item {
            std::string literal;
            StringType();
            std::shared_ptr<Item> copy();
            std::shared_ptr<Item> get(int index);
            void set(const std::string &v);
            std::string str(bool singleLine = false) const;
        };

        struct ListType : Item {
            std::vector<std::shared_ptr<Item>> list;
            ListType(int n = 0);
            void build(const std::vector<std::shared_ptr<Item>> &objects);
            void add(const std::shared_ptr<Item> &item);
            std::shared_ptr<Item> get(int index);
            std::shared_ptr<Item> copy();
            std::string str(bool singleLine = false) const;
        };


        struct FunctionType : Item {
            std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor)> lambda;
            std::vector<std::string> params;
            std::string body;
            bool inner;
            FunctionType();
            FunctionType(const std::function<std::shared_ptr<Item>(const std::vector<std::shared_ptr<Item>> &operands, std::shared_ptr<Item> &ctx, Cursor *cursor)> &lambda,
                        const std::vector<std::string> &params = {});
            
            void set(const std::string &body, const std::vector<std::string> &params = {});
            
            std::shared_ptr<Item> copy();
            std::string str(bool singleLine = false) const;
        };

        struct InterruptType : Item {
            int intype;
            std::shared_ptr<Item> payload;
            std::shared_ptr<Item> copy();
            InterruptType(int intype = InterruptTypes::STOP, std::shared_ptr<Item> payload = std::make_shared<Item>(Item()));
            std::string str(bool singleLine = false) const;
        };

        struct ProtoType : Item {

            ProtoType();
            void populate(const std::vector<std::string> &names, const std::vector<std::shared_ptr<Item>> &values, std::shared_ptr<Item> &ctx, Cursor *cursor);

            void add(const std::string &name, std::shared_ptr<Item> &item, std::shared_ptr<Item> &ctx, Cursor *cursor);

            std::vector<std::string> getKeys();
            // std::shared_ptr<Item> copy(){
            // 	auto copied = std::shared_ptr<MOCK>(new MOCK());
            // 	for(auto &it : this->items){
            // 		copied->items[it.first] = it.second;
            // 	}
            // 	return std::static_pointer_cast<Item>(copied);
            // }

            std::string str(bool singleLine = false) const;
            

        };

        std::shared_ptr<CV::Item> create(double n);
        std::shared_ptr<CV::Item> createContext(const std::shared_ptr<CV::Item> &head = std::shared_ptr<CV::Item>(NULL));
        std::shared_ptr<CV::ListType> createList(int n);
        std::shared_ptr<CV::ProtoType> createProto();
        std::shared_ptr<CV::StringType> createString(const std::string &str = "");


        std::shared_ptr<Item> infer(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);
        std::shared_ptr<Item> eval(const std::string &input, std::shared_ptr<Item> &ctx, Cursor *cursor);

        std::string printContext(std::shared_ptr<CV::Item> &ctx, bool ignoreInners = true);

        void registerEmbeddedOperators(std::shared_ptr<CV::Item> &ctx);
    }

    #include "libs/io.hpp"
    #include "libs/brush.hpp"
    #include "libs/math.hpp"


#endif