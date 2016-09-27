/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

/**
 * DexFile tests (Dalvik-specific).
 */
public class Main {
    private static final String DEX_CLASS_LOADER_CLASS = "dalvik.system.DexClassLoader";
    private static final String PATH_CLASS_LOADER_CLASS = "dalvik.system.PathClassLoader";
    private static final String DEX_LOCATION = System.getenv("DEX_LOCATION");
    private static final String SECONDARY_DEX = "978-dexfile-32-ex.jar";
    private static final String SECONDARY_DEX_PATH = DEX_LOCATION + "/" + SECONDARY_DEX;
    private static final String ODEX_DIR = DEX_LOCATION + "/dalvik-cache";
    private static final String LIB_DIR = "/nowhere/nothing/";

    private static String sInstructionSet;
    private static String sInstructionSet32;
    private static boolean sIs64Bit;
    private static boolean sTestForPathClassLoader = true;

    static {
        try {
            Class<?> vmruntime_class = Class.forName("dalvik.system.VMRuntime");
            Method getRuntime_method = vmruntime_class.getDeclaredMethod("getRuntime");
            Object runtime = getRuntime_method.invoke(null);
            Method is64Bit_method = vmruntime_class.getDeclaredMethod("is64Bit");
            Method getCurrentInstructionSet_method =
                    vmruntime_class.getDeclaredMethod("getCurrentInstructionSet");
            Method get32BitInstructionSet_method =
                    vmruntime_class.getDeclaredMethod("get32BitInstructionSet", String.class);
            sIs64Bit = (boolean)is64Bit_method.invoke(runtime);
            sInstructionSet = (String)getCurrentInstructionSet_method.invoke(null);
            sInstructionSet32 = (String)get32BitInstructionSet_method.invoke(null, sInstructionSet);
        } catch (final Exception e) {
            throw new Error(e);
        }
    }

    private static final String getOdexDir() {
        return ODEX_DIR;
    }

    private static final String getOdexDir32() {
        File odex_dir = new File(ODEX_DIR);
        odex_dir.mkdir();
        File parent = new File(getOdexDir());
        File child = new File(parent, sInstructionSet32);
        child.mkdir();
        return child.getAbsolutePath();
    }

    private static final String getOdexDir64() {
        File parent = new File(getOdexDir());
        File child = new File(parent, sInstructionSet);
        child.mkdir();
        return child.getAbsolutePath();
    }

    private static final String getFileNameNoExtension(File file) {
        String fileName = file.getName();
        int index = fileName.lastIndexOf(".");
        if (index != -1) {
            return fileName.substring(0, index);
        }
        return fileName;
    }

    public static void main(String[] args) throws Exception {
        /*
         * This test runs in two VMs. The first VM is 64-bit and cross compiles
         * a dex file to a 32-bit OAT file. The second VM tests the generated OAT
         * file.
         */
        if (sIs64Bit) {
            System.out.println("* Running on 64-bit VM");
            if (sTestForPathClassLoader) {
                compile32ForPathClassLoader();
            } else {
                compile32ForDexClassLoader();
            }
            System.out.flush();
            testDexFileWithDexClassLoader(getOdexDir64());
        } else {
            System.out.println("* Running on 32-bit VM");
            if (sTestForPathClassLoader) {
                testDexFileWithPathClassLoader();
            } else {
                testDexFileWithDexClassLoader(getOdexDir32());
            }
        }
        System.out.println("* Done");
    }

    private static void compile32ForPathClassLoader() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> dexClassLoader_class = classLoader.loadClass(DEX_CLASS_LOADER_CLASS);
        Method compileFor32Bit_method =
                dexClassLoader_class.getDeclaredMethod(
                    "compileFor32Bit", File.class);
        File dexFile = new File(SECONDARY_DEX_PATH);
        compileFor32Bit_method.invoke(null, dexFile);
    }

    private static void compile32ForDexClassLoader() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> dexClassLoader_class = classLoader.loadClass(DEX_CLASS_LOADER_CLASS);
        Method compileFor32Bit_method =
                dexClassLoader_class.getDeclaredMethod(
                    "compileFor32Bit", File.class, File.class);
        File dexFile = new File(SECONDARY_DEX_PATH);
        String oatFileName = getFileNameNoExtension(dexFile) + ".dex";
        File oatFile = new File(getOdexDir32(), oatFileName);
        compileFor32Bit_method.invoke(null, dexFile, oatFile);
    }

    /*
     * Create an instance of DexClassLoader.  The test harness doesn't
     * have visibility into dalvik.system.*, so we do this through
     * reflection.
     */
    private static ClassLoader getDexClassLoader(String odexDirectory) throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> DexClassLoader = classLoader.loadClass(DEX_CLASS_LOADER_CLASS);
        Constructor<?> DexClassLoader_init = DexClassLoader.getConstructor(String.class,
                                                                           String.class,
                                                                           String.class,
                                                                           ClassLoader.class);
        // create an instance, using the path we found
        return (ClassLoader) DexClassLoader_init.newInstance(SECONDARY_DEX_PATH,
                                                             odexDirectory,
                                                             LIB_DIR,
                                                             classLoader);
    }

    private static ClassLoader getPathClassLoader() throws Exception {
        ClassLoader classLoader = Main.class.getClassLoader();
        Class<?> dexClassLoader_class = classLoader.loadClass(PATH_CLASS_LOADER_CLASS);
        Constructor<?> DexClassLoader_init = dexClassLoader_class.getConstructor(String.class,
                                                                                 String.class,
                                                                                 ClassLoader.class);
        // create an instance, using the path we found
        return (ClassLoader) DexClassLoader_init.newInstance(SECONDARY_DEX_PATH,
                                                             LIB_DIR,
                                                             classLoader);
    }

    private static void testDexFileWithPathClassLoader() throws Exception {
        ClassLoader classLoader = getPathClassLoader();
        Class<?> anotherClass = classLoader.loadClass("Another");
        Constructor<?> anotherClass_init = anotherClass.getConstructor();
        anotherClass_init.newInstance();
    }

    private static void testDexFileWithDexClassLoader(String odexDir) throws Exception {
        ClassLoader classLoader = getDexClassLoader(odexDir);
        Class<?> anotherClass = classLoader.loadClass("Another");
        Constructor<?> anotherClass_init = anotherClass.getConstructor();
        anotherClass_init.newInstance();
    }
}
