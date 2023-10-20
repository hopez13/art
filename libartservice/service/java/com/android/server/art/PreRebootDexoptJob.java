/*
 * Copyright (C) 2023 The Android Open Source Project
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

package com.android.server.art;

import static com.android.server.art.IArtdChroot.CHROOT_DIR;

import android.annotation.NonNull;
import android.app.job.JobParameters;
import android.content.Context;
import android.os.ArtModuleServiceManager;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.RequiresApi;

import com.android.internal.annotations.VisibleForTesting;
import com.android.server.art.model.ArtServiceJobInterface;
import com.android.server.art.prereboot.PreRebootManager;

import dalvik.system.DelegateLastClassLoader;
import dalvik.system.VMRuntime;

/** @hide */
@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class PreRebootDexoptJob implements ArtServiceJobInterface {
    private static final String TAG = ArtManagerLocal.TAG;

    public static final int JOB_ID = 27873781;

    @NonNull private final Injector mInjector;

    public PreRebootDexoptJob(@NonNull Context context) {
        this(new Injector(context));
    }

    @VisibleForTesting
    public PreRebootDexoptJob(@NonNull Injector injector) {
        mInjector = injector;
    }

    @Override
    public boolean onStartJob(
            @NonNull BackgroundDexoptJobService jobService, @NonNull JobParameters params) {
        // "true" means the job will continue running until `jobFinished` is called.
        return false;
    }

    @Override
    public boolean onStopJob(@NonNull JobParameters params) {
        // "true" means to execute again in the same interval with the default retry policy.
        return true;
    }

    public void run() {
        try {
            setUp();
            runImpl();
            tearDown();
        } catch (RemoteException e) {
            Utils.logArtdException(e);
        } catch (ReflectiveOperationException | InterruptedException e) {
            Log.e(TAG, "Failed to run pre-reboot dexopt", e);
        }
    }

    public void setUp() throws RemoteException {
        mInjector.getArtdChroot().setUp();
    }

    public void runImpl() throws ReflectiveOperationException {
        var cancellationSignal = new CancellationSignal();
        runFromChroot(cancellationSignal);
    }

    public void tearDown() throws RemoteException, InterruptedException {
        // Force run GC and finalization to trigger the shutdown of artd and unload the dex file.
        System.gc();
        System.runFinalization();
        // Wait for the service manager to shut down artd.
        Thread.sleep(5000);
        mInjector.getArtdChroot().tearDown();
    }

    private void runFromChroot(@NonNull CancellationSignal cancellationSignal)
            throws ReflectiveOperationException {
        String chrootArtDir = CHROOT_DIR + "/apex/com.android.art";
        String dexPath = chrootArtDir + "/javalib/service-art.jar";
        var classLoader =
                new DelegateLastClassLoader(dexPath, this.getClass().getClassLoader() /* parent */);
        Class<?> preRebootManagerClass =
                classLoader.loadClass("com.android.server.art.prereboot.PreRebootManager");
        // Check if the dex file is loaded successfully. Note that the constructor of
        // `DelegateLastClassLoader` does not throw when the load fails.
        if (preRebootManagerClass == PreRebootManager.class) {
            throw new IllegalStateException(String.format("Failed to load %s", dexPath));
        }
        preRebootManagerClass
                .getMethod("run", ArtModuleServiceManager.class, Context.class,
                        CancellationSignal.class)
                .invoke(null, ArtModuleServiceInitializer.getArtModuleServiceManager(),
                        mInjector.getContext(), cancellationSignal);
    }

    /**
     * Injector pattern for testing purpose.
     *
     * @hide
     */
    @VisibleForTesting
    public static class Injector {
        @NonNull private final Context mContext;

        Injector(@NonNull Context context) {
            mContext = context;
        }

        @NonNull
        public Context getContext() {
            return mContext;
        }

        @NonNull
        public IArtdChroot getArtdChroot() {
            return GlobalInjector.getInstance().getArtdChroot();
        }
    }
}
