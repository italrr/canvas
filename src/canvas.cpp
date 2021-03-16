#include <iostream>
#include "Context.hpp"
#include "Object.hpp"
#include "Eval.hpp"




int main(int argc, const char *argv[]){
    Context main;
    Cursor cursor;

    cursor.column += 1;
    cursor.line += 1;

   
    // eval("(for (.. 0 10) with i as (if (= i 5)(continue)(print i)))", main, cursor);
    eval("(def a (fn (a)(print a)))", main, cursor);
    eval("(a 5)", main, cursor);

    // std::cout << r->str() << std::endl;
    std::cout << main.str(true) << std::endl;

    if(cursor.error){
        std::cout << cursor.message << std::endl;
    }


    return 0;
}