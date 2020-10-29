#ifndef ANDROID_OS_STATISTICS_JAVAVMSUPERVISION_CALLBACKS_H_
#define ANDROID_OS_STATISTICS_JAVAVMSUPERVISION_CALLBACKS_H_

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>
#include <string>

#include "jni.h"

namespace android {
namespace os {
namespace statistics {

struct JavaVMInterface {
  void (*setThreadPerfSupervisionOn)(JNIEnv* env, bool isOn);
  bool (*isThreadPerfSupervisionOn)(JNIEnv* env);
  void (*getThreadInfo)(JNIEnv* env, int32_t& thread_id, std::shared_ptr<std::string>& thread_name);
  jclass (*getCurrentClass)(JNIEnv* env);
  jobject (*createJavaStackBackTrace)(JNIEnv* env, bool needFillIn);
  void (*fillInJavaStackBackTrace)(JNIEnv* env, jobject stackBackTrace);
  void (*resetJavaStackBackTrace)(JNIEnv* env, jobject stackBackTrace);
  jobject (*cloneJavaStackBackTrace)(JNIEnv* env, jobject stackBackTrace);
  jobjectArray (*resolveJavaStackTrace)(JNIEnv* env, jobject stackBackTrace);
  jobjectArray (*resolveClassesOfBackTrace)(JNIEnv* env, jobject stackBackTrace);
};

#define CONDITION_AWAKEN_NOTIFIED 0
#define COMDITION_AWAKEN_TIMEDOUT 1
#define COMDITION_AWAKEN_INTERRUPTED 2

struct JavaVMSupervisionCallBacks {
  void (*exportJavaVMInterface)(struct JavaVMInterface *interface);
  bool (*isPerfSupervisionOn)();
  int64_t (*getUptimeMillisFast)();
  void (*reportLockReleased)(JNIEnv* env, int64_t monitorId, int64_t beginUptimeMs, int64_t endUptimeMs);
  void (*reportLockAccquired)(JNIEnv* env, int64_t monitorId, int64_t beginUptimeMs, int64_t endUptimeMs);
  void (*reportConditionAwaken)(JNIEnv* env, int64_t monitorId, int32_t peerThreadId, int64_t beginUptimeMs, int64_t endUptimeMs);
  void (*reportConditionSatisfied)(JNIEnv* env, int64_t monitorId, int32_t awakenReason, int64_t beginUptimeMs, int64_t endUptimeMs);
  void (*reportJniMethodInvocation)(JNIEnv* env, int64_t beginUptimeMs, int64_t endUptimeMs, int32_t& reportedTimeMillis);
};

} //namespace statistics
} //namespace os
} //namespace android

#endif  // ANDROID_OS_STATISTICS_JAVAVMSUPERVISION_CALLBACKS_H_
