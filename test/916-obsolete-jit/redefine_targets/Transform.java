/*
 * Copyright (C) 2020 The Android Open Source Project
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

import java.util.function.Consumer;

class Transform {
  private void Start(Consumer<String> reporter) {
    reporter.accept("Hello - private - Transformed");
  }

  private void Finish(Consumer<String> reporter) {
    reporter.accept("Goodbye - private - Transformed");
  }

  public void sayHi(Runnable r, Consumer<String> reporter) {
    reporter.accept("pre Start private method call - Transformed");
    Start(reporter);
    reporter.accept("post Start private method call - Transformed");
    r.run();
    reporter.accept("pre Finish private method call - Transformed");
    Finish(reporter);
    reporter.accept("post Finish private method call - Transformed");
  }
}
