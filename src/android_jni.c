//========================================================================
// GLFW 3.3 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>
//
// This file contains glfwJNI*() functions.

#include "internal.h"

#include <android_native_app_glue.h>
#include <jni.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

//
// Accessing methods and the JNIEnv
//

static jmethodID glfwJNIgetMethod(JNIEnv* env, jclass c, const char* name,
                                  const char* args) {
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "glfwJNIgetMethod(%s): env is NULL",
                    name);
    return NULL;
  }
  if (!c) {
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "glfwJNIgetMethod(%s): jclass is NULL", name);
    return NULL;
  }
  jmethodID r = (*env)->GetMethodID(env, c, name, args);
  if (!r) {
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "GetMethodID(%s) returned NULL", name);
  }
  return r;
}

static void glfwJNIdestroy(ANativeActivity* activity, int mustDetach)
{
  if (mustDetach) {
    jint result = (*activity->vm)->DetachCurrentThread(activity->vm);
    if (result != JNI_OK) {
      _glfwInputError(GLFW_PLATFORM_ERROR,
                      "glfwJNIdestroy: DetachCurrentThread failed: %lld",
                      (long long)result);
    }
  }
}

#define EXCEPTION_STRLEN (256)
// jniGetString enforces EXCEPTION_STRLEN on 'out'.
static int jniGetString(JNIEnv* env, jobject o, char* out, jmethodID m) {
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "jniGetString: env is NULL");
    return 1;
  }
  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "jniGetString: object is NULL");
    return 1;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "jniGetString: methodID is NULL");
    return 1;
  }

  jstring str = (jstring) (*env)->CallObjectMethod(env, o, m);
  if (!str) {
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "jniGetString: CallObjectMethod failed");
    return 1;
  }
  jthrowable ex = (*env)->ExceptionOccurred(env);
  if (ex) {
    (*env)->ExceptionClear(env);
    _glfwInputError(GLFW_PLATFORM_ERROR, "jniGetString: throws exception");
    (*env)->DeleteLocalRef(env, ex);
    return 1;
  }
  jint strlen = (*env)->GetStringUTFLength(env, str);
  if (strlen > EXCEPTION_STRLEN - 1) {
    strlen = EXCEPTION_STRLEN - 1;
  }
  (*env)->GetStringUTFRegion(env, str, 0, strlen, out);
  out[strlen] = 0;
  (*env)->DeleteLocalRef(env, str);
  return 0;
}

static int jniCheckException(JNIEnv* env, char* typeStr) {
  jthrowable ex = (*env)->ExceptionOccurred(env);
  if (!ex) {
    return 0;
  }
  (*env)->ExceptionClear(env);

  jmethodID jgetClass = glfwJNIgetMethod(env, (*env)->GetObjectClass(env, ex),
                                         "getClass", "()Ljava/lang/Class;");
  if (!jgetClass) {
    snprintf(typeStr, EXCEPTION_STRLEN, "GetMethodID(%s) returned NULL",
             "getClass");
    return 1;
  }
  jobject o = (*env)->CallObjectMethod(env, ex, jgetClass);
  if (!o) {
    snprintf(typeStr, EXCEPTION_STRLEN, "CallObjectMethod(getClass) failed");
    return 1;
  }
  jthrowable ex2 = (*env)->ExceptionOccurred(env);
  if (ex2) {
    (*env)->ExceptionClear(env);
    snprintf(typeStr, EXCEPTION_STRLEN,
             "getClass of exception also throws exception");
    (*env)->DeleteLocalRef(env, ex2);
    return 1;
  }
  (*env)->DeleteLocalRef(env, ex);
  if (jniGetString(
          env, o, typeStr,
          glfwJNIgetMethod(env, (*env)->GetObjectClass(env, o), "getName",
                           "()Ljava/lang/String;"))) {
    snprintf(typeStr, EXCEPTION_STRLEN, "jniGetString(exception) failed");
    return 1;
  }
  return 1;
}

