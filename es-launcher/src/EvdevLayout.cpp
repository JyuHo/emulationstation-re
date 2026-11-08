// SPDX-License-Identifier: LGPL-3.0-or-later
// This file is part of es-launcher, the game launcher of EmulationStation-RE.
// Copyright (c) 2026 The EmulationStation-RE contributors.
// The orders follow SDL3 (src/joystick/linux/SDL_sysjoystick.c, ConfigJoystick) and RetroArch
// (input/drivers_joypad/udev_joypad.c, udev_add_pad).

#include "es-launcher/EvdevLayout.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace es::launcher
{
	namespace
	{
		constexpr int udevMaxButtons = 64;
		constexpr int udevMaxAxes = 32;

		constexpr size_t longBits = sizeof(unsigned long) * 8;
		constexpr size_t nbits(size_t x) { return (x + longBits - 1) / longBits; }

		bool testBit(const unsigned long* bits, int bit)
		{
			return (bits[bit / longBits] >> (bit % longBits)) & 1UL;
		}

		bool isHatAxis(int code) { return code >= ABS_HAT0X && code <= ABS_HAT3Y; }

		const EvdevLayout::Abs* findAbs(const std::vector<EvdevLayout::Abs>& abs, int code)
		{
			auto it = std::ranges::find(abs, code, &EvdevLayout::Abs::code);
			return it == abs.end() ? nullptr : &*it;
		}

		// GuessIfAxesAreDigitalHat of SDL3
		bool digitalHat(const EvdevLayout::Abs* x, const EvdevLayout::Abs* y, bool allDigital)
		{
			if (x == nullptr && y == nullptr)
				return false;
			if (allDigital)
				return true;
			if ((x == nullptr || (x->minimum == -1 && x->maximum == 1)) && (y == nullptr || (y->minimum == -1 && y->maximum == 1)))
				return true;
			auto noFilter = [](const EvdevLayout::Abs* a) { return a == nullptr || (!a->fuzz && !a->flat && !a->resolution); };
			return noFilter(x) && noFilter(y);
		}

		int indexOf(const std::vector<int>& codes, int code)
		{
			auto it = std::ranges::find(codes, code);
			return it == codes.end() ? -1 : static_cast<int>(it - codes.begin());
		}

		std::optional<InputBinding> convert(const InputBinding& input,
			const std::vector<int>& fromButtons, const std::vector<int>& toButtons,
			const std::vector<int>& fromAxes, const std::vector<int>& toAxes,
			bool toUdev, const std::vector<int>& sdlHats)
		{
			InputBinding result = input;

			switch (input.type)
			{
			case InputBinding::Type::Button:
			{
				if (input.id < 0 || input.id >= static_cast<int>(fromButtons.size()))
					return std::nullopt;
				int index = indexOf(toButtons, fromButtons[input.id]);
				if (index < 0)
					return std::nullopt;
				result.id = index;
				return result;
			}

			case InputBinding::Type::Axis:
			{
				if (input.id < 0 || input.id >= static_cast<int>(fromAxes.size()))
					return std::nullopt;
				int index = indexOf(toAxes, fromAxes[input.id]);
				if (index < 0)
					return std::nullopt;
				result.id = index;
				return result;
			}

			case InputBinding::Type::Hat:
			{
				if (toUdev)
				{
					if (input.id < 0 || input.id >= static_cast<int>(sdlHats.size()))
						return std::nullopt;
					result.id = sdlHats[input.id];
				}
				else
				{
					int index = indexOf(sdlHats, input.id);
					if (index < 0)
						return std::nullopt;
					result.id = index;
				}
				return result;
			}

			case InputBinding::Type::Key:
				return result;
			}

			return std::nullopt;
		}
	}

	bool sdlDigitalHats()
	{
		const char* value = std::getenv("SDL_JOYSTICK_LINUX_DIGITAL_HATS");
		if (value == nullptr || value[0] == 0)
			return true;
		return std::strcmp(value, "0") != 0 && std::strcmp(value, "false") != 0;
	}

	EvdevLayout EvdevLayout::fromCapabilities(std::string name, std::vector<int> keys, std::vector<Abs> abs, bool digitalHats)
	{
		EvdevLayout layout;
		layout.mName = std::move(name);

		std::ranges::sort(keys);
		auto has = [&](int code) { return std::ranges::binary_search(keys, code); };

		// SDL3
		for (int code : keys)
			if (code >= BTN_JOYSTICK && code < KEY_MAX)
				layout.mSdlButtons.push_back(code);
		for (int code : keys)
			if (code >= 0 && code < BTN_JOYSTICK)
				layout.mSdlButtons.push_back(code);

		bool hatPair[4] = {};
		for (int pair = 0; pair < 4; ++pair)
		{
			int x = ABS_HAT0X + pair * 2;
			if (digitalHat(findAbs(abs, x), findAbs(abs, x + 1), digitalHats))
			{
				hatPair[pair] = true;
				layout.mSdlHats.push_back(pair);
			}
		}

		std::ranges::sort(abs, {}, &Abs::code);
		for (const Abs& a : abs)
		{
			if (a.code < 0 || a.code >= ABS_MAX)
				continue;
			if (isHatAxis(a.code) && hatPair[(a.code - ABS_HAT0X) / 2])
				continue;
			layout.mSdlAxes.push_back(a.code);
		}

		// RetroArch udev
		auto addUdev = [&](int from, int to) {
			for (int code = from; code < to && static_cast<int>(layout.mUdevButtons.size()) < udevMaxButtons; ++code)
				if (has(code))
					layout.mUdevButtons.push_back(code);
		};
		addUdev(KEY_UP, KEY_DOWN + 1);
		addUdev(BTN_MISC, KEY_MAX);
		addUdev(0, KEY_UP);
		addUdev(KEY_DOWN + 1, BTN_MISC);

		for (const Abs& a : abs)
		{
			if (a.code < 0 || a.code >= ABS_MISC || isHatAxis(a.code))
				continue;
			if (a.maximum > a.minimum && static_cast<int>(layout.mUdevAxes.size()) < udevMaxAxes)
				layout.mUdevAxes.push_back(a.code);
		}

		return layout;
	}

	std::optional<EvdevLayout> EvdevLayout::read(const std::string& devicePath)
	{
		if (devicePath.empty() || !devicePath.starts_with("/dev/input/"))
			return std::nullopt;

		int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd < 0)
			return std::nullopt;

		unsigned long keybit[nbits(KEY_MAX)] = {};
		unsigned long absbit[nbits(ABS_MAX)] = {};
		char name[256] = {};

		if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0 || ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)
		{
			close(fd);
			return std::nullopt;
		}
		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 0)
			name[0] = 0;

		std::vector<int> keys;
		for (int code = 0; code < KEY_MAX; ++code)
			if (testBit(keybit, code))
				keys.push_back(code);

		std::vector<Abs> abs;
		for (int code = 0; code < ABS_MAX; ++code)
		{
			if (!testBit(absbit, code))
				continue;
			input_absinfo info {};
			if (ioctl(fd, EVIOCGABS(code), &info) < 0)
				continue;
			abs.push_back({ code, info.minimum, info.maximum, info.fuzz, info.flat, info.resolution });
		}

		close(fd);
		return fromCapabilities(name, std::move(keys), std::move(abs), sdlDigitalHats());
	}

	std::optional<InputBinding> EvdevLayout::sdlToUdev(const InputBinding& input) const
	{
		return convert(input, mSdlButtons, mUdevButtons, mSdlAxes, mUdevAxes, true, mSdlHats);
	}

	std::optional<InputBinding> EvdevLayout::udevToSdl(const InputBinding& input) const
	{
		return convert(input, mUdevButtons, mSdlButtons, mUdevAxes, mSdlAxes, false, mSdlHats);
	}

	Controller EvdevLayout::sdlToUdev(const Controller& pad) const
	{
		Controller result = pad;
		result.inputs.clear();
		for (const auto& [name, input] : pad.inputs)
			if (auto converted = sdlToUdev(input))
				result.inputs[name] = *converted;
		result.buttonCount = static_cast<int>(mUdevButtons.size());
		result.axisCount = static_cast<int>(mUdevAxes.size());
		return result;
	}

	bool EvdevLayout::sameNumbering() const
	{
		if (mSdlButtons != mUdevButtons || mSdlAxes != mUdevAxes)
			return false;
		for (size_t i = 0; i < mSdlHats.size(); ++i)
			if (mSdlHats[i] != static_cast<int>(i))
				return false;
		return true;
	}
}
