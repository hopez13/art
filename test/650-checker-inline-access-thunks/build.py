# Copyright (C) 2023 The Android Open Source Project
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

# Perform a mostly normal build.
# This test depends on the call to Main$Nested.$noinline$setPrivateIntField to
# be InvokeStaticOrDirect, but the D8 flag
# com.android.tools.r8.emitNestAnnotationsInDex would make the call to the
# public method in the nested class to be InvokeVirtual. We want to test the
# old behavior, so make sure that D8 does not use the new flag.


# Use release mode to check optimizations do not break JNI.
def build(ctx):
  ctx.default_build(d8_flags=[])