static JNIEnv* glfwJNIinit(ANativeActivity* activity, int* mustDetach)
{
  *mustDetach = GLFW_FALSE;
  JNIEnv* env = NULL;

  // https://developer.android.com/training/articles/perf-jni
  // "In theory you can have multiple JavaVMs per process, but Android only
  // allows one." Thus there is always only one env per thread.
  jint r = (*activity->vm)->GetEnv(activity->vm, (void**)&env,
                                   JNI_VERSION_1_6);
  if (r != JNI_OK && r != JNI_EDETACHED) {
    if (r == JNI_EVERSION) {
      _glfwInputError(GLFW_PLATFORM_ERROR,
                      "glfwJNIinit: GetEnv: v1.6 not supported: JNI_EVERSION");
    } else {
      _glfwInputError(GLFW_PLATFORM_ERROR, "glfwJNIinit: GetEnv failed: %lld",
                      (long long)r);
    }
    return NULL;
  }
  if (r == JNI_OK) {
    if (!env) {
      _glfwInputError(GLFW_PLATFORM_ERROR, "%s returned NULL",
                      "glfwJNIinit: GetEnv succeeded but");
      return NULL;
    }
    return env;
  }

  // This thread is not yet attached. Attach it.
  JavaVMAttachArgs arg;
  arg.version = JNI_VERSION_1_6;
  arg.name = "glfwAndroid";  // Shows up in stack traces.
  arg.group = NULL;
  r = (*activity->vm)->AttachCurrentThread(activity->vm, &env, &arg);
  if (r != JNI_OK) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "AttachCurrentThread failed: %lld",
                    (long long)r);
    return NULL;
  }
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "AttachCurrentThread failed: env is NULL");
    return NULL;
  }
  char exTypeStr[EXCEPTION_STRLEN];
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", "glfwJNIinit",
                    exTypeStr);
    glfwJNIdestroy(activity, GLFW_TRUE);
    return NULL;
  }
  *mustDetach = GLFW_TRUE;
  return env;
}

static int voidCall(JNIEnv* env, jobject o, jmethodID m,
                    const char* methodName, ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;

  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: NULL object", methodName);
    return 1;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return 1;
  }

  va_start(ap, methodName);
  (*env)->CallVoidMethodV(env, o, m, ap);
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return 1;
  }
  return 0;
}

static jobject objectCall(JNIEnv* env, jobject o, jmethodID m,
                          const char* methodName, ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;
  jobject r;

  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: NULL object", methodName);
    return NULL;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return NULL;
  }

  va_start(ap, methodName);
  if (!strncmp(methodName, "new(", 4)) {
    r = (*env)->NewObjectV(env, o, m, ap);
  } else {
    r = (*env)->CallObjectMethodV(env, o, m, ap);
  }
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return NULL;
  }
  if (!r) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s returned NULL", methodName);
  }
  return r;
}

static jobject objectCallQuiet(JNIEnv* env, jobject o, jmethodID m,
                               const char* methodName, ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;
  jobject r;

  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: NULL object", methodName);
    return NULL;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return NULL;
  }

  va_start(ap, methodName);
  if (!strncmp(methodName, "new(", 4)) {
    r = (*env)->NewObjectV(env, o, m, ap);
  } else {
    r = (*env)->CallObjectMethodV(env, o, m, ap);
  }
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return NULL;
  }
  return r;
}

static int intCall(JNIEnv* env, jobject o, jmethodID m, const char* methodName,
                   ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;

  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: NULL object", methodName);
    return -42;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return -42;
  }

  va_start(ap, methodName);
  int r = (*env)->CallIntMethodV(env, o, m, ap);
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return -42;
  }
  return r;
}

static int floatCall(JNIEnv* env, jobject o, jmethodID m,
                     const char* methodName, ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;

  if (!o) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: NULL object", methodName);
    return NAN;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return NAN;
  }

  va_start(ap, methodName);
  float r = (*env)->CallFloatMethodV(env, o, m, ap);
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return NAN;
  }
  return r;
}

static jobject staticObjectCall(JNIEnv* env, jclass c, jmethodID m,
                                const char* methodName, ...) {
  char exTypeStr[EXCEPTION_STRLEN];
  va_list ap;

  if (!c) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: FindClass failed", methodName);
    return NULL;
  }
  if (!m) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", methodName);
    return NULL;
  }

  va_start(ap, methodName);
  jobject r = (*env)->CallStaticObjectMethodV(env, c, m, ap);
  va_end(ap);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s", methodName,
                    exTypeStr);
    return NULL;
  }
  if (!r) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s returned NULL", methodName);
  }
  return r;
}

void glflog(char level, const char* fmt, ...);
static jclass safeFindClass(JNIEnv* env, const char* className)
{
  jclass c = (*env)->FindClass(env, className);
  char exTypeStr[EXCEPTION_STRLEN];
  if (jniCheckException(env, exTypeStr)) {
    if (!_glfw.android.oneAndOnlyWindow) {
      glflog('E', "FindClass(%s) exception: %s", className, exTypeStr);
    } else {
      _glfwInputError(GLFW_PLATFORM_ERROR, "FindClass(%s) exception: %s",
                      className, exTypeStr);
    }
    return NULL;
  } else if (!c) {
    if (!_glfw.android.oneAndOnlyWindow) {
      glflog('E', "FindClass(%s) failed", className);
    } else {
      _glfwInputError(GLFW_PLATFORM_ERROR, "FindClass(%s) failed", className);
    }
    return NULL;
  }
  return c;
}

