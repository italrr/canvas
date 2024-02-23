#include <iostream>
#include <memory>
#include <signal.h>
#include <fstream>


#include "cv.hpp"


CV::Cursor cursor;
std::shared_ptr<CV::Item> ctx = CV::createContext();

static void onExit(){
	std::cout << CV::printContext(ctx) << std::endl;
}

struct ExecArg {
	std::string name;
	std::string val;
	bool valid;
	ExecArg(const std::string &name, const std::string &val = ""){
		this->name = name;
		this->val = val;
		this->valid = val.size() > 0;
	}
};

std::shared_ptr<ExecArg> getParam(std::vector<std::string> &params, const std::string &name, bool single = false){
	for(int i = 0; i < params.size(); ++i){
		if(params[i] == name){
			if(i < params.size()-1 && !single){
				return std::make_shared<ExecArg>(ExecArg(params[i], params[i + 1]));
			}else{
				auto v = std::make_shared<ExecArg>(ExecArg(params[i]));
				v->valid = single;
				return v;
			}
		}
	}
	return std::shared_ptr<ExecArg>(NULL);
}

int main(int argc, char* argv[]){
	// Context with standard operators
	CV::registerEmbeddedOperators(ctx);

	std::vector<std::string> params;
	for(int i = 0; i < argc; ++i){
		params.push_back(std::string(argv[i]));
	}

	auto dashFile = getParam(params, "-f");
	auto dashRepl = getParam(params, "-r", true);
	auto dashRelax = getParam(params, "--relaxed", true);

	if(dashFile.get() && dashFile->valid){
		std::ifstream file(dashFile->val);
		std::string line;
		int n = 1;
		for( std::string line; std::getline(file, line );){
			cursor.line = n;
			if(line.size() > 0){
				std::cout << CV::eval(line, ctx, &cursor)->str() << std::endl;
				if(cursor.error){
					std::cout << "Line #" << cursor.line << ": " << cursor.message << std::endl;
				}
			}
			++n;
		}
		std::exit(0);
	}else
	if(dashRepl.get() && dashRepl->valid){
		while(true){
			std::cout << std::endl;
			std::string input;
			std::cout << "cv> ";
			std::getline (std::cin, input);

			if(input.size() > 0){
				std::cout << eval(input, ctx, &cursor)->str();
				if(cursor.error){
					std::cout <<  cursor.message << std::endl;
					std::exit(1);
				}
			}
		}
	}else{
		std::string cmd = "";
		for(int i = 1; i < params.size(); ++i){
			cmd += params[i];
			if(i < params.size()-1) cmd += " ";
		}
		std::cout << eval(cmd, ctx, &cursor)->str();
		if(cursor.error){
			std::cout <<  cursor.message << std::endl;
			std::exit(1);
		}
		std::exit(0);		
	}		

	return 0;

}