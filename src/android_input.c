//========================================================================
// GLFW 3.3 - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2017-2018 Volcano Authors <volcano.authors@gmail.com>
//
// This file contains _glfwPlatformAndroidHandleEvent() and related functions.

#include "internal.h"

#include <android_native_app_glue.h>
#include <android/log.h>
#include <math.h>
#include <string.h>

static const char logTag[] = "volcano";
void glflog(char level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  android_LogPriority prio = ANDROID_LOG_UNKNOWN;
  switch (level) {
    case 'V':
      prio = ANDROID_LOG_VERBOSE;
      break;
    case 'D':
      prio = ANDROID_LOG_DEBUG;
      break;
    case 'I':
      prio = ANDROID_LOG_INFO;
      break;
    case 'W':
      prio = ANDROID_LOG_WARN;
      break;
    case 'E':
      prio = ANDROID_LOG_ERROR;
      break;
    case 'F':
      prio = ANDROID_LOG_FATAL;
      break;
    default:
      break;
  }
  __android_log_vprint(prio, logTag, fmt, ap);
  va_end(ap);
  if (prio == ANDROID_LOG_FATAL) {
    __android_log_assert("call to logF()", logTag, "printing backtrace:");
  }
}

void _glfwPlatformUpdateGamepadGUID(char* guid) {}

int _glfwPlatformPollJoystick(_GLFWjoystick* js, int mode) {
  return js->present;
}

static int metaStateToMods(int32_t metaState) {
  int mods = 0;
  if (metaState & (AMETA_ALT_ON | AMETA_ALT_LEFT_ON | AMETA_ALT_RIGHT_ON)) {
    mods |= GLFW_MOD_ALT;
  }
  if (metaState &
      (AMETA_SHIFT_ON | AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_RIGHT_ON)) {
    mods |= GLFW_MOD_SHIFT;
  }
  if (metaState & (AMETA_CTRL_ON | AMETA_CTRL_LEFT_ON | AMETA_CTRL_RIGHT_ON)) {
    mods |= GLFW_MOD_CONTROL;
  }
  if (metaState & (AMETA_META_ON | AMETA_META_LEFT_ON | AMETA_META_RIGHT_ON)) {
    mods |= GLFW_MOD_SUPER;
  }
  if (metaState & AMETA_CAPS_LOCK_ON) {
    mods |= GLFW_MOD_CAPS_LOCK;
  }
  if (metaState & AMETA_NUM_LOCK_ON) {
    mods |= GLFW_MOD_NUM_LOCK;
  }
  if (metaState & AMETA_SCROLL_LOCK_ON) {
    // TODO: Scroll Lock
  }
  if (metaState & AMETA_SYM_ON) {
    // TODO: Sym modifier
  }
  if (metaState & AMETA_FUNCTION_ON) {
    // TODO: Fn key modifier
  }
  return mods;
}

static void handleJoystickButtonInput(_GLFWjoystick* js, int jid, int key,
                                      int mods, int action) {
  if (!_glfw.android.oneAndOnlyWindow) {
    return;
  }
  // This updates js->buttons[].
  _glfwInputJoystickButton(js, key, action);

  // This notifies the app of the change.
  GLFWinputEvent ie;
  memset(&ie, 0, sizeof(ie));
  ie.inputDevice = GLFW_INPUT_JOYSTICK;
  ie.num = jid;
  ie.action = action;
  _glfwInputMulitouchEvents(_glfw.android.oneAndOnlyWindow,
                            &ie, 1, mods);
}

