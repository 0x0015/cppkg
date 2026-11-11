#include <iostream>
#include <filesystem>
#include <fstream>
#include "project.hpp"
#include "depMan.hpp"
#include "builder.hpp"

const char* defaultProject = R"({
	"name": "New Project",
	"version": "0.0.1",
	"type": "executable",
	"binName": "project",
	"source": [
		"main.cpp"
	],
	"compileFlags": ["-std=c++20"],
	"linkFlags": [],
	"debug": {
		"compileFlags": ["-g", "-Wall"]
	},
	"release": {
		"compileFlags": ["-O2"]
	}
})";

int main(int argc, char** argv){
	if(argc == 2 && std::string_view(argv[1]) == "init"){
		if(!std::filesystem::exists("project.json")){
			std::ofstream projFile("project.json");
			projFile.write(defaultProject, strlen(defaultProject));
			std::ofstream gitignore(".gitignore", std::ios_base::app);
			gitignore << "\n_cppkg";
			return(0);
		}
	}else if(argc >= 2 && std::string_view(argv[1]) == "build"){
		bool release = false;
		if(argc == 3){
			if(std::string_view(argv[2]) == "debug")
				release = false;
			else if(std::string_view(argv[2]) == "release")
				release = true;
			else{
				std::cerr<<"Error: unknown parameter \""<<argv[2]<<"\""<<std::endl;
				return(1);
			}
		}
		//read, then resolve deps, then build
		if(!std::filesystem::exists("project.json")){
			std::cerr<<"Error: no project file in current directory"<<std::endl;
			return(1);
		}
		project proj;
		proj.deserializeFromFile("project.json");
		depMan deps(proj);
		if(!deps.resolveDeps()){
			std::cerr<<"Error: Failed to resolve dependencies"<<std::endl;
			return(1);
		}
		builder bldr(deps);
		if(release)
			bldr.mode = builder::RELEASE;
		else
			bldr.mode = builder::DEBUG;
		if(!bldr.build()){
			std::cerr<<"Error: Build failed"<<std::endl;
			return(1);
		}
		return(0);
	}else if(argc == 2 && std::string_view(argv[1]) == "build"){
		if(!std::filesystem::exists("project.json")){
			std::cerr<<"Error: no project file in current directory"<<std::endl;
			return(1);
		}
		project proj;
		proj.deserializeFromFile("project.json");
		if(std::filesystem::exists(proj.binName))
			std::filesystem::remove(proj.binName);
		if(std::filesystem::exists("_cppkg"))
			std::filesystem::remove_all("_cppkg");
	}else{
		std::cout<<"cppkg Options:\n\tinit - initialize an empty project in the current directory\n\tbuild - build the project in the current directory\n\tclean - clean the project in the current directory\n\thelp (or anything else) - print this message"<<std::endl;
		return(0);
	}
}

