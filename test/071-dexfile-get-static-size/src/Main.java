/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class Main {
    private static final String TEST_JAR = System.getenv("DEX_LOCATION") + "/071-dexfile-get-static-size-ex.jar";

    private static long getDexFileSize(String filename) throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> DexFile = classLoader.loadClass("dalvik.system.DexFile");
        Method DexFile_loadDex = DexFile.getMethod("loadDex",
                                                   String.class,
                                                   String.class,
                                                   Integer.TYPE);
        Method DexFile_getStaticSizeOfDexFile = DexFile.getMethod("getStaticSizeOfDexFile");
        Object dexFile = DexFile_loadDex.invoke(null, filename, null, 0);
        return (Long) DexFile_getStaticSizeOfDexFile.invoke(dexFile);
    }

    public static void main(String[] args) throws Exception {
        System.out.println("Size of dex file: " + getDexFileSize(TEST_JAR));
    }
}
