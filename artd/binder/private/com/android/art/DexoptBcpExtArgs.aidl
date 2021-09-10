/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.art;

import com.android.art.Isa;

/**
 * FIXME better doc, maybe in the next CL
 * Important: When adding any new arguments, consider that a malicious party can tamper with the
 * value at will. There must be checks before the data is consumed by the compiler. At the very
 * least, the malicious party must not be allowed to affect the compilation to generate code they
 * want.
 *
 * The compiler should not assume the data behi
 *
 * {@hide}
 */
parcelable DexoptBcpExtArgs {
    String[] dexPaths;
    int[] dexFds;
    int[] bootClasspathFds;
    int profileFd = -1;
    int dirtyImageObjectsFd = -1;

    String oatLocation;
    int oatFd = -1;
    int vdexFd = -1;
    int imageFd = -1;

    Isa isa;

    int[] cpuSet;
    int threads;
}