//
// Platform methods that call into the JNI
//

static float getDisplayMetricField(JNIEnv* env, jclass jDM, jobject obj,
                                   const char* field) {
  jfieldID fieldID = (*env)->GetFieldID(env, jDM, field, "F");
  if (!fieldID) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "GetFieldID(%s) failed", field);
    return 0.0f;
  }
  return (*env)->GetFloatField(env, obj, fieldID);
}

static void _glfwPlatformAndroidDisplayMetricsInner(ANativeActivity* activity,
                                                    JNIEnv* env)
{
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformAndroidGetDisplayMetrics", "glfwJNIinit");
    return;
  }
  jclass jDM = safeFindClass(env, "android/util/DisplayMetrics");
  jclass jResources = safeFindClass(env, "android/content/res/Resources");
  jmethodID getDM = glfwJNIgetMethod(
      env, jResources, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
  jmethodID gr = glfwJNIgetMethod(
      env, (*env)->GetObjectClass(env, activity->clazz), "getResources",
      "()Landroid/content/res/Resources;");
  jobject resources = objectCall(env, activity->clazz, gr, "getResources");
  jobject obj = objectCall(env, resources, getDM, "getDisplayMetrics");
  if (!obj || !jDM) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformAndroidGetDisplayMetrics", "getClass()");
    return;
  }

  _glfw.android.density = getDisplayMetricField(env, jDM, obj, "density");
  _glfw.android.xdpi = getDisplayMetricField(env, jDM, obj, "xdpi");
  _glfw.android.ydpi = getDisplayMetricField(env, jDM, obj, "ydpi");
  (*env)->DeleteLocalRef(env, obj);
  (*env)->DeleteLocalRef(env, resources);
  (*env)->DeleteLocalRef(env, jResources);
  (*env)->DeleteLocalRef(env, jDM);
}

void _glfwPlatformAndroidGetDisplayMetrics(void)
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  _glfwPlatformAndroidDisplayMetricsInner(activity, env);
  glfwJNIdestroy(activity, mustDetach);
}

static void _glfwPlatformAndroidFullscreenInner(
    ANativeActivity* activity, JNIEnv* env, int isFullscreen)
{
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformAndroidSetFullscreen", "glfwJNIinit");
    return;
  }
  jclass jView = safeFindClass(env, "android/view/View");
  jclass jWindow = safeFindClass(env, "android/view/Window");
  if (!jView || !jWindow) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformAndroidSetFullscreen", "getClass()");
    return;
  }

  jint flags = 0;  // TODO: add SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN if fullscreen?
  static const char* flagNames[] = {
      "SYSTEM_UI_FLAG_LOW_PROFILE",       // Requires API Level 14.
      "SYSTEM_UI_FLAG_HIDE_NAVIGATION",   // Requires API Level 14.
      "SYSTEM_UI_FLAG_FULLSCREEN",        // Requires API Level 16.
      "SYSTEM_UI_FLAG_IMMERSIVE_STICKY",  // Requires API Level 19.
  };
  if (isFullscreen) {  // if not fullscreen, flags remains 0.
    for (unsigned i = 0; i < sizeof(flagNames)/sizeof(flagNames[0]); i++) {
      jfieldID field = (*env)->GetStaticFieldID(env, jView, flagNames[i], "I");
      jint val = (*env)->GetStaticIntField(env, jView, field);
      flags |= val;
    }
  }

  jmethodID getWindow = glfwJNIgetMethod(
      env, (*env)->GetObjectClass(env, activity->clazz), "getWindow",
      "()Landroid/view/Window;");
  jobject window = objectCall(env, activity->clazz, getWindow, "getWindow");
  jobject view = objectCall(
      env, window,
      glfwJNIgetMethod(env, jWindow, "getDecorView", "()Landroid/view/View;"),
      "getDecorView");
  if (voidCall(env, view,
               glfwJNIgetMethod(env, jView, "setSystemUiVisibility", "(I)V"),
               "setSystemUiVisibility", flags)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformAndroidSetFullscreen",
                    "setSystemUiVisibility");
    return;
  }
  (*env)->DeleteLocalRef(env, view);
  (*env)->DeleteLocalRef(env, window);
  (*env)->DeleteLocalRef(env, jWindow);
  (*env)->DeleteLocalRef(env, jView);
}

