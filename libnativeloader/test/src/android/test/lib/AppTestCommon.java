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

package android.test.lib;

import android.test.productsharedlib.ProductSharedLib;
import android.test.systemextsharedlib.SystemExtSharedLib;
import android.test.systemsharedlib.SystemSharedLib;
import android.test.vendorsharedlib.VendorSharedLib;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public abstract class AppTestCommon {
    public enum AppLocation { DATA, SYSTEM, PRODUCT, VENDOR }

    public abstract AppLocation getAppLocation();

    // Loading private libs using absolute paths through shared libs should only
    // depend on the location of the shared lib, so these tests are shared for
    // all apps, regardless of location.

    @Test
    public void testLoadPrivateLibrariesViaSystemSharedLibWithAbsolutePaths() {
        if (TestUtils.canLoadPrivateLibsFromSamePartition()) {
            SystemSharedLib.load(TestUtils.libPath("/system", "system_private7"));
            SystemSharedLib.load(TestUtils.libPath("/system_ext", "systemext_private7"));
        }
        TestUtils.assertLibraryInaccessible(
                () -> { SystemSharedLib.load(TestUtils.libPath("/product", "product_private7")); });
        TestUtils.assertLibraryInaccessible(
                () -> { SystemSharedLib.load(TestUtils.libPath("/vendor", "vendor_private7")); });
    }

    @Test
    public void testLoadPrivateLibrariesViaSystemExtSharedLibWithAbsolutePaths() {
        if (TestUtils.canLoadPrivateLibsFromSamePartition()) {
            SystemExtSharedLib.load(TestUtils.libPath("/system", "system_private8"));
            SystemExtSharedLib.load(TestUtils.libPath("/system_ext", "systemext_private8"));
        }
        TestUtils.assertLibraryInaccessible(() -> {
            SystemExtSharedLib.load(TestUtils.libPath("/product", "product_private8"));
        });
        TestUtils.assertLibraryInaccessible(() -> {
            SystemExtSharedLib.load(TestUtils.libPath("/vendor", "vendor_private8"));
        });
    }

    @Test
    public void testLoadPrivateLibrariesViaProductSharedLibWithAbsolutePaths() {
        if (getAppLocation() != AppLocation.SYSTEM) {
            // System apps can load system private libs by absolute path through the classloader
            // namespace.
            TestUtils.assertLibraryInaccessible(() -> {
                ProductSharedLib.load(TestUtils.libPath("/system", "system_private9"));
            });
            TestUtils.assertLibraryInaccessible(() -> {
                ProductSharedLib.load(TestUtils.libPath("/system_ext", "systemext_private9"));
            });
        }
        if (TestUtils.canLoadPrivateLibsFromSamePartition()) {
            ProductSharedLib.load(TestUtils.libPath("/product", "product_private9"));
        }
        TestUtils.assertLibraryInaccessible(
                () -> { ProductSharedLib.load(TestUtils.libPath("/vendor", "vendor_private9")); });
    }

    @Test
    public void testLoadPrivateLibrariesViaVendorSharedLibWithAbsolutePaths() {
        if (getAppLocation() != AppLocation.SYSTEM) {
            // System apps can load system private libs by absolute path through the classloader
            // namespace.
            TestUtils.assertLibraryInaccessible(() -> {
                VendorSharedLib.load(TestUtils.libPath("/system", "system_private10"));
            });
            TestUtils.assertLibraryInaccessible(() -> {
                VendorSharedLib.load(TestUtils.libPath("/system_ext", "systemext_private10"));
            });
        }
        TestUtils.assertLibraryInaccessible(() -> {
            VendorSharedLib.load(TestUtils.libPath("/product", "product_private10"));
        });
        if (TestUtils.canLoadPrivateLibsFromSamePartition()) {
            VendorSharedLib.load(TestUtils.libPath("/vendor", "vendor_private10"));
        }
    }
}
