//========================================================================
// GLFW 3.3 X11 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "posix_thread.h"
#include "posix_time.h"

#define _glfw_dlopen(name) dlopen(name, RTLD_LAZY | RTLD_LOCAL)
#define _glfw_dlclose(handle) dlclose(handle)
#define _glfw_dlsym(handle, name) dlsym(handle, name)

// EGL is not yet supported on Android.
#define _GLFW_EGL_CONTEXT_STATE enum { _GLFW_EGL_CONTEXT_STATE_vALUE = 0 }
#define _GLFW_EGL_LIBRARY_CONTEXT_STATE \
    enum { _GLFW_EGL_LIBRARY_CONTEXT_STATE_vALUE = 0 }
// OSMESA does not exist on Android.
#define _GLFW_OSMESA_CONTEXT_STATE \
    enum { _GLFW_OSMESA_CONTEXT_STATE_vALUE = 0 }
#define _GLFW_OSMESA_LIBRARY_CONTEXT_STATE \
    enum { _GLFW_OSMESA_LIBRARY_CONTEXT_STATE_vALUE = 0 }

#define _GLFW_PLATFORM_LIBRARY_CONTEXT_STATE \
    enum { _GLFW_PLATFORM_LIBRARY_WINDOW_STATE_vALUE = 0 }
#define _GLFW_PLATFORM_CONTEXT_STATE \
    enum { _GLFW_PLATFORM_CONTEXT_STATE_vALUE = 0 }
#define _GLFW_PLATFORM_WINDOW_STATE \
    enum { _GLFW_PLATFORM_WINDOW_STATE_vALUE = 0 }
#define _GLFW_PLATFORM_MONITOR_STATE \
    enum { _GLFW_PLATFORM_MONITOR_STATE_vALUE = 0 }
#define _GLFW_PLATFORM_CURSOR_STATE \
    enum { _GLFW_PLATFORM_CURSOR_STATE_vALUE = 0 }
#define _GLFW_PLATFORM_LIBRARY_JOYSTICK_STATE \
    enum { _GLFW_PLATFORM_LIBRARY_JOYSTICK_STATE_vALUE = 0 }

#define _GLFW_PLATFORM_MAPPING_NAME "Android"
#define _GLFW_PLATFORM_JOYSTICK_STATE _GLFWjoystickAndroid android
#define _GLFW_PLATFORM_LIBRARY_WINDOW_STATE _GLFWlibraryAndroid android
#define _GLFW_PLATFORM_MAX_EVENTS (20)

typedef struct _GLFWlibraryAndroid {
    _GLFWwindow* oneAndOnlyWindow;
    int64_t lastJoystickNanos;
    int width;  // ANativeWindow size
    int height;
    float density;
    float xdpi;
    float ydpi;
    unsigned numPrev;
    int softInputDisplayed;
    GLFWinputEvent prev[_GLFW_PLATFORM_MAX_EVENTS];
    GLFWinputEvent next[_GLFW_PLATFORM_MAX_EVENTS];
    char clipboardData[256];
} _GLFWlibraryAndroid;

typedef struct _GLFWjoystickAndroidRange {
    float base;
    float range;
    float flat;
    int id;  // Axis ID, such as AXIS_X, AXIS_Y, etc.
} _GLFWjoystickAndroidRange;

typedef struct _GLFWjoystickAndroid {
    int id;  // Stores Java's InputDevice.getId() (also getDeviceIds()).
    int mark;  // Used to detect if joystick was not touched during polling.
    _GLFWjoystickAndroidRange* axis;
} _GLFWjoystickAndroid;

typedef struct AInputEvent AInputEvent;
typedef struct ANativeWindow ANativeWindow;
typedef struct ANativeActivity ANativeActivity;

ANativeWindow* _glfwPlatformANativeWindow();
ANativeActivity* _glfwPlatformANativeActivity();
void _glfwPlatformAndroidNotifyWindowSize();
void _glfwPlatformKeepScreenOn(int screenOn);
void _glfwPlatformSetFullscreenState(int fullscreen);
int32_t _glfwPlatformAndroidHandleEvent(struct android_app* app,
                                        AInputEvent* event);
void _glfwPlatformAndroidGetDisplayMetrics();
void _glfwPlatformAndroidSetFullscreen(int fullscreen);
void _glfwAndroidJNIpollDeviceIds();
void _glfwAndroidJNIinitClipboardManager();
void wrapPollAndroidForGLFW(int timeout);
int jniGetUnicodeChar(AInputEvent* event);
void handleKeyEvent(struct android_app* app, AInputEvent* event, int mods);
