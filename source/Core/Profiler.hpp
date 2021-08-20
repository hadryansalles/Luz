#include "optick.h"

#define LUZ_PROFILE_FRAME()      OPTICK_FRAME("MainThread")
#define LUZ_PROFILE_FUNC()       OPTICK_EVENT()
#define LUZ_PROFILE_THREAD(name) OPTICK_EVENT((name))