// Standalone probe, NOT part of DsiBringup.nds or the real game build.
//
// Question this answers: does the actual BlocksDS/Wonderful Toolchain
// arm-none-eabi-g++ support real std::thread/std::mutex/std::condition_variable
// and C++ exceptions, or -- like the generic `apt install gcc-arm-none-eabi`
// package used for local syntax-checking in this sandbox (configured with
// --disable-threads --disable-libstdc++-v3) -- does it lack a thread backend?
//
// This matters because the shared engine code (ThreadSleepForever,
// ThreadDownloadResources, NetworkManager, etc. under src/net/minecraft/src)
// uses real std::thread/std::mutex unconditionally, the same as it does on
// PS2 and Wii, both of which link fine because their SDKs (PS2SDK, libogc)
// provide a working thread backend. Whether BlocksDS/libnds does too for the
// DSi's ARM9 is not documented anywhere findable from this sandbox, and my
// own local ARM toolchain gives a false negative (it was built without any
// libstdc++ at all), so the only trustworthy way to answer this is to link a
// probe with the real toolchain in CI. See .github/workflows/dsi-bringup.yml's
// "thread-probe" job. This file lives outside src/dsi/tools/ (DsiBringup's
// SOURCEDIRS) on purpose: two main()s in the same link would break that
// unrelated, already-working build.
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace
{
std::atomic_bool g_ran(false);

void worker()
{
	g_ran.store(true);
}
} // namespace

int main()
{
	std::mutex m;
	std::condition_variable cv;
	{
		std::lock_guard<std::mutex> lock(m);
		(void)cv;
	}

	std::thread t(worker);
	t.join();

	bool caught = false;
	try
	{
		throw std::runtime_error("probe");
	}
	catch (const std::exception&)
	{
		caught = true;
	}

	return (g_ran.load() && caught) ? 0 : 1;
}
