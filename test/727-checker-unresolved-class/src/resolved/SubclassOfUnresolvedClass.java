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

package resolved;

import unresolved.UnresolvedClass;

public class SubclassOfUnresolvedClass extends UnresolvedClass {
  public static void $noinline$main() {
    $noinline$testPublicFieldInResolvedPackagePrivateClass();
    $noinline$testPublicFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass();
    $noinline$testPrivateFieldInResolvedPackagePrivateClass();
    $noinline$testPrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass();
    $noinline$testPackagePrivateFieldInResolvedPackagePrivateClass();
    $noinline$testPackagePrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass();

    $noinline$testPublicMethodInResolvedPackagePrivateClass();
    $noinline$testPublicMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass();
    $noinline$testPrivateMethodInResolvedPackagePrivateClass();
    $noinline$testPrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass();
    $noinline$testPackagePrivateMethodInResolvedPackagePrivateClass();
    $noinline$testPackagePrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass();

    System.out.println("SubclassOfUnresolvedClass passed");
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static void $noinline$testPublicFieldInResolvedPackagePrivateClass() {
    // TODO: Use StaticFieldSet.
    ResolvedPackagePrivateClass.publicIntField = 42;
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static void $noinline$testPublicFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    // TODO: Use StaticFieldSet.
    ResolvedPublicSubclassOfPackagePrivateClass.publicIntField = 42;
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static void $noinline$testPrivateFieldInResolvedPackagePrivateClass() {
    try {
      ResolvedPackagePrivateClass.privateIntField = 42;
      throw new Error("Unreachable");
    } catch (IllegalAccessError expected) {}
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static void $noinline$testPrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    try {
      ResolvedPublicSubclassOfPackagePrivateClass.privateIntField = 42;
      throw new Error("Unreachable");
    } catch (IllegalAccessError expected) {}
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateFieldInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static void $noinline$testPackagePrivateFieldInResolvedPackagePrivateClass() {
    // TODO: Use StaticFieldSet.
    ResolvedPackagePrivateClass.intField = 42;
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: UnresolvedStaticFieldSet

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: StaticFieldSet
  static
  void $noinline$testPackagePrivateFieldInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    // TODO: Use StaticFieldSet.
    ResolvedPublicSubclassOfPackagePrivateClass.intField = 42;
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$publicStaticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: InvokeUnresolved method_name:{{[^$]*}}$noinline$publicStaticMethod
  static void $noinline$testPublicMethodInResolvedPackagePrivateClass() {
    ResolvedPackagePrivateClass.$noinline$publicStaticMethod();
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$publicStaticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPublicMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: InvokeUnresolved method_name:{{[^$]*}}$noinline$publicStaticMethod
  static void $noinline$testPublicMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    ResolvedPublicSubclassOfPackagePrivateClass.$noinline$publicStaticMethod();
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK: InvokeUnresolved method_name:{{[^$]*}}$noinline$privateStaticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$privateStaticMethod
  static void $noinline$testPrivateMethodInResolvedPackagePrivateClass() {
    try {
      ResolvedPackagePrivateClass.$noinline$privateStaticMethod();
      throw new Error("Unreachable");
    } catch (IllegalAccessError expected) {}
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: InvokeUnresolved method_name:{{[^$]*}}$noinline$privateStaticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$privateStaticMethod
  static void $noinline$testPrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    try {
      ResolvedPublicSubclassOfPackagePrivateClass.$noinline$privateStaticMethod();
      throw new Error("Unreachable");
    } catch (IllegalAccessError expected) {}
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$staticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateMethodInResolvedPackagePrivateClass() builder (after)
  /// CHECK-NOT: InvokeUnresolved method_name:{{[^$]*}}$noinline$staticMethod
  static void $noinline$testPackagePrivateMethodInResolvedPackagePrivateClass() {
    ResolvedPackagePrivateClass.$noinline$staticMethod();
  }

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK: InvokeStaticOrDirect method_name:{{[^$]*}}$noinline$staticMethod

  /// CHECK-START: void resolved.SubclassOfUnresolvedClass.$noinline$testPackagePrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() builder (after)
  /// CHECK-NOT: InvokeUnresolved method_name:{{[^$]*}}$noinline$staticMethod
  static
  void $noinline$testPackagePrivateMethodInPackagePrivateClassReferencedViaResolvedPublicSubclass() {
    ResolvedPublicSubclassOfPackagePrivateClass.$noinline$staticMethod();
  }
}
