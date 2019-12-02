//========================================================================
// GLFW 3.3 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>
//
// This file contains glfwAndroidMain() and related functions.

#include "internal.h"

#include <android_native_app_glue.h>
#include <android/window.h> /* for AWINDOW_FLAG_KEEP_SCREEN_ON */
#include <errno.h>
#include <string.h>  /* for strerror */
#include <unistd.h>

// Android Global state.
enum {
    // Android activity lifecycle
    STATE_START = 1,
    STATE_RESUME = 2,
    STATE_FOCUS = 4,
    STATE_SURFACE = 8,

    // Before app starts, all the above bits must be set.
    STATE_OK_TO_START_APP = STATE_SURFACE * 2 - 1,

    STATE_RUN = 16,  // This library has prepared everything. App can run now.
    STATE_DIRTY = 32,  // Request a reload of state on the main thread.
    STATE_FULLSCREEN = 64,
    STATE_SEND_DELAYED_FOCUS = 128,  // Wait, send focus event to a window.
    STATE_WANT_CLIPBOARDMGR = 256,
    STATE_APP_IN_PAUSE = 512,
};

typedef struct _globals {
    struct android_app* app;
    volatile unsigned state; // Track Android activity lifecycle.
} _globals;
_globals g;

//
// Android app lifecycle functions
//

static int pollAndroid(int timeout) {
    int events;
    struct android_poll_source* source;
    int ident = ALooper_pollAll(timeout, NULL, &events, (void**)(&source));
    if (ident < 0) {
        return ident;
    } else if (ident < LOOPER_ID_USER) {
        if (source != NULL) source->process(g.app, source);
    }
    // If you add user-defined event IDs here, be careful to only act on events
    // after APP_CMD_INIT_WINDOW but not after APP_CMD_TERM_WINDOW.
    return 0;
}

#define MIN_PAUSE_STATE (STATE_START)
void wrapPollAndroidForGLFW(int timeout) {
    int code = pollAndroid(timeout);
    if (code) {
        // Only exit if pollAndroid returns non-zero and destroyRequested.
        if (!g.app->destroyRequested) code = 0;
    }

    if (code || !(g.state & STATE_RUN)) {
        if (g.state & STATE_APP_IN_PAUSE &&
                (g.state & MIN_PAUSE_STATE) == MIN_PAUSE_STATE) {
            return;
        }
        _glfwInputWindowCloseRequest(_glfw.android.oneAndOnlyWindow);
        return;
    }
    if (_glfw.android.oneAndOnlyWindow && (g.state & STATE_RUN) &&
            (g.state & STATE_SEND_DELAYED_FOCUS)) {
        g.state &= ~STATE_SEND_DELAYED_FOCUS;
        _glfwInputWindowFocus(_glfw.android.oneAndOnlyWindow, GLFW_TRUE);
    }
}

int glfwIsPaused() {
    if (g.state & STATE_RUN) {
        g.state &= ~STATE_APP_IN_PAUSE;
        return GLFW_FALSE;
    }
    if ((g.state & (MIN_PAUSE_STATE | STATE_RUN)) == MIN_PAUSE_STATE) {
        // This app supports pausing - save that. Then glfwPollEvents() won't
        // _glfwInputWindowCloseRequest unless the app is actually exiting.
        g.state |= STATE_APP_IN_PAUSE;
        return GLFW_TRUE;
    }
    return GLFW_FALSE;
}

static void updateAppWithNewState(const char* event) {
    if ((g.state & STATE_OK_TO_START_APP) == STATE_OK_TO_START_APP) {
        if (!(g.state & STATE_RUN)) {
            g.state |= STATE_RUN | STATE_SEND_DELAYED_FOCUS;
        }
        return;
    }
    // g.state has dropped below the required STATE_OK_TO_START_APP
    // Tell the app to shut down.
    g.state &= ~STATE_RUN;

    // instance->surface must not be used any time after updateAppWithNewState
    // returns. Reset it here.
    //
    // Note that this can happen surprisingly easily, for example, just by
    // pulling down the top-of-screen drawer to change a wifi setting will
    // *completely* terminate the app. See:
    // https://developer.nvidia.com/fixing-common-android-lifecycle-issues-games
    if (_glfw.android.oneAndOnlyWindow) {
        _glfwInputWindowFocus(_glfw.android.oneAndOnlyWindow, GLFW_FALSE);
        // Ask app to exit. Android lifecycle will restart it.
        _glfwInputWindowCloseRequest(_glfw.android.oneAndOnlyWindow);
    }
}

