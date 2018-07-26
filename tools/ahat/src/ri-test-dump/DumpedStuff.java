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

import java.lang.ref.WeakReference;

// We take a heap dump that includes a single instance of this
// DumpedStuff class. Objects stored as fields in this class can be easily
// found in the hprof dump by searching for the instance of the DumpedStuff
// class and reading the desired field.
public class DumpedStuff {
  public static class Finalizable {
    static int count = 0;

    public Finalizable() {
      count++;
    }

    @Override
    protected void finalize() {
      count--;
    }
  }

  public WeakReference aWeakRefToFinalizable = new WeakReference(new Finalizable());
}
