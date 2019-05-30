#pragma once

#define NOMINMAX

#if defined _WIN32
#define S_IRWXU _S_IREAD | _S_IWRITE
#endif

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

#include <tbb/concurrent_queue.h>
#include <uv.h>

#include "FileInfoManager.h"
#include "FNamespace.h"

namespace sefile {

	class UvFileStream
	{
	public:
		using createStreamCb_t = std::function<void(int, size_t)>;
		using errorCb_t = std::function<void(int, std::string, ErrorCode)>;

	private:
		using queue_t = std::pair<int, std::string>;

		uv_buf_t buffer_;
		bool callIdle_ = true;
		errorCb_t errorCallback_;
		FileInfoManager fileInfoManager_;
		uv_idle_t idler_;
		std::unique_ptr<uv_loop_t, void(*)(uv_loop_t*)> loop_;
		std::atomic<size_t> maxFileSize_ = 2 << 24; // 32MB
		std::string pathToFile_;
		std::shared_timed_mutex pathToFileMutex_;
		tbb::concurrent_queue<queue_t> queue_;
		std::pair<int, std::string> queueItem_;
		uv_timer_t timer_;
		uv_fs_t writeReq_;
		uv_fs_t test_;

	public:
		UvFileStream(errorCb_t errorCallback);
		UvFileStream(const UvFileStream&) = delete;
		UvFileStream& operator=(const UvFileStream&) = delete;
		UvFileStream(UvFileStream&&) = delete;
		UvFileStream& operator=(UvFileStream&&) = delete;
		~UvFileStream();

		void closeFileStreamAsync(int fileDescriptor);
		void createFileStreamAsync(const std::string& fileName, createStreamCb_t callback);
		std::string getPath();
		void runLoop();
		void setPath(const std::string& path);
		// "stop" function is closing all open file descriptors and stops loop.
		// After it returns to create new file stream one has to call "runLoop" again.
		void stop();
		void writeData(int fileDescriptor, const std::string& msg) {
			queue_.emplace(std::make_pair(fileDescriptor, msg));
		}
		void writeData(int fileDescriptor, std::string&& msg) {
			queue_.emplace(std::make_pair(fileDescriptor, move(msg)));
		}

	private:
		std::string addTimeStamp(const std::string& fileName);
		static void onIdle(uv_idle_t* handle);
		static void onOpen(uv_fs_t* handle);
		static void onTimer(uv_timer_t* handle);
		static void onWrite(uv_fs_t* handle);
		void queueNextMessage();
		void setDefaultPath();
		void stopLoop();
		void write();

		friend class UvFileStreamTest;
		friend int isIdlerActive(const UvFileStream&);
		friend int isLoopActive(const UvFileStream&);
		friend int isTimerActive(const UvFileStream&);
	};

} // namespace sefile