void _glfwPlatformAndroidSetFullscreen(int fullscreen)
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  _glfwPlatformAndroidFullscreenInner(activity, env, fullscreen);
  glfwJNIdestroy(activity, mustDetach);
}

static int jniGetUnicodeCharInner(JNIEnv* env, AInputEvent* event) {
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "jniGetUnicodeChar", "glfwJNIinit");
    return -42;
  }
  jclass jKeyEvent = safeFindClass(env, "android/view/KeyEvent");
  int met = AKeyEvent_getMetaState(event);
  jmethodID m = glfwJNIgetMethod(env, jKeyEvent, "getUnicodeChar",
                                 met ? "(I)I" : "()I");
  jobject o = objectCall(
      env, jKeyEvent, glfwJNIgetMethod(env, jKeyEvent, "<init>", "(II)V"),
      "new(KeyEvent)", AInputEvent_getType(event), AKeyEvent_getKeyCode(event));
  int r = met == 0 ? intCall(env, o, m, "gu") : intCall(env, o, m, "gu", met);
  // NOTE: o does not need to be freed. But this keeps # of local refs down.
  (*env)->DeleteLocalRef(env, o);
  (*env)->DeleteLocalRef(env, jKeyEvent);
  return r;
}

jint jniGetUnicodeChar(AInputEvent* event) {
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  jint r = jniGetUnicodeCharInner(env, event);
  glfwJNIdestroy(activity, mustDetach);
  return r;
}

typedef struct jInputDevice {
  jclass cls;
  jclass listCls;
  jclass rangeCls;
  jmethodID getDevice;
  jmethodID getDescriptor;
  jmethodID getName;
  jmethodID isVirtual;
  jmethodID getMotionRanges;
  jmethodID listSize;
  jmethodID listGet;
  jmethodID rangeId;
  jmethodID rangeMin;
  jmethodID rangeMax;
  jmethodID getFlat;  // Self-centering sticks provide a "centered" range.
  char name[EXCEPTION_STRLEN];
  char desc[EXCEPTION_STRLEN];
  int virtual;  // 0 or non-zero: result of calling isVirtual
  int axisCount;
  _GLFWjoystickAndroidRange* axis;
} jInputDevice;

static int jniGetDeviceDescriptor(JNIEnv* env, jInputDevice* dev, int id)
{
  jobject o = staticObjectCall(env, dev->cls, dev->getDevice, "getDevice", id);
  if (!o ||
      jniGetString(env, o, dev->desc, dev->getDescriptor) ||
      jniGetString(env, o, dev->name, dev->getName)) {
    return 1;
  }

  char exTypeStr[EXCEPTION_STRLEN];
  jboolean v = (*env)->CallBooleanMethod(env, o, dev->isVirtual);
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: isVirtual(%d) exception: %s",
                    "_glfwAndroidJNIpollDeviceIds", id, exTypeStr);
    return 1;
  }
  dev->virtual = v;

  // Call and unpack List<InputDevice.MotionRange> getMotionRanges()
  jobject list = objectCall(env, o, dev->getMotionRanges, "getMotionRanges");
  dev->axisCount = intCall(env, list, dev->listSize, "List.size");
  if (dev->axisCount < 0) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: getMotionsRanges(%d).size: %d",
                    "_glfwAndroidJNIpollDeviceIds", id, dev->axisCount);
    return 1;
  }
  dev->axis = malloc(sizeof(dev->axis[0]) * dev->axisCount);
  for (int i = 0; i < dev->axisCount; i++) {
    jobject r = objectCall(env, list, dev->listGet, "List.get", i);
    dev->axis[i].id = intCall(env, r, dev->rangeId, "getAxis");
    if (dev->axis[i].id == -42) {
      free(dev->axis);
      return 1;
    }
    float lo = floatCall(env, r, dev->rangeMin, "getMin");
    if (isnan(lo)) {
      free(dev->axis);
      return 1;
    }
    float hi = floatCall(env, r, dev->rangeMax, "getMax");
    if (isnan(hi)) {
      free(dev->axis);
      return 1;
    }
    dev->axis[i].flat = floatCall(env, r, dev->getFlat, "getFlat");
    if (isnan(dev->axis[i].flat)) {
      free(dev->axis);
      return 1;
    }
    dev->axis[i].base = lo;
    dev->axis[i].range = hi - lo;
    (*env)->DeleteLocalRef(env, r);
  }
  (*env)->DeleteLocalRef(env, list);
  (*env)->DeleteLocalRef(env, o);

  // Sort axis by axis[i].id:
  for (int i = 0; i < dev->axisCount - 1; i++) {
    int jMin = -1;
    int less = dev->axis[i].id;
    for (int j = i + 1; j < dev->axisCount; j++) {
      if (dev->axis[j].id < less) {
        less = dev->axis[j].id;
        jMin = j;
      }
    }
    if (jMin != -1) {
      // Swap axis[i] and axis[jMin]
      _GLFWjoystickAndroidRange a;
      memcpy(&a, &dev->axis[i], sizeof(a));
      memcpy(&dev->axis[i], &dev->axis[jMin], sizeof(dev->axis[i]));
      memcpy(&dev->axis[jMin], &a, sizeof(dev->axis[jMin]));
      break;
    }
  }
  return 0;
}

