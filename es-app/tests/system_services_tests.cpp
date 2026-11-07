// Unit tests of system/SystemServices against stub nmcli, bluetoothctl, pactl, timedatectl and localectl.
// Built with -DES_TESTS=ON, run with ctest: each stub prints canned output and logs its arguments.

#include "system/SystemServices.h"
#include "Log.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

// The services only log: keep the real logger (and its Settings/Paths dependencies) out
LogLevel Log::mReportingLevel = LogError;
bool Log::mEnabled = false;
Log::Log(LogLevel level) : mLevel(level) {}
Log::~Log() {}
std::ostringstream& Log::stream() { static std::ostringstream sink; return sink; }

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

	std::string stubDir;

	void writeStub(const std::string& name, const std::string& body)
	{
		std::string path = stubDir + "/" + name;
		std::ofstream(path) << "#!/bin/sh\necho \"" << name << " $*\" >> \"" << stubDir << "/calls.log\"\n" << body << "\nexit 0\n";
		chmod(path.c_str(), 0755);
	}

	std::string calls()
	{
		std::ifstream in(stubDir + "/calls.log");
		std::stringstream ss;
		ss << in.rdbuf();
		return ss.str();
	}

	void clearCalls() { std::ofstream(stubDir + "/calls.log", std::ios::trunc); }

	bool called(const std::string& line) { return calls().find(line + "\n") != std::string::npos; }

	void setUp()
	{
		char tmpl[] = "/tmp/es-services-XXXXXX";
		stubDir = mkdtemp(tmpl);
		setenv("PATH", (stubDir + ":/usr/bin:/bin").c_str(), 1);

		writeStub("nmcli", R"(case "$*" in
  "radio wifi") echo enabled;;
  *"--fields SSID device wifi list"*) printf 'Home\\:Net\nCafe\n\nHome\\:Net\n';;
  *"--fields ACTIVE,SSID"*) printf 'no:Cafe\nyes:Home\\:Net\n';;
  *"wifi connect Hidden"*) exit 10;;
  *"connection up id Hidden"*) exit 0;;
  *"wifi connect Bad"*) exit 10;;
  *"connection up id Bad"*) exit 10;;
esac)");

		writeStub("bluetoothctl", R"(case "$*" in
  devices) printf 'Device AA:BB:CC:DD:EE:01 Xbox Wireless Controller\nDevice AA:BB:CC:DD:EE:02 JBL "Speaker"\n';;
  "info AA:BB:CC:DD:EE:01") printf 'Device AA:BB:CC:DD:EE:01 (public)\n\tName: Xbox Wireless Controller\n\tAlias: Xbox Wireless Controller\n\tIcon: input-gaming\n\tPaired: yes\n\tConnected: yes\n';;
  *"scan on") sleep 30;;
  "info AA:BB:CC:DD:EE:02") printf 'Device AA:BB:CC:DD:EE:02 (public)\n\tName: JBL "Speaker"\n\tAlias: JBL "Speaker"\n\tIcon: audio-card\n\tPaired: no\n\tConnected: no\n';;
esac)");

		writeStub("pactl", R"(case "$*" in
  get-default-sink) echo alsa_output.pci.analog-stereo;;
  "--format=json list sinks") echo '[{"index":1,"name":"alsa_output.pci.analog-stereo","description":"Built-in Audio Analog Stereo","properties":{"device.name":"alsa_card.pci"}},{"index":2,"name":"alsa_output.hdmi","description":"HDMI Output","properties":{"device.name":"alsa_card.hdmi"}}]';;
  "--format=json list cards") echo '[{"index":0,"name":"alsa_card.pci","properties":{"device.description":"Built-in Audio"},"profiles":{"off":{"description":"Off","available":true},"output:analog-stereo":{"description":"Analog Stereo Output","available":true},"output:hdmi":{"description":"HDMI","available":false}},"active_profile":"output:analog-stereo"}]';;
esac)");

		writeStub("timedatectl", R"(case "$*" in
  list-timezones) printf 'Europe/Paris\nUTC\n';;
  "show --property=Timezone --value") echo Europe/Paris;;
