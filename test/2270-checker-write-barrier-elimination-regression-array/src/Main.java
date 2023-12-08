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

public class Main {
    public static void main(String[] args) throws Error {
        String[] arr = {"Hello", "World"};
        // When the bug was present, $noinline$testArraySets will never set the dirty bit as true
        // (which is incorrect).
        // We first run the gc + finalization to make sure the card is clean.
        System.gc();
        System.runFinalization();
        $noinline$testArraySets(arr, null, "Universe");
    }

    // arr[1] write barrier depends on arr[0]'s write barrier. It shouldn't skip arr[0] with a null
    // check.

    /// CHECK-START: java.lang.String[] Main.$noinline$testArraySets(java.lang.String[], java.lang.String, java.lang.String) disassembly (after)
    /// CHECK: ArraySet value_can_be_null:true needs_type_check:false can_trigger_gc:false write_barrier_kind:EmitBeingReliedOn
    /// CHECK: ArraySet value_can_be_null:true needs_type_check:false can_trigger_gc:false write_barrier_kind:DontEmit
    private static java.lang.String[] $noinline$testArraySets(String[] arr, String o, String o2) {
        arr[0] = o;
        arr[1] = o2;
        return arr;
    }
}
