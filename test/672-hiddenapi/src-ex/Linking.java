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

class LinkFieldGetHidden {
  public static int access() {
    return new ParentClass().fieldPublicHidden;
  }
}

class LinkFieldSetHidden {
  public static void access(int x) {
    new ParentClass().fieldPublicHidden = x;
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

class LinkFieldGetStaticHidden {
  public static int access() {
    return ParentClass.fieldPublicStaticHidden;
  }
}

class LinkFieldSetStaticHidden {
  public static void access(int x) {
    ParentClass.fieldPublicStaticHidden = x;
  }
}

class LinkMethod {
  public static int access() {
    return new ParentClass().methodPublic();
  }
}

class LinkMethodHidden {
  public static int access() {
    return new ParentClass().methodPublicHidden();
  }
}

class LinkMethodStatic {
  public static int access() {
    return ParentClass.methodPublicStatic();
  }
}

class LinkMethodStaticHidden {
  public static int access() {
    return ParentClass.methodPublicStaticHidden();
  }
}
