#include <iostream>
#include <memory>
#include <signal.h>
#include <fstream>


#include "cv.hpp"
#include "libs/io.hpp"
#include "libs/brush.hpp"
#include "libs/math.hpp"
#include "libs/time.hpp"


CV::Cursor cursor;
std::shared_ptr<CV::Item> ctx = CV::createContext(std::shared_ptr<CV::Item>(NULL), false);

static void onExit(){
	std::cout << CV::printContext(ctx.get()) << std::endl;
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
	auto v = std::make_shared<ExecArg>(name);

	for(int i = 0; i < params.size(); ++i){
		if(params[i] == name){
			if(i < params.size()-1 && !single){
				v->val = params[i + 1];
				v->valid = true;
			}else{
				v->valid = single;
			}
			return v;
		}
	}
	
	return v;
}

static void printCVEntry(){
	printf("CANVAS v%.0f.%.0f.%.0f %s [%s] \n", CANVAS_LANG_VERSION[0], CANVAS_LANG_VERSION[1], CANVAS_LANG_VERSION[2], CV::SupportedArchitecture::name(CV::ARCH).c_str(), CV::SupportedPlatform::name(CV::PLATFORM).c_str());	
}

int main(int argc, char* argv[]){
	// Context with standard operators
	CV::registerEmbeddedOperators(ctx);
	io::registerLibrary(ctx);
	brush::registerLibrary(ctx);
	math::registerLibrary(ctx);
	timelib::registerLibrary(ctx);
	
	std::vector<std::string> params;
	for(int i = 0; i < argc; ++i){
		params.push_back(std::string(argv[i]));
	}

	auto dashFile = getParam(params, "-f");
	auto dashRepl = getParam(params, "-r", true);
	auto dashRelax = getParam(params, "--relaxed", true);
	auto dashV = getParam(params, "-v", true);
	auto dashVersion = getParam(params, "--version", true);

	if(dashV->valid || dashVersion->valid){
		printCVEntry();
		return 0;
	}

	if(dashFile.get() && dashFile->valid){
		std::ifstream file(dashFile->val);
		std::string line;
		int n = 1;
		std::string buffer = "";
		for(std::string line; std::getline(file, line);){
			cursor.line = n;
			if(line.size() > 0){
				buffer += line;
				if(CV::Tools::isLineComplete(buffer)){
					CV::eval(buffer, ctx, &cursor);
					if(cursor.error){
						std::cout << "Line #" << cursor.line << ": " << cursor.message << std::endl;
						if(!dashRelax->valid){
							std::exit(1);
						}
					}
					buffer = "";
				}
			}
			++n;
		}

		if(buffer.size() > 0 && !CV::Tools::isLineComplete(buffer)){
			std::cout << "Line #" << cursor.line << ": Block wasn't properly closed"  << std::endl;
			std::exit(1);			
		}
		std::exit(0);
	}else
	if(dashRepl.get() && dashRepl->valid){
		printCVEntry();
		while(true){
			std::cout << std::endl;
			std::string input;
			std::cout << "cv> ";
			std::getline (std::cin, input);

			if(input.size() > 0){
				std::cout << eval(input, ctx, &cursor)->str() << std::endl;
				if(cursor.error){
					std::cout <<  cursor.message << std::endl;
					if(!dashRelax->valid){
						std::exit(1);
					}
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