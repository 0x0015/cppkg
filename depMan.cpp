#include "depMan.hpp"
#include "paths.hpp"
#include <curl/curl.h>
#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <archive.h>
#include <archive_entry.h>

bool create_tar_xz(const std::string& outputPath, const std::vector<std::filesystem::path>& files){
	struct archive* a = archive_write_new();

	archive_write_add_filter_xz(a);
	archive_write_set_format_pax_restricted(a);

	if(archive_write_open_filename(a, outputPath.c_str()) != ARCHIVE_OK){
		std::cerr<<"Failed to open output archive: "<<archive_error_string(a)<<std::endl;
		archive_write_free(a);
		return false;
	}

	for(const auto& path : files){
		if(!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
			continue;

		struct archive_entry* entry = archive_entry_new();
		archive_entry_set_pathname(entry, std::filesystem::relative(path).c_str());
		archive_entry_set_size(entry, std::filesystem::file_size(path));
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);

		archive_write_header(a, entry);

		std::ifstream file(path.string(), std::ios::binary);
		std::vector<uint8_t> filedata(std::filesystem::file_size(path));
		file.read((char*)filedata.data(), filedata.size());
		archive_write_data(a, filedata.data(), filedata.size());
		archive_entry_free(entry);
	}

	archive_write_close(a);
	archive_write_free(a);
	return true;
}

bool extract_tar_xz(const std::string& archivePath, const std::filesystem::path& outputPath){
	struct archive* a = archive_read_new();

	archive_read_support_filter_xz(a);
	archive_read_support_format_tar(a);

	if(archive_read_open_filename(a, archivePath.c_str(), 10240) != ARCHIVE_OK){
		std::cerr<<"Failed to open archive: "<<archive_error_string(a)<<std::endl;
		archive_read_free(a);
		return false;
	}

	std::filesystem::create_directories(outputPath);

	struct archive_entry* entry;
	int r;

	while((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK){
		std::filesystem::path dest = outputPath / archive_entry_pathname(entry);
		if(dest.filename().empty())
			continue;

		// Create parent directories
		std::filesystem::create_directories(dest.parent_path());

		std::ofstream file(dest, std::ios::binary);
		if(!file){
			std::cerr<<"Failed to create file: "<<dest<<std::endl;
			archive_read_free(a);
			return false;
		}

		char buffer[8192];
		la_ssize_t size;

		while((size = archive_read_data(a, buffer, sizeof(buffer))) > 0){
			file.write(buffer, size);
		}

		if(size < 0){
			std::cerr<<"Failed to read archive data: "<<archive_error_string(a)<<std::endl;
			archive_read_free(a);
			return false;
		}
	}

	if(r != ARCHIVE_EOF){
		std::cerr<<"Error reading archive: "<<archive_error_string(a)<<std::endl;
		archive_read_free(a);
		return false;
	}

	archive_read_close(a);
	archive_read_free(a);
	return true;
}

std::size_t write_data(void* ptr, std::size_t size, std::size_t nmemb, FILE* stream){
	return fwrite(ptr, size, nmemb, stream);
}

enum fileDownloadStatus{
	SUCCESS,
	ALREADY_UP_TO_DATE
};
std::optional<fileDownloadStatus> downloadFile(const std::string& url, const std::string& dstFilename){
	CURL* curl = curl_easy_init();
	if(!curl){
		std::cerr<<"Failed to init curl"<<std::endl;
		return {};
	}

	if(std::filesystem::exists(dstFilename)){
		std::error_code ec;
		auto ftime = std::filesystem::last_write_time(dstFilename, ec);
		if(!ec){
			auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
			auto unix_sec = std::chrono::duration_cast<std::chrono::seconds>(sys_time.time_since_epoch()).count();
			curl_easy_setopt(curl, CURLOPT_TIMECONDITION, (long)CURL_TIMECOND_IFMODSINCE);
			curl_easy_setopt(curl, CURLOPT_TIMEVALUE_LARGE, (curl_off_t)unix_sec);
		}
	}

	std::string tempFilename = dstFilename + ".tmp";

	FILE* fp = fopen(tempFilename.c_str(), "wb");
	if(!fp){
		std::cerr<<"Failed to open file to download into"<<std::endl;
		curl_easy_cleanup(curl);
		return {};
	}
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);

	long status_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);

	curl_off_t remoteTime = -1;
	curl_easy_getinfo(curl, CURLINFO_FILETIME_T, &remoteTime);

	curl_easy_cleanup(curl);
	fclose(fp);

	if(res == CURLE_OK && status_code == 304){
		std::filesystem::remove(tempFilename);
		return fileDownloadStatus::ALREADY_UP_TO_DATE;
	}

	if(res != CURLE_OK || status_code < 200 || status_code >= 300){
		std::cerr<<"Failed to download file \""<<url<<"\" with error: "<<curl_easy_strerror(res)<<" (HTTP "<<status_code<<")"<<std::endl;
		std::filesystem::remove(tempFilename);
		return {};
	}

	std::error_code ec;
	std::filesystem::rename(tempFilename, dstFilename, ec);
	if(ec){
		std::cerr<<"Failed to move downloaded tmp file to correct location"<<std::endl;
		std::filesystem::remove(tempFilename);
		return {};
	}

	if(remoteTime >= 0){
		auto sysTime = std::chrono::system_clock::time_point{std::chrono::seconds(remoteTime)};
		std::filesystem::last_write_time(dstFilename, std::chrono::file_clock::from_sys(sysTime));
	}

	return fileDownloadStatus::SUCCESS;
}

