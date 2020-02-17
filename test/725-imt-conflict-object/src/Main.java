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

import java.lang.reflect.Method;

interface Itf {
  public Class<?> method1();
  public Class<?> method2();
  public Class<?> method3();
  public Class<?> method4();
  public Class<?> method5();
  public Class<?> method6();
  public Class<?> method7();
  public Class<?> method8();
  public Class<?> method9();
  public Class<?> method10();
  public Class<?> method11();
  public Class<?> method12();
  public Class<?> method13();
  public Class<?> method14();
  public Class<?> method15();
  public Class<?> method16();
  public Class<?> method17();
  public Class<?> method18();
  public Class<?> method19();
  public Class<?> method20();
  public Class<?> method21();
  public Class<?> method22();
  public Class<?> method23();
  public Class<?> method24();
  public Class<?> method25();
  public Class<?> method26();
  public Class<?> method27();
  public Class<?> method28();
  public Class<?> method29();
  public Class<?> method30();
  public Class<?> method31();
  public Class<?> method32();
  public Class<?> method33();
  public Class<?> method34();
  public Class<?> method35();
  public Class<?> method36();
  public Class<?> method37();
  public Class<?> method38();
  public Class<?> method39();
  public Class<?> method40();
  public Class<?> method41();
  public Class<?> method42();
  public Class<?> method43();
  public Class<?> method44();
  public Class<?> method45();
  public Class<?> method46();
  public Class<?> method47();
  public Class<?> method48();
  public Class<?> method49();
  public Class<?> method50();
  public Class<?> method51();
  public Class<?> method52();
  public Class<?> method53();
  public Class<?> method54();
  public Class<?> method55();
  public Class<?> method56();
  public Class<?> method57();
  public Class<?> method58();
  public Class<?> method59();
  public Class<?> method60();
  public Class<?> method61();
  public Class<?> method62();
  public Class<?> method63();
  public Class<?> method64();
  public Class<?> method65();
  public Class<?> method66();
  public Class<?> method67();
  public Class<?> method68();
  public Class<?> method69();
  public Class<?> method70();
  public Class<?> method71();
  public Class<?> method72();
  public Class<?> method73();
  public Class<?> method74();
  public Class<?> method75();
  public Class<?> method76();
  public Class<?> method77();
  public Class<?> method78();
  public Class<?> method79();
}

public class Main implements Itf {
  public static Itf main;
  public static void main(String[] args) throws Exception {
    main = new Main();
    Class<?> c = Class.forName("TestCase");
    Method m = c.getMethod("test");
    String result = (String)m.invoke(null);
    if (!"MainInstance".equals(result)) {
      throw new Error("Expected 'MainInstance', got '" + result + "'");
    }
  }

  public String toString() {
    return "MainInstance";
  }

  public Class<?> method1() { return Main.class; }
  public Class<?> method2() { return Main.class; }
  public Class<?> method3() { return Main.class; }
  public Class<?> method4() { return Main.class; }
  public Class<?> method5() { return Main.class; }
  public Class<?> method6() { return Main.class; }
  public Class<?> method7() { return Main.class; }
  public Class<?> method8() { return Main.class; }
  public Class<?> method9() { return Main.class; }
  public Class<?> method10() { return Main.class; }
  public Class<?> method11() { return Main.class; }
  public Class<?> method12() { return Main.class; }
  public Class<?> method13() { return Main.class; }
  public Class<?> method14() { return Main.class; }
  public Class<?> method15() { return Main.class; }
  public Class<?> method16() { return Main.class; }
  public Class<?> method17() { return Main.class; }
  public Class<?> method18() { return Main.class; }
  public Class<?> method19() { return Main.class; }
  public Class<?> method20() { return Main.class; }
  public Class<?> method21() { return Main.class; }
  public Class<?> method22() { return Main.class; }
  public Class<?> method23() { return Main.class; }
  public Class<?> method24() { return Main.class; }
  public Class<?> method25() { return Main.class; }
  public Class<?> method26() { return Main.class; }
  public Class<?> method27() { return Main.class; }
  public Class<?> method28() { return Main.class; }
  public Class<?> method29() { return Main.class; }
  public Class<?> method30() { return Main.class; }
  public Class<?> method31() { return Main.class; }
  public Class<?> method32() { return Main.class; }
  public Class<?> method33() { return Main.class; }
  public Class<?> method34() { return Main.class; }
  public Class<?> method35() { return Main.class; }
  public Class<?> method36() { return Main.class; }
  public Class<?> method37() { return Main.class; }
  public Class<?> method38() { return Main.class; }
  public Class<?> method39() { return Main.class; }
  public Class<?> method40() { return Main.class; }
  public Class<?> method41() { return Main.class; }
  public Class<?> method42() { return Main.class; }
  public Class<?> method43() { return Main.class; }
  public Class<?> method44() { return Main.class; }
  public Class<?> method45() { return Main.class; }
  public Class<?> method46() { return Main.class; }
  public Class<?> method47() { return Main.class; }
  public Class<?> method48() { return Main.class; }
  public Class<?> method49() { return Main.class; }
  public Class<?> method50() { return Main.class; }
  public Class<?> method51() { return Main.class; }
  public Class<?> method52() { return Main.class; }
  public Class<?> method53() { return Main.class; }
  public Class<?> method54() { return Main.class; }
  public Class<?> method55() { return Main.class; }
  public Class<?> method56() { return Main.class; }
  public Class<?> method57() { return Main.class; }
  public Class<?> method58() { return Main.class; }
  public Class<?> method59() { return Main.class; }
  public Class<?> method60() { return Main.class; }
  public Class<?> method61() { return Main.class; }
  public Class<?> method62() { return Main.class; }
  public Class<?> method63() { return Main.class; }
  public Class<?> method64() { return Main.class; }
  public Class<?> method65() { return Main.class; }
  public Class<?> method66() { return Main.class; }
  public Class<?> method67() { return Main.class; }
  public Class<?> method68() { return Main.class; }
  public Class<?> method69() { return Main.class; }
  public Class<?> method70() { return Main.class; }
  public Class<?> method71() { return Main.class; }
  public Class<?> method72() { return Main.class; }
  public Class<?> method73() { return Main.class; }
  public Class<?> method74() { return Main.class; }
  public Class<?> method75() { return Main.class; }
  public Class<?> method76() { return Main.class; }
  public Class<?> method77() { return Main.class; }
  public Class<?> method78() { return Main.class; }
  public Class<?> method79() { return Main.class; }
}
