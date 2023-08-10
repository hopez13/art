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

import android.annotation.NonNull;
import android.app.job.JobParameters;
import android.content.Context;
import android.os.Build;
import android.os.RemoteException;

import androidx.annotation.RequiresApi;

import com.android.internal.annotations.VisibleForTesting;
import com.android.server.art.model.ArtServiceJobInterface;

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
        IArtdChroot artdChroot = Utils.getArtdChroot();
        try {
            artdChroot.setUp();
        } catch (RemoteException e) {
            Utils.logArtdException(e);
        }
    }

    public void testOnlyTearDown() {
        IArtdChroot artdChroot = Utils.getArtdChroot();
        try {
            artdChroot.tearDown();
        } catch (RemoteException e) {
            Utils.logArtdException(e);
        }
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
    }
}
