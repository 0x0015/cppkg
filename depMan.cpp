#include "depMan.hpp"
#include "paths.hpp"
#include <curl/curl.h>
#include <iostream>

std::size_t write_data(void* ptr, std::size_t size, std::size_t nmemb, FILE* stream){
	return fwrite(ptr, size, nmemb, stream);
}

bool downloadFile(const std::string& url, const std::string& dstFilename){
	CURL* curl = curl_easy_init();
	if(!curl){
		std::cerr<<"Failed to init curl"<<std::endl;
		return false;
	}

	FILE* fp = fopen(dstFilename.c_str(), "wb");
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	fclose(fp);

	if(res != CURLE_OK){
		std::cerr<<"Failed to download file \""<<url<<"\" with error: "<<curl_easy_strerror(res)<<std::endl;
		return false;
	}

	return true;
}

bool downloadGithubFile(const std::string& user, const std::string& repo, const std::string& branch, const std::string& file, const std::string& dstFilename){
	return downloadFile("https://github.com/" + user + "/" + repo + "/raw/" + branch + "/" + file, dstFilename);
}

bool depMan::resolveDeps(){
	auto pkgPath = getTempPath("pkg");
	return true;
}

