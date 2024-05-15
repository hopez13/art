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

import static com.android.server.art.proto.PreRebootStats.Status;

import android.annotation.NonNull;
import android.os.Build;

import androidx.annotation.RequiresApi;

import com.android.server.art.AsLog;
import com.android.server.art.Utils;
import com.android.server.art.proto.PreRebootStats;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;

/**
 * A helper class to report the Pre-reboot Dexopt metrics to StatsD.
 *
 * This class is not thread-safe.
 *
 * During Pre-reboot Dexopt, both the old version and the new version of this code is run. The old
 * version writes to disk first, and the new version writes to disk later. After reboot, the new
 * version loads from disk.
 *
 * @hide
 */
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class PreRebootStatsReporter {
    private static final String FILENAME = "/data/system/pre-reboot-stats.pb";

    private @NonNull PreRebootStats.Builder mStatsBuilder = PreRebootStats.newBuilder();

    public void recordJobScheduled() {
        mStatsBuilder.setStatus(Status.STATUS_NOT_RUN)
                .setJobScheduledTimestampMillis(System.currentTimeMillis());
        save();
    }

    public void recordJobStarted() {
        mStatsBuilder.setStatus(Status.STATUS_STARTED)
                .setJobStartedTimestampMillis(System.currentTimeMillis());
        save();
    }

    public void recordJobEnded(Status status) {
        Utils.check(status == Status.STATUS_FINISHED || status == Status.STATUS_FAILED
                || status == Status.STATUS_CANCELLED);

        mStatsBuilder.setStatus(status).setJobEndedTimestampMillis(System.currentTimeMillis());
        save();
    }

    public void recordProgress(int skippedPackageCount, int optimizedPackageCount,
            int failedPackageCount, int totalPackageCount) {
        mStatsBuilder.setSkippedPackageCount(skippedPackageCount)
                .setOptimizedPackageCount(optimizedPackageCount)
                .setFailedPackageCount(failedPackageCount)
                .setTotalPackageCount(totalPackageCount);
        save();
    }

    public void load() {
        try (InputStream in = new FileInputStream(FILENAME)) {
            mStatsBuilder.mergeFrom(in);
        } catch (IOException e) {
            // Nothing else we can do but to start from scratch.
            AsLog.e("Failed to load pre-reboot stats", e);
        }
    }

    private void save() {
        var file = new File(FILENAME);
        File tempFile = null;
        try {
            tempFile = File.createTempFile(file.getName(), null /* suffix */, file.getParentFile());
            try (OutputStream out = new FileOutputStream(tempFile.getPath())) {
                mStatsBuilder.build().writeTo(out);
            }
            Files.move(tempFile.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING,
                    StandardCopyOption.ATOMIC_MOVE);
        } catch (IOException e) {
            AsLog.e("Failed to save pre-reboot stats", e);
        } finally {
            Utils.deleteIfExistsSafe(tempFile);
        }
    }

    private void delete() {
        Utils.deleteIfExistsSafe(new File(FILENAME));
    }
}
