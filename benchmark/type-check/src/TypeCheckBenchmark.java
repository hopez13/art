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

public class TypeCheckBenchmark {
    // This is a baseline to the compare the type check benchmarks against.
    public void timeCheckNull(int count) {
        for (int i = 0; i < count; ++i) {
            if (o1 == null) {
              throw new NullPointerException();
            }
        }
    }

    public void timeCheckCastLevel1ToLevel1(int count) {
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) o1;
        }
    }

    public void timeCheckCastLevel2ToLevel1(int count) {
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) o2;
        }
    }

    public void timeCheckCastLevel3ToLevel1(int count) {
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) o3;
        }
    }

    public void timeCheckCastLevel9ToLevel1(int count) {
        for (int i = 0; i < count; ++i) {
            Level1 l1 = (Level1) o9;
        }
    }

    public void timeCheckCastLevel9ToLevel2(int count) {
        for (int i = 0; i < count; ++i) {
            Level2 l2 = (Level2) o9;
        }
    }

    public void timeInstanceOfLevel1ToLevel1(int count) {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            if (o1 instanceof Level1) {
                ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel2ToLevel1(int count) {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            if (o2 instanceof Level1) {
                ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel3ToLevel1(int count) {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            if (o3 instanceof Level1) {
                ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel9ToLevel1(int count) {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            if (o9 instanceof Level1) {
                ++sum;
            }
        }
        result = sum;
    }

    public void timeInstanceOfLevel9ToLevel2(int count) {
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            if (o9 instanceof Level2) {
                ++sum;
            }
        }
        result = sum;
    }

    volatile Object o1 = new Level1();
    volatile Object o2 = new Level2();
    volatile Object o3 = new Level3();
    volatile Object o9 = new Level9();
    int result;
}

class Level1 { }
class Level2 extends Level1 { }
class Level3 extends Level2 { }
class Level4 extends Level3 { }
class Level5 extends Level4 { }
class Level6 extends Level5 { }
class Level7 extends Level6 { }
class Level8 extends Level7 { }
class Level9 extends Level8 { }
