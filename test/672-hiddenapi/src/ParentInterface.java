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

public interface ParentInterface {
  static int fieldPublicStatic = 1;
  static int fieldPublicStaticHiddenBlacklist = 2;
  static int fieldPublicStaticHiddenGreylist = 3;

  static int methodPublicStatic() { return 4; }
  static int methodPublicStaticHiddenBlacklist() { return 5; }
  static int methodPublicStaticHiddenGreylist() { return 6; }
  int methodPublic();
  int methodPublicHiddenBlacklist();
  int methodPublicHiddenGreylist();
  default int methodDefaultPublic() { return 7; }
  default int methodDefaultPublicHiddenBlacklist() { return 8; }
  default int methodDefaultPublicHiddenGreylist() { return 9; }
}
