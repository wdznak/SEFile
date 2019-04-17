#include "stdafx.h"
#include "FileInfoManager.h"

namespace sefile {

FileInfoManager::FileInfoManager()
{
}

FileInfoManager::~FileInfoManager()
{
}

size_t FileInfoManager::addFileInfo(const FileInfo& fileInfo) {
	fileInfoList_.emplace(std::make_pair(uniqueId_, fileInfo));
	return uniqueId_++;
}

size_t FileInfoManager::addFileInfo(FileInfo && fileInfo) {
	fileInfoList_.emplace(std::make_pair(uniqueId_, fileInfo));
	return uniqueId_++;
}

std::vector<int> FileInfoManager::activeFileDescriptors() {
	std::vector<int> result;
	for (auto& item : fileInfoList_) {
		result.push_back(item.second.fileDescriptor);
	}

	return result;
}

size_t FileInfoManager::count() const {
	return fileInfoList_.size();
}

bool FileInfoManager::fileInfoExists(size_t uniqueId) const {
	return fileInfoList_.count(uniqueId);
}

FileInfo & FileInfoManager::getFileInfo(size_t uniqueId) {
	return fileInfoList_.at(uniqueId);
}

void FileInfoManager::removeFileInfo(size_t uniqueId) {
	fileInfoList_.erase(uniqueId);
}

void FileInfoManager::updateUsersInfo() {
	for (auto& info : fileInfoList_) {
		info.second.userCallback(info.second.fileDescriptor,
								 info.second.fileSize);
	}
}

} // namespace sefile