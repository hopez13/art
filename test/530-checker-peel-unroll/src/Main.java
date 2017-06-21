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

//
// Test on loop optimizations,
// in particular loop peeling and loop unrolling on different control flows.
//
// Please note: this file is aimed to show the potential of SuperblockClone; it's a temporary one
// and will be removed from this CL. A proper checker test will be written with a CL enabling
// peeling/unrolling by default.
//
public class Main {

  // TODO: add checker tests for control flow check.

  // relatively simple loop
  void sbc11(int[] a, int x) {
    int[] b = new int[128];
    for(int i=0; i<128; i++) {
      a[x]++;
      b[i] = b[i] + 1;
    }
  }

  // nested loop
  // [[[]]]
  void sbc21(int[] a, int[] b, int x) {
    for(int k=0; k<128; k++) {
      for(int j=0; j<128; j++) {
        for(int i=0; i<128; i++) {
          b[x]++;
          a[i] = a[i] + 1;
        }
      }
    }
  }

  // nested loop:
  // [
  //   if [] else []
  // ]
  void sbc22(int[] a, int[] b, int x) {
     for(int k=0; k<128; k++) {
       if (x > 100) {
         for(int j=0; j<128; j++) {
           a[x] ++;
           b[j] = b[j] + 1;
         }
       } else {
         for(int i=0; i<128; i++) {
           b[x]++;
           a[i] = a[i] + 1;
         }
       }
     }
  }

  // nested loop:
  // [
  //   [] [] [] []
  // ]
  void sbc23(int[] a, int[] b, int[] c, int[] d, int x) {
    for(int t=0; t<128; t++) {
      for(int i=0; i<128; i++) {
        b[x]++;
        a[i] = a[i] + 1;
      }
      for(int j=0; j<128; j++) {
        a[x] ++;
        b[j] = b[j] + 1;
      }
      for(int k=0; k<128; k++) {
        d[x] ++;
        c[k] = c[k] + 1;
      }
      for(int l=0; l<128; l++) {
        c[x] ++;
        b[l] = b[l] + 1;
      }
    }
  }

  // complicated control flow
  void sbc31(boolean[] a, int x) {
    int[] b = new int[128];
    for(int i=0; i<128; i++) {
      if(a[0]) {
        if(a[1]) {
          if(a[2]) {
            if(a[3]) {
              if(a[4]) {
                if(a[5]) {
                  if(a[6]) {
                    if(a[7]) {
                      b[i] = b[i] + 1;
                    }}}}}}}
      } else {
        b[i] = b[i] - 10;
      }
    }
  }

  // complicated switch case control flow, without PackedSwitch
  void sbc41(int[] b, int x) {
    for(int k=0; k<128; k++) {
      for(int j=0; j<128; j++) {
        for(int i=0; i<128; i++) {
          switch(x) {
          case 1:
            b[i] = b[i] + 10;
            break;
          case 2:
            b[i] = b[i] + 20;
            break;
          case 3:
            b[i] = b[i] + 20;
            break;
          default:
            b[x] = 10;
            break;
          }
        }
      }
    }
  }

  // switch-case with PackedSwitch generated
  void sbc42(int[] b, int x) {
    for(int i=0; i<128; i++) {
      switch(x) {
      case 1:
        b[i] = b[i] + 10;
        break;
      case 2:
        b[i] = b[i] + 20;
        break;
      case 3:
        b[i] = b[i] + 30;
        break;
      case 4:
        b[i] = b[i] + 40;
        break;
      case 5:
        b[i] = b[i] + 50;
        break;
      case 6:
        b[i] = b[i] + 60;
        break;
      case 7:
        b[i] = b[i] + 70;
        break;
      default:
        b[i]++;
        break;
      }
    }
  }

  // complicated switch case control flow,
  // with complicated control flow in some case block;
  // without PackedSwitch.
  void sbc43(boolean[] a, int[] b, int x) {
    for(int k=0; k<128; k++) {
      for(int j=0; j<128; j++) {
        for(int i=0; i<128; i++) {
          switch(x) {
          case 1:
            if(a[0]) {
              if(a[1]) {
                if(a[2]) {
                  b[i]++;
                }}}
            break;
          case 2:
            if(!a[0]) {
              if(!a[1]) {
                if(!a[2]) {
                  b[i]--;
                }}}
            break;
          case 3:
            b[i] = b[i] + 20;
            break;
          default:
            b[x] = 10;
            break;
          }
        }
      }
    }
  }

  private static void cf_example(int[] a, int[] b) {
    for (int i = 3; i < 128; i++) {
      int s = a[i] += b[i];
      if (i % 4 == 1) {
        s += 2;
        continue;
      }
      if (a[i] == b[i]/2) {
        b[i]= a[i]+ s;
      }
      a[i]++;
    }
  }

  private static void invariant1(int[] a) {
    boolean flag = true;
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (!flag) {
        a[i] += 1;
      }
      flag = false;
    }
  }

  private static void invariant2(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n && n < 0) {
        a[i] += 1;
      }
    }
  }

  private static void invariant3(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n) {
        break;
      }
    }
  }

  private static void invariant3_return(int[] a, int n, int m) {
    for (int i = 1; i < 128; i++) {
      a[i] += 4;
      if (m < n) {
        return;
      }
    }
  }

  private static void invariant3_nested(int[] a, int[] b, int n, int m) {
    for (int j = 1; j < 5; j++) {
      for (int i = 1; i < 128; i++) {
        a[i] += 4;
        if (m < n) {
          break;
        }
      }

      for (int i = 1; i < 128; i++) {
        a[i] -= 4;
        if (m < n) {
          break;
        }
      }
    }
  }

  public static boolean foo(int a) {
    return Math.min(a,4) << a < 4;
  }

  private static short break_outer(int[] a) {
    outer:
    for (int i = 1; i < 128; i++) {
      for (int j = 0; j < 128; j++) {
        if (foo(i+j)) {
          break outer;
        }
        a[j]=i;
      }
     a[i]=i;
    }
    return 0;
  }

  public static void main(String[] args) {
    System.out.println("passed");
  }
}
