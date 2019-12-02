//========================================================================
// GLFW 3.3 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>
//
// This file contains null methods and methods that return constants.

#include "internal.h"

const char* _glfwPlatformGetVersionString(void)
{
    return _GLFW_VERSION_NUMBER " Android"
#if defined(_POSIX_TIMERS) && defined(_POSIX_MONOTONIC_CLOCK)
        " clock_gettime"
#else
        " gettimeofday"
#endif
#if defined(_GLFW_BUILD_DLL)
        " shared"
#endif
        ;
}

void _glfwPlatformTerminate(void) {
}

int _glfwPlatformGetPhysicalDevicePresentationSupport(VkInstance instance,
                                                      VkPhysicalDevice device,
                                                      uint32_t queuefamily)
{
    // Vulkan spec, 29.4.1. Android Platform:
    // "On Android, all physical devices and queue families must be capable of
    // presentation with any native window. As a result there is no
    // Android-specific query for these capabilities."
    return GLFW_TRUE;
}

void _glfwPlatformSetWindowTitle(_GLFWwindow* w, const char* s)
{
}

void _glfwPlatformSetWindowIcon(_GLFWwindow* w, int n, const GLFWimage* a)
{
}

void _glfwPlatformShowWindow(_GLFWwindow* w)
{   // Android OS does not let an app show itself. Only the user can do it.
}

void _glfwPlatformFocusWindow(_GLFWwindow* w)
{
}

void _glfwPlatformSetWindowPos(_GLFWwindow* w, int xpos, int ypos)
{
}

void _glfwPlatformSetWindowSize(_GLFWwindow* w, int width, int height)
{
}

void _glfwPlatformMaximizeWindow(_GLFWwindow* w)
{
}

void _glfwPlatformSetWindowAspectRatio(_GLFWwindow* w, int numer, int denom)
{
}

void _glfwPlatformHideWindow(_GLFWwindow* w)
{
}

int _glfwPlatformWindowFocused(_GLFWwindow* w)
{
    return GLFW_TRUE;
}

int _glfwPlatformWindowIconified(_GLFWwindow* w)
{
    return GLFW_FALSE;
}

int _glfwPlatformWindowHovered(_GLFWwindow* w)
{
    return GLFW_TRUE;
}

int _glfwPlatformWindowVisible(_GLFWwindow* w)
{
    return GLFW_TRUE;
}

int _glfwPlatformWindowMaximized(_GLFWwindow* w)
{
    return GLFW_FALSE;
}

int _glfwPlatformFramebufferTransparent(_GLFWwindow* w)
{
    return GLFW_FALSE;
}

void _glfwPlatformSetWindowResizable(_GLFWwindow* w, GLFWbool enabled)
{
}

void _glfwPlatformSetWindowDecorated(_GLFWwindow* w, GLFWbool enabled)
{
}

void _glfwPlatformSetWindowFloating(_GLFWwindow* w, GLFWbool enabled)
{
}

void _glfwPlatformSetWindowSizeLimits(_GLFWwindow* w, int minw, int minh,
                                      int maxw, int maxh)
{
}

float _glfwPlatformGetWindowOpacity(_GLFWwindow* w)
{
    return 1.f;
}

void _glfwPlatformSetWindowOpacity(_GLFWwindow* w, float opacity)
{
}

void _glfwPlatformFreeMonitor(_GLFWmonitor* m)
{   // To free any Android-specific handles for this m. Nothing to do here.
}

void _glfwPlatformGetMonitorPos(_GLFWmonitor* m, int* xpos, int* ypos)
{
    if (xpos) *xpos = 0;
    if (ypos) *ypos = 0;
}

GLFWbool _glfwPlatformGetGammaRamp(_GLFWmonitor* m, GLFWgammaramp* ramp)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "Android: Gamma ramp access not supported");
    return GLFW_FALSE;
}

void _glfwPlatformSetGammaRamp(_GLFWmonitor* m, const GLFWgammaramp* ramp)
{
}

void _glfwPlatformPostEmptyEvent(void)
{
}

void _glfwPlatformSetCursorPos(_GLFWwindow* w, double x, double y)
{
}

void _glfwPlatformSetCursorMode(_GLFWwindow* w, int mode)
{
}

int _glfwPlatformCreateCursor(_GLFWcursor* c, const GLFWimage* i,
                              int xhot, int yhot)
{
    return GLFW_TRUE;
}

int _glfwPlatformCreateStandardCursor(_GLFWcursor* cursor, int shape)
{
    return GLFW_TRUE;
}

void _glfwPlatformDestroyCursor(_GLFWcursor* cursor)
{
}

void _glfwPlatformSetCursor(_GLFWwindow* window, _GLFWcursor* cursor)
{
}

GLFWbool _glfwPlatformRawMouseMotionSupported(void) {
    return GLFW_FALSE;
}

void _glfwPlatformSetRawMouseMotion(_GLFWwindow* window, GLFWbool enabled) {
}
