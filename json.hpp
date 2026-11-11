#pragma once
#include <nlohmann/json.hpp>

class jsonSerializable{
public:
	virtual nlohmann::json serialize() const = 0;
	virtual void deserialize(const nlohmann::json& j) = 0;
	void deserializeFromFile(const std::string& filename);
	void serializeToFile(const std::string& filename) const;

	template<typename T> static inline bool trySetVal(const nlohmann::json& j, const std::string& name, T& value){
		if(j.contains(name)){
			value = j[name];
			return true;
		}
		return false;
	}
};