std::optional<fileDownloadStatus> downloadGithubFile(const std::string& user, const std::string& repo, const std::string& branch, const std::string& file, const std::string& dstFilename){
	return downloadFile("https://github.com/" + user + "/" + repo + "/raw/" + branch + "/" + file, dstFilename);
}

bool depMan::resolveDeps(){
	auto pkgPath = getTempPath("pkg");
	const auto downloadRepoFile = [](const std::string& file, const std::string& dstFilename){return downloadGithubFile("0x0015", "cppkg", "pkg", file, dstFilename);};

	std::unordered_set<std::string> allPackagesNeeded(proj.deps.begin(), proj.deps.end());

	std::cout<<"Downloading package project manifests..."<<std::endl;
	downloadedManifests = {};
	bool foundNewManifests = true;
	bool allManifestsUpToDate = true;
	while(foundNewManifests){
		foundNewManifests = false;
		std::vector<std::string> depsToAddNextLoop;
		for(const auto& dep : allPackagesNeeded){
			if(downloadedManifests.contains(dep))
				continue;
			auto manifestPath = pkgPath / (dep + ".json");
			auto downloadStatus = downloadRepoFile(dep + "/project.json", manifestPath.string());
			if(!downloadStatus){
				std::cerr<<"Error: failed to download project manifest for "<<dep<<std::endl;
				return false;
			}
			if(*downloadStatus != fileDownloadStatus::ALREADY_UP_TO_DATE)
				allManifestsUpToDate = false;
			auto& depManifest = downloadedManifests[dep];
			depManifest.deserializeFromFile(manifestPath.string());
			for(const auto& subdep : depManifest.deps)
				depsToAddNextLoop.push_back(subdep);
			foundNewManifests = true;
		}
		for(const auto& dep : depsToAddNextLoop)
			allPackagesNeeded.insert(dep);
	}
	if(allManifestsUpToDate)
		std::cout<<"\tUp to Date!"<<std::endl;

	//we now will have all dependencies downloaded, and all the manifests loaded
	//now we can download the actual package contents
	for(const auto& [dep, depProj] : downloadedManifests){
		std::cout<<"Downloading project: "<<dep<<"..."<<std::endl;;
		auto depPath = pkgPath / dep;
		if(!std::filesystem::exists(depPath))
			std::filesystem::create_directories(depPath);
		auto depPkgFilename = dep + "-" + proj.os.createOsString() + "-" + (proj.mode == minimalProjectInfo::DEBUG ? "debug" : "release") + ".tar.xz";
		auto downloadStatus = downloadRepoFile(dep + "/" + depPkgFilename, (depPath / depPkgFilename).string());
		if(downloadStatus){
			if(*downloadStatus == ALREADY_UP_TO_DATE)
				std::cout<<"\tUp to Date!"<<std::endl;
		}else{
			std::cerr<<"Error: failed to download project archive for "<<dep<<std::endl;
			return false;
		}
		if(!extract_tar_xz((depPath / depPkgFilename).string(), depPath)){
			std::cerr<<"Error: failed to extract project archive for "<<dep<<std::endl;
			return false;
		}
	}

	return true;
}

bool depMan::createPackage(){
	std::vector<std::filesystem::path> filesToInclude;
	if(!std::filesystem::exists(proj.getTrueBinName())){
		filesToInclude.push_back(std::filesystem::path(proj.getTrueBinName()));
	}
	
	for(const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(std::filesystem::current_path())){
		if(pathContainsDirectory(dir_entry.path(), "_cppkg"))
			continue;
		if(dir_entry.is_regular_file() && (dir_entry.path().extension() == ".hpp" || dir_entry.path().extension() == ".h" || dir_entry.path().extension() == ".inc" || dir_entry.path().extension() == ".a" || dir_entry.path().extension() == ".lib" || dir_entry.path().extension() == ".so" || dir_entry.path().extension() == ".dll")){
			filesToInclude.push_back(dir_entry.path());
		}
	}

	if(!create_tar_xz(proj.name + "-" + proj.os.createOsString() + "-" + (proj.mode == minimalProjectInfo::DEBUG ? "debug" : "release") + ".tar.xz", filesToInclude)){
		std::cerr<<"Error: failed to create project package archive"<<std::endl;
		return false;
	}
	return true;
}

