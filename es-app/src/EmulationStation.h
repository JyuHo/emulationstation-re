#pragma once
#ifndef ES_APP_EMULATION_STATION_H
#define ES_APP_EMULATION_STATION_H

// The version comes from the release tag (CMake sets ES_VERSION_STRING, see ES_VERSION)
#ifndef ES_VERSION_STRING
#define ES_VERSION_STRING "0.0.0-dev"
#endif
#define PROGRAM_VERSION_STRING		ES_VERSION_STRING

#define PROGRAM_BUILT_STRING __DATE__ " - " __TIME__

#ifndef SCREENSCRAPER_SOFTNAME

#define SCREENSCRAPER_SOFTNAME			"Emulationstation"

#endif

#endif // ES_APP_EMULATION_STATION_H
