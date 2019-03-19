import java.util.*;

public class Main{

  static int n = 10;

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertFloatEquals(float expected, float result) {
    if (Float.compare(expected,result)!=0) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
  public static void assertDoubleEquals(double expected, double result) {
    if (Double.compare(expected,result)!=0) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

/// CHECK-START: void Main.Test1() loop_optimization (before)
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 6          loop:none
/// CHECK-DAG: <<Cst2:i\d+>> IntConstant 1          loop:none
/// CHECK-DAG: <<Inc:i\d+>>  IntConstant 2          loop:none
/// CHECK-DAG: <<Phi:i\d+>>  Phi loop:<<Loop:B\d+>>
/// CHECK-DAG: <<Mul:i\d+>>  Mul [<<Phi>>,<<Cst1>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Mul>>,<<Cst2>>] loop:<<Loop>>
/// CHECK-DAG:               Add loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi>>,<<Inc>>]  loop:<<Loop>>

//
/// CHECK-START: void Main.Test1() loop_optimization (after)
/// CHECK-DAG: <<Cst0:i\d+>> IntConstant 0           loop:none
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 6           loop:none
/// CHECK-DAG: <<Cst2:i\d+>> IntConstant 1           loop:none
/// CHECK-DAG: <<Inc1:i\d+>> IntConstant 2           loop:none
/// CHECK-DAG: <<Inc2:i\d+>> IntConstant 12          loop:none
/// CHECK-DAG: <<Mul1:i\d+>> Mul [<<Cst0>>,<<Cst1>>] loop:none
/// CHECK-DAG:               Add [<<Mul1>>,<<Cst0>>] loop:none
/// CHECK-DAG: <<Mul2:i\d+>> Mul [<<Cst0>>,<<Cst1>>] loop:none
/// CHECK-DAG:               Add [<<Mul2>>,<<Inc2>>] loop:none
/// CHECK-DAG: <<Biv:i\d+>>  Phi                     loop:<<Loop:B\d+>>
/// CHECK-DAG: <<Phi1:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Phi2:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG:               Mul [<<Phi1>>,<<Cst1>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi2>>,<<Cst2>>] loop:<<Loop>>
/// CHECK-DAG:               Add                     loop:<<Loop>>
/// CHECK-DAG:               Add [<<Biv>>,<<Inc1>>]  loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi1>>,<<Inc2>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi2>>,<<Inc2>>] loop:<<Loop>>

/// CHECK-START: void Main.Test1() dead_code_elimination$final (after)
/// CHECK-NOT:               Mul         loop:<<B\d+>>
/// CHECK-NOT:               Mul         loop:none
public static void Test1() {
  int arr[] = new int[50];
  int k =0; int j=0;
  while (k<n) {
    j = 6*k+1;
   arr[j] = arr[j]-2;
   k = k+2;
  }
}

/// CHECK-START: int Main.Test2() loop_optimization (before)
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 11           loop:none
/// CHECK-DAG: <<Cst2:i\d+>> IntConstant 5           loop:none
/// CHECK-DAG: <<Inc:i\d+>>  IntConstant 1           loop:none
/// CHECK-DAG: <<Phi1:i\d+>> Phi                     loop:<<Loop:B5>>
/// CHECK-DAG: <<Phi2:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Mul:i\d+>>  Mul [<<Phi2>>,<<Cst1>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Mul>>,<<Cst2>>]  loop:<<Loop>>
/// CHECK-DAG:               Add                     loop:<<Loop>>
/// CHECK-DAG:               Add                     loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi2>>,<<Inc>>]  loop:<<Loop>>

//
/// CHECK-START: int Main.Test2() loop_optimization (after)
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 0           loop:none
/// CHECK-DAG: <<Cst2:i\d+>> IntConstant 5           loop:none
/// CHECK-DAG: <<Inc1:i\d+>> IntConstant 1           loop:none
/// CHECK-DAG: <<Inc2:i\d+>> IntConstant 11          loop:none
/// CHECK-DAG: <<Mul1:i\d+>> Mul [<<Cst1>>,<<Inc2>>] loop:none
/// CHECK-DAG:               Add [<<Mul1>>,<<Cst1>>] loop:none
/// CHECK-DAG: <<Mul2:i\d+>> Mul [<<Cst1>>,<<Inc2>>] loop:none
/// CHECK-DAG:               Add [<<Mul2>>,<<Cst2>>]     loop:none
/// CHECK-DAG: <<Phi1:i\d+>> Phi                     loop:<<Loop:B5>>
/// CHECK-DAG: <<Phi2:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Phi3:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Phi4:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Mul:i\d+>>  Mul [<<Phi2>>,<<Inc2>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi4>>,<<Cst2>>] loop:<<Loop>>
/// CHECK-DAG:               Add                     loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi2>>,<<Inc1>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi3>>,<<Inc2>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi4>>,<<Inc2>>] loop:<<Loop>>

/// CHECK-START: int Main.Test2() dead_code_elimination$final (after)
/// CHECK-NOT:               Mul            loop:none
/// CHECK-NOT:               Mul         loop:<<B\d+>>
public static int Test2() {
 int n = 36;
 float [] radius = new float [n];
Random r = new Random();
for (int i = 0; i< n ; i++) {
  radius[i] = r.nextFloat();
}
int sum = 0;
for (int ii= 0 ; ii< n; ii++){
  int k = 11*ii;
  int m = k + 5;
  sum += radius[ii] + m;
}
 return sum;
}

/// CHECK-START: int Main.Test3() loop_optimization (before)
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 6 loop:none
/// CHECK-DAG: <<Inc:i\d+>>  IntConstant 1 loop:none
/// CHECK-DAG: <<Phi1:i\d+>> Phi           loop:<<Loop:B\d+>>
/// CHECK-DAG: <<Phi2:i\d+>> Phi           loop:<<Loop>>
/// CHECK-DAG: Shl                         loop:<<Loop>>
/// CHECK-DAG: Mul [<<Phi2>>,<<Cst1>>]     loop:<<Loop>>
/// CHECK-DAG: Add [<<Phi2>>,<<Inc>>]      loop:<<Loop>>

//
/// CHECK-START: int Main.Test3() loop_optimization (after)
/// CHECK-DAG: <<Cst1:i\d+>> IntConstant 0           loop:none
/// CHECK-DAG: <<Inc1:i\d+>> IntConstant 1           loop:none
/// CHECK-DAG: <<Inc2:i\d+>> IntConstant 6           loop:none
/// CHECK-DAG: <<Mul:i\d+>>  Mul [<<Cst1>>,<<Inc2>>] loop:none
/// CHECK-DAG:               Add <<Mul>>,<<Cst1>>]   loop:none
/// CHECK-DAG: <<Phi1:i\d+>> Phi                     loop:<<Loop:B\d+>>
/// CHECK-DAG: <<Phi2:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG: <<Phi3:i\d+>> Phi                     loop:<<Loop>>
/// CHECK-DAG:               Shl                     loop:<<Loop>>
/// CHECK-DAG:               Mul [<<Phi2>>,<<Inc2>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi2>>,<<Inc1>>] loop:<<Loop>>
/// CHECK-DAG:               Add [<<Phi3>>,<<Inc2>>] loop:<<Loop>>

/// CHECK-START: int Main.Test3() dead_code_elimination$final (after)
/// CHECK-NOT: Mul loop:<<B\d+>>
public static int Test3() {
 int k =0;
 for (int i= 0; i< n; i++){
   if (i%2 == 0) {
     k = (int) 6.14*i;
   }
   else {
    k = (int)2.69* i;
   }
 }
return k;
}

public static void main(String[] args) {

  Test1();
  assertIntEquals(7110, Test2());
  assertFloatEquals (18.0f, Test3());
}


}
