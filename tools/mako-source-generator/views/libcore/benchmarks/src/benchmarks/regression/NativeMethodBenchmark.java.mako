## -*- coding: utf-8 -*-
##
## Copyright (C) 2016 The Android Open Source Project
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##       http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##
/*
 * Copyright (C) 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * AUTOMATICALLY GENERATED FROM art/tools/mako-source-generator/...../NativeMethodBenchmark.java.mako
 *
 * !!! DO NOT EDIT DIRECTLY !!!
 *
 */
package benchmarks.regression;

import org.apache.harmony.dalvik.NativeTestTarget;

public class NativeMethodBenchmark {
    public void time_emptyJniStaticSynchronizedMethod0(int reps) throws Exception {
        for (int i = 0; i < reps; ++i) {
            NativeTestTarget.emptyJniStaticSynchronizedMethod0();
        }
    }

    public void time_emptyJniSynchronizedMethod0(int reps) throws Exception {
        NativeTestTarget n = new NativeTestTarget();
        for (int i = 0; i < reps; ++i) {
            n.emptyJniSynchronizedMethod0();
        }
    }

% for annotation, jni_suffix in [("// Normal native", ""), ("@FastNative", "_Fast"), ("@CriticalNative", "_Critical")]:
<%
# @CriticalNative doesn't support passing objects as parameters and doesn't support
# virtual or synchronized methods. So don't generate tests for those.
test_refs = jni_suffix != "_Critical"
%>
% if test_refs:
    public void time_emptyJniMethod0${jni_suffix}(int reps) throws Exception {
        NativeTestTarget n = new NativeTestTarget();
        for (int i = 0; i < reps; ++i) {
            n.emptyJniMethod0${jni_suffix}();
        }
    }

    public void time_emptyJniMethod6${jni_suffix}(int reps) throws Exception {
        int a = -1;
        int b = 0;
        NativeTestTarget n = new NativeTestTarget();
        for (int i = 0; i < reps; ++i) {
            n.emptyJniMethod6${jni_suffix}(a, b, 1, 2, 3, i);
        }
    }

    public void time_emptyJniMethod6L${jni_suffix}(int reps) throws Exception {
        NativeTestTarget n = new NativeTestTarget();
        for (int i = 0; i < reps; ++i) {
            n.emptyJniMethod6L${jni_suffix}(null, null, null, null, null, null);
        }
    }

    public void time_emptyJniStaticMethod6L${jni_suffix}(int reps) throws Exception {
        for (int i = 0; i < reps; ++i) {
            NativeTestTarget.emptyJniStaticMethod6L${jni_suffix}(null, null, null, null, null, null);
        }
    }
% endif
    public void time_emptyJniStaticMethod0${jni_suffix}(int reps) throws Exception {
        for (int i = 0; i < reps; ++i) {
            NativeTestTarget.emptyJniStaticMethod0${jni_suffix}();
        }
    }

    public void time_emptyJniStaticMethod6${jni_suffix}(int reps) throws Exception {
        int a = -1;
        int b = 0;
        for (int i = 0; i < reps; ++i) {
            NativeTestTarget.emptyJniStaticMethod6${jni_suffix}(a, b, 1, 2, 3, i);
        }
    }
% endfor
}
