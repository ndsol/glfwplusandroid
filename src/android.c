//========================================================================
// GLFW 3.3 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>

#include "internal.h"

#include <android_native_app_glue.h>
#include <stdlib.h>
#include <string.h>

int _glfwPlatformInit(void) {
    // _glfw has just been memset(0)'d, leaving no way to pass a pointer in
    // from glfwAndroidMain. (It's clear why glfwInit() does that memset. But
    // it means the struct android_app* must be a global var. The rest of the
    // globals could be stored in the 'userData' of struct android_app.
    //
    // Instead, _globals g at the top of android_main.c stores all that.

    _glfwPlatformAndroidNotifyWindowSize();
    _glfwPlatformAndroidGetDisplayMetrics();
    _glfwAndroidJNIpollDeviceIds();

    _GLFWmonitor* monitor = _glfwAllocMonitor(
        "Android", _glfw.android.width / 10, _glfw.android.height / 10);
    _glfwInputMonitor(monitor, GLFW_CONNECTED, _GLFW_INSERT_FIRST);
    return GLFW_TRUE;
}

//
// Android Vulkan functions
//

void _glfwPlatformGetRequiredInstanceExtensions(char** extensions)
{
    if (!_glfw.vk.KHR_surface)
        return;

    if (!_glfw.vk.KHR_android_surface)
        return;

    // NOTE: internal.h defines extensions[2] with size 2.
    extensions[0] = "VK_KHR_surface";
    extensions[1] = "VK_KHR_android_surface";
}

VkResult _glfwPlatformCreateWindowSurface(VkInstance instance, _GLFWwindow* w,
                                          const VkAllocationCallbacks* alloc,
                                          VkSurfaceKHR* surface) {
    if (!_glfw.vk.KHR_surface) {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "Android: Vulkan instance missing VK_KHR_xcb_surface extension");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    if (!_glfw.vk.KHR_android_surface) {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "Android: Vulkan instance missing VK_KHR_android_surface extension");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkAndroidSurfaceCreateInfoKHR a;
    memset(&a, 0, sizeof(a));
    a.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    a.window = _glfwPlatformANativeWindow();
    return vkCreateAndroidSurfaceKHR(instance, &a, alloc, surface);
}

void _glfwPlatformAndroidNotifyWindowSize(void) {
    ANativeWindow* anativeWindow = _glfwPlatformANativeWindow();
    if (!anativeWindow) {
        // This can happen if the app launches with the screen locked.
        // Ignore the notification. Another comes when the screen is unlocked.
        return;
    }
    _glfw.android.width = ANativeWindow_getWidth(anativeWindow);
    _glfw.android.height = ANativeWindow_getHeight(anativeWindow);
    if (_glfw.android.oneAndOnlyWindow) {
        _glfwInputWindowSize(_glfw.android.oneAndOnlyWindow,
                             _glfw.android.width, _glfw.android.height);
    }
}

// GLFW platform functions

int _glfwPlatformCreateWindow(_GLFWwindow* w, const _GLFWwndconfig* g,
                              const _GLFWctxconfig* c, const _GLFWfbconfig* f)
{
    if (!_glfw.android.oneAndOnlyWindow) {
        _glfw.android.oneAndOnlyWindow = w;
        return GLFW_TRUE;
    }
    _glfwInputError(GLFW_OUT_OF_MEMORY,
                    "Android: attempt to create a second window");
    return GLFW_FALSE;
}

void _glfwPlatformDestroyWindow(_GLFWwindow* w)
{
    if (_glfw.android.oneAndOnlyWindow == w) {
        _glfw.android.oneAndOnlyWindow = NULL;
        return;
    }
    _glfwInputError(GLFW_OUT_OF_MEMORY,
                    "Android: destroyed a different window");
}

void _glfwPlatformIconifyWindow(_GLFWwindow* w)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "iconify");
        return;
    }
    _glfwPlatformSetFullscreenState(GLFW_FALSE);
}

void _glfwPlatformRestoreWindow(_GLFWwindow* w)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "restore");
        return;
    }
    _glfwPlatformSetFullscreenState(GLFW_FALSE);
}

void _glfwPlatformGetWindowPos(_GLFWwindow* w, int* xpos, int* ypos)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "getWindowPos");
        return;
    }
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
}

