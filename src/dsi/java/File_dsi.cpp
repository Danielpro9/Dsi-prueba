// File_dsi.cpp — DSi implementation of java/File.h, over BlocksDS's FAT
// (the SD card, mounted by dsiEnsureStorage() -- see DsiEarlyInit.h).
//
// Copied from src/wii/java/File_wii.cpp, which already documents why this is
// the easy case (libfat/BlocksDS's fatfs both give newlib a real POSIX
// filesystem, unlike the PS2's Memory Card workarounds in File_ps2.cpp) and
// why realpath() must not be used (device-prefixed paths like "sd:/..."
// don't survive it). Same two reasons apply here verbatim.
#ifdef DSI_PLATFORM

#include "java/File.h"
#include "platform/Storage.h"
#include "platform/storage/PathUtils.h"
#include "platform/storage/PosixFileSystem.h"

#include <string>
#include <fstream>
#include <memory>

#include "net/minecraft/src/GameResources.h"
#include "util/Memory.h"
#include "dsi/DsiEarlyInit.h"

namespace
{

class File_Impl : public File
{
private:
	std::string u8path;

public:
	explicit File_Impl(const jstring &p)
	{
		// File objects can be built before main() runs its own storage check
		// (DsiBootstrap::initialize() calls dsiEnsureStorage() too, but this
		// makes no assumption about ordering), so the mount cannot be taken
		// for granted here either.
		dsiEnsureStorage();
		u8path = PlatformStorage::normalizeSlashes(static_cast<const std::string &>(p));
		this->path = u8path;
	}

	bool createNewFile() const override
	{
		return PlatformStorage::createFile(u8path);
	}

	bool remove() const override
	{
		return PlatformStorage::removePath(u8path);
	}

	bool renameTo(const File &dest) const override
	{
		return PlatformStorage::renamePath(u8path, dest.toString());
	}

	bool exists() const override
	{
		return PlatformStorage::exists(u8path);
	}

	bool isDirectory() const override
	{
		return PlatformStorage::isDirectory(u8path);
	}

	bool isFile() const override
	{
		return PlatformStorage::isFile(u8path);
	}

	long_t lastModified() const override
	{
		return static_cast<long_t>(PlatformStorage::lastModifiedMs(u8path));
	}

	long_t length() const override
	{
		const std::int64_t size = PlatformStorage::fileSize(u8path);
		return size >= 0 ? static_cast<long_t>(size) : 0;
	}

	std::vector<std::unique_ptr<File>> listFiles() const override
	{
		std::vector<std::unique_ptr<File>> files;
		if (!isDirectory())
			return files;

		std::vector<std::string> entries;
		if (!PlatformStorage::listEntries(u8path, entries))
			return files;

		for (const std::string &entry : entries)
			files.push_back(Util::make_unique<File_Impl>(jstring(PlatformStorage::join(u8path, entry))));
		return files;
	}

	File *getParentFile() const override
	{
		return new File_Impl(jstring(PlatformStorage::parent(u8path)));
	}

	bool mkdir() const override
	{
		if (PlatformStorage::exists(u8path))
			return false;
		return PlatformStorage::makeDirectory(u8path, 0755);
	}

	std::istream *toStreamIn() const override
	{
		auto is = Util::make_unique<std::ifstream>(u8path, std::ios::binary);
		if (!is->is_open() || !is->good())
			return nullptr;
		return is.release();
	}

	std::ostream *toStreamOut() const override
	{
		auto os = Util::make_unique<std::ofstream>(u8path, std::ios::binary);
		if (!os->is_open() || !os->good())
			return nullptr;
		return os.release();
	}
};

} // namespace

File *File::open(const jstring &path)
{
	return new File_Impl(path);
}

File *File::open(const File &parent, const jstring &child)
{
	return new File_Impl(jstring(PlatformStorage::join(parent.toString(), child)));
}

File *File::openResourceDirectory()
{
	return new File_Impl(jstring(GameResources::getAssetsDir()));
}

File *File::openWorkingDirectory(const jstring &name)
{
	return new File_Impl(jstring(PlatformStorage::join(GameResources::getExeDir(), name)));
}

#endif // DSI_PLATFORM
