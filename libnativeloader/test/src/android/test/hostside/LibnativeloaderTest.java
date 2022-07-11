/*
 * Copyright (C) 2022 The Android Open Source Project
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

package android.test.hostside;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.AfterClassWithInfo;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.BeforeClassWithInfo;
import com.android.tradefed.util.CommandResult;
import com.google.common.io.ByteStreams;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test libnativeloader behavior for apps and libs in various partitions by overlaying them over
 * the system partitions. Requires root.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class LibnativeloaderTest extends BaseHostJUnit4Test {
    private static final String TAG = "LibnativeloaderTest";
    private static final String CLEANUP_PATHS_KEY = TAG + ":CLEANUP_PATHS";
    private static final String LOG_FILE_NAME = "TestActivity.log";

    @BeforeClassWithInfo
    public static void beforeClassWithDevice(TestInformation testInfo) throws Exception {
        DeviceContext ctx = new DeviceContext(testInfo.getDevice());

        // A soft reboot is slow, so do setup for all tests and reboot once.
        ctx.mDevice.remountSystemWritable();
        ZipFile libApk = openLibContainerApk();
        ctx.pushSystemOemLibs(libApk);
        ctx.pushProductLibs(libApk);

        // For testSystemApp
        ctx.pushResource("/loadlibrarytest_system_app.apk",
                "/system/app/loadlibrarytest_system_app/loadlibrarytest_system_app.apk");

        // For testVendorApp
        ctx.pushResource("/loadlibrarytest_vendor_app.apk",
                "/vendor/app/loadlibrarytest_vendor_app/loadlibrarytest_vendor_app.apk");

        ctx.softReboot();

        testInfo.properties().put(CLEANUP_PATHS_KEY, ctx.mCleanup.getPathList());
    }

    @AfterClassWithInfo
    public static void afterClassWithDevice(TestInformation testInfo) throws Exception {
        String cleanupPathList = testInfo.properties().get(CLEANUP_PATHS_KEY);
        CleanupPaths cleanup = new CleanupPaths(testInfo.getDevice(), cleanupPathList);
        cleanup.cleanup();
    }

    @Test
    public void testLoadLibrariesFromSystemApp() throws Exception {
        DeviceContext ctx = new DeviceContext(getDevice());
        Map<String, Set<String>> appResult = parseAppResultLog(ctx.startActivityAndReturnLog(
                "android.test.app.system", "android.test.app.TestActivity"));
        assertThat(appResult).containsExactly(
                "LOAD_SUCCESS", Set.of("foo.oem1", "bar.oem1", "foo.oem2", "bar.oem2"),
                "LOAD_FAILED", Set.of("foo.product1", "bar.product1"));
        ctx.mCleanup.cleanup();
    }

    @Test
    public void testLoadLibrariesFromVendorApp() throws Exception {
        DeviceContext ctx = new DeviceContext(getDevice());
        Map<String, Set<String>> appResult = parseAppResultLog(ctx.startActivityAndReturnLog(
                "android.test.app.vendor", "android.test.app.TestActivity"));
        assertThat(appResult).containsExactly(
                "LOAD_FAILED", Set.of("foo.oem1", "bar.oem1", "foo.oem2", "bar.oem2",
                        "foo.product1", "bar.product1"));
        ctx.mCleanup.cleanup();
    }

    // Utility class that keeps track of a set of paths the need to be deleted after testing.
    private static class CleanupPaths {
        private ITestDevice mDevice;
        private List<String> mCleanupPaths;

        CleanupPaths(ITestDevice device) {
            mDevice = device;
            mCleanupPaths = new ArrayList<String>();
        }

        CleanupPaths(ITestDevice device, String pathList) {
            mDevice = device;
            mCleanupPaths = Arrays.asList(pathList.split(":"));
        }

        String getPathList() { return String.join(":", mCleanupPaths); }

        // Adds the given path, or its topmost nonexisting parent directory, to the list of paths to
        // clean up.
        void addPath(String devicePath) throws DeviceNotAvailableException {
            File path = new File(devicePath);
            while (true) {
                File parentPath = path.getParentFile();
                if (parentPath == null || mDevice.doesFileExist(parentPath.toString())) {
                    break;
                }
                path = parentPath;
            }
            String nonExistingPath = path.toString();
            if (!mCleanupPaths.contains(nonExistingPath)) {
                mCleanupPaths.add(nonExistingPath);
            }
        }

        void cleanup() throws DeviceNotAvailableException {
            // Clean up in reverse order in case several pushed files were in the same nonexisting
            // directory.
            for (int i = mCleanupPaths.size() - 1; i >= 0; --i) {
                mDevice.deleteFile(mCleanupPaths.get(i));
            }
        }
    }

    // Class for code that needs an ITestDevice. It is instantiated both in tests and in
    // (Before|After)ClassWithInfo.
    private static class DeviceContext {
        ITestDevice mDevice;
        CleanupPaths mCleanup;
        private String mPrimaryArch;

        DeviceContext(ITestDevice device) {
            mDevice = device;
            mCleanup = new CleanupPaths(mDevice);
        }

        void pushSystemOemLibs(ZipFile libApk) throws Exception {
            pushNativeTestLib(libApk, "/system/${LIB}/libfoo.oem1.so");
            pushNativeTestLib(libApk, "/system/${LIB}/libbar.oem1.so");
            pushString("libfoo.oem1.so\n"
                            + "libbar.oem1.so\n",
                    "/system/etc/public.libraries-oem1.txt");

            pushNativeTestLib(libApk, "/system/${LIB}/libfoo.oem2.so");
            pushNativeTestLib(libApk, "/system/${LIB}/libbar.oem2.so");
            pushString("libfoo.oem2.so\n"
                            + "libbar.oem2.so\n",
                    "/system/etc/public.libraries-oem2.txt");
        }

        void pushProductLibs(ZipFile libApk) throws Exception {
            pushNativeTestLib(libApk, "/product/${LIB}/libfoo.product1.so");
            pushNativeTestLib(libApk, "/product/${LIB}/libbar.product1.so");
            pushString("libfoo.product1.so\n"
                            + "libbar.product1.so\n",
                    "/product/etc/public.libraries-product1.txt");
        }

        void softReboot() throws DeviceNotAvailableException {
            assertCommandSucceeds("stop");
            assertCommandSucceeds("start");
            mDevice.waitForDeviceAvailable();
        }

        String getPrimaryArch() throws DeviceNotAvailableException {
            if (mPrimaryArch == null) {
                mPrimaryArch = assertCommandSucceeds("getprop ro.bionic.arch");
            }
            return mPrimaryArch;
        }

        // Pushes the given file contents to the device at the given destination path. destPath is
        // assumed to have no risk of overlapping with existing files, and is deleted in tearDown(),
        // along with any directory levels that had to be created.
        void pushString(String fileContents, String destPath) throws DeviceNotAvailableException {
            mCleanup.addPath(destPath);
            assertThat(mDevice.pushString(fileContents, destPath)).isTrue();
        }

        // Like pushString, but extracts a Java resource and pushes that.
        void pushResource(String resourceName, String destPath) throws Exception {
            File hostTempFile = extractResourceToTempFile(resourceName);
            mCleanup.addPath(destPath);
            assertThat(mDevice.pushFile(hostTempFile, destPath)).isTrue();
        }

        // Like pushString, but extracts libnativeloader_testlib.so from the library_container_app
        // APK and pushes it to destPath. "${LIB}" is replaced with "lib" or "lib64" as appropriate.
        void pushNativeTestLib(ZipFile libApk, String destPath) throws Exception {
            String libApkPath = "lib/" + getPrimaryArch() + "/libnativeloader_testlib.so";
            ZipEntry entry = libApk.getEntry(libApkPath);
            assertWithMessage("Failed to find " + libApkPath + " in library_container_app.apk")
                    .that(entry).isNotNull();

            InputStream inStream = libApk.getInputStream(entry);
            File libraryTempFile = writeStreamToTempFile("libnativeloader_testlib.so", inStream);

            String libDir;
            if (getPrimaryArch().contains("64")) {
                libDir = "lib64";
            } else {
                libDir = "lib";
            }
            destPath = destPath.replace("${LIB}", libDir);

            mCleanup.addPath(destPath);
            assertThat(mDevice.pushFile(libraryTempFile, destPath)).isTrue();
        }

        String assertCommandSucceeds(String command, int timeoutSecs)
                throws DeviceNotAvailableException {
            CommandResult result =
                    mDevice.executeShellV2Command(command, timeoutSecs, TimeUnit.SECONDS);
            assertWithMessage(result.toString()).that(result.getExitCode()).isEqualTo(0);
            // Remove trailing \n's.
            return result.getStdout().trim();
        }

        String assertCommandSucceeds(String command) throws DeviceNotAvailableException {
            return assertCommandSucceeds(command, 120);
        }

        // Starts an activity in the given app, waits until it has written its output into
        // LOG_FILE_NAME in its data directory, and returns that output.
        String startActivityAndReturnLog(String appName, String activity) throws Exception {
            Future<String> appResult = waitForAndRetrieveAppLog(appName);
            try {
                assertCommandSucceeds("am start-activity -S " + appName + "/" + activity);
                return appResult.get();
            } finally {
                assertCommandSucceeds("am force-stop " + appName);
            }
        }

        // Returns a future that waits for the app to replace LOG_FILE_NAME in its data directory
        // and then returns the contents of the file that the app put there.
        Future<String> waitForAndRetrieveAppLog(String appName) throws DeviceNotAvailableException {
            String filePath = "/data/data/" + appName + "/" + LOG_FILE_NAME;
            // We create an empty log file and start a thread to watch it with inotifyd. inotifyd
            // will exit when the file ceases to exist, so assuming the app will replace it through
            // a rename, we can pull it directly afterwards.
            mCleanup.addPath(filePath);
            pushString("", filePath);
            FutureTask<String> future = new FutureTask<>(() -> {
                // If this command fails with a timeout then the test app failed to put the log file
                // into place - look for errors from it in logcat. Shorten the timeout here to save
                // time in that case, since if it works it should finish in well under a second.
                assertCommandSucceeds("inotifyd - " + filePath, 30);
                return mDevice.pullFileContents(filePath);
            });
            (new Thread(future)).start();
            return future;
        }
    }

    static private ZipFile openLibContainerApk() throws Exception {
        return new ZipFile(extractResourceToTempFile("/library_container_app.apk"));
    }

    static private File extractResourceToTempFile(String resourceName) throws Exception {
        assertThat(resourceName).startsWith("/");
        InputStream inStream = LibnativeloaderTest.class.getResourceAsStream(resourceName);
        assertWithMessage("Failed to extract resource " + resourceName).that(inStream).isNotNull();
        return writeStreamToTempFile(resourceName.substring(1), inStream);
    }

    static private File writeStreamToTempFile(String tempFileBaseName, InputStream inStream)
            throws Exception {
        File hostTempFile = File.createTempFile(tempFileBaseName, null);
        ByteStreams.copy(inStream, new FileOutputStream(hostTempFile));
        return hostTempFile;
    }

    static private Map<String, Set<String>> parseAppResultLog(String appResult) {
        Map<String, Set<String>> res = new HashMap<>();
        for (String line : appResult.split("\n")) {
            if (!line.isEmpty()) {
                String[] fields = line.split(": *");
                if (fields.length != 2) {
                    throw new RuntimeException(
                            "Invalid line \"" + line + "\" in app result log:\n" + appResult);
                }
                Set<String> vals = res.getOrDefault(fields[0], new HashSet<>());
                vals.add(fields[1]);
                res.put(fields[0], vals);
            }
        }
        return res;
    }
}