void _glfwPlatformGetWindowSize(_GLFWwindow* w, int* width, int* height)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "getWindowSize");
        return;
    }
    if (width) *width = _glfw.android.width;
    if (height) *height = _glfw.android.height;
}

void _glfwPlatformGetFramebufferSize(_GLFWwindow* w, int* width, int* height)
{
    _glfwPlatformGetWindowSize(w, width, height);
}

void _glfwPlatformGetWindowFrameSize(_GLFWwindow* w, int* left, int* top,
                                     int* right, int* bottom)
{
    if (left) *left = 0;
    if (top) *top = 0;
    if (right) *right = 0;
    if (bottom) *bottom = 0;
}

void _glfwPlatformGetWindowContentScale(_GLFWwindow* w, float* x, float* y)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "getWindowContentScale");
        return;
    }
    if (x) *x = _glfw.android.density;
    if (y) *y = _glfw.android.density;
}

void _glfwPlatformSetWindowMonitor(_GLFWwindow* w, _GLFWmonitor* m,
                                   int x, int y, int width, int height,
                                   int refreshRate)
{
    if (_glfw.android.oneAndOnlyWindow != w) {
        _glfwInputError(GLFW_OUT_OF_MEMORY, "Android: %s: unknown window",
                        "setWindowMonitor");
        return;
    }
    _glfwInputWindowMonitor(w, m);  // Tell all of glfw w is using this m.
    if (m != NULL) {
        _glfwPlatformKeepScreenOn(GLFW_TRUE);
        _glfwPlatformSetFullscreenState(GLFW_TRUE);
    } else {
        _glfwPlatformKeepScreenOn(GLFW_FALSE);
        _glfwPlatformSetFullscreenState(GLFW_FALSE);
    }
}

void _glfwPlatformGetMonitorContentScale(_GLFWmonitor* m, float* x, float* y)
{
    if (x) *x = _glfw.android.density;
    if (y) *y = _glfw.android.density;
}

GLFWvidmode* _glfwPlatformGetVideoModes(_GLFWmonitor* m, int* count)
{
    *count = 1;
    GLFWvidmode* result = calloc(1, sizeof(GLFWvidmode));
    _glfwPlatformGetVideoMode(m, result);
    return result;
}

static int _glfwPlatformGetRGBbits()
{
    switch (ANativeWindow_getFormat(_glfwPlatformANativeWindow())) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
        return 24;
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
        return 16;
    case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
        return 48;
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
        return 30;
    case AHARDWAREBUFFER_FORMAT_BLOB:
    case AHARDWAREBUFFER_FORMAT_D16_UNORM:
    case AHARDWAREBUFFER_FORMAT_D24_UNORM:
    case AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT:
    case AHARDWAREBUFFER_FORMAT_D32_FLOAT:
    case AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT:
    case AHARDWAREBUFFER_FORMAT_S8_UINT:
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "_glfwPlatformGetVideoMode: not a color format");
        break;
    }
    return 16;  // Totally invalid value. Hopefully it is harmless.
}

void _glfwPlatformGetVideoMode(_GLFWmonitor* monitor, GLFWvidmode* mode)
{
    mode->width = _glfw.android.width;
    mode->height = _glfw.android.height;
    mode->refreshRate = 0;
    _glfwSplitBPP(_glfwPlatformGetRGBbits(),
                  &mode->redBits, &mode->greenBits, &mode->blueBits);
}

void _glfwPlatformGetMonitorWorkarea(_GLFWmonitor* m, int* xpos, int* ypos,
                                     int* width, int* height) {
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
    if (width) *width = _glfw.android.width;
    if (height) *height = _glfw.android.height;
}

void _glfwPlatformPollEvents(void)
{
  // timeout of 0 for non-blocking polling (but presentMode also throttles the
  // app so this is not a complete CPU hog in most situations).
  // timeout of -1 to block (useful if app is not animating).
  wrapPollAndroidForGLFW(0);
}

void _glfwPlatformWaitEvents(void)
{
  // timeout of 0 for non-blocking polling (but presentMode also throttles the
  // app so this is not a complete CPU hog in most situations).
  // timeout of -1 to block (useful if app is not animating).
  wrapPollAndroidForGLFW(-1);
}

void _glfwPlatformWaitEventsTimeout(double timeout)
{
  wrapPollAndroidForGLFW((int)(timeout*1000));
}
