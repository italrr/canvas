#include "Context.hpp"
#include "Object.hpp"

#include "Eval.hpp"

Context::Context(Context *head){
    // default operators
    this->add(std::make_shared<Function>(Function("+", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sum<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("-", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __sub<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("*", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __mult<double>(r, operands[i]);
        }
        return r;
    })));

    this->add(std::make_shared<Function>(Function("/", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __div<double>(r, operands[i]);
        }
        return r;
    })));  

    this->add(std::make_shared<Function>(Function("pow", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            r = __pow<double>(r, operands[i]);
        }
        return r;
    })));    

    this->add(std::make_shared<Function>(Function("=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) != *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));        

    this->add(std::make_shared<Function>(Function("!=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) == *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));    

    this->add(std::make_shared<Function>(Function(">", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) <= *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));          

    this->add(std::make_shared<Function>(Function("<", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) >= *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));      

    this->add(std::make_shared<Function>(Function(">=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) < *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));          

    this->add(std::make_shared<Function>(Function("<=", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(*static_cast<double*>(r->data) > *static_cast<double*>(operands[i]->data)){
                return infer("0", ctx, cursor);        
            }
            r = operands[i];
        }
        return infer("1", ctx, cursor);
    })));      

    this->add(std::make_shared<Function>(Function("..", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        if(operands.size() < 2){
            cursor.setError("operator '..' expects 2 operands");
            return r;
        }
        if(operands[0]->type != ObjectType::NUMBER || operands[1]->type != ObjectType::NUMBER){
            cursor.setError("operator '..' expects real numbers");
            return r;
        }
        std::vector<std::shared_ptr<Object>> elements;
        double step = 1; // hard coded for now
        double s = *static_cast<double*>(operands[0]->data);
        double e = *static_cast<double*>(operands[1]->data);
        if(s == e){
            return r;
        }
        bool inc = e > s;
        for(double i = s; (inc ? i < e : i > e); i += (inc ? step : -step)){
            elements.push_back(infer(std::to_string(i), ctx, cursor));
        }
        r->build(elements);
        return r;
    })));    

    this->add(std::make_shared<Function>(Function("l-reverse", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        std::vector<std::shared_ptr<Object>> elements;
        for(int j = 0; j < operands.size(); ++j){
            std::vector<std::shared_ptr<Object>> _elements;
            auto list = std::static_pointer_cast<List>(operands[operands.size() - 1 - j]);
            for(double i = 0; i < list->n; ++i){
                auto &item = list->list[list->n - 1 - i];
                _elements.push_back(item->copy());
            }            
            if(operands.size() == 1){
                elements = _elements;
            }else{
                auto r = std::make_shared<List>(List());
                r->build(_elements);
                elements.push_back(r);
            }
        }
        r->build(elements);
        return r;
    })));    


    this->add(std::make_shared<Function>(Function("cpy", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        
        if(operands.size() < 1){
            cursor.setError("operator 'cpy' expects at least 1 operands");
            return std::static_pointer_cast<Object>(r);
        }    
        
        if(operands.size() > 1){
            std::vector<std::shared_ptr<Object>> list;
            for(int i = 0; i < operands.size(); ++i){
                auto cpy = operands[i]->copy();
                list.push_back(cpy);
            }
            r->build(list);
        }else{
            auto first = operands[0]->copy();
            return first;
        }

        return std::static_pointer_cast<Object>(r);
    })));   

      

    this->add(std::make_shared<Function>(Function(">>", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        if(operands.size() < 2){
            cursor.setError("operator '>>' expects at least 2 operands");
            return  std::static_pointer_cast<Object>(std::make_shared<List>(List()));
        }    

        auto first = operands[0];
        for(int i = 1; i < operands.size(); ++i){
            if(operands[i]->type != ObjectType::LIST){
                if(first->type == ObjectType::LIST){
                    auto origlist = std::static_pointer_cast<List>(first->copy());
                    origlist->list.insert(origlist->list.begin(), operands[i]->copy());
                    auto r = std::make_shared<List>(List());
                    r->build(origlist->list);
                    first = r;                    
                }else{            
                    std::vector<std::shared_ptr<Object>> _elements;
                    _elements.push_back(operands[i]->copy());
                    _elements.push_back(first);
                    auto r = std::make_shared<List>(List());
                    r->build(_elements);
                    first = r;               
                }
            }else{
                auto origlist = std::static_pointer_cast<List>(operands[i]->copy());
                origlist->list.push_back(first);
                auto r = std::make_shared<List>(List());
                r->build(origlist->list);
                first = r;
            }
        }

        return first;
    })));


    this->add(std::make_shared<Function>(Function("<<", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        if(operands.size() < 2){
            cursor.setError("operator '<<' expects at least 2 operands");
            return  std::static_pointer_cast<Object>(std::make_shared<List>(List()));
        }    

        auto first = operands[operands.size()-1];
        for(int i = 1; i < operands.size(); ++i){
            int e = operands.size() - 1 - i;
            if(operands[e]->type != ObjectType::LIST){
                if(first->type == ObjectType::LIST){
                    auto origlist = std::static_pointer_cast<List>(first->copy());
                    origlist->list.push_back(operands[e]->copy());
                    auto r = std::make_shared<List>(List());
                    r->build(origlist->list);
                    first = r;                    
                }else{            
                    std::vector<std::shared_ptr<Object>> _elements;
                    _elements.push_back(first);
                    _elements.push_back(operands[e]->copy());
                    auto r = std::make_shared<List>(List());
                    r->build(_elements);
                    first = r;               
                }
            }else{
                auto origlist = std::static_pointer_cast<List>(operands[e]->copy());
                origlist->list.insert(origlist->list.begin(), first);
                auto r = std::make_shared<List>(List());
                r->build(origlist->list);
                first = r;
            }
        }

        return first;
    })));


    this->add(std::make_shared<Function>(Function("l-flatten", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());

        std::vector<std::shared_ptr<Object>> elements;

        std::function<void(const std::shared_ptr<Object> &obj)> recursive = [&](const std::shared_ptr<Object> &obj){
            if(obj->type == ObjectType::LIST){
                auto list = std::static_pointer_cast<List>(obj);
                for(int i = 0; i < list->list.size(); ++i){
                    recursive(list->list[i]);
                }
            }else{
                elements.push_back(obj->copy());
            }
        };

        auto temp = std::make_shared<List>(List());
        temp->build(operands);
        recursive(temp);

        r->build(elements);

        return r;
    })));    



    this->add(std::make_shared<Function>(Function("splice", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        auto r = std::make_shared<List>(List());
        std::vector<std::shared_ptr<Object>> elements;
        for(int i = 0; i < operands.size(); ++i){
            if(operands[i]->type == ObjectType::LIST){
                auto list = std::static_pointer_cast<List>(operands[i]);
                for(int j = 0; j < list->n; ++j){
                    elements.push_back(list->list[j]->copy());
                }
            }else{
                elements.push_back(operands[i]);
            }
        }
        r->build(elements);
        return r;
    })));    


    this->add(std::make_shared<Function>(Function("noop", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
        return std::make_shared<List>(List());
    })));        


    // this->add(std::make_shared<Function>(Function("first", [](const std::vector<std::shared_ptr<Object>> &operands, Context &ctx, Cursor &cursor){
    //     auto r = std::make_shared<List>(List());
    //     std::vector<std::shared_ptr<Object>> elements;
    //     auto list = std::static_pointer_cast<List>(operands[0]);
    //     for(double i = 0; i < list->n; ++i){
    //         elements.push_back(list->list[list->n - 1 - i]);
    //     }
    //     r->build(elements);
    //     return r;
    // })));              

    this->head = head;              
}

void Context::add(const std::shared_ptr<Object> &object){
    if(object->name.empty()){
        return;
    }
    this->objecs[object->name] = object;
}
Context::operator std::string() const {
    return str();
}

std::string Context::str(bool ignoreInners) const {
    std::string output = "CONTEXT\n";

    int fcol = 5;
    int scol = 5;

    for(auto &it : objecs){
        if(ignoreInners && it.second->innerType){
            continue;
        }
        while(it.first.length() > fcol){
            fcol += 5;
        }
    }

    // scol = 5;

    for(auto &it : objecs){
        if(ignoreInners && it.second->innerType){
            continue;
        }    
        output += it.first;
        int toAdd = fcol - it.first.length();
        for(int i = 0; i < toAdd; ++i){
            output += " ";
        }
        output += "->";
        for(int i = 0; i < scol; ++i){
            output += " ";
        }    
        output += "'"+it.second->str()+"'\n";
    }

    return output;
}

std::shared_ptr<Object> Context::find(const std::string &name){
    auto it = objecs.find(name);
    if(it == objecs.end()){
        return head == NULL ? std::make_shared<Object>(Object()) : head->find(name);
    }
    return it->second;
}
