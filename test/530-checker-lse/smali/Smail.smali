.class public LSmali;
.super Ljava/lang/Object;

##  CHECK-START: int Smali.test4(TestClass, boolean) load_store_elimination (before)
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: Return
##  CHECK: InstanceFieldSet

##  CHECK-START: int Smali.test4(TestClass, boolean) load_store_elimination (after)
##  CHECK: InstanceFieldSet
##  CHECK-NOT: NullCheck
##  CHECK-NOT: InstanceFieldGet
##  CHECK: Return
##  CHECK: InstanceFieldSet

#   Set and merge the same value in two branches.
#   Note: Dexer such as D8 can perform valid LSE as well so we must write these
#         tests in Smali.
.method public static test4(LTestClass;Z)I
    # if (b) {
    #   obj.i = 1;
    # } else {
    #   obj.i = 1;
    # }
    # return obj.i;

    .registers 3

    .prologue
    const/4 v0, 0x1

    .line 184
    if-eqz p1, :cond_8

    .line 185
    iput v0, p0, LTestClass;->i:I

    .line 189
    :goto_5
    iget v0, p0, LTestClass;->i:I

    return v0

    .line 187
    :cond_8
    iput v0, p0, LTestClass;->i:I

    goto :goto_5
.end method

##  CHECK-START: int Smali.test5(TestClass, boolean) load_store_elimination (before)
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: Return
##  CHECK: InstanceFieldSet

##  CHECK-START: int Smali.test5(TestClass, boolean) load_store_elimination (after)
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: Return
##  CHECK: InstanceFieldSet

#   Set and merge different values in two branches.
#   Note: Dexer such as D8 can perform valid LSE as well so we must write these
#         tests in Smali.
.method public static test5(LTestClass;Z)I
    # if (b) {
    #   obj.i = 1;
    # } else {
    #   obj.i = 2;
    # }
    # return obj.i;

    .registers 3
    .param p0, "obj"    # LTestClass;
    .param p1, "b"    # Z

    .prologue
    .line 206
    if-eqz p1, :cond_8

    .line 207
    const/4 v0, 0x1

    iput v0, p0, LTestClass;->i:I

    .line 211
    :goto_5
    iget v0, p0, LTestClass;->i:I

    return v0

    .line 209
    :cond_8
    const/4 v0, 0x2

    iput v0, p0, LTestClass;->i:I

    goto :goto_5
.end method

##  CHECK-START: int Smali.test23(boolean) load_store_elimination (before)
##  CHECK: NewInstance
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: Return
##  CHECK: InstanceFieldGet
##  CHECK: InstanceFieldSet

##  CHECK-START: int Smali.test23(boolean) load_store_elimination (after)
##  CHECK: NewInstance
##  CHECK-NOT: InstanceFieldSet
##  CHECK-NOT: InstanceFieldGet
##  CHECK: InstanceFieldSet
##  CHECK: InstanceFieldGet
##  CHECK: Return
##  CHECK-NOT: InstanceFieldGet
##  CHECK: InstanceFieldSet

#   Test store elimination on merging.
#   Note: Dexer such as D8 can perform valid LSE as well so we must write these
#         tests in Smali.
.method public static test23(Z)I
    # TestClass obj = new TestClass();
    # obj.i = 3;      // This store can be eliminated since the value flows into each branch.
    # if (b) {
    #   obj.i += 1;   // This store cannot be eliminated due to the merge later.
    # } else {
    #   obj.i += 2;   // This store cannot be eliminated due to the merge later.
    # }
    # return obj.i;

    .registers 3
    .param p0, "b"    # Z

    .prologue
    .line 582
    new-instance v0, LTestClass;

    invoke-direct {v0}, LTestClass;-><init>()V

    .line 583
    .local v0, "obj":LTestClass;
    const/4 v1, 0x3

    iput v1, v0, LTestClass;->i:I

    .line 584
    if-eqz p0, :cond_13

    .line 585
    iget v1, v0, LTestClass;->i:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, LTestClass;->i:I

    .line 589
    :goto_10
    iget v1, v0, LTestClass;->i:I

    return v1

    .line 587
    :cond_13
    iget v1, v0, LTestClass;->i:I

    add-int/lit8 v1, v1, 0x2

    iput v1, v0, LTestClass;->i:I

    goto :goto_10
.end method
