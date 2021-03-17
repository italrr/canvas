#ifndef CANVAS_EVAL_HPP
    #define CANVAS_EVAL_HPP

    #include <memory>
    #include <iostream>
    
    #include "Object.hpp"
    #include "Context.hpp"


    std::vector<std::string> parse(const std::string &input, Cursor &cursor);
    std::shared_ptr<Object> infer(const std::string &input, Context &ctx, Cursor &cursor);
    std::unordered_map<std::string, std::string> fetchSpecs(std::vector<std::string> input, std::vector<std::string> &output, Context &ctx);
    std::shared_ptr<Object> eval(const std::string &input, Context &ctx, Cursor &cursor);

#endif