// Reference:
// https://developer.android.com/training/game-controllers/controller-input
static void handleJoystickButton(struct android_app* app, AInputEvent* aevent,
                                 _GLFWjoystick* js, int jid) {
  int mods = metaStateToMods(AKeyEvent_getMetaState(aevent));
  int key = AKeyEvent_getKeyCode(aevent);
  int reps = AKeyEvent_getRepeatCount(aevent);  // Has two meanings.
  int32_t action = AKeyEvent_getAction(aevent);
  switch (action) {
  case AKEY_EVENT_ACTION_DOWN:
    // reps > 0 for DOWN means this is a key repeat event.
    handleJoystickButtonInput(js, jid, key, mods,
                              reps == 0 ? GLFW_PRESS : GLFW_REPEAT);
    break;
  case AKEY_EVENT_ACTION_UP:
    // reps for UP is poorly documented, but can be ignored.
    handleJoystickButtonInput(js, jid, key, mods, GLFW_RELEASE);
    break;
  case AKEY_EVENT_ACTION_MULTIPLE:
    for (int i = 0; i < reps; i++) {  // reps is number of DOWN/UP pairs.
      handleJoystickButtonInput(js, jid, key, mods, GLFW_PRESS);
      handleJoystickButtonInput(js, jid, key, mods, GLFW_RELEASE);
    }
    break;
  }
}

static int32_t handleJoystick(struct android_app* app, AInputEvent* aevent,
                              int eventType, _GLFWjoystick* js, int jid) {
  if (_glfw.android.softInputDisplayed) {
    // Some devices send joystick input both to soft keyboard and app.
    return 0;  // Leave event for soft input - report it was NOT handled.
  }
  if (eventType == AINPUT_EVENT_TYPE_KEY) {
    handleJoystickButton(app, aevent, js, jid);
    return 1;  // event was handled
  }

  // Handle eventType == AINPUT_EVENT_TYPE_MOTION:
  int mods = metaStateToMods(AMotionEvent_getMetaState(aevent));
  unsigned eventCount = AMotionEvent_getPointerCount(aevent);
  int32_t action = AMotionEvent_getAction(aevent);

  // Log event action / eventCount that are unexpected. Why did this happen?
  switch (action & AMOTION_EVENT_ACTION_MASK) {
  case AMOTION_EVENT_ACTION_MOVE:
    if (eventCount != 1) {
      glflog('E', "type:motion joy move eventCount=%u", eventCount);
      return 1;  // event was handled
    }
    break;

  case AMOTION_EVENT_ACTION_DOWN:
  case AMOTION_EVENT_ACTION_BUTTON_PRESS:
    glflog('E', "type:motion joy button press");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_CANCEL:  // Adds that gesture should be ignored.
    glflog('E', "type:motion joy button cancel");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_UP:
    glflog('E', "type:motion joy button up");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
    glflog('E', "type:motion joy button release");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_OUTSIDE:
    glflog('E', "type:motion joy outside");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_SCROLL:
    glflog('E', "type:motion joy scroll");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_POINTER_DOWN:
    glflog('E', "type:motion joy pointer down");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_POINTER_UP:
    glflog('E', "type:motion joy pointer up");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_HOVER_ENTER:
    glflog('E', "type:motion joy hover enter");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_HOVER_MOVE:
    glflog('E', "type:motion joy hover move");
    return 1;  // event was handled

  case AMOTION_EVENT_ACTION_HOVER_EXIT:
    glflog('E', "type:motion joy hover exit");
    return 1;  // event was handled
  }

  for (unsigned i = 0; i < eventCount; i++) {
    for (unsigned ai = 0; ai < js->axisCount; ai++) {
      _GLFWjoystickAndroidRange* range = &js->android.axis[ai];
      // range->id is output of MotionRange.getAxis(), e.g. AXIS_X or AXIS_Y.
      float raw = AMotionEvent_getAxisValue(aevent, range->id, i);
      if (raw > -range->flat && raw < range->flat) {
        // FIXME: only force 0.f after a small delay of continuously "flat."
        raw = 0.f;  // No other way to tell app "raw value is centered."
      } else {
        raw -= range->base;  // From (base, base+range) to (0, range)
        raw /= range->range; // From (0, range) to (0, 1)
        raw *= 2.f;          // From (0, 1)     to (0, 2)
        raw -= 1.f;          // From (0, 2)     to (-1, 1) required by GLFW
      }
      _glfwInputJoystickAxis(js, ai, raw);
    }
  }

  if (_glfw.android.oneAndOnlyWindow) {
    GLFWinputEvent ie;
    memset(&ie, 0, sizeof(ie));
    ie.inputDevice = GLFW_INPUT_JOYSTICK;
    ie.num = jid;
    ie.action = GLFW_CURSORPOS;
    _glfwInputMulitouchEvents(_glfw.android.oneAndOnlyWindow,
                              &ie, 1, mods);
  }
  return 1;  // event was handled
}

