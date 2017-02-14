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

class SubA {
  int getValue() { return 42; }
}

class SubB {
  int getValue() { return 38; }
}

public class Main {

  public static int inlineMonomorphicSubA(Super a) {
    return a.getValue();
  }

  public static int inlinePolymophicSubASubB(Super a) {
    return a.getValue();
  }

  public static int inlinePolymophicCrossDexSubASubC(Super a) {
    return a.getValue();
  }

  public static int inlineMegamorphic(Super a) {
    return a.getValue();
  }

  public static int noInlineCache(Super a) {
    return a.getValue();
  }

  public void testInlineMonomorphic() {
    if (inlineMonomorphicSubA(new SubA()) != 42) {
      throw new Error("Expected 42");
    }

    // Call with a different type than the one from the inline cache.
    if (inlineMonomorphicSubA(new SubB()) != 38) {
      throw new Error("Expected 38");
    }
  }

  public void testInlinePolymorhic() {
    if (inlinePolymophicSubASubB(new SubA()) != 42) {
      throw new Error("Expected 42");
    }

    if (inlinePolymophicSubASubB(new SubB()) != 38) {
      throw new Error("Expected 38");
    }

    // Call with a different type than the one from the inline cache.
    if (inlinePolymophicSubASubB(new SubC()) != 24) {
      throw new Error("Expected 25");
    }

    if (inlinePolymophicCrossDexSubASubC(new SubA()) != 42) {
      throw new Error("Expected 42");
    }

    if (inlinePolymophicCrossDexSubASubC(new SubC()) != 38) {
      throw new Error("Expected 38");
    }

    // Call with a different type than the one from the inline cache.
    if (inlinePolymophicCrossDexSubASubC(new SubB()) != 24) {
      throw new Error("Expected 25");
    }
  }

  public void testInlineMegamorphic() {
    if (inlineMegamorphic(new SubA()) != 42) {
      throw new Error("Expected 42");
    }
  }

  public void testNoInlineCache() {
    if (noInlineCache(new SubA()) != 42) {
      throw new Error("Expected 42");
    }
  }

  public static void main(String[] args) {
    testInlineMonomorphic();
    testInlinePolymorhic();
    testInlineMegamorphic();
    testNoInlineCache();
  }

}
