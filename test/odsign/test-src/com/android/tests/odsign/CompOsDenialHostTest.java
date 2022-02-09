/*
 * Copyright 2022 The Android Open Source Project
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

package com.android.tests.odsign;

import static com.android.tests.odsign.CompOsTestUtils.*;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;
import com.android.tradefed.testtype.junit4.AfterClassWithInfo;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;
import com.android.tradefed.testtype.junit4.BeforeClassWithInfo;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Test to check if bad CompOS pending artifacts can be denied by odsign. */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CompOsDenialHostTest extends BaseHostJUnit4Test {

    private static final String PENDING_ARTIFACTS_DIR =
            "/data/misc/apexdata/com.android.art/compos-pending";
    private static final String PENDING_ARTIFACTS_BACKUP_DIR =
            "/data/misc/apexdata/com.android.art/compos-pending-backup";

    private static final String TIMESTAMP_REBOOT_KEY = "compos_test_timestamp_reboot";

    private OdsignTestUtils mTestUtils;

    @BeforeClassWithInfo
    public static void beforeClassWithDevice(TestInformation testInfo) throws Exception {
        ITestDevice device = testInfo.getDevice();
        OdsignTestUtils testUtils = new OdsignTestUtils(testInfo);

        assumeCompOsPresent(device);
        testUtils.enableAdbRootOrSkipTest();

        testUtils.installTestApex();

        // Compile once, then backup the compiled artifacts to be reused by each test case just to
        // reduce testing time.
        runCompilationJobEarlyAndWait(device);
        testUtils.assertCommandSucceeds(
                "mv " + PENDING_ARTIFACTS_DIR + " " + PENDING_ARTIFACTS_BACKUP_DIR);

        testInfo.properties().put(TIMESTAMP_REBOOT_KEY, testUtils.getDeviceCurrentTimestamp());
        testUtils.reboot();
    }

    @AfterClassWithInfo
    public static void afterClassWithDevice(TestInformation testInfo) throws Exception {
        testInfo.getDevice().executeShellV2Command("rm -rf " + PENDING_ARTIFACTS_BACKUP_DIR);

        OdsignTestUtils testUtils = new OdsignTestUtils(testInfo);
        testUtils.uninstallTestApex();
        testUtils.reboot();
        testUtils.restoreAdbRoot();
    }

    @Before
    public void setUp() throws Exception {
        mTestUtils = new OdsignTestUtils(getTestInformation());

        // Restore the pending artifacts for each test to mess up with.
        mTestUtils.assertCommandSucceeds("rm -rf " + PENDING_ARTIFACTS_DIR);
        mTestUtils.assertCommandSucceeds(
                "cp -rp " + PENDING_ARTIFACTS_BACKUP_DIR + " " + PENDING_ARTIFACTS_DIR);
    }

    @After
    public void tearDown() throws Exception {
    }

    @Test
    public void denyDueToInconsistentFileName() throws Exception {
        // Attack emulation: swap file names
        String output = mTestUtils.assertCommandSucceeds(
                "find " + PENDING_ARTIFACTS_DIR + " |grep 'jar@classes.odex'| head -2");
        String[] lines = output.split("\n");
        assertThat(lines.length).isGreaterThan(1);
        String oat1 = lines[0];
        String oat2 = lines[1];
        String temp = PENDING_ARTIFACTS_DIR + "/temp";
        mTestUtils.assertCommandSucceeds(
                "mv " + oat1 + " " + temp + "; " +
                "mv " + oat2 + " " + oat1 + "; " +
                "mv " + temp + " " + oat2);

        // Expect the pending artifacts to be denied by odsign during the reboot.
        mTestUtils.reboot();
        expectNoCurrentFilesFromCompOs();
    }

    @Test
    public void denyDueToMissingFile() throws Exception {
        // Attack emulation: delete a file
        String oat = mTestUtils.assertCommandSucceeds(
                "find " + PENDING_ARTIFACTS_DIR + " -typef | head -1").trim();
        mTestUtils.assertCommandSucceeds("rm -f " + oat);

        // Expect the pending artifacts to be denied by odsign during the reboot.
        mTestUtils.reboot();
        expectNoCurrentFilesFromCompOs();
    }

    private void expectNoCurrentFilesFromCompOs() throws DeviceNotAvailableException {
        // None of the files should have a timestamp earlier than the first reboot.
        int numFiles = mTestUtils.countFilesCreatedBeforeTime(
                OdsignTestUtils.ART_APEX_DALVIK_CACHE_DIRNAME,
                getTestInformation().properties().get(TIMESTAMP_REBOOT_KEY));
        assertThat(numFiles).isEqualTo(0);

        // odsign should have deleted the pending directory.
        mTestUtils.assertCommandSucceeds("test ! -d " + PENDING_ARTIFACTS_DIR);
    }
}
