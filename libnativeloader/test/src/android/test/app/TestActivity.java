/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.test.app;

import android.app.Activity;
import android.content.pm.ApplicationInfo;
import android.os.Bundle;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;

public class TestActivity extends Activity {
    private static final String LOG_FILE_NAME = "TestActivity.log";

    private File mTempLogFile;
    private OutputStreamWriter mLogStream;

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        openLogFile();
        try {
            tryLoadingLib("foo.oem1");
            tryLoadingLib("bar.oem1");
            tryLoadingLib("foo.oem2");
            tryLoadingLib("bar.oem2");
            tryLoadingLib("foo.product1");
            tryLoadingLib("bar.product1");
        } finally {
            finalizeLogFile();
        }
    }

    private void tryLoadingLib(String name) {
        try {
            System.loadLibrary(name);
            Log("LOAD_SUCCESS: " + name);
        } catch (UnsatisfiedLinkError e) {
            Log("LOAD_FAILED: " + name);
            Log.d(getPackageName(), null, e);
        }
    }

    private void openLogFile() {
        try {
            mTempLogFile = File.createTempFile("TestActivity.log", null, getDataDir());
            mLogStream = new OutputStreamWriter(new FileOutputStream(mTempLogFile));
        } catch (IOException e) {
            throw new RuntimeException("Failed to create temporary log file in " + getDataDir(), e);
        }
    }

    private void finalizeLogFile() {
        File logFile = new File(getDataDir() + "/" + LOG_FILE_NAME);
        try {
            mLogStream.close();
            if (!mTempLogFile.renameTo(logFile)) {
                throw new RuntimeException("Failed to rename temporary log file to " + logFile);
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to finalize log file " + logFile, e);
        }
    }

    private void Log(String msg) {
        Log.d(getPackageName(), msg);
        new PrintWriter(mLogStream).println(msg);
    }
}
