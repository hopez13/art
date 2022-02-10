/*
 * Copyright (C) 2022 The Android Open Source Project
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

// Check that we have only one call before the inliner.

/// CHECK-START: boolean BaseClassWrapper.recursiveTwoWrappers() inliner (before)
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
/// CHECK-NOT:   InvokeVirtual method_name:BaseClass.recursiveTwoWrappers

// After the inliner we will have
// If (receiver == BaseClassAnotherWrapper) {
//   // At this point we have no way to know that the Invoke was going
//  // to be a recursive one, but it cuts at the first iteration.
//   HInvokeVirtual
// } else {
//   HInvokeVirtual
// }

/// CHECK-START: boolean BaseClassWrapper.recursiveTwoWrappers() inliner (after)
/// CHECK:       LoadClass load_kind:BssEntry class_name:BaseClassAnotherWrapper
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
class BaseClassWrapper extends BaseClass {
  protected final BaseClass mBaseClass;

  public BaseClassWrapper(BaseClass BaseClass) {
    mBaseClass = BaseClass;
  }

  boolean recursiveTwoWrappers() {
    return mBaseClass.recursiveTwoWrappers();
  }
}

// Same thing here but swapping BaseClassWrapper and BaseClassAnotherWrapper.

/// CHECK-START: boolean BaseClassAnotherWrapper.recursiveTwoWrappers() inliner (before)
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
/// CHECK-NOT:   InvokeVirtual method_name:BaseClass.recursiveTwoWrappers

/// CHECK-START: boolean BaseClassAnotherWrapper.recursiveTwoWrappers() inliner (after)
/// CHECK:       LoadClass load_kind:BssEntry class_name:BaseClassWrapper
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
/// CHECK:       InvokeVirtual method_name:BaseClass.recursiveTwoWrappers
class BaseClassAnotherWrapper extends BaseClass {
  protected final BaseClass mBaseClass;

  public BaseClassAnotherWrapper(BaseClass BaseClass) {
    mBaseClass = BaseClass;
  }

  boolean recursiveTwoWrappers() {
    return mBaseClass.recursiveTwoWrappers();
  }
}

// Check that we have only one call before the inliner.

/// CHECK-START: boolean BaseClassShallowWrapper.recursiveShallow() inliner (before)
/// CHECK:       InvokeVirtual method_name:BaseClassShallow.recursiveShallow
/// CHECK-NOT:   InvokeVirtual method_name:BaseClassShallow.recursiveShallow

// After the inliner we will have
// If (receiver == BaseClassShallowNoRecursion) {
//  return false;
// } else {
//   HInvokeVirtual
// }

/// CHECK-START: boolean BaseClassShallowWrapper.recursiveShallow() inliner (after)
/// CHECK:       LoadClass load_kind:BssEntry class_name:BaseClassShallowNoRecursion
/// CHECK:       InvokeVirtual method_name:BaseClassShallow.recursiveShallow
/// CHECK-NOT:   InvokeVirtual method_name:BaseClassShallow.recursiveShallow
class BaseClassShallowWrapper extends BaseClassShallow {
  protected final BaseClassShallow mBaseClassShallow;

  public BaseClassShallowWrapper(BaseClassShallow BaseClassShallow) {
    mBaseClassShallow = BaseClassShallow;
  }

  boolean recursiveShallow() {
    return mBaseClassShallow.recursiveShallow();
  }
}

class BaseClassShallowNoRecursion extends BaseClassShallow {
  protected final BaseClassShallow mBaseClassShallow;

  public BaseClassShallowNoRecursion(BaseClassShallow BaseClassShallow) {
    mBaseClassShallow = BaseClassShallow;
  }

  boolean recursiveShallow() {
    return false;
  }
}

public class Main {
  public static void main(String[] args) {}
}