static int toolTypeToInputDevice(int32_t toolType) {
  switch (toolType) {
  case AMOTION_EVENT_TOOL_TYPE_UNKNOWN:
    break;
  case AMOTION_EVENT_TOOL_TYPE_FINGER:
    return GLFW_INPUT_FINGER;
  case AMOTION_EVENT_TOOL_TYPE_STYLUS:
    return GLFW_INPUT_STYLUS;
  case AMOTION_EVENT_TOOL_TYPE_MOUSE:
    return GLFW_INPUT_FIXED;
  case AMOTION_EVENT_TOOL_TYPE_ERASER:
    return GLFW_INPUT_ERASER;
  }
  return GLFW_INPUT_UNDEFINED;
}

static int toolTypeToButtons(int32_t toolType, AInputEvent* aevent) {
  int32_t b = AMotionEvent_getButtonState(aevent);
  switch (toolType) {
  case AMOTION_EVENT_TOOL_TYPE_UNKNOWN:
    break;

  case AMOTION_EVENT_TOOL_TYPE_FINGER:
    // Report button 0 pressed. (Android does not report a button at all).
    // Setting ie->buttons = 1 this way works for all but a GLFW_RELEASE event.
    return 1;

  case AMOTION_EVENT_TOOL_TYPE_MOUSE:
    return ((b & AMOTION_EVENT_BUTTON_PRIMARY) ? 1 : 0) |
           ((b & AMOTION_EVENT_BUTTON_SECONDARY) ? 2 : 0) |
           ((b & AMOTION_EVENT_BUTTON_TERTIARY) ? 4 : 0);

  case AMOTION_EVENT_TOOL_TYPE_ERASER:
    return 1;
  }
  return 0;
}