static void cbAppCmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:  // Most often happens before GAINED_FOCUS.
        g.state |= STATE_SURFACE;
        updateAppWithNewState("APP_CMD_INIT_WINDOW");
        break;

    case APP_CMD_TERM_WINDOW:  // Most often happens after LOST_FOCUS.
        g.state &= ~STATE_SURFACE;
        updateAppWithNewState("APP_CMD_TERM_WINDOW");
        break;

    case APP_CMD_WINDOW_RESIZED:
        updateAppWithNewState("APP_CMD_WINDOW_RESIZED");
        break;

    case APP_CMD_WINDOW_REDRAW_NEEDED:
        _glfwPlatformAndroidNotifyWindowSize();
        break;

    case APP_CMD_CONTENT_RECT_CHANGED:
        _glfwPlatformAndroidNotifyWindowSize();
        break;

    case APP_CMD_GAINED_FOCUS:
        g.state |= STATE_FOCUS;
        updateAppWithNewState("APP_CMD_GAINED_FOCUS");
        break;

    case APP_CMD_LOST_FOCUS:  // App window loses focus.
        g.state &= ~STATE_FOCUS;
        updateAppWithNewState("APP_CMD_LOST_FOCUS");
        break;

    case APP_CMD_CONFIG_CHANGED:
        updateAppWithNewState("APP_CMD_CONFIG_CHANGED");
        break;

    case APP_CMD_LOW_MEMORY:
        updateAppWithNewState("APP_CMD_LOW_MEMORY");
        break;

    case APP_CMD_START:
        g.state |= STATE_START;  // Most often happens before GAINED_FOCUS.
        updateAppWithNewState("APP_CMD_START");
        break;

    case APP_CMD_RESUME:
        g.state |= STATE_RESUME;  // Most often happens before GAINED_FOCUS.
        updateAppWithNewState("APP_CMD_RESUME");
        break;

    case APP_CMD_SAVE_STATE:  // Most often happens after LOST_FOCUS.
        updateAppWithNewState("APP_CMD_SAVE_STATE");
        break;

    case APP_CMD_PAUSE:  // Most often happens after LOST_FOCUS.
        g.state &= ~STATE_RESUME;
        updateAppWithNewState("APP_CMD_PAUSE");
        break;

    case APP_CMD_STOP:  // Most often happens after LOST_FOCUS.
        g.state &= ~STATE_START;
        updateAppWithNewState("APP_CMD_STOP");
        break;

    case APP_CMD_DESTROY:
        break;

    case APP_CMD_MAIN_THREAD:
        if (g.state & STATE_DIRTY) {
            g.state &= ~STATE_DIRTY;
            _glfwPlatformAndroidSetFullscreen(!!(g.state & STATE_FULLSCREEN));
        }
        if (g.state & STATE_WANT_CLIPBOARDMGR) {
            g.state &= ~STATE_WANT_CLIPBOARDMGR;
            _glfwAndroidJNIinitClipboardManager();
        }
        break;
    }
}

static int32_t cbInputEvent(struct android_app* app, AInputEvent* event)
{
    if (!(g.state & STATE_RUN)) {
        // Drop any stray events when the instance has not been constructed.
        return 1;
    }
    return _glfwPlatformAndroidHandleEvent(app, event);
}

static void cbNativeWindowResized(ANativeActivity* act, ANativeWindow* w) {
  // Not called when you think, on all platforms! Do not use!
  // https://issuetracker.google.com/issues/37054453
  // https://groups.google.com/forum/#!topic/golang-codereviews/nBeOQl52bsY
}

static void android_app_write_cmd(int8_t cmd) {
    if (write(g.app->msgwrite, &cmd, sizeof(cmd)) != sizeof(cmd)) {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "android_app_write_cmd(msgwrite): %d %s", errno,
                        strerror(errno));
    }
}

static void cbNativeWindowRedrawNeeded(ANativeActivity* act, ANativeWindow* w) {
    android_app_write_cmd(APP_CMD_WINDOW_REDRAW_NEEDED);
}

static void cbContentRectChanged(ANativeActivity* act, const ARect* r) {
    android_app_write_cmd(APP_CMD_CONTENT_RECT_CHANGED);
}