static int jniJoystickAdded(JNIEnv* env, jInputDevice* dev, int id, int index)
{
  if (dev->virtual) {
    return 0;  // Virtual devices should be ignored, Android docs suggest.
  }

  int jid = 0;
  int strSize = sizeof(_glfw.joysticks[jid].guid) - 1;
  for (; jid <= GLFW_JOYSTICK_LAST; jid++) {
    if (!_glfw.joysticks[jid].present) {
      continue;
    }
    if (!strncmp(_glfw.joysticks[jid].guid, dev->desc, strSize)) {
      break;  // Found duplicate
    }
  }
  if (jid <= GLFW_JOYSTICK_LAST) {
    // Joystick was found in a previous call to getDeviceIds.
    // Store device ID to match any AInputEvent* to this dev.
    _glfw.joysticks[jid].android.id = id;
    _glfw.joysticks[jid].android.mark = 1;
    return 0;
  }

  dev->desc[strSize] = 0;
  // FIXME: Android reports hats using AXIS_HAT_X, AXIS_HAT_Y. They could be
  // silently removed from the "axes" and presented as hats, but there is no
  // guarantee that the inputs mapped to AXIS_HAT_X by the controller will
  // be digital inputs or will actually be hats...
  int hatCount = 0;

  int buttonCount = AKEYCODE_REFRESH;  // Highest known keycode
  _GLFWjoystick* js = _glfwAllocJoystick(
      dev->name, dev->desc, dev->axisCount, buttonCount, hatCount);
  if (!js) {
    _glfwInputError(
        GLFW_PLATFORM_ERROR,
        "%s: id=%d name=\"%s\" desc=\"%s\": GLFW joysticks array is full",
        "_glfwAndroidJNIpollDeviceIds", id, dev->name, dev->desc);
    return 0;
  }
  // Store device ID to match any AInputEvent* to this dev.
  js->android.id = id;
  js->android.mark = 1;
  js->android.axis = dev->axis;
  dev->axis = NULL;

  // State is not updated until first AInputEvent*, but connect device now.
  _glfwInputJoystick(js, GLFW_CONNECTED);
  return 0;
}