static void handleMotionEvent(struct android_app* app, AInputEvent* aevent,
                              int source) {
  int notifyOfEnter = 0;
  int enterState = 0;
  int moreThanFingers = 0;
  int anyButtonsAtAll = 0;

  int mods = metaStateToMods(AMotionEvent_getMetaState(aevent));
  int32_t action = AMotionEvent_getAction(aevent);
  int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
      AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
  double w = _glfw.android.width;
  double h = _glfw.android.height;
  double perW = 1.f / w, perH = 1.f / h;
  w /= _glfw.android.density;
  h /= _glfw.android.density;

  unsigned eventCount = AMotionEvent_getPointerCount(aevent);
  if (eventCount > _GLFW_PLATFORM_MAX_EVENTS) {
    eventCount = _GLFW_PLATFORM_MAX_EVENTS;
  }

  unsigned dst = 0;
  for (unsigned i = 0; i < eventCount; i++) {
    GLFWinputEvent* ie = &_glfw.android.next[dst];

    float o = AMotionEvent_getOrientation(aevent, i);
    double sino = sin(o), coso = cos(o);
    double x = AMotionEvent_getX(aevent, i) * perW - 0.5;
    double y = AMotionEvent_getY(aevent, i) * perH - 0.5;
    ie->x = (coso * x + sino * y + 0.5) * w;
    ie->y = (coso * y - sino * x + 0.5) * h;
    ie->xoffset = 0;
    ie->yoffset = 0;
    ie->dx = 0;
    ie->dy = 0;

    int32_t toolType = AMotionEvent_getToolType(aevent, i);
    ie->buttons = toolTypeToButtons(toolType, aevent);
    ie->inputDevice = toolTypeToInputDevice(toolType);
    ie->hover = 0;
    ie->num = i;
    if (ie->inputDevice != GLFW_INPUT_FINGER) {
      moreThanFingers = 1;
    }

    switch (action & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_BUTTON_PRESS:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      ie->action = GLFW_PRESS;
      ie->actionButton = 1;  // These events do not have a pointerIndex.
      break;

    case AMOTION_EVENT_ACTION_CANCEL:  // Adds that gesture should be ignored.
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_BUTTON_RELEASE:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      ie->buttons = 0;
      ie->action = GLFW_RELEASE;
      ie->actionButton = 1;  // These events do not have a pointerIndex.
      break;

    case AMOTION_EVENT_ACTION_MOVE:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      // TODO: history of points since last MOVE: AMotionEvent_getHistorySize
      ie->action = GLFW_CURSORPOS;
      ie->actionButton = 0;  // These events do not have a pointerIndex.
      break;

    case AMOTION_EVENT_ACTION_OUTSIDE:
      if (source != AINPUT_SOURCE_MOUSE) {
        continue;
      }
      ie->action = GLFW_CURSORPOS;
      ie->actionButton = 0;  // These events do not have a pointerIndex.
      break;

    case AMOTION_EVENT_ACTION_SCROLL:
      if (source != AINPUT_SOURCE_MOUSE) {
        continue;
      }
      ie->xoffset = AMotionEvent_getAxisValue(aevent, AMOTION_EVENT_AXIS_HSCROLL, i);
      ie->yoffset = AMotionEvent_getAxisValue(aevent, AMOTION_EVENT_AXIS_VSCROLL, i);
      if (ie->xoffset != 0 || ie->yoffset != 0) {
        ie->action = GLFW_SCROLL;
      } else {
        ie->action = GLFW_CURSORPOS;
      }
      break;

    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      if (pointerIndex == i) {
        ie->action = GLFW_PRESS;
        ie->actionButton = 1;
      } else {
        ie->action = GLFW_CURSORPOS;
        ie->actionButton = 0;
      }
      break;

    case AMOTION_EVENT_ACTION_POINTER_UP:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      if (pointerIndex == i) {
        ie->action = GLFW_RELEASE;
        ie->actionButton = 1;

        // toolTypeToButtons always sets ie->buttons = 1 for fingers. That's
        // correct for all other cases (if the finger wasn't touching, Android
        // wouldn't report it, so that implies buttons = 1). But this one case
        // Android is reporting the finger was released.
        ie->buttons = 0;
      } else {
        ie->action = GLFW_CURSORPOS;
        ie->actionButton = 0;
      }
      break;

    case AMOTION_EVENT_ACTION_HOVER_ENTER:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      ie->action = GLFW_CURSORPOS;
      ie->actionButton = 0;  // These events do not have a pointerIndex.
      ie->hover = 1;
      notifyOfEnter = 1;
      enterState = 1;
      break;

    case AMOTION_EVENT_ACTION_HOVER_MOVE:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      ie->action = GLFW_CURSORPOS;
      ie->actionButton = 1;  // These events do not have a pointerIndex.
      ie->hover = 1;
      break;

    case AMOTION_EVENT_ACTION_HOVER_EXIT:
      if ((source & AINPUT_SOURCE_CLASS_MASK) != AINPUT_SOURCE_CLASS_POINTER) {
        continue;
      }
      ie->action = GLFW_CURSORPOS;
      ie->actionButton = 1;  // These events do not have a pointerIndex.
      ie->hover = 1;
      notifyOfEnter = 1;
      enterState = 0;
      break;

    default:
      glflog('E', "motion event source:%d action:%d pointer:%d ignored\n",
             source, action & AMOTION_EVENT_ACTION_MASK, pointerIndex);
      return;
    }
    if (ie->buttons) {
      anyButtonsAtAll = 1;
    }

    // Android can reorder "pointer ids" once a pointer goes up "since the
    // start of the current gesture" (see docs for AMotionEvent_getPointerId).
    if (i < _glfw.android.numPrev && _glfw.android.prev[i].inputDevice == ie->inputDevice) {
      if (ie->inputDevice != GLFW_INPUT_FINGER || ie->buttons != 0) {
        ie->dx = ie->x - _glfw.android.prev[i].x;
        ie->dy = ie->y - _glfw.android.prev[i].y;
      }
    }

    // Increment dst only if no 'continue' statement occurred.
    dst++;
  }
  _glfw.android.numPrev = dst;
  memcpy(_glfw.android.prev, _glfw.android.next, sizeof(_glfw.android.prev[0]) * dst);

  if (!moreThanFingers && !notifyOfEnter && anyButtonsAtAll) {
    // Only finger type inputs. Simulate enter (may fire multiple times).
    _glfwInputCursorEnter(_glfw.android.oneAndOnlyWindow, 1);
  } else if (notifyOfEnter) {
    // Only send the last enter/leave event.
    _glfwInputCursorEnter(_glfw.android.oneAndOnlyWindow, enterState);
  }
  _glfwInputMulitouchEvents(_glfw.android.oneAndOnlyWindow,
                            _glfw.android.next, dst, mods);
}

