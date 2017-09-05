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

package art;

import java.lang.reflect.Method;

public class Monitors {
  public native static void setupMonitorEvents(
      Class<?> method_klass,
      Method monitor_contended_enter_event,
      Method monitor_contended_entered_event,
      Method monitor_wait_event,
      Method monitor_waited_event,
      Class<?> lock_klass,
      Thread thr);
  public native static void stopMonitorEvents();
}