static void JNIpollDeviceIdsInner(JNIEnv* env)
{
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwAndroidJNIpollDeviceIds", "glfwJNIinit");
    return;
  }

  jInputDevice device;
  device.cls = safeFindClass(env, "android/view/InputDevice");
  device.listCls = safeFindClass(env, "java/util/List");
  device.rangeCls = safeFindClass(env, "android/view/InputDevice$MotionRange");
  if (!device.cls || !device.listCls || !device.rangeCls) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwAndroidJNIpollDeviceIds", "getClass()");
    return;
  }
  device.getDevice = (*env)->GetStaticMethodID(
      env, device.cls, "getDevice", "(I)Landroid/view/InputDevice;");
  device.getDescriptor = glfwJNIgetMethod(
      env, device.cls, "getDescriptor", "()Ljava/lang/String;");
  device.getName = glfwJNIgetMethod(
      env, device.cls, "getName", "()Ljava/lang/String;");
  device.isVirtual = glfwJNIgetMethod(
      env, device.cls, "isVirtual", "()Z");
  // Declaration is: java.util.List<InputDevice.MotionRange> getMotionRanges
  device.getMotionRanges = glfwJNIgetMethod(
      env, device.cls, "getMotionRanges", "()Ljava/util/List;");
  device.listSize = glfwJNIgetMethod(env, device.listCls, "size", "()I");
  device.listGet = glfwJNIgetMethod(env, device.listCls, "get",
                                    "(I)Ljava/lang/Object;");
  device.rangeId = glfwJNIgetMethod(env, device.rangeCls, "getAxis", "()I");
  device.rangeMin = glfwJNIgetMethod(env, device.rangeCls, "getMin", "()F");
  device.rangeMax = glfwJNIgetMethod(env, device.rangeCls, "getMax", "()F");
  device.getFlat = glfwJNIgetMethod(env, device.rangeCls, "getFlat", "()F");
  if (!device.getDevice || !device.getDescriptor || !device.getName ||
      !device.isVirtual || !device.listSize || !device.listGet ||
      !device.rangeId || !device.rangeMin || !device.rangeMax ||
      !device.getFlat) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "GetMethodID(%s) returned NULL",
                    "getters for InputDevice");
    return;
  }
  jintArray r = (jintArray) staticObjectCall(
    env, device.cls,
    (*env)->GetStaticMethodID(env, device.cls, "getDeviceIds", "()[I"),
    "getDeviceIds");
  if (!r) {
    return;
  }
  int rlen = (*env)->GetArrayLength(env, r);
  jint* rdata = (*env)->GetIntArrayElements(env, r, 0);
  for (int jid = 0; jid <= GLFW_JOYSTICK_LAST; jid++) {
    _glfw.joysticks[jid].android.mark = 0;
  }
  for (int i = 0; i < rlen; i++) {
    if (jniGetDeviceDescriptor(env, &device, rdata[i])) {
      (*env)->ReleaseIntArrayElements(env, r, rdata, JNI_ABORT);
      return;
    }
    if (jniJoystickAdded(env, &device, rdata[i], i)) {
      free(device.axis);
      (*env)->ReleaseIntArrayElements(env, r, rdata, JNI_ABORT);
      return;
    }
  }
  (*env)->ReleaseIntArrayElements(env, r, rdata, JNI_ABORT);

  for (int jid = 0; jid <= GLFW_JOYSTICK_LAST; jid++) {
    if (!_glfw.joysticks[jid].present || _glfw.joysticks[jid].android.mark) {
      continue;
    }
    // getDeviceIds says this device is gone.
    free(_glfw.joysticks[jid].android.axis);
    _glfw.joysticks[jid].android.axis = NULL;
    _glfwFreeJoystick(&_glfw.joysticks[jid]);
    _glfwInputJoystick(&_glfw.joysticks[jid], GLFW_DISCONNECTED);
  }

  (*env)->DeleteLocalRef(env, r);
  (*env)->DeleteLocalRef(env, device.rangeCls);
  (*env)->DeleteLocalRef(env, device.listCls);
  (*env)->DeleteLocalRef(env, device.cls);
}

void _glfwAndroidJNIpollDeviceIds()
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  JNIpollDeviceIdsInner(env);
  glfwJNIdestroy(activity, mustDetach);
}

static jobject jniClipboardManager(JNIEnv* env)
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  jclass jActivity = (*env)->GetObjectClass(env, activity->clazz);
  jmethodID getSystemService = glfwJNIgetMethod(
      env, jActivity, "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");
  // Context.CLIPBOARD_SERVICE (class Activity inherits from Context)
  jstring CLIPBOARD_SERVICE = (*env)->NewStringUTF(env, "clipboard");
  jobject r = (*env)->CallObjectMethod(env, activity->clazz, getSystemService,
      CLIPBOARD_SERVICE);
  (*env)->DeleteLocalRef(env, CLIPBOARD_SERVICE);
  (*env)->DeleteLocalRef(env, jActivity);
  return r;
}

void _glfwAndroidJNIinitClipboardManager()
{
  // Just by calling jniClipboardManager() from the main thread, Android
  // caches an instance which then can be retrieved from any thread.
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  (void)jniClipboardManager(env);
  glfwJNIdestroy(activity, mustDetach);
}

static void jniSetClipboardStringInner(JNIEnv* env, const char* text)
{
  jobject clipboardMgr = jniClipboardManager(env);
  if (!clipboardMgr) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformSetClipboardString", "jniClipboardManager");
    return;
  }
  jclass jCM = (*env)->GetObjectClass(env, clipboardMgr);
  // setText() was deprecated in API 11 but still exists and does exactly what
  // is needed: create a ClippedItem holding text, set it as the primary clip.
  // It has no label or icon.
  jmethodID setText = glfwJNIgetMethod(
      env, jCM, "setText", "(Ljava/lang/CharSequence;)V");
  if (!setText) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getMethod(%s) failed", "setText");
    return;
  }

  // java.lang.String inherits from java.lang.CharSequence. Create a String:
  jobject charSeq = (*env)->NewStringUTF(env, text);
  if (voidCall(env, clipboardMgr, setText, "setText", charSeq)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformSetClipboardString", "setText");
    return;
  }
  (*env)->DeleteLocalRef(env, charSeq);
  (*env)->DeleteLocalRef(env, jCM);
  (*env)->DeleteLocalRef(env, clipboardMgr);
}

