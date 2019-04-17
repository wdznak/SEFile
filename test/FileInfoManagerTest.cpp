#include "pch.h"
#include "FileInfoManager.h"

namespace sefile {

	TEST(FileInfoManager, CanAddRemoveFileInfoObj) {
		FileInfoManager mgr;

		mgr.addFileInfo(FileInfo{});
		EXPECT_EQ(1, mgr.count());

		int fileId = mgr.addFileInfo(FileInfo{});
		mgr.addFileInfo(FileInfo{});
		EXPECT_TRUE(mgr.fileInfoExists(fileId));
		EXPECT_EQ(3, mgr.count());

		mgr.removeFileInfo(fileId);
		EXPECT_EQ(2, mgr.count());
		mgr.removeFileInfo(99);
		EXPECT_EQ(2, mgr.count());

		fileId = mgr.addFileInfo(FileInfo{ "TestName", 1 });
		const FileInfo& fi = mgr.getFileInfo(fileId);
		EXPECT_STREQ("TestName", fi.fileName.c_str());
		EXPECT_EQ(1, fi.fileDescriptor);
		EXPECT_EQ(0, fi.fileSize);
	}

	TEST(FileInfoManager, ReturnsAllStoredFileDescriptors) {
		FileInfoManager mgr;

		int fileId = mgr.addFileInfo(FileInfo{ "TestName", 4 });
		mgr.addFileInfo(FileInfo{ "TestName2", 2 });
		mgr.addFileInfo(FileInfo{ "TestName3", 8 });

		std::vector<int> descriptorList = mgr.activeFileDescriptors();
		ASSERT_THAT(descriptorList, ::testing::UnorderedElementsAre(2,4,8));

		mgr.removeFileInfo(fileId);
		mgr.addFileInfo(FileInfo{ "TestName4", 9 });

		descriptorList = mgr.activeFileDescriptors();
		ASSERT_THAT(descriptorList, ::testing::UnorderedElementsAre(2,8,9));
	}

	TEST(FileInfoManager, CallsUserCallbackForAllMembers) {
		bool a, b, c;
		FileInfo fa, fb, fc;
		FileInfoManager mgr;

		fa.fileDescriptor = 1;
		fa.userCallback = [&](int, size_t) { a = true; };
		fb.fileDescriptor = 1;
		fb.userCallback = [&](int, size_t) { b = true; };
		fc.fileDescriptor = 1;
		fc.userCallback = [&](int, size_t) { c = true; };

		mgr.addFileInfo(fb);
		mgr.addFileInfo(fa);
		mgr.addFileInfo(fc);

		mgr.updateUsersInfo();

		EXPECT_TRUE(a);
		EXPECT_TRUE(b);
		EXPECT_TRUE(c);
	}

} // namespace sefile