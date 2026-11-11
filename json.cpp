#include "json.hpp"
#include <fstream>

void jsonSerializable::deserializeFromFile(const std::string& filename){
	std::ifstream file(filename);
	auto j = nlohmann::json::parse(file);
	if(!j.empty()){
		deserialize(j);
	}
}

void jsonSerializable::serializeToFile(const std::string& filename) const{
	std::string data = serialize().dump(1, '\t');
	std::ofstream file(filename);
	file.write(data.c_str(), data.size());
}

