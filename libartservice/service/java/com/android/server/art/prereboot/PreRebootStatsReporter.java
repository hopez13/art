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

import static com.android.server.art.proto.PreRebootStats.JobRun;
import static com.android.server.art.proto.PreRebootStats.Status;

import android.annotation.NonNull;
import android.os.Build;

import androidx.annotation.RequiresApi;

import com.android.server.LocalManagerRegistry;
import com.android.server.art.ArtManagerLocal;
import com.android.server.art.ArtStatsLog;
import com.android.server.art.ArtdRefCache;
import com.android.server.art.AsLog;
import com.android.server.art.ReasonMapping;
import com.android.server.art.Utils;
import com.android.server.art.model.DexoptStatus;
import com.android.server.art.proto.PreRebootStats;
import com.android.server.pm.PackageManagerLocal;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;

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
    private @NonNull Set<String> packagesWithArtifacts = new HashSet<>();

    public void recordJobScheduled() {
        mStatsBuilder.clear();
        mStatsBuilder.setStatus(Status.STATUS_SCHEDULED)
                .setJobScheduledTimestampMillis(System.currentTimeMillis());
        save();
    }

    public void recordJobStarted() {
        Utils.check(mStatsBuilder.getStatus() != Status.STATUS_UNKNOWN);

        JobRun.Builder runBuilder =
                JobRun.newBuilder().setJobStartedTimestampMillis(System.currentTimeMillis());
        mStatsBuilder.setStatus(Status.STATUS_STARTED)
                .addJobRuns(runBuilder)
                .setSkippedPackageCount(0)
                .setOptimizedPackageCount(0)
                .setFailedPackageCount(0)
                .setTotalPackageCount(0);
        save();
    }

    public void recordJobEnded(Status status) {
        Utils.check(status == Status.STATUS_FINISHED || status == Status.STATUS_FAILED
                || status == Status.STATUS_CANCELLED);

        List<JobRun> jobRuns = mStatsBuilder.getJobRunsList();
        JobRun lastRun = jobRuns.size() > 0 ? jobRuns.get(jobRuns.size() - 1) : null;
        if (lastRun == null || lastRun.getJobEndedTimestampMillis() > 0) {
            // An ad-hoc run, not a job.
            return;
        }

        JobRun.Builder runBuilder =
                JobRun.newBuilder(lastRun).setJobEndedTimestampMillis(System.currentTimeMillis());
        mStatsBuilder.setStatus(status).setJobRuns(jobRuns.size() - 1, runBuilder);
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

    public void recordPackageWithArtifacts(@NonNull String packageName) {
        packagesWithArtifacts.add(packageName);
    }

    public void reportAsync() {
        new CompletableFuture().runAsync(this::report).exceptionally(t -> {
            AsLog.e("Failed to report stats", t);
            return null;
        });
    }

    private void report() {
        load();
        delete();

        if (mStatsBuilder.getStatus() == Status.STATUS_UNKNOWN) {
            // Job not scheduled, probably because Pre-reboot Dexopt is not enabled.
            return;
        }

        long startTime = System.currentTimeMillis();
        PackageManagerLocal packageManagerLocal =
                Objects.requireNonNull(LocalManagerRegistry.getManager(PackageManagerLocal.class));
        ArtManagerLocal artManagerLocal =
                Objects.requireNonNull(LocalManagerRegistry.getManager(ArtManagerLocal.class));

        int packagesWithArtifactsUsableCount;
        try (var snapshot = packageManagerLocal.withFilteredSnapshot();
                var pin = ArtdRefCache.getInstance().new Pin()) {
            // For simplicity, we consider all artifacts usable if we see at least one
            // `REASON_PRE_REBOOT_DEXOPT` because it's not easy to know which files are committed.
            packagesWithArtifactsUsableCount =
                    (int) packagesWithArtifacts.stream()
                            .map((packageName)
                                            -> artManagerLocal.getDexoptStatus(
                                                    snapshot, packageName))
                            .filter(status
                                    -> status.getDexContainerFileDexoptStatuses().stream().anyMatch(
                                            fileStatus
                                            -> fileStatus.getCompilationReason().equals(
                                                    ReasonMapping.REASON_PRE_REBOOT_DEXOPT)))
                            .count();
        }
        AsLog.e("jiakaiz took " + (System.currentTimeMillis() - startTime) + "ms");

        List<JobRun> jobRuns = mStatsBuilder.getJobRunsList();
        JobRun lastRun = jobRuns.size() > 0 ? jobRuns.get(jobRuns.size() - 1) : null;
        long jobDurationMs = (lastRun != null && lastRun.getJobEndedTimestampMillis() > 0)
                ? (lastRun.getJobEndedTimestampMillis() - lastRun.getJobStartedTimestampMillis())
                : -1;
        long jobLatencyMs =
                (jobRuns.size() > 0 && mStatsBuilder.getJobScheduledTimestampMillis() > 0)
                ? (jobRuns.get(0).getJobStartedTimestampMillis()
                          - mStatsBuilder.getJobScheduledTimestampMillis())
                : -1;

        ArtStatsLog.write(ArtStatsLog.PREREBOOT_DEXOPT_JOB_ENDED, getStatusForStatsd(),
                mStatsBuilder.getOptimizedPackageCount(), mStatsBuilder.getFailedPackageCount(),
                mStatsBuilder.getSkippedPackageCount(), mStatsBuilder.getTotalPackageCount(),
                jobDurationMs, jobLatencyMs, packagesWithArtifacts.size(),
                packagesWithArtifactsUsableCount, jobRuns.size());
    }

    private int getStatusForStatsd() {
        switch (mStatsBuilder.getStatus()) {
            case STATUS_UNKNOWN:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_UNKNOWN;
            case STATUS_SCHEDULED:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_SCHEDULED;
            case STATUS_STARTED:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_STARTED;
            case STATUS_FINISHED:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_FINISHED;
            case STATUS_FAILED:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_FAILED;
            case STATUS_CANCELLED:
                return ArtStatsLog.PRE_REBOOT_DEXOPT_JOB_ENDED__STATUS__STATUS_CANCELLED;
            default:
                throw new IllegalStateException("Unknown status: " + mStatsBuilder.getStatus());
        }
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

    public void delete() {
        Utils.deleteIfExistsSafe(new File(FILENAME));
    }
}
