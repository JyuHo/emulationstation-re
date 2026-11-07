// Unit tests of services/HttpFileAccess: paths kept inside their root, and access rules of the file manager.
// Built with -DES_TESTS=ON, run with ctest.

#include "services/HttpFileAccess.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using HttpFileAccess::Access;

namespace
{
	int failures = 0;

	void check(bool condition, const char* expression, const char* file, int line)
	{
		if (!condition)
		{
			std::fprintf(stderr, "%s:%d: CHECK(%s) failed\n", file, line, expression);
			++failures;
		}
	}

	#define CHECK(x) check((x), #x, __FILE__, __LINE__)

	std::string base;

	bool resolves(const std::string& root, const std::string& relative, std::string* out = nullptr, bool mustExist = false)
	{
		std::string resolved;
		bool ok = HttpFileAccess::resolvePath(base + "/" + root, relative, resolved, mustExist);
		if (out)
			*out = resolved;
		return ok;
	}

	void testPaths()
	{
		fs::create_directories(base + "/roms/nes/sub");
		fs::create_directories(base + "/roms/snes");
		fs::create_directories(base + "/bios");
		fs::create_directories(base + "/secret");
		std::ofstream(base + "/roms/nes/mario.nes") << "rom";
		std::ofstream(base + "/secret/key") << "secret";
		fs::create_directory_symlink(base + "/secret", base + "/bios/outside");
		fs::create_directory_symlink(base + "/roms/nes/sub", base + "/roms/nes/inside");
		fs::create_symlink(base + "/nowhere", base + "/roms/nes/dangling");

		std::string out;

		CHECK(resolves("roms/nes", "", &out) && out == fs::canonical(base + "/roms/nes").string());
		CHECK(resolves("roms/nes", "mario.nes", &out, true) && out == fs::canonical(base + "/roms/nes").string() + "/mario.nes");
		CHECK(resolves("roms/nes", "./sub//mario.nes", &out)); // new file in an existing folder
		CHECK(resolves("roms/nes", "new.nes"));
		CHECK(!resolves("roms/nes", "new.nes", nullptr, true));

		// Leaving the root
		CHECK(!resolves("roms/nes", ".."));
		CHECK(!resolves("roms/nes", "../snes"));
		CHECK(!resolves("roms/nes", "sub/../../snes"));
		CHECK(!resolves("roms/nes", "..\\snes"));
		CHECK(!resolves("roms/nes", "/../../secret/key"));
		CHECK(!resolves("bios", "outside"));
		CHECK(!resolves("bios", "outside/key"));
		CHECK(!resolves("bios", "outside/new"));
		CHECK(!resolves("roms/nes", std::string("mario.nes\0/../x", 15)));

		// A root that is a prefix of a sibling folder does not reach it
		fs::create_directories(base + "/roms/nes2");
		fs::create_directory_symlink(base + "/roms/nes2", base + "/roms/nes/sibling");
		CHECK(!resolves("roms/nes", "sibling"));

		// Links staying inside the root are followed, and the link path is kept
		CHECK(resolves("roms/nes", "inside", &out) && out == fs::canonical(base + "/roms/nes").string() + "/inside");
		CHECK(resolves("roms/nes", "dangling"));

		// Missing parent folder or missing root
		CHECK(!resolves("roms/nes", "missing/new.nes"));
		CHECK(!resolves("missing", ""));
	}

	void testAccess()
	{
		// Basic base64("user:pw") and base64(":s3cr€t")
		const std::string userPw = "Basic dXNlcjpwdw==";
		const std::string euroPw = "Basic OnMzY3Ligqx0";

		// From this computer, with a local Host header: no password needed
		CHECK(HttpFileAccess::check("127.0.0.1", "127.0.0.1:1234", "", "") == Access::Granted);
		CHECK(HttpFileAccess::check("127.0.0.1", "localhost:1234", "", "pw") == Access::Granted);
		CHECK(HttpFileAccess::check("::1", "[::1]:1234", "", "") == Access::Granted);

		// DNS rebinding: local connection, foreign host name
		CHECK(HttpFileAccess::check("127.0.0.1", "evil.example:1234", "", "") == Access::NoPassword);
		CHECK(HttpFileAccess::check("127.0.0.1", "evil.example:1234", "", "pw") == Access::MissingPassword);

		// From the network
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", "", "") == Access::NoPassword);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", userPw, "") == Access::NoPassword);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", "", "pw") == Access::MissingPassword);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", userPw, "pw") == Access::Granted);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", userPw, "pw2") == Access::WrongPassword);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", userPw, "p") == Access::WrongPassword);
		CHECK(HttpFileAccess::check("192.168.1.20", "retro:1234", euroPw, "s3cr€t") == Access::Granted);
		CHECK(HttpFileAccess::check("192.168.1.20", "127.0.0.1:1234", "", "pw") == Access::MissingPassword); // Host header is not trusted from the network
	}
}

int main()
{
	char dir[] = "/tmp/es-http-file-access-XXXXXX";
	if (mkdtemp(dir) == nullptr)
		return 1;

	base = dir;

	testPaths();
	testAccess();

	fs::remove_all(base);

	if (failures == 0)
		std::printf("http_file_access_tests: all checks passed\n");

	return failures == 0 ? 0 : 1;
}
