#include <stdio.h>
#include "Thirdparty/cpp-linenoise.hpp"
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
							CV::Tools::setTextColor(CV::Tools::Color::RESET): "\n");
		printf(
			text.c_str(),
			CV::GetPrompt().c_str(),
			CV::VERSION[0],
			CV::VERSION[1],
			CV::VERSION[2],
			CV::SupportedArchitecture::name(CV::ARCH).c_str(),
			CV::SupportedPlatform::name(CV::PLATFORM).c_str(),
			CV::RELEASE.c_str()
		);
	};
	if(useVersion){
		printVersion();
		return 0;
	}

	CV::SetUseColor(useColor);	

	if(useREPL && useFile.size() > 0){
		printf("REPL cannot be used while reading a file. Start REPL mode and import a file by using \"[bring LIBRAY]\"\n");
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

        program->root = program->createContext();
        CV::SetupCore(program);

        auto file = CV::Tools::readFile(useFile);

        auto entrypoint = CV::Compile(file, program, cursor, program->root);
        if(cursor->error){
            std::cout << cursor->getRaised() << std::endl;
            program->end();
            return 1;
        }

        CV::Data *result = nullptr;
        CV::InsType current = entrypoint;

        while(current){
            auto st = std::make_shared<CV::ControlFlow>();
            st->state = CV::ControlFlowState::CONTINUE;

            result = CV::Execute(current, program, cursor, program->root, st, false);
            if(cursor->error){
                std::cout << cursor->getRaised() << std::endl;
                program->end();
                return 1;
            }
            
            program->quickGC();

            if(current->next == 0){
                break;
            }

            current = program->getIns(current->next);
        }

        (void)result;

        program->end();
        return 0;
    }else
	// REPL
	if(useREPL){
        auto cursor = std::make_shared<CV::Cursor>();
        auto program = std::make_shared<CV::Program>();
        auto st = std::make_shared<CV::ControlFlow>();

        // Persistent REPL root context
        program->root = program->createContext();

        // Load native/core functions
        CV::SetupCore(program);

        // Add easy "exit" function for REPL
        program->root->registerFunction(program, "exit",
            [&](
                const std::shared_ptr<CV::Program> &prog,
                const std::string &name,
                const std::vector<std::pair<std::string, std::shared_ptr<CV::Instruction>>> &args,
                const std::shared_ptr<CV::Token> &token,
                const std::shared_ptr<CV::Cursor> &cursor,
                const std::shared_ptr<CV::Context> &ctx,
                const std::shared_ptr<CV::ControlFlow> &st
            ) -> CV::Data* {
                std::exit(0);
                return prog->buildNil();
            }
        );

        printVersion(false, useRelaxed ? "RELAXED" : "");

        linenoise::SetCompletionCallback([&](const char* editBuffer, std::vector<std::string>& completions){
            // Simple prefix completion against current root context names
            std::lock_guard<std::mutex> lock(program->root->mutexNames);

            for(const auto &it : program->root->names){
                if(editBuffer && editBuffer[0] != '\0'){
                    if(!it.first.empty() && it.first[0] == editBuffer[0]){
                        completions.push_back(it.first);
                    }
                }else{
                    completions.push_back(it.first);
                }
            }
        });

        linenoise::SetMultiLine(false);
        linenoise::SetHistoryMaxLen(1000);

        while(true){
            st = std::make_shared<CV::ControlFlow>();

            std::string input = "";
            auto quit = linenoise::Readline(
                (CV::GetPrompt() + "> ").c_str(),
                input
            );

            if(quit){
                break;
            }

            if(input.empty()){
                program->quickGC();
                continue;
            }

            linenoise::AddHistory(input.c_str());

            // Compile into the persistent REPL root context
            auto entrypoint = CV::Compile(input, program, cursor, program->root);
            if(cursor->error){
                std::cout << cursor->getRaised() << std::endl << std::endl;
                if(useRelaxed){
                    cursor->clear();
                    program->quickGC();
                    continue;
                }else{
                    program->end();
                    return 1;
                }
            }

            // Execute in the persistent REPL root context
            auto result = CV::Execute(entrypoint, program, cursor, program->root, st);
            if(cursor->error){
                std::cout << cursor->getRaised() << std::endl << std::endl;
                if(useRelaxed){
                    cursor->clear();
                    program->quickGC();
                    continue;
                }else{
                    program->end();
                    return 1;
                }
            }

            if(!useNoReturn){
                std::cout << CV::DataToText(program, result) << std::endl;
            }

            program->quickGC();
        }

        program->end();
        return cursor->error && !useRelaxed ? 1 : 0;
	}else{
	// Inline
        std::string cmd = "";
        for(int i = 1; i < params.size(); ++i){
            cmd += params[i];
            if(i < params.size() - 1){
                cmd += " ";
            }
        }

        if(cmd.empty()){
            printVersion();
            return 0;
        }

        auto cursor = std::make_shared<CV::Cursor>();
        auto program = std::make_shared<CV::Program>();
        auto st = std::make_shared<CV::ControlFlow>();

        program->root = program->createContext();

        CV::SetupCore(program);

        auto entrypoint = CV::Compile(cmd, program, cursor, program->root);
        if(cursor->error){
            std::cout << cursor->getRaised() << std::endl;
            program->end();
            return 1;
        }

        auto result = CV::Execute(entrypoint, program, cursor, program->root, st);
        if(cursor->error){
            std::cout << cursor->getRaised() << std::endl;
            program->end();
            return 1;
        }

        if(!useNoReturn){
            std::cout << CV::DataToText(program, result) << std::endl;
        }

        program->end();
        return 0;
	}    



    return 0;
        
        
}