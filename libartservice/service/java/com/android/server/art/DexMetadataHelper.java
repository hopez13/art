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

import android.annotation.NonNull;
import android.os.Build;
import android.util.Log;

import androidx.annotation.RequiresApi;

import com.android.internal.annotations.VisibleForTesting;
import com.android.server.art.proto.DexMetadataConfig;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.NoSuchFileException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * A helper class to handle dex metadata (dm) files.
 *
 * Note that this is not the only consumer of dm files. A dm file is a container that contains
 * various types of files for various purposes, passed down to various layers and consumed by them.
 *
 * @hide
 */
@RequiresApi(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
public class DexMetadataHelper {
    private static final String TAG = ArtManagerLocal.TAG;

    @NonNull private final Injector mInjector;

    public DexMetadataHelper() {
        this(new Injector());
    }

    @VisibleForTesting
    public DexMetadataHelper(@NonNull Injector injector) {
        mInjector = injector;
    }

    @NonNull
    public DexMetadataInfo getDexMetadataInfo(@NonNull String dexPath) {
        String dmPath = mInjector.getDmPath(dexPath);
        try (var zipFile = new ZipFile(dmPath)) {
            ZipEntry entry = zipFile.getEntry("config.pb");
            if (entry == null) {
                return new DexMetadataInfo(
                        true /* exists */, DexMetadataConfig.getDefaultInstance());
            }
            try (InputStream stream = zipFile.getInputStream(entry)) {
                return new DexMetadataInfo(true /* exists */, DexMetadataConfig.parseFrom(stream));
            }
        } catch (IOException e) {
            if (!(e instanceof FileNotFoundException || e instanceof NoSuchFileException)) {
                Log.e(TAG, String.format("Failed to read dm file '%s'", dmPath), e);
            }
            return getDefaultDexMetadataInfo();
        }
    }

    @NonNull
    public DexMetadataInfo getDefaultDexMetadataInfo() {
        return new DexMetadataInfo(false /* exists */, DexMetadataConfig.getDefaultInstance());
    }

    /**
     * @param exists Whether the dm file exists.
     * @param config The config deserialized from `config.pb`, if exists. Or the default instance if
     *         the file doesn't exist or an error occurred.
     */
    public record DexMetadataInfo(boolean exists, @NonNull DexMetadataConfig config) {}

    /**
     * Injector pattern for testing purpose.
     *
     * @hide
     */
    @VisibleForTesting
    public static class Injector {
        @NonNull
        String getDmPath(@NonNull String dexPath) {
            int pos = dexPath.lastIndexOf(".");
            return (pos != -1 ? dexPath.substring(0, pos) : dexPath) + ".dm";
        }
    }
}
