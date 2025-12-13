#include <iostream>
#include "CV.hpp"

int main(){



	auto cursor = std::make_shared<CV::Cursor>();
	auto program = std::make_shared<CV::Program>();
	auto st = std::make_shared<CV::ControlFlow>();    
    CV::SetupCore(program);
    auto entrypoint = CV::Compile("[[let t [1 2 3]][t]]", program, cursor);


    if(cursor->error){
        std::cerr << cursor->getRaised() << std::endl;
        program->end();
        return 1;
    }

    auto result = CV::Execute(entrypoint, program, cursor, program->root, st);
    if(cursor->error){
        std::cerr << cursor->getRaised() << std::endl;
        program->end();
        return 1;
    }
    std::cout << CV::DataToText(program, result) << std::endl;

    program->end();

    return 0;
        
        
}