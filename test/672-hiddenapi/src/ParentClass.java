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

  public static int fieldPublicStaticHiddenBlacklist = 21;
  static int fieldPackageStaticHiddenBlacklist = 22;
  protected static int fieldProtectedStaticHiddenBlacklist = 23;
  private static int fieldPrivateStaticHiddenBlacklist = 24;

  public static int fieldPublicStaticHiddenGreylist = 31;
  static int fieldPackageStaticHiddenGreylist = 32;
  protected static int fieldProtectedStaticHiddenGreylist = 33;
  private static int fieldPrivateStaticHiddenGreylist = 34;

  public int fieldPublic = 41;
  int fieldPackage = 42;
  protected int fieldProtected = 43;
  private int fieldPrivate = 44;

  public int fieldPublicHiddenBlacklist = 51;
  int fieldPackageHiddenBlacklist = 52;
  protected int fieldProtectedHiddenBlacklist = 53;
  private int fieldPrivateHiddenBlacklist = 54;

  public int fieldPublicHiddenGreylist = 61;
  int fieldPackageHiddenGreylist = 62;
  protected int fieldProtectedHiddenGreylist = 63;
  private int fieldPrivateHiddenGreylist = 64;

  public static int methodPublicStatic() { return 71; }
  static int methodPackageStatic() { return 72; }
  protected static int methodProtectedStatic() { return 73; }
  private static int methodPrivateStatic() { return 74; }

  public static int methodPublicStaticHiddenBlacklist() { return 81; }
  static int methodPackageStaticHiddenBlacklist() { return 82; }
  protected static int methodProtectedStaticHiddenBlacklist() { return 83; }
  private static int methodPrivateStaticHiddenBlacklist() { return 84; }

  public static int methodPublicStaticHiddenGreylist() { return 91; }
  static int methodPackageStaticHiddenGreylist() { return 92; }
  protected static int methodProtectedStaticHiddenGreylist() { return 93; }
  private static int methodPrivateStaticHiddenGreylist() { return 94; }

  public int methodPublic() { return 101; }
  int methodPackage() { return 102; }
  protected int methodProtected() { return 103; }
  private int methodPrivate() { return 104; }

  public int methodPublicHiddenBlacklist() { return 111; }
  int methodPackageHiddenBlacklist() { return 112; }
  protected int methodProtectedHiddenBlacklist() { return 113; }
  private int methodPrivateHiddenBlacklist() { return 114; }

  public int methodPublicHiddenGreylist() { return 121; }
  int methodPackageHiddenGreylist() { return 122; }
  protected int methodProtectedHiddenGreylist() { return 123; }
  private int methodPrivateHiddenGreylist() { return 124; }

  public ParentClass(int x) {}
  ParentClass(float x) {}
  protected ParentClass(long x) {}
  private ParentClass(double x) {}

  // Blacklist
  public ParentClass(int x, char y) {}
  ParentClass(float x, char y) {}
  protected ParentClass(long x, char y) {}
  private ParentClass(double x, char y) {}

  // Greylist
  public ParentClass(int x, boolean y) {}
  ParentClass(float x, boolean y) {}
  protected ParentClass(long x, boolean y) {}
  private ParentClass(double x, boolean y) {}
}
