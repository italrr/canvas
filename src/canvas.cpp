#include <iostream>
#include "CV.hpp"

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

int main(int argc, char* argv[]){

	std::vector<std::string> params;
	for(int i = 0; i < argc; ++i){
		params.push_back(std::string(argv[i]));
	}

    auto dashFile = getParam(params, "-f");

    auto dashRepl = getParam(params, "--repl", true);    
    
    auto dashC = getParam(params, "-c", true);
    auto dashColor = getParam(params, "--color", true);

	auto dashRelax = getParam(params, "--relaxed", true);
	auto dashR = getParam(params, "-r", true);

	auto dashVersion = getParam(params, "--version", true);
	auto dashV = getParam(params, "-v", true);

	const bool relaxed = dashRelax->valid || dashR->valid;

	auto printCVEntry = [&](bool nl = true, const std::string &mode = ""){
		std::string text = std::string("canvas[~] v%.0f.%.0f.%.0f %s [%s]")+(nl || mode.size() > 0 ? "\n" : "")+(mode.length() > 0 ? ">>>>> "+mode+" <<<<<\n": "");
		printf(text.c_str(), CANVAS_LANG_VERSION[0], CANVAS_LANG_VERSION[1], CANVAS_LANG_VERSION[2], CV::SupportedArchitecture::name(CV::ARCH).c_str(), CV::SupportedPlatform::name(CV::PLATFORM).c_str());
	};
	if(dashV->valid || dashVersion->valid){
		printCVEntry();
		return 0;
	}    
    CV::SetUseColor(dashC->valid || dashColor->valid);	

    if(dashFile.get() && dashFile->valid){
    }else
    if(dashRepl.get() && dashRepl->valid){

    }else{
        auto cursor = std::make_shared<CV::Cursor>();
        auto stack = std::make_shared<CV::Stack>();
        auto context = stack->createContext();        

        std::string cmd = "";
        for(int i = 1; i < params.size(); ++i){
            cmd += params[i];
            if(i < params.size()-1) cmd += " ";
        }
        if(cmd.size() == 0){
            printCVEntry();
        return 0;
            }
        auto result = CV::QuickInterpret(cmd, stack, context, cursor);
        std::cout << result << std::endl;;
    }

    return 0;
}