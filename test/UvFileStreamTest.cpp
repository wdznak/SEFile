#include "pch.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>
#include <uv.h>

#include "UvFileStream.h"

namespace fs = std::filesystem;

namespace sefile {

	class UvFileStreamTest : public ::testing::Test
	{
	protected:
		static fs::path tempPath;
		size_t fileCount = 0;
		std::atomic<int> fileDescriptor = -2;
		sefile::UvFileStream fs_{ [](int, std::string, ErrorCode) {} };
		std::thread loopThread_;

		static void SetUpTestCase() {
			fs::create_directories(tempPath);
		}
		static void TearDownTestCase() {
			fs::remove_all(tempPath);
		}

		void SetUp() override {
			for (const auto& entry : fs::directory_iterator(tempPath)) {
				fileCount++;
			}
			ASSERT_EQ(fileCount, 0);
			loopThread_ = std::thread(&UvFileStream::runLoop, std::ref(fs_));
			fs_.createFileStreamAsync("TestName", [&](int fd, size_t size) {
				fileDescriptor = fd;
			});
		}

		void TearDown() override {
			fs_.stop();
			for (const auto& entry : fs::directory_iterator(tempPath)) {
				fs::remove(entry);
			}
		}

		UvFileStreamTest() {
			fs_.setPath(tempPath.string());
		}

		~UvFileStreamTest() {
			loopThread_.join();
		}

		void setMaxFileSize(size_t size) {
			fs_.maxFileSize_ = size;
		}

		// Not thread safe
		bool fileInfoExists(size_t fileDescriptor) const {
			return fs_.fileInfoManager_.fileInfoExists(fileDescriptor);
		}
	};

	fs::path UvFileStreamTest::tempPath = fs::temp_directory_path() / "Test\\";

	int isIdlerActive(const UvFileStream& fs_) { return uv_is_active((uv_handle_t*)&fs_.idler_); };
	int isLoopActive(const UvFileStream& fs_) { return uv_loop_alive(fs_.loop_.get()); };
	int isTimerActive(const UvFileStream& fs_) { return uv_is_active((uv_handle_t*)&fs_.timer_); };

	TEST(UvFileStream, CanCreateAccessAndModifyPathToFile) {
		sefile::UvFileStream fs{ [](int, std::string, ErrorCode) {} };
		size_t BUFFERSIZE = 100;
		std::unique_ptr<char[]> pathBuffer{ std::make_unique<char[]>(BUFFERSIZE) };
		std::string reqDefaultPath;

		if (uv_os_homedir(pathBuffer.get(), &BUFFERSIZE) >= 0) {
			reqDefaultPath = std::string(pathBuffer.get(), BUFFERSIZE) + "\\Documents\\";
		}
		const std::string fakePath = "C:\\path\\to\\file";

		EXPECT_EQ(fs.getPath(), reqDefaultPath);
		fs.setPath(fakePath);
		EXPECT_EQ(fs.getPath(), fakePath);
	}

	// Tests may fail on system high load
	// The other way to test these functions is to 
	// define while loop with a timer and check if
	// fileDescriptor is >= 0
	TEST_F(UvFileStreamTest, CanOpenNewFileStreamAndCreateFile) {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(0.5s);

		ASSERT_TRUE(fileDescriptor >= 0);
		for (const auto& entry : fs::directory_iterator(tempPath)) {
			fileCount++;
		}

		EXPECT_EQ(fileCount, 1);
	}

	TEST_F(UvFileStreamTest, CanWriteDataToFile) {
		using namespace std::chrono_literals;
		fs::path filePath;
		std::string testMsg{ "Can Write Data To File!" };
		std::this_thread::sleep_for(.5s);

		fs_.writeData(fileDescriptor, testMsg);
		std::this_thread::sleep_for(.5s);

		for (const auto& entry : fs::directory_iterator(tempPath)) {
			filePath = entry.path();
			fileCount++;
		}
		ASSERT_EQ(1, fileCount);

		std::ifstream is(filePath, std::ios::binary | std::ios::ate);
		
		std::streampos size = is.tellg();
		std::string str(size, '\0');
		is.seekg(0);
		is.read(&str[0], size);

		ASSERT_THAT(str, ::testing::HasSubstr(testMsg));
	}

	TEST_F(UvFileStreamTest, CreatesNewFileWhenMaxFileSizeIsReached) {
		using namespace std::chrono_literals;

		for (const auto& entry : fs::directory_iterator(tempPath)) {
			fileCount++;
		}
		ASSERT_EQ(fileCount, 0);

		setMaxFileSize(8);
		std::this_thread::sleep_for(1s);

		fs_.writeData(fileDescriptor, "TestMessage1");
		fs_.writeData(fileDescriptor, "TestMessage2");
		std::this_thread::sleep_for(0.5s);

		for (const auto& entry : fs::directory_iterator(tempPath)) {
			fileCount++;
		}

		EXPECT_EQ(fileCount, 2);
	}

	TEST_F(UvFileStreamTest, CanCloseFileStream) {
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(0.5s);

		ASSERT_TRUE(fileInfoExists(fileDescriptor));
		fs_.closeFileStreamAsync(fileDescriptor);
		std::this_thread::sleep_for(0.5s);

		ASSERT_FALSE(fileInfoExists(fileDescriptor));
	}

	TEST(UvFileStream, CanStopLoop) {
		using namespace std::chrono_literals;
		UvFileStream fs_{ [](int, std::string, ErrorCode) {} };
		std::thread loopThread_{ &UvFileStream::runLoop, std::ref(fs_) };
		std::this_thread::sleep_for(0.5s);
		EXPECT_TRUE(isLoopActive(fs_));
		
		fs_.stop();

		EXPECT_FALSE(isLoopActive(fs_));
		EXPECT_FALSE(isTimerActive(fs_));
		EXPECT_FALSE(isIdlerActive(fs_));

		loopThread_.join();
	}
}