
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>


#include "vkvg.h"

#include "vkh_device.h"
#include "vkh_presenter.h"
#include "vkh_phyinfo.h"

#ifndef MIN
# define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#if defined(_WIN32) || defined(_WIN64)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h> // Windows.h -> WinDef.h defines min() max()

#else
	#include <sys/time.h>
#endif


void perform_test_onscreen (void(*testfunc)(void), const char *testName, int argc, char* argv[]);
