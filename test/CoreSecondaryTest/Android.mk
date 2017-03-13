# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := core-secondary-test
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := CoreSecondaryTest.java
LOCAL_JAVA_LIBRARIES := core-oj core-libart
LOCAL_NO_STANDARD_LIBRARIES := true
include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := core-secondary-test-alt
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := CoreSecondaryTestAlt.java
LOCAL_JAVA_LIBRARIES := core-oj core-libart
LOCAL_NO_STANDARD_LIBRARIES := true
include $(BUILD_JAVA_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := core-secondary-test-hostdex
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := CoreSecondaryTest.java
include $(BUILD_HOST_DALVIK_JAVA_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := core-secondary-test-alt-hostdex
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := CoreSecondaryTestAlt.java
include $(BUILD_HOST_DALVIK_JAVA_LIBRARY)

