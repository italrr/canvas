#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <signal.h>
#include <fstream>

#include "CV.hpp"
#include "libs/io.hpp"

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
				params.erase(params.begin() + i, params.begin() + i + 1);
			}else{
				v->valid = single;
				params.erase(params.begin() + i);
			}
			return v;
		}
	}

	return v;
}
static CV::Cursor cursor;
static std::shared_ptr<CV::Context> ctx = std::make_shared<CV::Context>();
static bool finished = false;
static bool isBusy = false;

static void RunContextIsDone(){
	while(!finished){
		isBusy = CV::ContextStep(ctx);
		if(!isBusy){
			CV::Tools::sleep(20);
		}
	}
}

int main(int argc, char* argv[]){

	std::vector<std::string> params;
	for(int i = 0; i < argc; ++i){
		params.push_back(std::string(argv[i]));
	}


	auto dashC = getParam(params, "-c", true);
	auto dashFile = getParam(params, "-f");

	auto dashRepl = getParam(params, "--repl", true);

	auto dashRelax = getParam(params, "--relaxed", true);
	auto dashR = getParam(params, "-r", true);

	auto dashInteractive = getParam(params, "-i", true);
	auto dashI = getParam(params, "--interactive", true);
	
	auto dashVersion = getParam(params, "--version", true);
	auto dashV = getParam(params, "-v", true);


	const bool relaxed = dashRelax->valid || dashR->valid;

	auto printCVEntry = [&](bool nl = true){
		std::string text = std::string("canvas[~] v%.0f.%.0f.%.0f %s [%s]")+(nl || relaxed ? "\n" : "")+(relaxed ? ">>>>> RELAXED MODE <<<<<\n": "");
		printf(text.c_str(), CANVAS_LANG_VERSION[0], CANVAS_LANG_VERSION[1], CANVAS_LANG_VERSION[2], CV::SupportedArchitecture::name(CV::ARCH).c_str(), CV::SupportedPlatform::name(CV::PLATFORM).c_str());
	};

	if(dashV->valid || dashVersion->valid){
		printCVEntry();
		return 0;
	}
	CV::setUseColor(dashC->valid);
	
	ctx->copyable = false; // top contexts shouldn't be copied (only referenced)

	// std::cout << CV::ItemToText(ctx.get()) << std::endl;

	if(dashFile.get() && dashFile->valid){
		// std::ifstream file(dashFile->val);
		// std::string line;
		// int n = 1;
		// std::string buffer = "";
		// for(std::string line; std::getline(file, line);){
		// 	cursor.line = n;
		// 	if(line.size() > 0){
		// 		buffer += line;
		// 		if(CV::Tools::isLineComplete(buffer)){
		// 			CV::eval(buffer, ctx, &cursor);
		// 			if(cursor.error){
		// 				std::cout << "Line #" << cursor.line << ": " << cursor.message << std::endl;
		// 				if(!dashRelax->valid){
		// 					std::exit(1);
		// 				}
		// 			}
		// 			buffer = "";
		// 		}
		// 	}
		// 	++n;
		// }

		// if(buffer.size() > 0 && !CV::Tools::isLineComplete(buffer)){
		// 	std::cout << "Line #" << cursor.line << ": Block wasn't properly closed"  << std::endl;
		// 	std::exit(1);			
		// }
		// std::exit(0);
	}else
	if(dashRepl.get() && dashRepl->valid){
		printCVEntry(false);
		CV::AddStandardOperators(ctx);
		io::registerLibrary(ctx);		
		std::thread loop(&RunContextIsDone);
		while(true){
			std::cout << std::endl;
			std::string input = "";
			std::cout << "CV> ";
			std::getline (std::cin, input);

			if(input.size() > 0){
				std::cout << CV::ItemToText(CV::interpret(input, &cursor, ctx).get()) << std::endl;
				if(cursor.error){
					std::cout <<  cursor.message << std::endl;
					if(!relaxed){
						std::exit(1);
					}
					cursor.clear();
				}
			}
		}
		while(ctx->jobs.size() > 0)CV::Tools::sleep(20);
		finished = true;
		loop.join();
		std::cout << "Bye!" << std::endl;
	}else{
		CV::AddStandardOperators(ctx);
		io::registerLibrary(ctx);			
		std::thread loop(&RunContextIsDone);
		std::string cmd = "";
		for(int i = 1; i < params.size(); ++i){
			cmd += params[i];
			if(i < params.size()-1) cmd += " ";
		}
		if(cmd.size() == 0){
			printCVEntry();
			return 0;
		}
		auto result = CV::interpret(cmd, &cursor, ctx);
		std::cout << CV::ItemToText(result.get());
		if(cursor.error){
			std::cout << "\n" << cursor.message << std::endl;
			std::exit(1);
		}
		while(ctx->jobs.size() > 0)CV::Tools::sleep(20);
		finished = true;
		loop.join();
	}		

	return 0;

}