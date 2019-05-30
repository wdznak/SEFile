#include "stdafx.h"
#include "UvFileStream.h"

#include <ctime>
#include <future>
#include <utility>

#include "Async.h"

namespace sefile {

	UvFileStream::UvFileStream(errorCb_t errorCallback)
		: errorCallback_(errorCallback),
		  loop_(uv_default_loop(), [](uv_loop_t* l) 
			{
		        uv_loop_close(l);
	        })
	{
		setDefaultPath();
	}


	UvFileStream::~UvFileStream()
	{
	}

	void UvFileStream::closeFileStreamAsync(int fileDescriptor)
	{
		Async* request = new Async(loop_.get());
		auto data = new std::pair<int, UvFileStream*>{ fileDescriptor, this };
		request->setData(static_cast<void*>(data));

		request->start([](Async* request) {
			auto data = static_cast<std::pair<int, UvFileStream*>*>(request->getData());
			UvFileStream* fileStream = data->second;

			if (fileStream->fileInfoManager_.fileInfoExists(data->first)) {
				const FileInfo& fileInfo = fileStream->fileInfoManager_.getFileInfo(data->first);
				uv_fs_t closeReq;
				uv_fs_close(fileStream->loop_.get(), &closeReq, fileInfo.fileDescriptor, nullptr);
				fileStream->fileInfoManager_.removeFileInfo(data->first);
			}
			else {
				fileStream->errorCallback_(data->first, "File descriptor does not exists!", EEXISTFS);
			}

			delete data;
			request->close();
		});

		request->send();
	}

	void UvFileStream::createFileStreamAsync(const std::string& fileName, createStreamCb_t callback)
	{
		FileInfo* fileInfo = new FileInfo();
		fileInfo->fileName = fileName;
		fileInfo->userData = static_cast<void*>(this);
		fileInfo->userCallback = callback;
		uv_fs_t* openRequest = new uv_fs_t;
		openRequest->data = static_cast<void*>(fileInfo);
		std::string path = getPath() + addTimeStamp(fileName + "_INIT");

		uv_fs_open(loop_.get(), openRequest, path.data(),
			UV_FS_O_RDWR | UV_FS_O_CREAT, S_IRWXU, onOpen);
	}

	std::string UvFileStream::getPath()
	{
		std::shared_lock<std::shared_timed_mutex> lock(pathToFileMutex_);
		return pathToFile_;
	}

	void UvFileStream::runLoop()
	{
		timer_.data = static_cast<void*>(this);
		idler_.data = static_cast<void*>(this);
		uv_timer_init(loop_.get(), &timer_);
		uv_idle_init(loop_.get(), &idler_);

		queueNextMessage();
		uv_run(loop_.get(), UV_RUN_DEFAULT);
	}

	void UvFileStream::setPath(const std::string& path)
	{
		std::lock_guard<std::shared_timed_mutex> lock(pathToFileMutex_);
		pathToFile_ = path;
	}

	void UvFileStream::stop()
	{
		Async* request = new Async(loop_.get());
		std::promise<bool> isStoppedP;
		std::future<bool> isStoppedF = isStoppedP.get_future();
		auto data = new std::pair<UvFileStream*, std::promise<bool>>{ this, std::move(isStoppedP) };

		request->setData(data);
		request->start([](Async* request) {
			auto data = static_cast<std::pair<UvFileStream*, std::promise<bool>>*>(request->getData());

			data->first->stopLoop();
			request->close();
			data->second.set_value(true);
		});

		request->send();
		isStoppedF.wait();
		delete data;
	}

	void UvFileStream::queueNextMessage()
	{
		while (queue_.try_pop(queueItem_)) {
			if (fileInfoManager_.fileInfoExists(queueItem_.first)) {
				write();
				return;
			}
			else {
				errorCallback_(queueItem_.first, "File descriptor does not exists!", EEXISTFS);
			}
		}
		if (callIdle_) {
			uv_idle_start(&idler_, onIdle);
		}
		else {
			uv_timer_start(&timer_, onTimer, 100, 0);
		}
	}

	void UvFileStream::setDefaultPath()
	{
		size_t BUFFERSIZE = 100;
		std::unique_ptr<char[]> pathBuffer{ std::make_unique<char[]>(BUFFERSIZE) };
		
		if (uv_os_homedir(pathBuffer.get(), &BUFFERSIZE) >= 0) {
			setPath(std::string(pathBuffer.get(), BUFFERSIZE) + "\\Documents\\");
		}
	}

