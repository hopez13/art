.class public LMain;
.super Ljava/lang/Object;
.source "Main.java"


# annotations
.annotation system Ldalvik/annotation/MemberClasses;
    value = {
        LMain$SubMain;,
        LMain$Itf;
    }
.end annotation


# static fields
.field static final LENGTH:I = 0x1000

.field private static final LENGTH_A:I = 0x1000

.field private static final LENGTH_B:I = 0x10

.field private static final RESULT_POS:I = 0x4


# instance fields
.field a:[I

.field b:[I

.field mA:[[D

.field mB:[[D

.field mC:[[D


# direct methods
.method public constructor <init>()V
    .registers 6

    .line 33
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 22
    const/16 v0, 0x1000

    new-array v1, v0, [I

    iput-object v1, p0, LMain;->a:[I

    .line 23
    new-array v1, v0, [I

    iput-object v1, p0, LMain;->b:[I

    .line 34
    new-array v1, v0, [[D

    iput-object v1, p0, LMain;->mA:[[D

    .line 35
    const/16 v1, 0x10

    new-array v2, v1, [[D

    iput-object v2, p0, LMain;->mB:[[D

    .line 36
    new-array v2, v1, [[D

    iput-object v2, p0, LMain;->mC:[[D

    .line 37
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_1c
    if-ge v2, v0, :cond_27

    .line 38
    iget-object v3, p0, LMain;->mA:[[D

    new-array v4, v1, [D

    aput-object v4, v3, v2

    .line 37
    add-int/lit8 v2, v2, 0x1

    goto :goto_1c

    .line 41
    .end local v2    # "i":I
    :cond_27
    const/4 v2, 0x0

    .restart local v2    # "i":I
    :goto_28
    if-ge v2, v1, :cond_39

    .line 42
    iget-object v3, p0, LMain;->mB:[[D

    new-array v4, v0, [D

    aput-object v4, v3, v2

    .line 43
    iget-object v3, p0, LMain;->mC:[[D

    new-array v4, v1, [D

    aput-object v4, v3, v2

    .line 41
    add-int/lit8 v2, v2, 0x1

    goto :goto_28

    .line 45
    .end local v2    # "i":I
    :cond_39
    return-void
.end method

.method private static expectEquals(II)V
    .registers 5
    .param p0, "expected"    # I
    .param p1, "result"    # I

    .line 1111
    if-ne p0, p1, :cond_3

    .line 1114
    return-void

    .line 1112
    :cond_3
    new-instance v0, Ljava/lang/Error;

    new-instance v1, Ljava/lang/StringBuilder;

    invoke-direct {v1}, Ljava/lang/StringBuilder;-><init>()V

    const-string v2, "Expected: "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v1, p0}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    const-string v2, ", found: "

    invoke-virtual {v1, v2}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v1, p1}, Ljava/lang/StringBuilder;->append(I)Ljava/lang/StringBuilder;

    invoke-virtual {v1}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v1

    invoke-direct {v0, v1}, Ljava/lang/Error;-><init>(Ljava/lang/String;)V

    throw v0
.end method

.method private static final initDoubleArray([D)V
    .registers 4
    .param p0, "a"    # [D

    .line 62
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    array-length v1, p0

    if-ge v0, v1, :cond_c

    .line 63
    rem-int/lit8 v1, v0, 0x4

    int-to-double v1, v1

    aput-wide v1, p0, v0

    .line 62
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 65
    .end local v0    # "i":I
    :cond_c
    return-void
.end method

.method private static final initIntArray([I)V
    .registers 3
    .param p0, "a"    # [I

    .line 56
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    array-length v1, p0

    if-ge v0, v1, :cond_b

    .line 57
    rem-int/lit8 v1, v0, 0x4

    aput v1, p0, v0

    .line 56
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 59
    .end local v0    # "i":I
    :cond_b
    return-void
.end method

.method private static final initMatrix([[D)V
    .registers 6
    .param p0, "m"    # [[D

    .line 48
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    array-length v1, p0

    if-ge v0, v1, :cond_1a

    .line 49
    const/4 v1, 0x0

    .local v1, "j":I
    :goto_5
    aget-object v2, p0, v0

    array-length v2, v2

    if-ge v1, v2, :cond_17

    .line 50
    aget-object v2, p0, v0

    mul-int/lit16 v3, v0, 0x1000

    add-int/lit8 v4, v1, 0x1

    div-int/2addr v3, v4

    int-to-double v3, v3

    aput-wide v3, v2, v1

    .line 49
    add-int/lit8 v1, v1, 0x1

    goto :goto_5

    .line 48
    .end local v1    # "j":I
    :cond_17
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 53
    .end local v0    # "i":I
    :cond_1a
    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .registers 4
    .param p0, "args"    # [Ljava/lang/String;

    .line 1192
    new-instance v0, LMain;

    invoke-direct {v0}, LMain;-><init>()V

    .line 1194
    .local v0, "obj":LMain;
    invoke-virtual {v0}, LMain;->verifyUnrolling()V

    .line 1195
    invoke-virtual {v0}, LMain;->verifyPeeling()V

    .line 1197
    sget-object v1, Ljava/lang/System;->out:Ljava/io/PrintStream;

    const-string v2, "passed"

    invoke-virtual {v1, v2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 1198
    return-void
.end method

.method private static final noUnrollingNotKnownTripCount([II)V
    .registers 5
    .param p0, "a"    # [I
    .param p1, "n"    # I

    .line 846
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    if-ge v0, p1, :cond_f

    .line 847
    aget v1, p0, v0

    add-int/lit8 v2, v0, 0x1

    aget v2, p0, v2

    add-int/2addr v1, v2

    aput v1, p0, v0

    .line 846
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 849
    .end local v0    # "i":I
    :cond_f
    return-void
.end method

.method private static final noUnrollingOddTripCount([I)V
    .registers 4
    .param p0, "a"    # [I

    .line 813
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0xfff

    if-ge v0, v1, :cond_11

    .line 814
    aget v1, p0, v0

    add-int/lit8 v2, v0, 0x1

    aget v2, p0, v2

    add-int/2addr v1, v2

    aput v1, p0, v0

    .line 813
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 816
    .end local v0    # "i":I
    :cond_11
    return-void
.end method

.method private static final peelingAddInts([I)V
    .registers 3
    .param p0, "a"    # [I

    .line 942
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    if-eqz p0, :cond_f

    array-length v1, p0

    if-ge v0, v1, :cond_f

    .line 943
    aget v1, p0, v0

    add-int/lit8 v1, v1, 0x1

    aput v1, p0, v0

    .line 942
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 945
    .end local v0    # "i":I
    :cond_f
    return-void
.end method

.method private static final peelingBreakFromNest([IZ)V
    .registers 5
    .param p0, "a"    # [I
    .param p1, "f"    # Z

    .line 986
    const/4 v0, 0x1

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x20

    if-ge v0, v1, :cond_19

    .line 987
    const/4 v1, 0x0

    .local v1, "j":I
    :goto_6
    const/16 v2, 0x1000

    if-ge v1, v2, :cond_16

    .line 988
    if-eqz p1, :cond_d

    .line 989
    goto :goto_19

    .line 991
    :cond_d
    aget v2, p0, v1

    add-int/lit8 v2, v2, 0x1

    aput v2, p0, v1

    .line 987
    add-int/lit8 v1, v1, 0x1

    goto :goto_6

    .line 986
    .end local v1    # "j":I
    :cond_16
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 994
    .end local v0    # "i":I
    :cond_19
    :goto_19
    return-void
.end method

.method private static final peelingHoistOneControl(I)I
    .registers 3
    .param p0, "x"    # I

    .line 1021
    const/4 v0, 0x0

    .line 1023
    .local v0, "i":I
    :goto_1
    if-nez p0, :cond_5

    .line 1024
    const/4 v1, 0x1

    return v1

    .line 1025
    :cond_5
    add-int/lit8 v0, v0, 0x1

    goto :goto_1
.end method

.method private static final peelingHoistOneControl(II)I
    .registers 3
    .param p0, "x"    # I
    .param p1, "y"    # I

    .line 1040
    :goto_0
    if-nez p0, :cond_4

    .line 1041
    const/4 v0, 0x1

    return v0

    .line 1042
    :cond_4
    if-nez p1, :cond_8

    .line 1043
    const/4 v0, 0x2

    return v0

    .line 1044
    :cond_8
    add-int/lit8 p1, p1, -0x1

    goto :goto_0
.end method

.method private static final peelingHoistTwoControl(III)I
    .registers 4
    .param p0, "x"    # I
    .param p1, "y"    # I
    .param p2, "z"    # I

    .line 1060
    :goto_0
    if-nez p0, :cond_4

    .line 1061
    const/4 v0, 0x1

    return v0

    .line 1062
    :cond_4
    if-nez p1, :cond_8

    .line 1063
    const/4 v0, 0x2

    return v0

    .line 1064
    :cond_8
    if-nez p2, :cond_c

    .line 1065
    const/4 v0, 0x3

    return v0

    .line 1066
    :cond_c
    add-int/lit8 p2, p2, -0x1

    goto :goto_0
.end method

.method private static final peelingSimple([IZ)V
    .registers 4
    .param p0, "a"    # [I
    .param p1, "f"    # Z

    .line 909
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x1000

    if-ge v0, v1, :cond_11

    .line 910
    if-eqz p1, :cond_8

    .line 911
    goto :goto_11

    .line 913
    :cond_8
    aget v1, p0, v0

    add-int/lit8 v1, v1, 0x1

    aput v1, p0, v0

    .line 909
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 915
    .end local v0    # "i":I
    :cond_11
    :goto_11
    return-void
.end method

.method private static final unrollingFull([I)V
    .registers 4
    .param p0, "a"    # [I

    .line 1105
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/4 v1, 0x2

    if-ge v0, v1, :cond_10

    .line 1106
    aget v1, p0, v0

    add-int/lit8 v2, v0, 0x1

    aget v2, p0, v2

    add-int/2addr v1, v2

    aput v1, p0, v0

    .line 1105
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 1108
    .end local v0    # "i":I
    :cond_10
    return-void
.end method

.method private static final unrollingInTheNest([I[II)V
    .registers 8
    .param p0, "a"    # [I
    .param p1, "b"    # [I
    .param p2, "x"    # I

    .line 378
    const/4 v0, 0x0

    .local v0, "k":I
    :goto_1
    const/16 v1, 0x10

    if-ge v0, v1, :cond_22

    .line 379
    const/4 v2, 0x0

    .local v2, "j":I
    :goto_6
    if-ge v2, v1, :cond_1f

    .line 380
    const/4 v3, 0x0

    .local v3, "i":I
    :goto_9
    const/16 v4, 0x80

    if-ge v3, v4, :cond_1c

    .line 381
    aget v4, p1, p2

    add-int/lit8 v4, v4, 0x1

    aput v4, p1, p2

    .line 382
    aget v4, p0, v3

    add-int/lit8 v4, v4, 0x1

    aput v4, p0, v3

    .line 380
    add-int/lit8 v3, v3, 0x1

    goto :goto_9

    .line 379
    .end local v3    # "i":I
    :cond_1c
    add-int/lit8 v2, v2, 0x1

    goto :goto_6

    .line 378
    .end local v2    # "j":I
    :cond_1f
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 386
    .end local v0    # "k":I
    :cond_22
    return-void
.end method

.method private static final unrollingLiveOutsNested([I)I
    .registers 7
    .param p0, "a"    # [I

    .line 680
    const/4 v0, 0x1

    .line 681
    .local v0, "s":I
    const/4 v1, 0x2

    .line 682
    .local v1, "t":I
    const/4 v2, 0x0

    .local v2, "j":I
    :goto_3
    const/16 v3, 0x10

    if-ge v2, v3, :cond_1d

    .line 683
    const/4 v3, 0x0

    .local v3, "i":I
    :goto_8
    const/16 v4, 0xffe

    if-ge v3, v4, :cond_1a

    .line 684
    add-int/lit8 v4, v3, 0x1

    aget v4, p0, v4

    .line 685
    .local v4, "temp":I
    add-int/2addr v0, v4

    .line 686
    mul-int/2addr v1, v4

    .line 687
    aget v5, p0, v3

    add-int/2addr v5, v0

    aput v5, p0, v3

    .line 683
    .end local v4    # "temp":I
    add-int/lit8 v3, v3, 0x1

    goto :goto_8

    .line 682
    .end local v3    # "i":I
    :cond_1a
    add-int/lit8 v2, v2, 0x1

    goto :goto_3

    .line 690
    .end local v2    # "j":I
    :cond_1d
    add-int v2, v0, v1

    return v2
.end method

.method private static final unrollingLoadStoreElimination([I)V
    .registers 4
    .param p0, "a"    # [I

    .line 108
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0xffe

    if-ge v0, v1, :cond_11

    .line 109
    aget v1, p0, v0

    add-int/lit8 v2, v0, 0x1

    aget v2, p0, v2

    add-int/2addr v1, v2

    aput v1, p0, v0

    .line 108
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 111
    .end local v0    # "i":I
    :cond_11
    return-void
.end method

.method private static final unrollingRInnerproduct([[D[[D[[DII)V
    .registers 14
    .param p0, "result"    # [[D
    .param p1, "a"    # [[D
    .param p2, "b"    # [[D
    .param p3, "row"    # I
    .param p4, "column"    # I

    .line 325
    aget-object v0, p0, p3

    const-wide/16 v1, 0x0

    aput-wide v1, v0, p4

    .line 326
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_7
    const/16 v1, 0x10

    if-ge v0, v1, :cond_20

    .line 327
    aget-object v1, p0, p3

    aget-object v2, p0, p3

    aget-wide v3, v2, p4

    aget-object v2, p1, p3

    aget-wide v5, v2, v0

    aget-object v2, p2, v0

    aget-wide v7, v2, p4

    mul-double/2addr v5, v7

    add-double/2addr v3, v5

    aput-wide v3, v1, p4

    .line 326
    add-int/lit8 v0, v0, 0x1

    goto :goto_7

    .line 329
    :cond_20
    return-void
.end method

.method private static final unrollingSimpleLiveOuts([I)I
    .registers 6
    .param p0, "a"    # [I

    .line 529
    const/4 v0, 0x1

    .line 530
    .local v0, "s":I
    const/4 v1, 0x2

    .line 531
    .local v1, "t":I
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_3
    const/16 v3, 0xffe

    if-ge v2, v3, :cond_15

    .line 532
    add-int/lit8 v3, v2, 0x1

    aget v3, p0, v3

    .line 533
    .local v3, "temp":I
    add-int/2addr v0, v3

    .line 534
    mul-int/2addr v1, v3

    .line 535
    aget v4, p0, v2

    add-int/2addr v4, v0

    aput v4, p0, v2

    .line 531
    .end local v3    # "temp":I
    add-int/lit8 v2, v2, 0x1

    goto :goto_3

    .line 538
    .end local v2    # "i":I
    :cond_15
    add-int v2, v0, v1

    const/4 v3, 0x1

    div-int/2addr v3, v2

    return v3
.end method

.method private static final unrollingSwapElements([I)V
    .registers 5
    .param p0, "array"    # [I

    .line 265
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0xffe

    if-ge v0, v1, :cond_1c

    .line 266
    aget v1, p0, v0

    add-int/lit8 v2, v0, 0x1

    aget v2, p0, v2

    if-le v1, v2, :cond_19

    .line 267
    add-int/lit8 v1, v0, 0x1

    aget v1, p0, v1

    .line 268
    .local v1, "temp":I
    add-int/lit8 v2, v0, 0x1

    aget v3, p0, v0

    aput v3, p0, v2

    .line 269
    aput v1, p0, v0

    .line 265
    .end local v1    # "temp":I
    :cond_19
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 272
    .end local v0    # "i":I
    :cond_1c
    return-void
.end method

.method private static final unrollingSwitch([I)V
    .registers 4
    .param p0, "a"    # [I

    .line 212
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x1000

    if-ge v0, v1, :cond_15

    .line 213
    rem-int/lit8 v1, v0, 0x3

    const/4 v2, 0x2

    if-eq v1, v2, :cond_b

    goto :goto_12

    .line 215
    :cond_b
    aget v1, p0, v0

    add-int/lit8 v1, v1, 0x1

    aput v1, p0, v0

    .line 216
    nop

    .line 212
    :goto_12
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 221
    .end local v0    # "i":I
    :cond_15
    return-void
.end method

.method private static final unrollingTwoLoopsInTheNest([I[II)V
    .registers 7
    .param p0, "a"    # [I
    .param p1, "b"    # [I
    .param p2, "x"    # I

    .line 451
    const/4 v0, 0x0

    .local v0, "k":I
    :goto_1
    const/16 v1, 0x80

    if-ge v0, v1, :cond_25

    .line 452
    const/16 v2, 0x64

    if-le p2, v2, :cond_16

    .line 453
    const/4 v2, 0x0

    .local v2, "j":I
    :goto_a
    if-ge v2, v1, :cond_15

    .line 454
    aget v3, p0, p2

    add-int/lit8 v3, v3, 0x1

    aput v3, p0, p2

    .line 453
    add-int/lit8 v2, v2, 0x1

    goto :goto_a

    :cond_15
    goto :goto_22

    .line 457
    .end local v2    # "j":I
    :cond_16
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_17
    if-ge v2, v1, :cond_22

    .line 458
    aget v3, p1, p2

    add-int/lit8 v3, v3, 0x1

    aput v3, p1, p2

    .line 457
    add-int/lit8 v2, v2, 0x1

    goto :goto_17

    .line 451
    .end local v2    # "i":I
    :cond_22
    :goto_22
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 462
    .end local v0    # "k":I
    :cond_25
    return-void
.end method

.method private static final unrollingWhile([I)V
    .registers 5
    .param p0, "a"    # [I

    .line 167
    const/4 v0, 0x0

    .line 168
    .local v0, "i":I
    const/16 v1, 0x80

    .line 169
    .local v1, "s":I
    :goto_3
    add-int/lit8 v2, v0, 0x1

    .end local v0    # "i":I
    .local v2, "i":I
    const/16 v3, 0xffe

    if-ge v0, v3, :cond_14

    .line 170
    rem-int/lit8 v0, v2, 0x2

    if-nez v0, :cond_12

    .line 171
    add-int/lit8 v0, v1, 0x1

    .end local v1    # "s":I
    .local v0, "s":I
    aput v1, p0, v2

    .line 169
    move v1, v0

    .end local v2    # "i":I
    .local v0, "i":I
    .restart local v1    # "s":I
    :cond_12
    move v0, v2

    goto :goto_3

    .line 174
    .end local v0    # "i":I
    .restart local v2    # "i":I
    :cond_14
    return-void
.end method

.method private static final unrollingWhileLiveOuts([I)I
    .registers 5
    .param p0, "a"    # [I

    .line 598
    const/4 v0, 0x0

    .line 599
    .local v0, "i":I
    const/16 v1, 0x80

    .line 600
    .local v1, "s":I
    :goto_3
    add-int/lit8 v2, v0, 0x1

    .end local v0    # "i":I
    .local v2, "i":I
    const/16 v3, 0xffe

    if-ge v0, v3, :cond_14

    .line 601
    rem-int/lit8 v0, v2, 0x2

    if-nez v0, :cond_12

    .line 602
    add-int/lit8 v0, v1, 0x1

    .end local v1    # "s":I
    .local v0, "s":I
    aput v1, p0, v2

    .line 600
    move v1, v0

    .end local v2    # "i":I
    .local v0, "i":I
    .restart local v1    # "s":I
    :cond_12
    move v0, v2

    goto :goto_3

    .line 605
    .end local v0    # "i":I
    .restart local v2    # "i":I
    :cond_14
    return v1
.end method


# virtual methods
.method public unrollingCheckCast([ILjava/lang/Object;)V
    .registers 5
    .param p1, "a"    # [I
    .param p2, "o"    # Ljava/lang/Object;

    .line 776
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x10

    if-ge v0, v1, :cond_f

    .line 777
    move-object v1, p2

    check-cast v1, LMain$SubMain;

    if-ne v1, p2, :cond_c

    .line 778
    aput v0, p1, v0

    .line 776
    :cond_c
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 781
    .end local v0    # "i":I
    :cond_f
    return-void
.end method

.method public unrollingDivZeroCheck([II)V
    .registers 6
    .param p1, "a"    # [I
    .param p2, "r"    # I

    .line 730
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x10

    if-ge v0, v1, :cond_10

    .line 731
    aget v1, p1, v0

    aget v2, p1, v0

    div-int/2addr v2, p2

    add-int/2addr v1, v2

    aput v1, p1, v0

    .line 730
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 733
    .end local v0    # "i":I
    :cond_10
    return-void
.end method

.method public unrollingInstanceOf([I[Ljava/lang/Object;)V
    .registers 5
    .param p1, "a"    # [I
    .param p2, "obj_array"    # [Ljava/lang/Object;

    .line 708
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x10

    if-ge v0, v1, :cond_14

    .line 709
    aget-object v1, p2, v0

    instance-of v1, v1, Ljava/lang/Integer;

    if-eqz v1, :cond_11

    .line 710
    aget v1, p1, v0

    add-int/lit8 v1, v1, 0x1

    aput v1, p1, v0

    .line 708
    :cond_11
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 713
    .end local v0    # "i":I
    :cond_14
    return-void
.end method

.method public unrollingTypeConversion([I[D)V
    .registers 6
    .param p1, "a"    # [I
    .param p2, "b"    # [D

    .line 750
    const/4 v0, 0x0

    .local v0, "i":I
    :goto_1
    const/16 v1, 0x10

    if-ge v0, v1, :cond_d

    .line 751
    aget-wide v1, p2, v0

    double-to-int v1, v1

    aput v1, p1, v0

    .line 750
    add-int/lit8 v0, v0, 0x1

    goto :goto_1

    .line 753
    .end local v0    # "i":I
    :cond_d
    return-void
.end method

.method public verifyPeeling()V
    .registers 6

    .line 1156
    const/4 v0, 0x0

    invoke-static {v0}, LMain;->peelingHoistOneControl(I)I

    move-result v1

    const/4 v2, 0x1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1157
    invoke-static {v0, v0}, LMain;->peelingHoistOneControl(II)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1158
    invoke-static {v0, v2}, LMain;->peelingHoistOneControl(II)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1159
    invoke-static {v2, v0}, LMain;->peelingHoistOneControl(II)I

    move-result v1

    const/4 v3, 0x2

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1160
    invoke-static {v2, v2}, LMain;->peelingHoistOneControl(II)I

    move-result v1

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1161
    invoke-static {v0, v0, v0}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1162
    invoke-static {v0, v0, v2}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1163
    invoke-static {v0, v2, v0}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1164
    invoke-static {v0, v2, v2}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v2, v1}, LMain;->expectEquals(II)V

    .line 1165
    invoke-static {v2, v0, v0}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1166
    invoke-static {v2, v0, v2}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1167
    invoke-static {v2, v2, v0}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    const/4 v3, 0x3

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1168
    invoke-static {v2, v2, v2}, LMain;->peelingHoistTwoControl(III)I

    move-result v1

    invoke-static {v3, v1}, LMain;->expectEquals(II)V

    .line 1170
    iget-object v1, p0, LMain;->a:[I

    invoke-static {v1}, LMain;->initIntArray([I)V

    .line 1171
    iget-object v1, p0, LMain;->a:[I

    invoke-static {v1, v0}, LMain;->peelingSimple([IZ)V

    .line 1172
    iget-object v1, p0, LMain;->a:[I

    invoke-static {v1, v2}, LMain;->peelingSimple([IZ)V

    .line 1173
    iget-object v1, p0, LMain;->a:[I

    invoke-static {v1}, LMain;->peelingAddInts([I)V

    .line 1174
    const/4 v1, 0x0

    invoke-static {v1}, LMain;->peelingAddInts([I)V

    .line 1175
    iget-object v1, p0, LMain;->a:[I

    invoke-static {v1, v0}, LMain;->peelingBreakFromNest([IZ)V

    .line 1176
    iget-object v0, p0, LMain;->a:[I

    invoke-static {v0, v2}, LMain;->peelingBreakFromNest([IZ)V

    .line 1178
    iget-object v0, p0, LMain;->a:[I

    invoke-static {v0}, LMain;->unrollingSimpleLiveOuts([I)I

    .line 1179
    iget-object v0, p0, LMain;->a:[I

    invoke-static {v0}, LMain;->unrollingWhileLiveOuts([I)I

    .line 1180
    iget-object v0, p0, LMain;->a:[I

    invoke-static {v0}, LMain;->unrollingLiveOutsNested([I)I

    .line 1182
    const v0, 0x312d59a

    .line 1183
    .local v0, "expected":I
    const/4 v1, 0x0

    .line 1184
    .local v1, "found":I
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_95
    iget-object v3, p0, LMain;->a:[I

    array-length v4, v3

    if-ge v2, v4, :cond_a0

    .line 1185
    aget v3, v3, v2

    add-int/2addr v1, v3

    .line 1184
    add-int/lit8 v2, v2, 0x1

    goto :goto_95

    .line 1188
    .end local v2    # "i":I
    :cond_a0
    invoke-static {v0, v1}, LMain;->expectEquals(II)V

    .line 1189
    return-void
.end method

.method public verifyUnrolling()V
    .registers 8

    .line 1117
    iget-object v0, p0, LMain;->a:[I

    invoke-static {v0}, LMain;->initIntArray([I)V

    .line 1118
    iget-object v0, p0, LMain;->b:[I

    invoke-static {v0}, LMain;->initIntArray([I)V

    .line 1120
    iget-object v0, p0, LMain;->mA:[[D

    invoke-static {v0}, LMain;->initMatrix([[D)V

    .line 1121
    iget-object v0, p0, LMain;->mB:[[D

    invoke-static {v0}, LMain;->initMatrix([[D)V

    .line 1122
    iget-object v0, p0, LMain;->mC:[[D

    invoke-static {v0}, LMain;->initMatrix([[D)V

    .line 1124
    const v0, 0xa637a3b

    .line 1125
    .local v0, "expected":I
    const/4 v1, 0x0

    .line 1127
    .local v1, "found":I
    const/16 v2, 0x10

    new-array v3, v2, [D

    .line 1128
    .local v3, "doubleArray":[D
    invoke-static {v3}, LMain;->initDoubleArray([D)V

    .line 1130
    iget-object v4, p0, LMain;->a:[I

    new-array v2, v2, [Ljava/lang/Integer;

    invoke-virtual {p0, v4, v2}, LMain;->unrollingInstanceOf([I[Ljava/lang/Object;)V

    .line 1131
    iget-object v2, p0, LMain;->a:[I

    const/16 v4, 0xf

    invoke-virtual {p0, v2, v4}, LMain;->unrollingDivZeroCheck([II)V

    .line 1132
    iget-object v2, p0, LMain;->a:[I

    invoke-virtual {p0, v2, v3}, LMain;->unrollingTypeConversion([I[D)V

    .line 1133
    iget-object v2, p0, LMain;->a:[I

    new-instance v4, LMain$SubMain;

    invoke-direct {v4, p0}, LMain$SubMain;-><init>(LMain;)V

    invoke-virtual {p0, v2, v4}, LMain;->unrollingCheckCast([ILjava/lang/Object;)V

    .line 1135
    iget-object v2, p0, LMain;->a:[I

    invoke-static {v2}, LMain;->unrollingWhile([I)V

    .line 1136
    iget-object v2, p0, LMain;->a:[I

    invoke-static {v2}, LMain;->unrollingLoadStoreElimination([I)V

    .line 1137
    iget-object v2, p0, LMain;->a:[I

    invoke-static {v2}, LMain;->unrollingSwitch([I)V

    .line 1138
    iget-object v2, p0, LMain;->a:[I

    invoke-static {v2}, LMain;->unrollingSwapElements([I)V

    .line 1139
    iget-object v2, p0, LMain;->mC:[[D

    iget-object v4, p0, LMain;->mA:[[D

    iget-object v5, p0, LMain;->mB:[[D

    const/4 v6, 0x4

    invoke-static {v2, v4, v5, v6, v6}, LMain;->unrollingRInnerproduct([[D[[D[[DII)V

    .line 1140
    iget-object v2, p0, LMain;->a:[I

    iget-object v4, p0, LMain;->b:[I

    invoke-static {v2, v4, v6}, LMain;->unrollingInTheNest([I[II)V

    .line 1141
    iget-object v2, p0, LMain;->a:[I

    iget-object v4, p0, LMain;->b:[I

    invoke-static {v2, v4, v6}, LMain;->unrollingTwoLoopsInTheNest([I[II)V

    .line 1143
    iget-object v2, p0, LMain;->b:[I

    invoke-static {v2}, LMain;->noUnrollingOddTripCount([I)V

    .line 1144
    iget-object v2, p0, LMain;->b:[I

    const/16 v4, 0x80

    invoke-static {v2, v4}, LMain;->noUnrollingNotKnownTripCount([II)V

    .line 1146
    const/4 v2, 0x0

    .local v2, "i":I
    :goto_7a
    const/16 v4, 0x1000

    if-ge v2, v4, :cond_8b

    .line 1147
    iget-object v4, p0, LMain;->a:[I

    aget v4, v4, v2

    add-int/2addr v1, v4

    .line 1148
    iget-object v4, p0, LMain;->b:[I

    aget v4, v4, v2

    add-int/2addr v1, v4

    .line 1146
    add-int/lit8 v2, v2, 0x1

    goto :goto_7a

    .line 1150
    .end local v2    # "i":I
    :cond_8b
    iget-object v2, p0, LMain;->mC:[[D

    aget-object v2, v2, v6

    aget-wide v4, v2, v6

    double-to-int v2, v4

    add-int/2addr v1, v2

    .line 1152
    invoke-static {v0, v1}, LMain;->expectEquals(II)V

    .line 1153
    return-void
.end method
