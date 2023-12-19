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

package com.android.server.art.model;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.SystemApi;

import com.android.internal.annotations.Immutable;

import com.google.auto.value.AutoValue;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.Map;

/** @hide */
//@SystemApi(client = SystemApi.Client.SYSTEM_SERVER)
@Immutable
@AutoValue
public abstract class ArtManagedFileStats {
    /**
     *
     *
     * @hide
     */
    public static final int TYPE_DEXOPT_ARTIFACT = 0;
    /**
     *
     *
     * @hide
     */
    public static final int TYPE_REF_PROFILE = 1;
    /**
     *
     *
     * @hide
     */
    public static final int TYPE_CUR_PROFILE = 2;

    /** @hide */
    // clang-format off
    @IntDef(prefix = {"TYPE_"}, value = {
        TYPE_DEXOPT_ARTIFACT,
        TYPE_REF_PROFILE,
        TYPE_CUR_PROFILE,
    })
    // clang-format on
    @Retention(RetentionPolicy.SOURCE)
    public @interface FileTypes {}

    /** @hide */
    protected ArtManagedFileStats() {}

    /** @hide */
    public static @NonNull ArtManagedFileStats create(
            long artifactsSize, long refProfilesSize, long curProfilesSize) {
        return new AutoValue_ArtManagedFileStats(Map.of(TYPE_DEXOPT_ARTIFACT, artifactsSize,
                TYPE_REF_PROFILE, refProfilesSize, TYPE_CUR_PROFILE, curProfilesSize));
    }

    /** @hide */
    public abstract @NonNull Map<Integer, Long> getTotalSizesBytes();

    /**
     * Returns the total size, in bytes, of the files of the given type.
     *
     * @throws IllegalArgumentException if {@code fileType} is not one of those defined in {@link
     *         FileTypes}.
     *
     * @hide
     */
    public long getTotalSizeBytesByType(@FileTypes int fileType) {
        Long value = getTotalSizesBytes().get(fileType);
        if (value == null) {
            throw new IllegalArgumentException("Unknown file type " + fileType);
        }
        return value;
    }
}
