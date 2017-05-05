public class ExMain {
  static boolean doThrow = false;

  /// CHECK-START: void ExMain.doTest() code_sinking (before)
  /// CHECK: <<Int42:i\d+>>       IntConstant 42
  /// CHECK: <<LoadClass:l\d+>>   LoadClass load_kind:BssEntry class_name:ExOther
  /// CHECK: <<Clinit:l\d+>>      ClinitCheck [<<LoadClass>>]
  /// CHECK: <<NewInstance:l\d+>> NewInstance [<<Clinit>>]
  /// CHECK:                      InstanceFieldSet [<<NewInstance>>,<<Int42>>]
  /// CHECK:                      If
  /// CHECK:                      begin_block
  /// CHECK:                      Throw

  /// CHECK-START: void ExMain.doTest() code_sinking (after)
  /// CHECK: <<Int42:i\d+>>       IntConstant 42
  /// CHECK: <<LoadClass:l\d+>>   LoadClass load_kind:BssEntry class_name:ExOther
  /// CHECK: <<Clinit:l\d+>>      ClinitCheck [<<LoadClass>>]
  /// CHECK-NOT:                  NewInstance
  /// CHECK:                      If
  /// CHECK:                      begin_block
  /// CHECK: <<Error:l\d+>>       LoadClass class_name:java.lang.Error
  /// CHECK-NOT:                  LoadClass class_name:ExOther
  /// CHECK-NOT:                  begin_block
  /// CHECK: <<NewInstance:l\d+>> NewInstance [<<Clinit>>]
  /// CHECK-NOT:                  begin_block
  /// CHECK:                      InstanceFieldSet [<<NewInstance>>,<<Int42>>]
  /// CHECK-NOT:                  begin_block
  /// CHECK:                      NewInstance [<<Error>>]
  /// CHECK:                      Throw
  public static void doTest() {
    ExOther other = new ExOther();
    other.mField = 42;

    if (doThrow) {
      throw new Error(other.$opt$noinline$toString());
    }
  }
}
