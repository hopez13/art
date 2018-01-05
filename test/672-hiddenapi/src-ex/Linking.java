import java.lang.reflect.InvocationTargetException;

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

public class Linking {
  public static boolean canAccess(String className, boolean takesParameter) throws Exception {
    try {
      Class<?> c = Class.forName(className);
      if (takesParameter) {
        c.getDeclaredMethod("access", Integer.TYPE).invoke(null, 42);
      } else {
        c.getDeclaredMethod("access").invoke(null);
      }
      return true;
    } catch (InvocationTargetException ex) {
      if (ex.getCause() instanceof IllegalAccessError) {
        return false;
      } else {
        throw ex;
      }
    }
  }
}

class LinkFieldGet {
  public static int access() {
    return new ParentClass().fieldPublic;
  }
}

class LinkFieldSet {
  public static void access(int x) {
    new ParentClass().fieldPublic = x;
  }
}

class LinkFieldGetHiddenBlacklist {
  public static int access() {
    return new ParentClass().fieldPublicHiddenBlacklist;
  }
}

class LinkFieldGetHiddenGreylist {
  public static int access() {
    return new ParentClass().fieldPublicHiddenGreylist;
  }
}

class LinkFieldSetHiddenBlacklist {
  public static void access(int x) {
    new ParentClass().fieldPublicHiddenBlacklist = x;
  }
}

class LinkFieldSetHiddenGreylist {
  public static void access(int x) {
    new ParentClass().fieldPublicHiddenGreylist = x;
  }
}

class LinkFieldGetStatic {
  public static int access() {
    return ParentClass.fieldPublicStatic;
  }
}

class LinkFieldSetStatic {
  public static void access(int x) {
    ParentClass.fieldPublicStatic = x;
  }
}

class LinkFieldGetStaticHiddenBlacklist {
  public static int access() {
    return ParentClass.fieldPublicStaticHiddenBlacklist;
  }
}

class LinkFieldGetStaticHiddenGreylist {
  public static int access() {
    return ParentClass.fieldPublicStaticHiddenGreylist;
  }
}

class LinkFieldSetStaticHiddenBlacklist {
  public static void access(int x) {
    ParentClass.fieldPublicStaticHiddenBlacklist = x;
  }
}

class LinkFieldSetStaticHiddenGreylist {
  public static void access(int x) {
    ParentClass.fieldPublicStaticHiddenGreylist = x;
  }
}

class LinkMethod {
  public static int access() {
    return new ParentClass().methodPublic();
  }
}

class LinkMethodHiddenBlacklist {
  public static int access() {
    return new ParentClass().methodPublicHiddenBlacklist();
  }
}

class LinkMethodHiddenGreylist {
  public static int access() {
    return new ParentClass().methodPublicHiddenGreylist();
  }
}

class LinkMethodStatic {
  public static int access() {
    return ParentClass.methodPublicStatic();
  }
}

class LinkMethodStaticHiddenBlacklist {
  public static int access() {
    return ParentClass.methodPublicStaticHiddenBlacklist();
  }
}

class LinkMethodStaticHiddenGreylist {
  public static int access() {
    return ParentClass.methodPublicStaticHiddenGreylist();
  }
}
