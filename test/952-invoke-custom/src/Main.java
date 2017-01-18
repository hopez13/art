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

import dalvik.system.InMemoryDexClassLoader;

import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.Base64;

// This test is a stop-gap until we have support for generating invoke-custom
// in the Android tree.

public class Main {
    private static final String INVOKE_CUSTOM_DEX_FILE =
"ZGV4CjAzOAByaZGhS3Q1OTCOP7OA5xBf4sdQQT3IuCLYBwAAcAAAAHhWNBIAAAAAAAAAAPAGAAAs" +
"AAAAcAAAABQAAAAgAQAACAAAAHABAAADAAAA0AEAAAkAAADoAQAAAQAAADQCAAB8BQAAXAIAAEoD" +
"AABSAwAAVQMAAFoDAABpAwAAbAMAAHIDAACnAwAA2gMAAAsEAABABAAAXAQAAHMEAACGBAAAmgQA" +
"AK4EAADCBAAA2QQAAPYEAAAbBQAAPAUAAGUFAACHBQAApgUAALIFAAC1BQAAuQUAAL0FAADSBQAA" +
"1wUAAOYFAAD9BQAADAYAABsGAAAnBgAAOwYAAEEGAABPBgAAVwYAAF0GAABjBgAAaAYAAHEGAAB9" +
"BgAAAQAAAAYAAAAHAAAACAAAAAkAAAAKAAAACwAAAAwAAAANAAAADgAAAA8AAAAQAAAAEQAAABIA" +
"AAATAAAAFAAAABUAAAAWAAAAGAAAABsAAAACAAAAAAAAABQDAAAFAAAADAAAABwDAAAFAAAADgAA" +
"ACgDAAAEAAAADwAAAAAAAAAYAAAAEgAAAAAAAAAZAAAAEgAAADQDAAAaAAAAEgAAADwDAAAaAAAA" +
"EgAAAEQDAAADAAMAAwAAAAQADAAgAAAACgAGACgAAAAEAAQAAAAAAAQAAAAcAAAABAABACQAAAAE" +
"AAcAJgAAAAYABQApAAAACAAEAAAAAAANAAYAAAAAAA8AAgAhAAAAEAADACUAAADQBgAABAAAAAEA" +
"AAAIAAAAAAAAABcAAAD0AgAA1wYAAAAAAAAEAAAAAgAAAAEAAACgBgAAAQAAAMgGAAABAAEAAQAA" +
"AIQGAAAEAAAAcBAFAAAADgADAAIAAAAAAIkGAAADAAAAkAABAg8AAAAFAAMABAAAAJAGAAAQAAAA" +
"cQAIAAAADAAcAQQAbkAHABBDDAAiAQ0AcCAGAAEAEQEEAAEAAgAAAJkGAAAMAAAAYgACABIhEjL8" +
"IAAAIQAKAW4gBAAQAA4AAAAAAAAAAAACAAAAAAAAAAEAAABcAgAAAgAAAGQCAAACAAAAAAAAAAMA" +
"AAAPAAkAEQAAAAMAAAAHAAkAEQAAAAEAAAAAAAAAAQAAAA4AAAABAAAAEwAGPGluaXQ+AAFJAANJ" +
"SUkADUlOVk9LRV9TVEFUSUMAAUwABExMTEwAM0xjb20vYW5kcm9pZC9qYWNrL2Fubm90YXRpb25z" +
"L0NhbGxlZEJ5SW52b2tlQ3VzdG9tOwAxTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlvbnMvTGlu" +
"a2VyTWV0aG9kSGFuZGxlOwAvTGNvbS9hbmRyb2lkL2phY2svYW5ub3RhdGlvbnMvTWV0aG9kSGFu" +
"ZGxlS2luZDsAM0xjb20vYW5kcm9pZC9qYWNrL2phdmE3L2ludm9rZWN1c3RvbS90ZXN0MDAxL1Rl" +
"c3RzOwAaTGRhbHZpay9hbm5vdGF0aW9uL1Rocm93czsAFUxqYXZhL2lvL1ByaW50U3RyZWFtOwAR" +
"TGphdmEvbGFuZy9DbGFzczsAEkxqYXZhL2xhbmcvT2JqZWN0OwASTGphdmEvbGFuZy9TdHJpbmc7" +
"ABJMamF2YS9sYW5nL1N5c3RlbTsAFUxqYXZhL2xhbmcvVGhyb3dhYmxlOwAbTGphdmEvbGFuZy9p" +
"bnZva2UvQ2FsbFNpdGU7ACNMamF2YS9sYW5nL2ludm9rZS9Db25zdGFudENhbGxTaXRlOwAfTGph" +
"dmEvbGFuZy9pbnZva2UvTWV0aG9kSGFuZGxlOwAnTGphdmEvbGFuZy9pbnZva2UvTWV0aG9kSGFu" +
"ZGxlcyRMb29rdXA7ACBMamF2YS9sYW5nL2ludm9rZS9NZXRob2RIYW5kbGVzOwAdTGphdmEvbGFu" +
"Zy9pbnZva2UvTWV0aG9kVHlwZTsAClRlc3RzLmphdmEAAVYAAlZJAAJWTAATW0xqYXZhL2xhbmcv" +
"U3RyaW5nOwADYWRkAA1hcmd1bWVudFR5cGVzABVlbWl0dGVyOiBqYWNrLTQuMC1lbmcADWVuY2xv" +
"c2luZ1R5cGUADWZpZWxkQ2FsbFNpdGUACmZpbmRTdGF0aWMAEmludm9rZU1ldGhvZEhhbmRsZQAE" +
"a2luZAAMbGlua2VyTWV0aG9kAAZsb29rdXAABG1haW4ABG5hbWUAA291dAAHcHJpbnRsbgAKcmV0" +
"dXJuVHlwZQAFdmFsdWUAHgAHDgArAgAABw4AMQMAAAAHDqUANgEABw60AAABBB0cAhgAGAAiHAEd" +
"AgQdHAMYDxgJGBEfGAQjGwAnFyQnFxwqGAACBQErHAEYCwMWABccFQABAAQAAQkAgYAE7AQBCoQF" +
"AQqcBQEJzAUAEwAAAAAAAAABAAAAAAAAAAEAAAAsAAAAcAAAAAIAAAAUAAAAIAEAAAMAAAAIAAAA" +
"cAEAAAQAAAADAAAA0AEAAAUAAAAJAAAA6AEAAAcAAAABAAAAMAIAAAYAAAABAAAANAIAAAgAAAAB" +
"AAAAVAIAAAMQAAACAAAAXAIAAAEgAAAEAAAAbAIAAAYgAAABAAAA9AIAAAEQAAAGAAAAFAMAAAIg" +
"AAAsAAAASgMAAAMgAAAEAAAAhAYAAAQgAAACAAAAoAYAAAUgAAABAAAA0AYAAAAgAAABAAAA1wYA" +
"AAAQAAABAAAA8AYAAA==";

    public static void main(String[] args) throws Throwable {
        byte[] base64Data = INVOKE_CUSTOM_DEX_FILE.getBytes();
        Base64.Decoder decoder = Base64.getDecoder();
        ByteBuffer dexBuffer = ByteBuffer.wrap(decoder.decode(base64Data));

        InMemoryDexClassLoader classLoader =
                new InMemoryDexClassLoader(dexBuffer,
                                           ClassLoader.getSystemClassLoader());
        Class<?> testClass =
                classLoader.loadClass("com.android.jack.java7.invokecustom.test001.Tests");
        Method testMethod = testClass.getDeclaredMethod("main", String[].class);
        Object[] testArgs = new String[] { "A", "B" };
        testMethod.invoke(null, new Object[] { testArgs });
    }
}