esac)");

		writeStub("localectl", R"(case "$*" in
  status) printf '   System Locale: LANG=fr_FR.UTF-8\n       VC Keymap: fr\n      X11 Layout: fr,us\n       X11 Model: pc105\n     X11 Variant: azerty,\n';;
esac)");
		writeStub("setxkbmap", "");

		// A trimmed xkeyboard-config rules list
		mkdir((stubDir + "/xkb").c_str(), 0755);
		mkdir((stubDir + "/xkb/rules").c_str(), 0755);
		std::ofstream(stubDir + "/xkb/rules/evdev.lst") <<
			"! model\n  pc105           Generic 105-key PC\n\n"
			"! layout\n  us              English (US)\n  de              German\n  fr              French\n\n"
			"! variant\n  azerty          ara: Arabic (AZERTY)\n  azerty          fr: French (AZERTY)\n  bepo            fr: French (BEPO)\n  dvorak          us: English (Dvorak)\n\n"
			"! option\n  grp             Switching to another layout\n";
		setenv("XKB_CONFIG_ROOT", (stubDir + "/xkb").c_str(), 1);
	}

	void testWifi()
	{
		using namespace SystemServices;
		CHECK(Wifi::isAvailable());
		CHECK(Wifi::isEnabled());

		auto networks = Wifi::networks(false);
		CHECK(networks.size() == 2);
		CHECK(networks.size() == 2 && networks[0] == "Home:Net" && networks[1] == "Cafe");
		CHECK(Wifi::connectedSsid() == "Home:Net");

		clearCalls();
		CHECK(Wifi::connect("My 'Net' $(rm -rf ~)", "p@ss word"));
		CHECK(called("nmcli --wait 30 device wifi connect My 'Net' $(rm -rf ~) password p@ss word"));

		clearCalls();
		CHECK(Wifi::connect("Hidden", ""));
		CHECK(called("nmcli --wait 30 connection up id Hidden"));
		CHECK(!Wifi::connect("Bad", "x"));
		CHECK(!Wifi::connect("", "x"));

		clearCalls();
		CHECK(Wifi::setEnabled(false));
		CHECK(called("nmcli radio wifi off"));
	}

	void testBluetooth()
	{
		using namespace SystemServices;
		CHECK(Bluetooth::isAvailable());

		auto all = Bluetooth::devices();
		CHECK(all.size() == 2);

		auto paired = Bluetooth::pairedDevices();
		CHECK(paired.size() == 1);
		if (paired.size() == 1)
		{
			CHECK(paired[0].address == "AA:BB:CC:DD:EE:01");
			CHECK(paired[0].icon == "input-gaming");
			CHECK(paired[0].connected);
			CHECK(Bluetooth::toXml(paired[0]) == R"(<device id="AA:BB:CC:DD:EE:01" name="Xbox Wireless Controller" type="input-gaming" connected="yes" />)");
		}

		if (all.size() == 2)
			CHECK(Bluetooth::toXml(all[1], "added") == R"(<device id="AA:BB:CC:DD:EE:02" name="JBL &quot;Speaker&quot;" type="audio-card" connected="no" status="added" />)");

		clearCalls();
		CHECK(Bluetooth::pair("AA:BB:CC:DD:EE:02"));
		CHECK(called("bluetoothctl --agent NoInputNoOutput --timeout 30 pair AA:BB:CC:DD:EE:02"));
		CHECK(called("bluetoothctl trust AA:BB:CC:DD:EE:02"));
		CHECK(called("bluetoothctl --timeout 15 connect AA:BB:CC:DD:EE:02"));

		// Discovery reports the unpaired device once, then stops the scanner when asked
		std::vector<std::string> found;
		std::atomic<bool> stop{ false };
		std::thread stopper([&stop] { std::this_thread::sleep_for(std::chrono::milliseconds(500)); stop = true; });
		clearCalls();
		Bluetooth::scan([&found](const std::string& xml) { found.push_back(xml); }, stop);
		stopper.join();
		CHECK(found.size() == 1);
		CHECK(found.size() == 1 && found[0].find("AA:BB:CC:DD:EE:02") != std::string::npos);
		CHECK(called("bluetoothctl scan off"));
	}

	void testAudio()
	{
		using namespace SystemServices;
		CHECK(Audio::isAvailable());

		auto outputs = Audio::outputs();
		CHECK(outputs.size() == 3);
		CHECK(outputs.size() == 3 && outputs[0] == "auto\tAuto" && outputs[1] == "alsa_output.pci.analog-stereo\tBuilt-in Audio Analog Stereo");
		CHECK(Audio::currentOutput() == "alsa_output.pci.analog-stereo");

		auto profiles = Audio::profiles();
		CHECK(profiles.size() == 3); // auto, off, analog stereo (hdmi is unavailable)
		CHECK(Audio::currentProfile() == "alsa_card.pci|output:analog-stereo");

		clearCalls();
		CHECK(Audio::setProfile("alsa_card.pci|output:analog-stereo"));
		CHECK(called("pactl set-card-profile alsa_card.pci output:analog-stereo"));
		CHECK(Audio::setOutput("alsa_output.hdmi"));
		CHECK(called("pactl set-default-sink alsa_output.hdmi"));

		clearCalls();
		CHECK(Audio::setOutput("auto"));
		CHECK(calls().empty());
	}

	void testTimeZone()
	{
		using namespace SystemServices;
		CHECK(TimeZone::isAvailable());
		CHECK(TimeZone::list().size() == 2);
		CHECK(TimeZone::current() == "Europe/Paris");
		clearCalls();
		CHECK(TimeZone::set("UTC"));
		CHECK(called("timedatectl set-timezone UTC"));
		CHECK(!TimeZone::set(""));
	}

	void testKeyboard()
	{
		using namespace SystemServices;
		CHECK(Keyboard::isAvailable());

		auto layouts = Keyboard::layouts();
		CHECK(layouts.size() == 3);
		CHECK(layouts.size() == 3 && layouts[0] == "us\tEnglish (US)" && layouts[1] == "fr\tFrench" && layouts[2] == "de\tGerman");

		auto variants = Keyboard::variants("fr");
		CHECK(variants.size() == 2);
		CHECK(variants.size() == 2 && variants[0] == "azerty\tFrench (AZERTY)" && variants[1] == "bepo\tFrench (BEPO)");
		CHECK(Keyboard::variants("de").empty());

		CHECK(Keyboard::currentLayout() == "fr");
		CHECK(Keyboard::currentVariant() == "azerty");

		// X11 session: applied right away too
		setenv("DISPLAY", ":0", 1);
		unsetenv("WAYLAND_DISPLAY");
		bool appliedNow = false;
		clearCalls();
		CHECK(Keyboard::set("us", "dvorak", &appliedNow));
		CHECK(called("localectl set-x11-keymap us  dvorak"));
		CHECK(called("setxkbmap -layout us -variant dvorak"));
		CHECK(appliedNow);

		// Wayland: the compositor keeps its keymap until the next session
		setenv("WAYLAND_DISPLAY", "wayland-0", 1);
		clearCalls();
		CHECK(Keyboard::set("de", "", &appliedNow));
		CHECK(called("localectl set-x11-keymap de  "));
		CHECK(calls().find("setxkbmap") == std::string::npos);
		CHECK(!appliedNow);

		CHECK(!Keyboard::set("", "", &appliedNow));
	}
}

int main()
{
	setUp();
	testWifi();
	testBluetooth();
	testAudio();
	testTimeZone();
	testKeyboard();

	CHECK(!SystemServices::hasCommand("es-no-such-command"));
	CHECK(SystemServices::run({ "es-no-such-command" }) == 127);

	SystemServices::run({ "rm", "-rf", stubDir });

	if (failures == 0)
		std::printf("system services tests: all passed\n");
	return failures == 0 ? 0 : 1;
}