void glfwAndroidMain(struct android_app* app, glfwAndroidMainFunc mainFunc) {
    if (!app) {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "glfwAndroidMain: android_app pointer is NULL");
        ANativeActivity_finish(app->activity);
        return;
    }

    // Android NDK vulkan_wrapper.cpp call to load vulkan functions:
    if (!InitVulkan()) {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "vulkan_wrapper.cpp failed to load libvulkan.so");
        ANativeActivity_finish(app->activity);
        return;
    }

    g.state = STATE_WANT_CLIPBOARDMGR;
    g.app = app;
    app->activity->callbacks->onNativeWindowResized = cbNativeWindowResized;
    app->activity->callbacks->onContentRectChanged = cbContentRectChanged;
    app->activity->callbacks->onNativeWindowRedrawNeeded =
        cbNativeWindowRedrawNeeded;
    app->onAppCmd = cbAppCmd;
    app->onInputEvent = cbInputEvent;

#ifndef ANDROID_NDK_MAJOR
    // ANDROID_NDK_MAJOR is set for all android targets in
    // vendor/volcano/src/gn/config/android/BUILD.gn. To port this to vanilla
    // Android NDK, just #define ANDROID_NDK_MAJOR yourself.
#error C preprocessor cannot compare strings. ANDROID_NDK_VERSION is a string.
#elif ANDROID_NDK_MAJOR < 15
    // For NDK versions 14 and lower: Linker will think android_native_app_glue
    // is unused without this app_dummy.
    // https://developer.android.com/ndk/samples/sample_na.html
    // http://altdevblog.com/2012/02/28/running-native-code-on-android-part-2/
    //
    // Fixed in r15:
    // https://github.com/android-ndk/ndk/issues/381
    app_dummy();
#endif

    char* argv[] = { NULL };

    // Start event loop, wait for g.state to reach STATE_RUN.
    int everGotToRun = GLFW_FALSE;
    for (;;) {
        int r = pollAndroid(-1);
        if (r) {
          if (!app->destroyRequested) {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "ALooper_pollAll error while waiting for window");
          } else if (!everGotToRun) {
            _glfwInputError(GLFW_PLATFORM_ERROR,
                            "ALooper_pollAll got destroyRequested during init");
          }
          break;
        }

        // If g.state has not reached STATE_RUN:
        if (!(g.state & STATE_RUN)) {
          continue;
        }

        // Same logic as _glfwPlatformSetWindowMonitor:
        _glfwPlatformKeepScreenOn(GLFW_FALSE);
        _glfwPlatformSetFullscreenState(GLFW_FALSE);
        everGotToRun = GLFW_TRUE;
        r = mainFunc(0, argv);
        if (r) {
          break;
        }

        if (g.state & STATE_RUN) {
          break;  // STATE_RUN means app *could* run, but app wants to quit.
        }
        // sendAppEvent cleared STATE_RUN. Go into a suspended state.
    }

    ANativeActivity_finish(app->activity);
    // Pump Android event loop 10 times if destroyRequested is not yet set.
    for (int i = 0; i < 10 && !app->destroyRequested; i++) {
      pollAndroid(10);
    }
}

void _glfwPlatformSetFullscreenState(int fullscreen) {
    g.state &= ~STATE_FULLSCREEN;
    g.state |= STATE_DIRTY | (fullscreen ? STATE_FULLSCREEN : 0);
    char msg = 'A';
    if (write(g.app->mainThreadPipe[1], &msg, sizeof(msg)) != sizeof(msg)) {
        _glfwInputError(GLFW_API_UNAVAILABLE,
                        "write(mainThreadPipe) failed: %d %s\n", errno,
                        strerror(errno));
    }
}

void _glfwPlatformKeepScreenOn(int screenOn) {
    // Since GLFW just "handles" this (setting fullscreen always sets
    // this value), a workaround for your app would be to directly change
    // the setting via the struct android_app*
    if (screenOn) {
        ANativeActivity_setWindowFlags(g.app->activity,
                                       AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
    } else {
        ANativeActivity_setWindowFlags(g.app->activity, 0,
                                       AWINDOW_FLAG_KEEP_SCREEN_ON);
    }
}

ANativeWindow* _glfwPlatformANativeWindow() {
    return g.app->window;
}

ANativeActivity* _glfwPlatformANativeActivity() {
    return g.app->activity;
}

struct android_app* glfwGetAndroidApp() {
    return g.app;
}
