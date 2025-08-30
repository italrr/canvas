#include <stdio.h>
#include <iostream>
#include "CV.hpp"

struct ExecArg {
	std::string name;
	std::string val;
	bool valid;
    int toInt() const {
        return std::stoi(this->val);
    }
	ExecArg(const std::string &name, const std::string &val = ""){
		this->name = name;
		this->val = val;
		this->valid = val.size() > 0;
	}
};

static std::shared_ptr<ExecArg> getParam(std::vector<std::string> &params, const std::string &name, bool single = false){
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

int main(int argc, char* argv[]){
	
	// Gather parameters
	std::vector<std::string> params;
	for(int i = 0; i < argc; ++i){
		params.push_back(std::string(argv[i]));
	}

	// Boolean settings
	bool useColor = getParam(params, "-c", true)->valid || getParam(params, "--color", true)->valid;
	bool useREPL = getParam(params, "-e", true)->valid || getParam(params, "--repl", true)->valid;
	bool useVersion = getParam(params, "-v", true)->valid || getParam(params, "--version", true)->valid; 
	bool useRelaxed = getParam(params, "-r", true)->valid || getParam(params, "--relaxed", true)->valid; 
	bool useNoReturn = getParam(params, "-u", true)->valid || getParam(params, "--no-return", true)->valid; 

	// File
	auto dashF = getParam(params, "-f", false);
	auto dashFile = getParam(params, "--file", false);
	std::string useFile = dashF->valid ? dashF->val : (dashFile->valid ? dashFile->val : "");

	// Version Info
	auto printVersion = [&](bool nl = true, const std::string &mode = ""){
		std::string text = std::string("canvas%s v%.0f.%.0f.%.0f %s [%s] released in %s")+
						(nl || mode.size() > 0 ? "\n" : "")+
						(mode.length() > 0 ?
							CV::Tools::setTextColor(CV::Tools::Color::YELLOW)+
							">>>>> "+mode+" <<<<<\n"+
							CV::Tools::setTextColor(CV::Tools::Color::RESET): "");
		printf(
			text.c_str(),
			CV::GetLogo().c_str(),
			CV_RELEASE_VERSION[0],
			CV_RELEASE_VERSION[1],
			CV_RELEASE_VERSION[2],
			CV::SupportedArchitecture::name(CV::ARCH).c_str(),
			CV::SupportedPlatform::name(CV::PLATFORM).c_str(),
			CV_RELEASE_DATE.c_str()
		);
	};
	if(useVersion){
		printVersion();
		return 0;
	}

	CV::SetUseColor(useColor);	

	if(useREPL && useFile.size() > 0){
		std::cout << "REPL cannot be used while reading a file. Start REPL mode and import a file by using \"[bring LIBRAY]\"" << std::endl;
		return 1;
	}

	// Run File
	if(useFile.size() > 0){

		if(!CV::Tools::fileExists(useFile)){
			std::cout << "Failed to read file '" << useFile << "': It doesn't exist or cannot be read" << std::endl; 
			return 1;
		}

		auto cursor = std::make_shared<CV::Cursor>();
		auto program = std::make_shared<CV::Program>();
		auto st = std::make_shared<CV::ControlFlow>();
		program->rootContext = program->createContext();		
		CV::InitializeCore(program);

		auto file = CV::Tools::readFile(useFile);

		auto entrypoint = CV::Compile(file, program, cursor);
		if(cursor->error){
			std::cout << cursor->getRaised() << std::endl;
			program->end();
			return 1;
		}
		program->entrypointIns = entrypoint->id;
		auto result = CV::Execute(entrypoint, program->rootContext, program, cursor, st);
		if(cursor->error){
			std::cout << cursor->getRaised() << std::endl;
			program->end();
			return 1;
		}
		program->end();
		return 0;
	}else
	// REPL
	if(useREPL){

		auto cursor = std::make_shared<CV::Cursor>();
		auto program = std::make_shared<CV::Program>();
		auto st = std::make_shared<CV::ControlFlow>();
		program->rootContext = program->createContext();		
		CV::InitializeCore(program);

		printVersion(false, useRelaxed ? "RELAXED" : "");

		while(true){
			std::string input = "";
			std::cout << CV::GetLogo() << "> ";
			std::getline (std::cin, input);

			if(input.size() > 0){
				// Compile
				auto entrypoint = CV::Compile(input, program, cursor);
				if(cursor->error){
					std::cout << cursor->getRaised() << std:: endl << std::endl;
					if(useRelaxed){
						cursor->clear();
					}else{
						program->end();
						return 1;
					}
				}
				// Execute
				auto result = CV::Execute(entrypoint, program->rootContext, program, cursor, st);
				if(cursor->error){
					std::cout << cursor->getRaised() << std:: endl << std::endl;
					if(useRelaxed){
						cursor->clear();
					}else{
						program->end();
						return 1;
					}
				}
				// Print
				if(!useNoReturn){
					std::cout << CV::QuantToText(result) << std::endl;
				}
			}
		}
		program->end();
		return cursor->error && !useRelaxed ? 1 : 0;
	}else{
	// Inline
		std::string cmd = "";
		for(int i = 1; i < params.size(); ++i){
			cmd += params[i];
			if(i < params.size()-1) cmd += " ";
		}
		if(cmd.size() == 0){
			printVersion();
			return 0;
		}

		auto cursor = std::make_shared<CV::Cursor>();
		auto program = std::make_shared<CV::Program>();
		auto st = std::make_shared<CV::ControlFlow>();
		program->rootContext = program->createContext();
		
		CV::InitializeCore(program);

		auto entrypoint = CV::Compile(cmd, program, cursor);
		if(cursor->error){
			std::cout << cursor->getRaised() << std:: endl;
			program->end();
			return 1;
		}
		program->entrypointIns = entrypoint->id;
		auto result = CV::Execute(entrypoint, program->rootContext, program, cursor, st);
		if(cursor->error){
			std::cout << cursor->getRaised() << std:: endl;
			program->end();
			return 1;
		}
		if(!useNoReturn){
			std::cout << CV::QuantToText(result) << std::endl;
		}
		program->end();
		return 0;
	}

	return 0;
}