void _glfwPlatformSetClipboardString(const char* text)
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  jniSetClipboardStringInner(env, text);
  glfwJNIdestroy(activity, mustDetach);
}

static const char* jniGetClipboardTextPlain(
    JNIEnv* env, jclass jCM, jobject clipboardMgr)
{
  jmethodID getPrimaryClip = glfwJNIgetMethod(
      env, jCM, "getPrimaryClip", "()Landroid/content/ClipData;");
  jobject d = objectCall(env, clipboardMgr, getPrimaryClip, "getPrimaryClip");
  if (!d) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString", "getPrimaryClip");
    return NULL;
  }
  jclass jClipData = (*env)->GetObjectClass(env, d);
  jmethodID getItemAt = glfwJNIgetMethod(
      env, jClipData, "getItemAt", "(I)Landroid/content/ClipData$Item;");
  jobject item = objectCall(env, d, getItemAt, "getItemAt", 0);
  if (!item) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString", "getItemAt");
    return NULL;
  }
  jclass jClipData_Item = (*env)->GetObjectClass(env, item);
  jmethodID getText = glfwJNIgetMethod(
      env, jClipData_Item, "getText", "()Ljava/lang/CharSequence;");
  jobject charSeq = objectCall(env, item, getText, "getText");
  if (!charSeq) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString", "getText");
    return NULL;
  }
  jclass jCharSeq = (*env)->GetObjectClass(env, charSeq);
  jmethodID toString = glfwJNIgetMethod(
      env, jCharSeq, "toString", "()Ljava/lang/String;");
  if (jniGetString(env, charSeq, _glfw.android.clipboardData, toString)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString", "toString");
    return NULL;
  }
  (*env)->DeleteLocalRef(env, jCharSeq);
  (*env)->DeleteLocalRef(env, charSeq);
  (*env)->DeleteLocalRef(env, jClipData_Item);
  (*env)->DeleteLocalRef(env, item);
  (*env)->DeleteLocalRef(env, jClipData);
  (*env)->DeleteLocalRef(env, d);
  return &_glfw.android.clipboardData[0];
}

static const char* jniGetClipboardStringInner(JNIEnv* env)
{
  jobject clipboardMgr = jniClipboardManager(env);
  if (!clipboardMgr) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString", "jniClipboardManager");
    return NULL;
  }
  jclass jCM = (*env)->GetObjectClass(env, clipboardMgr);
  jmethodID getPrimaryClipDescription = glfwJNIgetMethod(
      env, jCM, "getPrimaryClipDescription",
      "()Landroid/content/ClipDescription;");
  jobject desc = objectCallQuiet(env, clipboardMgr, getPrimaryClipDescription,
                                 "getPrimaryClipDescription");
  if (!desc) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "_glfwPlatformGetClipboardString",
                    "getPrimaryClipDescription");
    return NULL;  // Clipboard is empty.
  }
  // Context.MIMETYPE_TEXT_PLAIN
  jstring MIMETYPE_TEXT_PLAIN = (*env)->NewStringUTF(env, "text/plain");
  jclass jCD = (*env)->GetObjectClass(env, desc);
  jmethodID hasMimeType = glfwJNIgetMethod(
      env, jCD, "hasMimeType", "(Ljava/lang/String;)Z");
  jboolean hasText = (*env)->CallBooleanMethod(env, desc, hasMimeType,
                                               MIMETYPE_TEXT_PLAIN);
  char exTypeStr[EXCEPTION_STRLEN];
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: hasMimeType exception: %s",
                    "_glfwPlatformGetClipboardString", exTypeStr);
    return NULL;
  }
  (*env)->DeleteLocalRef(env, MIMETYPE_TEXT_PLAIN);
  (*env)->DeleteLocalRef(env, desc);
  if (!hasText) {
    (*env)->DeleteLocalRef(env, jCM);
    (*env)->DeleteLocalRef(env, clipboardMgr);
    return "";  // No text/plain content in clipboard or clipboard empty.
  }

  const char* r = jniGetClipboardTextPlain(env, jCM, clipboardMgr);
  (*env)->DeleteLocalRef(env, jCM);
  (*env)->DeleteLocalRef(env, clipboardMgr);
  return r;
}

