#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace sefile {

	struct FileInfo{
		std::string fileName;
		int fileDescriptor = -1;
		void* userData;
		size_t fileSize = 0;
		std::function<void(int, size_t)> userCallback;
	};

	class FileInfoManager
	{
	private:
		std::unordered_map<size_t, FileInfo> fileInfoList_;
		size_t uniqueId_ = 0;
	public:
		FileInfoManager();
		~FileInfoManager();

		size_t addFileInfo(const FileInfo& fileInfo);
		size_t addFileInfo(FileInfo&& fileInfo);
		std::vector<int> activeFileDescriptors();
		size_t count() const;
		bool fileInfoExists(size_t uniqueId) const;
		FileInfo& getFileInfo(size_t uniqueId);
		void removeFileInfo(size_t uniqueId);
		void updateUsersInfo();
	};

} // namespace sefile