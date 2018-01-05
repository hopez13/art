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

public class ParentClass {
  public ParentClass() {}

  public static int fieldPublicStatic = 11;
  static int fieldPackageStatic = 12;
  protected static int fieldProtectedStatic = 13;
  private static int fieldPrivateStatic = 14;

  public static int fieldPublicStaticHidden = 21;
  static int fieldPackageStaticHidden = 22;
  protected static int fieldProtectedStaticHidden = 23;
  private static int fieldPrivateStaticHidden = 24;

  public int fieldPublic = 31;
  int fieldPackage = 32;
  protected int fieldProtected = 33;
  private int fieldPrivate = 34;

  public int fieldPublicHidden = 41;
  int fieldPackageHidden = 42;
  protected int fieldProtectedHidden = 43;
  private int fieldPrivateHidden = 44;

  public static int methodPublicStatic() { return 51; }
  static int methodPackageStatic() { return 52; }
  protected static int methodProtectedStatic() { return 53; }
  private static int methodPrivateStatic() { return 54; }

  public static int methodPublicStaticHidden() { return 61; }
  static int methodPackageStaticHidden() { return 62; }
  protected static int methodProtectedStaticHidden() { return 63; }
  private static int methodPrivateStaticHidden() { return 64; }

  public int methodPublic() { return 71; }
  int methodPackage() { return 72; }
  protected int methodProtected() { return 73; }
  private int methodPrivate() { return 74; }

  public int methodPublicHidden() { return 81; }
  int methodPackageHidden() { return 82; }
  protected int methodProtectedHidden() { return 83; }
  private int methodPrivateHidden() { return 84; }

  public ParentClass(int x) {}
  ParentClass(float x) {}
  protected ParentClass(long x) {}
  private ParentClass(double x) {}

  public ParentClass(int x, char y) {}
  ParentClass(float x, char y) {}
  protected ParentClass(long x, char y) {}
  private ParentClass(double x, char y) {}
}
