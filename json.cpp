#include "json.hpp"
#include <fstream>

void jsonSerializable::writeJsonToFile(const std::string& filename, const nlohmann::json& j){
	std::string data = j.dump(1, '\t');
	std::ofstream file(filename);
	file.write(data.c_str(), data.size());
}

nlohmann::json jsonSerializable::readJsonFromFile(const std::string& filename){
	std::ifstream file(filename);
	auto j = nlohmann::json::parse(file);
	return j;
}

void jsonSerializable::deserializeFromFile(const std::string& filename){
	auto j = readJsonFromFile(filename);
	if(!j.empty()){
		deserialize(j);
	}

}

void jsonSerializable::serializeToFile(const std::string& filename) const{
	writeJsonToFile(filename, serialize());
}

