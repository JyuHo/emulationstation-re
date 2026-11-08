#include "EmulationStation.h"

// CMake defines ES_VERSION_STRING for this file only (see ES_VERSION in the top CMakeLists.txt)
#ifndef ES_VERSION_STRING
#define ES_VERSION_STRING "0.0.0-dev"
#endif

const char* esProgramVersionString() { return ES_VERSION_STRING; }
const char* esProgramBuiltString() { return __DATE__ " - " __TIME__; }
