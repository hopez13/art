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

import com.android.art.CompilerFilter;
import com.android.art.Isa;

// FIXME add reasoning of security
/** {@hide} */
parcelable DexoptSystemServerArgs {
    int dexFd = -1;
    String dexPath;

    // TODO can we merge all these into one array?
    int[] bootClasspathFds;
    int[] bootClasspathImageFds;
    int[] bootClasspathVdexFds;
    int[] bootClasspathOatFds;

    int profileFd = -1;
    // TODO: Add a compiler filter field

    int updatableBcpPackagesTxtFd = -1;
    int[] classloaderFds;
    String[] classloaderContext; // FIXME security

    Isa isa;

    CompilerFilter compilerFilter;

    String oatLocation; // only for reference

    // Output
    int imageFd = -1;
    int vdexFd = -1;
    int oatFd = -1;

    /**
     * Timeout of the compiler run.
     *
     * Security: The value won't influence the compiler output.
     */
    int[] cpuSet;
    int threads;
}