const char* _glfwPlatformGetClipboardString(void)
{
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  const char* r = jniGetClipboardStringInner(env);
  glfwJNIdestroy(activity, mustDetach);
  return r;
}

static int jniShowSoftInput(ANativeActivity* activity, JNIEnv* env, int show) {
  if (!env) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed",
                    "glfwShowSoftInput", "glfwJNIinit");
    return 1;
  }

  jclass jNativeActivity = (*env)->GetObjectClass(env, activity->clazz);
  jclass jContext = safeFindClass(env, "android/content/Context");
  if (!jNativeActivity || !jContext) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "getClass(%s) failed",
                    "android.context.Context");
    char exTypeStr[EXCEPTION_STRLEN];
    if (jniCheckException(env, exTypeStr)) {
      _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s",
                      "getClass()", exTypeStr);
    }
    return 1;
  }
  jclass jWindow = safeFindClass(env, "android/view/Window");
  jclass jView = safeFindClass(env, "android/view/View");
  jclass jIMMgr = safeFindClass(
      env, "android/view/inputmethod/InputMethodManager");
  jmethodID getSystemService = glfwJNIgetMethod(
      env, jNativeActivity, "getSystemService",
      "(Ljava/lang/String;)Ljava/lang/Object;");
  jobject jIMETH = (*env)->GetStaticObjectField(
      env, jContext,
      (*env)->GetStaticFieldID(env, jContext, "INPUT_METHOD_SERVICE",
                               "Ljava/lang/String;"));
  if (!jWindow || !jView || !jIMMgr || !getSystemService || !jIMETH) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed", "glfwShowSoftInput",
                    "android.view.{Window|View} or InputMethodManager or "
                    "INPUT_METHOD_SERVICE");
    return 1;
  }

  jmethodID showInput = glfwJNIgetMethod(env, jIMMgr, "showSoftInput",
                                         "(Landroid/view/View;I)Z");
  jmethodID hideInput = glfwJNIgetMethod(env, jIMMgr, "hideSoftInputFromWindow",
                                         "(Landroid/os/IBinder;I)Z");
  jmethodID getWindowToken = glfwJNIgetMethod(env, jView, "getWindowToken",
                                              "()Landroid/os/IBinder;");
  jobject inputMethod = objectCall(env, activity->clazz, getSystemService,
                                   "getSystemService", jIMETH);
  jmethodID getWindow = glfwJNIgetMethod(
      env, (*env)->GetObjectClass(env, activity->clazz), "getWindow",
      "()Landroid/view/Window;");
  jobject window = objectCall(env, activity->clazz, getWindow, "getWindow");
  jmethodID getDecorView = glfwJNIgetMethod(env, jWindow, "getDecorView",
                                            "()Landroid/view/View;");
  if (!showInput || !hideInput || !getWindowToken || !inputMethod ||
      !getWindow || !getDecorView) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed", "glfwShowSoftInput",
         "{show|hide} or get{Window|WindowToken|SystemService|DecorView}");
    return 1;
  }

  jobject decorView = objectCall(env, window, getDecorView, "getDecorView");
  if (!decorView) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed", "glfwShowSoftInput",
                    "getDecorView");
    return 1;
  }
  if (show) {
    (*env)->CallBooleanMethod(env, inputMethod, showInput, decorView,
                              0 /*flags*/);
  } else {
    jobject token = objectCall(env, decorView, getWindowToken,
                               "getWindowToken");
    if (!token) {
      _glfwInputError(GLFW_PLATFORM_ERROR, "%s: %s failed", "glfwShowSoftInput",
                      "getWindowToken");
      return 1;
    }
    (*env)->CallBooleanMethod(env, inputMethod, hideInput, token, 0 /*flags*/);
  }
  char exTypeStr[EXCEPTION_STRLEN];
  if (jniCheckException(env, exTypeStr)) {
    _glfwInputError(GLFW_PLATFORM_ERROR, "%s exception: %s",
                    "{show|hide}SoftInput", exTypeStr);
    return 1;
  }
  _glfw.android.softInputDisplayed = show;
  return 0;
}

int glfwShowSoftInput(int show) {
  ANativeActivity* activity = _glfwPlatformANativeActivity();
  int mustDetach;
  JNIEnv* env = glfwJNIinit(activity, &mustDetach);
  int r = jniShowSoftInput(activity, env, show);
  glfwJNIdestroy(activity, mustDetach);
  return r;
}

void _glfwPlatformRequestWindowAttention(_GLFWwindow* w)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "TODO: Android notification or toast");
}