	void UvFileStream::stopLoop()
	{
		uv_fs_t closeReq;
		std::vector<int> fileDescriptors = fileInfoManager_.activeFileDescriptors();

		for (auto& item : fileDescriptors) {
			uv_fs_close(loop_.get(), &closeReq, item, nullptr);
			uv_fs_req_cleanup(&closeReq);
		}

		uv_idle_stop(&idler_);
		uv_timer_stop(&timer_);
		uv_stop(loop_.get());
	}

	void UvFileStream::write()
	{
		FileInfo& fileInfo = fileInfoManager_.getFileInfo(queueItem_.first);
		writeReq_.data = static_cast<void*>(&fileInfo);
		queueItem_.second += "\r\n";
		buffer_ = uv_buf_init(const_cast<char*>(queueItem_.second.data()), queueItem_.second.length());

		uv_fs_write(loop_.get(), &writeReq_, fileInfo.fileDescriptor, &buffer_, 1, -1, onWrite);
	}

	std::string UvFileStream::addTimeStamp(const std::string& fileName)
	{
		std::string result;
		std::time_t time = std::time(nullptr);
		size_t      BUFFERSIZE = 100;
		std::unique_ptr<char[]> timeBuffer{ new char[BUFFERSIZE] };

		if (std::strftime(timeBuffer.get(), BUFFERSIZE, "%F_%OH%OM%OS", std::localtime(&time)))
		{
			result = std::string(timeBuffer.get()) + "_" + fileName + ".txt";
		}
		return result;
	}

	void UvFileStream::onIdle(uv_idle_t* handle)
	{
		UvFileStream* fileStream = static_cast<UvFileStream*>(handle->data);
		fileStream->callIdle_ = false;
		uv_idle_stop(handle);
		fileStream->fileInfoManager_.updateUsersInfo();
		fileStream->queueNextMessage();
	}

	void UvFileStream::onOpen(uv_fs_t* handle)
	{
		FileInfo* fileInfo = static_cast<FileInfo*>(handle->data);
		UvFileStream* fileStream = static_cast<UvFileStream*>(fileInfo->userData);
		if (handle->result >= 0) {
			fileInfo->fileDescriptor = handle->result;
			fileInfo->userCallback(fileStream->fileInfoManager_.addFileInfo(*fileInfo), -1);
		}
		else {
			fileStream->errorCallback_(handle->result, "Unable to open file stream!", EOPENFS);
		}

		uv_fs_req_cleanup(handle);
		delete handle;

		fileStream->queueNextMessage();
	}

	void UvFileStream::onTimer(uv_timer_t* handle)
	{
		UvFileStream* fileStream = static_cast<UvFileStream*>(handle->data);
		uv_timer_stop(&fileStream->timer_);
		fileStream->queueNextMessage();
	}

	void UvFileStream::onWrite(uv_fs_t* handle)
	{
		FileInfo* fileInfo = static_cast<FileInfo*>(handle->data);
		UvFileStream* fileStream = static_cast<UvFileStream*>(fileInfo->userData);

		if (handle->result < 0) {
			fileStream->errorCallback_(fileInfo->fileDescriptor, "Write to file failed!", EWRITE);
		}
		else {
			fileInfo->fileSize += handle->result;

			if (fileInfo->fileSize > fileStream->maxFileSize_) {
				fileInfo->fileSize = 0;
				uv_fs_t request;
				if (uv_fs_close(handle->loop, &request, fileInfo->fileDescriptor, nullptr) < 0) {
					fileStream->errorCallback_(fileInfo->fileDescriptor, "There was an error when closing the file!", ECLOSEFS);
				}

				std::string path = fileStream->getPath() + fileStream->addTimeStamp(fileInfo->fileName);
				if (int fileDescriptor = uv_fs_open(handle->loop, 
													&request, 
													path.data(), 
													UV_FS_O_RDWR | UV_FS_O_CREAT, S_IRWXU, 
													nullptr)) {
					fileInfo->fileDescriptor = fileDescriptor;
				}
				else {
					fileStream->errorCallback_(fileInfo->fileDescriptor, "Unable to open file stream!", EOPENFS);
				}
				uv_fs_req_cleanup(&request);
			}
		}
		uv_fs_req_cleanup(handle);
		fileStream->queueNextMessage();
	}

} // namespace sefile
