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

package com.android.server.art;

import static android.os.IBinder.DeathRecipient;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.os.IBinder;
import android.os.RemoteException;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;

/**
 * A helper class that caches a reference to artd, to avoid repetitive calls to `waitForService`,
 * as the latter is considered expensive.
 *
 * @hide
 */
public class ArtdRefCache {
    private static final String TAG = ArtManagerLocal.TAG;
    @VisibleForTesting public static final long CACHE_TIMEOUT_MS = 5_000;

    @Nullable private static ArtdRefCache sInstance = null;

    @NonNull private final Injector mInjector;
    @NonNull private final Debouncer mDebouncer;

    /**
     * A lock that guards the <b>reference</b> to artd.
     *
     * Warning: This lock does not guard artd itself. Do not hold this lock when calling artd as it
     * will prevent parallelism.
     */
    @NonNull private final Object mLock = new Object();

    @GuardedBy("mLock") private int mPinCount = 0;
    @GuardedBy("mLock") @Nullable private IArtd mArtd = null;

    public ArtdRefCache() {
        this(new Injector());
    }

    @VisibleForTesting
    public ArtdRefCache(@NonNull Injector injector) {
        mInjector = injector;
        mDebouncer = new Debouncer(CACHE_TIMEOUT_MS, mInjector::createScheduledExecutor);
    }

    @NonNull
    public static synchronized ArtdRefCache getInstance() {
        if (sInstance == null) {
            sInstance = new ArtdRefCache();
        }
        return sInstance;
    }

    /** Returns a reference to artd, from the cache or created on demand. */
    @NonNull
    public IArtd getArtd() {
        synchronized (mLock) {
            if (mArtd != null) {
                return mArtd;
            }
            IArtd artd = mInjector.getArtd();
            if (mPinCount > 0) {
                try {
                    // Force clear the cache when the artd instance is dead.
                    artd.asBinder().linkToDeath(new CacheDeathRecipient(), 0 /* flags */);
                    // Cache the instance.
                    mArtd = artd;
                } catch (RemoteException e) {
                    Utils.logArtdException(e);
                }
            }
            return artd;
        }
    }

    private void dropIfNoPin() {
        synchronized (mLock) {
            if (mPinCount == 0) {
                mArtd = null;
            }
        }
    }

    /**
     * A scope that pins any reference to artd, either an existing one or one created within the
     * scope. The reference is dropped when there is no more pin within {@link #CACHE_TIMEOUT_MS}.
     */
    public class Pin implements AutoCloseable {
        public Pin() {
            synchronized (mLock) {
                mPinCount++;
            }
        }

        @Override
        public void close() {
            synchronized (mLock) {
                mPinCount--;
                Utils.check(mPinCount >= 0);
                if (mPinCount == 0) {
                    // During the timeout:
                    // - If there is no more pinning and unpinning, the cache will be dropped.
                    // - If there are pinnings and unpinnings, and `mPinCount` never reaches 0
                    //   again, `dropIfNoPin` will be run, but it will not drop the cache.
                    // - If there are pinnings and unpinnings, and `mPinCount` reaches 0 again,
                    //   `dropIfNoPin` will be debounced.
                    mDebouncer.maybeRunAsync(ArtdRefCache.this::dropIfNoPin);
                }
            }
        }
    }

    private class CacheDeathRecipient implements DeathRecipient {
        @Override
        public void binderDied() {
            // Legacy.
        }

        @Override
        public void binderDied(@NonNull IBinder who) {
            synchronized (mLock) {
                if (mArtd != null && mArtd.asBinder() == who) {
                    mArtd = null;
                }
            }
        }
    }

    /** Injector pattern for testing purpose. */
    @VisibleForTesting
    public static class Injector {
        Injector() {
            // Call the getters for various dependencies, to ensure correct initialization order.
            ArtModuleServiceInitializer.getArtModuleServiceManager();
        }

        @NonNull
        public IArtd getArtd() {
            IArtd artd =
                    IArtd.Stub.asInterface(ArtModuleServiceInitializer.getArtModuleServiceManager()
                                                   .getArtdServiceRegisterer()
                                                   .waitForService());
            if (artd == null) {
                throw new IllegalStateException("Unable to connect to artd");
            }
            return artd;
        }

        @NonNull
        public ScheduledExecutorService createScheduledExecutor() {
            return Executors.newSingleThreadScheduledExecutor();
        }
    }
}
