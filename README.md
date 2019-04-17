# SEFile
*SEFile* is a simple app to write data into files asynchronously.

###### Code example
```C++
UvFileStream fs{[](int id, std::string errMsg, ErrorCode){ 
  cout << "File stream nr: " << id << " has failed with with message: " << errMsg;
  }};
  
std::thread t{ &UvFileStream::runLoop, std::ref(fs_) };

// Callback will be called when file is open and later on every 
// time when the event loop is idle to update current file size.
fs.createFileStreamAsync("FileName", [&](int id, size_t size) {
  postOnEventLoop(id, size);
}

...
// If id does not exists in UvFileStream data will be removed from the queue
// and error callback will be called.
fs.writeData(id, "Data to write!");

...
fs.stop();
t.join();

```

###### Build (only Windows)
SEFile requires *[Intel Threading Building Blocks](https://www.threadingbuildingblocks.org/)* and *[libuv](https://libuv.org/)*. The easiest way to provide those is to install them via *[VCPKG](https://github.com/Microsoft/vcpkg)*.

###### Tests
Unit tests require *Google Test and Google Mock*. If you use Visual Studio they will be installed by NuGet Package Manager.