int32_t _glfwPlatformAndroidHandleEvent(struct android_app* app,
                                        AInputEvent* aevent)
{
  int source = AInputEvent_getSource(aevent);
  int eventType = AInputEvent_getType(aevent);
  int isJoystick = 0;
  switch (source & AINPUT_SOURCE_CLASS_MASK) {
  case AINPUT_SOURCE_CLASS_BUTTON:
    if (source != AINPUT_SOURCE_KEYBOARD) {
      isJoystick = 1;
      break;
    }
    // fall through
  case AINPUT_SOURCE_CLASS_NONE:
    isJoystick = (eventType != AINPUT_EVENT_TYPE_KEY) ? 1 : 0;
    break;
  case AINPUT_SOURCE_CLASS_POINTER:
    if (source == AINPUT_SOURCE_MOUSE || source == AINPUT_SOURCE_STYLUS) {
      isJoystick = (eventType != AINPUT_EVENT_TYPE_MOTION) ? 1 : 0;
    } else {
      isJoystick = 0;
    }
    break;
  case AINPUT_SOURCE_CLASS_NAVIGATION:
  case AINPUT_SOURCE_CLASS_POSITION:
    isJoystick = 0;
    break;
  default: // for AINPUT_SOURCE_CLASS_JOYSTICK and future-proofing:
    isJoystick = 1;
    break;
  }
  if (isJoystick) {
    int devId = AInputEvent_getDeviceId(aevent);
    int64_t nanos = (eventType == AINPUT_EVENT_TYPE_KEY ?
                     AKeyEvent_getEventTime(aevent) :
                     AMotionEvent_getEventTime(aevent));
    if (nanos - _glfw.android.lastJoystickNanos >= 20000000ll /*20ms*/) {
      // Wait at least 20ms. But polling only happens when there is input.
      _glfw.android.lastJoystickNanos = nanos;
      _glfwAndroidJNIpollDeviceIds();
    }
    for (int jid = 0; jid <= GLFW_JOYSTICK_LAST; jid++) {
      if (!_glfw.joysticks[jid].present ||
          _glfw.joysticks[jid].android.id != devId) {
        continue;
      }
      return handleJoystick(app, aevent, eventType, &_glfw.joysticks[jid],
                            jid);
    }
    _glfwInputError(GLFW_PLATFORM_ERROR, "Joystick event: device %d not found",
                    devId);
    return 1;
  }

  switch (eventType) {
    case AINPUT_EVENT_TYPE_MOTION:
      if (_glfw.android.oneAndOnlyWindow) {
        handleMotionEvent(app, aevent, source);
      }
      return 1;  // input event was handled
    case AINPUT_EVENT_TYPE_KEY:
      if (_glfw.android.oneAndOnlyWindow) {
        handleKeyEvent(app, aevent,
                       metaStateToMods(AMotionEvent_getMetaState(aevent)));
      }
      return 1;  // input event was handled
  }
  glflog('E', "input eventType:%d source:%d ignored\n", eventType, source);
  return 0;  // input event was not handled.
}

void _glfwPlatformGetCursorPos(_GLFWwindow* w, double* xpos, double* ypos)
{
    double x = 0, y = 0;
    if (_glfw.android.numPrev > 0) {
        x = _glfw.android.prev[0].x;
        y = _glfw.android.prev[0].y;
    }
    if (xpos) *xpos = x;
    if (ypos) *ypos = y;
}
