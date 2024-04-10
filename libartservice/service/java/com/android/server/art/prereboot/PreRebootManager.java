/*
 * Copyright (C) 2024 The Android Open Source Project
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

package com.android.server.art.prereboot;

import android.annotation.NonNull;
import android.content.Context;
import android.os.ArtModuleServiceManager;
import android.os.Build;
import android.os.CancellationSignal;

import androidx.annotation.RequiresApi;

import com.android.server.LocalManagerRegistry;
import com.android.server.art.ArtManagerLocal;
import com.android.server.art.ArtdRefCache;
import com.android.server.art.ReasonMapping;
import com.android.server.pm.PackageManagerLocal;

import java.util.Objects;

/**
 * The entry point of Pre-reboot Dexopt, called through reflection from an old version of the ART
 * module. This interface must be kept stable from one version of the ART module to another. In
 * principle, a method here should be kept until the platform release that contains the old version
 * of the ART module that calls the method no longer receives Mainline updates, unless there is a
 * good reason to drop the Pre-reboot Dexopt support earlier for certain versions of the ART module.
 * Dropping the support for certain versions will only make devices lose the opportunity to optimize
 * apps before the reboot, but it won't cause severe results such as crashes because even the oldest
 * version that uses this interface can elegantly handle reflection exceptions.
 *
 * During Pre-reboot Dexopt, the new version of this code is run.
 *
 * @hide
 */
@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class PreRebootManager {
    public static void run(@NonNull ArtModuleServiceManager artModuleServiceManager,
            @NonNull Context context, @NonNull CancellationSignal cancellationSignal) {
        try {
            PreRebootGlobalInjector.init(artModuleServiceManager, context);
            ArtManagerLocal artManagerLocal = new ArtManagerLocal(context);
            PackageManagerLocal packageManagerLocal = Objects.requireNonNull(
                    LocalManagerRegistry.getManager(PackageManagerLocal.class));
            try (var snapshot = packageManagerLocal.withFilteredSnapshot()) {
                artManagerLocal.dexoptPackages(snapshot, ReasonMapping.REASON_PRE_REBOOT_DEXOPT,
                        cancellationSignal, null /* processCallbackExecutor */,
                        null /* processCallback */);
            }
        } finally {
            ArtdRefCache.getInstance().reset();
        }
    }
}
