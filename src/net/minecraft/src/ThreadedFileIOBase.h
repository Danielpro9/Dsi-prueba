#pragma once

#include "platform/PlatformConfig.h"

#if PLATFORM_ASYNC_FILE_IO
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>
#endif

class IThreadedFileIO;

// net.minecraft.src.ThreadedFileIOBase
class ThreadedFileIOBase
{
public:
	static ThreadedFileIOBase threadedIOInstance;

	ThreadedFileIOBase(const ThreadedFileIOBase &) = delete;
	ThreadedFileIOBase &operator=(const ThreadedFileIOBase &) = delete;
	~ThreadedFileIOBase();

	void queueIO(IThreadedFileIO *task);
	void waitForFinish();

private:
	ThreadedFileIOBase();
#if PLATFORM_ASYNC_FILE_IO
	void run();
	void processQueue();

	std::vector<IThreadedFileIO *> threadedIOQueue;
	std::unordered_set<IThreadedFileIO *> requeueRequested;
	std::mutex queueMutex;
	std::condition_variable queueCondition;
	std::condition_variable finishCondition;
	std::thread worker;
	std::uint64_t writeQueuedCounter;
	std::uint64_t savedIOCounter;
	bool isThreadWaiting;
	bool stopping;
#endif
};
