#include "Tools.hpp"
#include "Eval.hpp"

std::vector<std::string> parse(const std::string &input){
    auto clean = removeExtraSpace(input);
    std::vector<std::string> tokens;
    int open = 0;
    bool comm = false;
    bool str = false;
    std::string hold;
    for(int i = 0; i < input.length(); ++i){
        char c = input[i];
        if(c == '(') ++open;
        if(c == ')') --open;
        if(c == '#'){
            comm = !comm;
            continue;
        }
        if(comm){
            continue;
        }
        if(c == '\''){
            str = !str;
        }           
        hold += c;     
        if(!str && open == 0){
            if(c == ')' || c == ' ' || i ==  input.length()-1){
                if(hold != " "){
                    tokens.push_back(removeExtraSpace(hold));
                }
                hold = "";
            }
        }
    }
    return tokens;
}

std::shared_ptr<Object> infer(const std::string &input, Context &ctx, Cursor &cursor){
    auto v = ctx.find(input);
    // number
    if(isNumber(input) && input.find(' ') == -1){
        auto result = std::make_shared<Object>(Object());
        auto v = std::stod(input);
        result->write(&v, sizeof(v), ObjectType::NUMBER);
        return result;
    }
    // string
    if(isString(input)){
        auto result = std::make_shared<String>(String());
        result->literal = input.substr(1, input.length() - 2);
        return result;        
    }
    // variable
    if(v->type != ObjectType::NONE){
        return v;
    }else{
        // consider it a list otherwise list
        auto result = std::make_shared<List>(List());
        auto tokens = parse(input);
        std::vector<std::shared_ptr<Object>> list;
        for(int i = 0; i < tokens.size(); ++i){
            list.push_back(eval(tokens[i], ctx, cursor));
        }
        result->build(list);
        return result;      
    }

    return std::make_shared<Object>(Object());
}

std::unordered_map<std::string, std::string> fetchSpecs(std::vector<std::string> input, std::vector<std::string> &output, Context &ctx){
    std::unordered_map<std::string, std::string> traits;
    for(int i = 1; i < input.size(); ++i){
        if((input[i] == "with" || input[i] == "as" || "->") && i < input.size()-1){
            traits[input[i]] = input[i + 1];
            input.erase(input.begin() + i, input.begin() + i + 2);
            --i;
        }
    }
    output = input;
    return traits;
}

std::shared_ptr<Object> eval(const std::string &input, Context &ctx, Cursor &cursor){
    auto tokens = parse(input);
    auto imp = tokens[0];    

    if(cursor.error){
        std::cout << "exception line " << cursor.line << " col " << cursor.column << ": " << cursor.message << std::endl;
        std::exit(1);
    }

    if(tokens.size() == 1 && tokens[0].length() > 1 && tokens[0][0] == '(' && tokens[0][tokens[0].length()-1] == ')'){
        imp = tokens[0].substr(1, tokens[0].length() - 2);
        return eval(imp, ctx, cursor);
    }

    auto op = ctx.find(imp);
    // keyword
    if(isKeyword(imp)){
        auto specs = fetchSpecs(tokens, tokens, ctx);
        // def
        if(imp == "def"){
            std::string vname = tokens[1];
            auto val = eval(tokens[2], ctx, cursor);
            val->name = vname;
            ctx.add(val);
            return val;
        }else
        if(imp == "fn"){
            auto largs = tokens[1];
            if(largs.length() > 1 && largs[0] == '(' && largs[largs.length()-1] == ')'){
                largs = largs.substr(1, largs.length() - 2);
            }
            auto args = parse(largs);
            auto fn = std::make_shared<Function>(Function());
            fn->set(tokens[2], args);
            return fn;
        }else    
        // for
        if(imp == "for"){
            Context fctx(ctx);
            auto range = eval(tokens[1], ctx, cursor);
            auto itn = specs["with"];
            if(range->type != ObjectType::LIST){
                cursor.setError("'for' expects a List as range: for (range) `with (x,y)/i` (code)");
                return std::make_shared<Object>(Object());
            }
            auto list = std::static_pointer_cast<List>(range);
            std::shared_ptr<Object> last;
            for(int i = 0; i < list->n; ++i){
                auto _it = list->list[i];
                _it->name = itn;
                fctx.add(_it);
                last = eval(specs["as"], fctx, cursor);
                if(last->type == ObjectType::INTERRUPT){
                    auto interrupt = std::static_pointer_cast<Interrupt>(last);
                    if(interrupt->intype == InterruptType::BREAK){
                        last = interrupt->payload;
                        break;
                    }
                    if(interrupt->intype == InterruptType::CONTINUE){
                        last = interrupt->payload;                        
                        continue;
                    }    
                    if(interrupt->intype == InterruptType::RETURN){
                        last = interrupt->payload;
                        break;
                    }                                       
                }
            }
            return last;
        }else
        // print (temporarily hardcoded)
        if(imp == "print"){
            auto r = eval(tokens[1], ctx, cursor);
            std::cout << r->str() << std::endl;
            return r;
        }else
        if(imp == "if"){
            bool v = false;
            auto cond = eval(tokens[1], ctx, cursor);
            if(cond->type != ObjectType::NUMBER){
                cursor.setError("'if' expects a real number as conditional");
                return std::make_shared<Object>(Object());                    
            }
            if(tokens.size() > 4){
                cursor.setError("'if' expects 3 params max: (if (cond)(branch-true)(branch-false)");
                return std::make_shared<Object>(Object());                  
            }
            v = *static_cast<double*>(cond->data);
            return v ? eval(tokens[2], ctx, cursor) : (tokens.size() > 3 ? eval(tokens[3], ctx, cursor) : std::make_shared<Object>(Object()));
        }else
        if(imp == "continue"){
            auto interrupt = std::make_shared<Interrupt>(Interrupt(InterruptType::CONTINUE));
            if(tokens.size() > 1){
                interrupt->payload = eval(tokens[1], ctx, cursor);
            }
            return interrupt;
        }else
        if(imp == "break"){
            auto interrupt = std::make_shared<Interrupt>(Interrupt(InterruptType::BREAK));
            if(tokens.size() > 1){
                interrupt->payload = eval(tokens[1], ctx, cursor);
            }
            return interrupt;            
        }else
        if(imp == "return"){
            auto interrupt = std::make_shared<Interrupt>(Interrupt(InterruptType::RETURN));
            if(tokens.size() > 1){
                interrupt->payload = eval(tokens[1], ctx, cursor);
            }
            return interrupt;            
        }
    }else
    // run lambda
    if(op->type == ObjectType::LAMBDA){
        Context lctx(ctx);
        auto func = static_cast<Function*>(op.get());
        bool useParams = func->params.size() > 0;
        if(useParams && func->params.size() != tokens.size()-1){
            std::cout << "lambda '" << func->name << "': expected (" << func->params.size() << ") arguments, received (" << tokens.size()-1 << ")" << std::endl;
            return std::make_shared<Object>(Object());
        }
        std::vector<std::shared_ptr<Object>> operands;
        for(int i = 1; i < tokens.size(); ++i){
            auto ev = eval(tokens[i], lctx, cursor);
            ev->name = std::to_string(i-1);
            if(useParams){
                ev->name = func->params[i-1];
            }
            lctx.add(ev);
            operands.push_back(ev);
        } 
        return func->lambda(operands, lctx, cursor);       
    }else{
    // deduce type
        return infer(input, ctx, cursor);
    }

    return std::make_shared<Object>(Object());
}