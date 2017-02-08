#
# Copyright (C) 2014 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

.class public LMain;
.super Ljava/lang/Object;
.source "Main.java"


# static fields
.field public static final CLASS_DEFINED_BITS:I = 0x763f

.field public static final FIELD_DEFINED_BITS:I = 0x50df

.field public static final INTERFACE_DEFINED_BITS:I = 0x763f

.field public static final METHOD_DEFINED_BITS:I = 0x1dff


# direct methods
.method public constructor <init>()V
    .registers 1

    .prologue
    .line 25
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private static check(Ljava/lang/String;)V
    .registers 14
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/lang/Exception;
        }
    .end annotation

    .prologue
    const v10, 0x89c0

    const/16 v12, 0x763f

    .line 85
    invoke-static {p0}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;

    move-result-object v0

    .line 86
    .local v0, "-l_1_R":Ljava/lang/Object;
    const-string/jumbo v9, "Inf"

    invoke-virtual {p0, v9}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v9

    if-nez v9, :cond_33

    .line 97
    invoke-virtual {v0}, Ljava/lang/Class;->isInterface()Z

    move-result v9

    if-nez v9, :cond_a4

    .line 100
    const v1, 0x89c0

    .line 101
    .local v1, "-l_2_I":I
    invoke-virtual {v0}, Ljava/lang/Class;->getModifiers()I

    move-result v9

    and-int/2addr v9, v10

    if-nez v9, :cond_ad

    .line 109
    :cond_22
    invoke-virtual {v0}, Ljava/lang/Class;->getDeclaredFields()[Ljava/lang/reflect/Field;

    move-result-object v2

    .local v2, "-l_2_R":Ljava/lang/Object;
    array-length v3, v2

    .local v3, "-l_3_I":I
    const/4 v4, 0x0

    .local v4, "-l_4_I":I
    :goto_28
    if-lt v4, v3, :cond_105

    .line 129
    invoke-virtual {v0}, Ljava/lang/Class;->getDeclaredMethods()[Ljava/lang/reflect/Method;

    move-result-object v2

    array-length v3, v2

    const/4 v4, 0x0

    :goto_30
    if-lt v4, v3, :cond_193

    .line 143
    return-void

    .line 87
    .end local v1    # "-l_2_I":I
    .end local v2    # "-l_2_R":Ljava/lang/Object;
    .end local v3    # "-l_3_I":I
    .end local v4    # "-l_4_I":I
    :cond_33
    invoke-virtual {v0}, Ljava/lang/Class;->isInterface()Z

    move-result v9

    if-eqz v9, :cond_9b

    .line 90
    const v1, 0x89c0

    .line 91
    .restart local v1    # "-l_2_I":I
    invoke-virtual {v0}, Ljava/lang/Class;->getModifiers()I

    move-result v9

    and-int/2addr v9, v10

    if-eqz v9, :cond_22

    .line 92
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Clazz.getModifiers(): "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v0}, Ljava/lang/Class;->getModifiers()I

    move-result v11

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 93
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "INTERFACE_DEF_BITS: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-static {v12}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 94
    new-instance v9, Ljava/lang/RuntimeException;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Undefined bits for an interface: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 88
    .end local v1    # "-l_2_I":I
    :cond_9b
    new-instance v9, Ljava/lang/RuntimeException;

    const-string/jumbo v10, "Expected an interface."

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 98
    :cond_a4
    new-instance v9, Ljava/lang/RuntimeException;

    const-string/jumbo v10, "Expected a class."

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 102
    .restart local v1    # "-l_2_I":I
    :cond_ad
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Clazz.getModifiers(): "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v0}, Ljava/lang/Class;->getModifiers()I

    move-result v11

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 103
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "CLASS_DEF_BITS: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-static {v12}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 104
    new-instance v9, Ljava/lang/RuntimeException;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Undefined bits for a class: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 109
    .restart local v2    # "-l_2_R":Ljava/lang/Object;
    .restart local v3    # "-l_3_I":I
    .restart local v4    # "-l_4_I":I
    :cond_105
    aget-object v5, v2, v4

    .line 110
    .local v5, "-l_5_R":Ljava/lang/Object;
    invoke-virtual {v5}, Ljava/lang/reflect/Field;->getName()Ljava/lang/String;

    move-result-object v6

    .line 111
    .local v6, "-l_6_R":Ljava/lang/Object;
    const v7, 0xaf20

    .line 112
    .local v7, "-l_7_I":I
    invoke-virtual {v5}, Ljava/lang/reflect/Field;->getModifiers()I

    move-result v9

    .line 111
    const v10, 0xaf20

    .line 112
    and-int/2addr v9, v10

    if-nez v9, :cond_130

    .line 117
    const-string/jumbo v9, "I"

    invoke-virtual {v6, v9}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v9

    if-nez v9, :cond_12c

    .line 121
    invoke-static {v6}, LMain;->getFieldMask(Ljava/lang/String;)I

    move-result v8

    .line 122
    .local v8, "-l_8_I":I
    invoke-virtual {v5}, Ljava/lang/reflect/Field;->getModifiers()I

    move-result v9

    and-int/2addr v9, v8

    if-eqz v9, :cond_18a

    .line 109
    .end local v8    # "-l_8_I":I
    :cond_12c
    add-int/lit8 v4, v4, 0x1

    goto/16 :goto_28

    .line 113
    :cond_130
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "f.getModifiers(): "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v5}, Ljava/lang/reflect/Field;->getModifiers()I

    move-result v11

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 114
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "FIELD_DEF_BITS: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    const/16 v11, 0x50df

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 115
    new-instance v9, Ljava/lang/RuntimeException;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Unexpected field bits: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10, v6}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 123
    .restart local v8    # "-l_8_I":I
    :cond_18a
    new-instance v9, Ljava/lang/RuntimeException;

    const-string/jumbo v10, "Expected field bit not set."

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 129
    .end local v5    # "-l_5_R":Ljava/lang/Object;
    .end local v6    # "-l_6_R":Ljava/lang/Object;
    .end local v7    # "-l_7_I":I
    .end local v8    # "-l_8_I":I
    :cond_193
    aget-object v5, v2, v4

    .line 130
    .restart local v5    # "-l_5_R":Ljava/lang/Object;
    invoke-virtual {v5}, Ljava/lang/reflect/Method;->getName()Ljava/lang/String;

    move-result-object v6

    .line 131
    .restart local v6    # "-l_6_R":Ljava/lang/Object;
    const v7, 0xe200

    .line 132
    .restart local v7    # "-l_7_I":I
    invoke-virtual {v5}, Ljava/lang/reflect/Method;->getModifiers()I

    move-result v9

    .line 131
    const v10, 0xe200

    .line 132
    and-int/2addr v9, v10

    if-nez v9, :cond_1b5

    .line 138
    invoke-static {v6}, LMain;->getMethodMask(Ljava/lang/String;)I

    move-result v8

    .line 139
    .restart local v8    # "-l_8_I":I
    invoke-virtual {v5}, Ljava/lang/reflect/Method;->getModifiers()I

    move-result v9

    and-int/2addr v9, v8

    if-eqz v9, :cond_20f

    .line 129
    add-int/lit8 v4, v4, 0x1

    goto/16 :goto_30

    .line 133
    .end local v8    # "-l_8_I":I
    :cond_1b5
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "m.getModifiers(): "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v5}, Ljava/lang/reflect/Method;->getModifiers()I

    move-result v11

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 134
    sget-object v9, Ljava/lang/System;->out:Ljava/io/PrintStream;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "METHOD_DEF_BITS: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    const/16 v11, 0x1dff

    invoke-static {v11}, Ljava/lang/Integer;->toBinaryString(I)Ljava/lang/String;

    move-result-object v11

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-virtual {v9, v10}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V

    .line 135
    new-instance v9, Ljava/lang/RuntimeException;

    new-instance v10, Ljava/lang/StringBuilder;

    invoke-direct {v10}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v11, "Unexpected method bits: "

    invoke-virtual {v10, v11}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10, v6}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v10

    invoke-virtual {v10}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v10

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9

    .line 140
    .restart local v8    # "-l_8_I":I
    :cond_20f
    new-instance v9, Ljava/lang/RuntimeException;

    const-string/jumbo v10, "Expected method bit not set."

    invoke-direct {v9, v10}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v9
.end method

.method private static getFieldMask(Ljava/lang/String;)I
    .registers 6

    .prologue
    const/4 v3, 0x0

    .line 146
    const-string/jumbo v2, "Field"

    invoke-virtual {p0, v2}, Ljava/lang/String;->indexOf(Ljava/lang/String;)I

    move-result v0

    .line 147
    .local v0, "-l_1_I":I
    if-gtz v0, :cond_24

    .line 171
    :cond_a
    new-instance v2, Ljava/lang/RuntimeException;

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v4, "Unexpected field name "

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    invoke-direct {v2, v3}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v2

    .line 148
    :cond_24
    invoke-virtual {p0, v3, v0}, Ljava/lang/String;->substring(II)Ljava/lang/String;

    move-result-object v1

    .line 149
    .local v1, "-l_2_R":Ljava/lang/Object;
    const-string/jumbo v2, "public"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_6a

    .line 152
    const-string/jumbo v2, "private"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_6c

    .line 155
    const-string/jumbo v2, "protected"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_6e

    .line 158
    const-string/jumbo v2, "static"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_70

    .line 161
    const-string/jumbo v2, "transient"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_73

    .line 164
    const-string/jumbo v2, "volatile"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_76

    .line 167
    const-string/jumbo v2, "final"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-eqz v2, :cond_a

    .line 168
    const/16 v2, 0x10

    return v2

    .line 150
    :cond_6a
    const/4 v2, 0x1

    return v2

    .line 153
    :cond_6c
    const/4 v2, 0x2

    return v2

    .line 156
    :cond_6e
    const/4 v2, 0x4

    return v2

    .line 159
    :cond_70
    const/16 v2, 0x8

    return v2

    .line 162
    :cond_73
    const/16 v2, 0x80

    return v2

    .line 165
    :cond_76
    const/16 v2, 0x40

    return v2
.end method

.method private static getMethodMask(Ljava/lang/String;)I
    .registers 6

    .prologue
    const/4 v3, 0x0

    .line 175
    const-string/jumbo v2, "Method"

    invoke-virtual {p0, v2}, Ljava/lang/String;->indexOf(Ljava/lang/String;)I

    move-result v0

    .line 176
    .local v0, "-l_1_I":I
    if-gtz v0, :cond_24

    .line 209
    :cond_a
    new-instance v2, Ljava/lang/RuntimeException;

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    const-string/jumbo v4, "Unexpected method name "

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3, p0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    invoke-direct {v2, v3}, Ljava/lang/RuntimeException;-><init>(Ljava/lang/String;)V

    throw v2

    .line 177
    :cond_24
    invoke-virtual {p0, v3, v0}, Ljava/lang/String;->substring(II)Ljava/lang/String;

    move-result-object v1

    .line 178
    .local v1, "-l_2_R":Ljava/lang/Object;
    const-string/jumbo v2, "public"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_85

    .line 181
    const-string/jumbo v2, "private"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_87

    .line 184
    const-string/jumbo v2, "protected"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_89

    .line 187
    const-string/jumbo v2, "static"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_8b

    .line 190
    const-string/jumbo v2, "synchronized"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_8e

    .line 193
    const-string/jumbo v2, "varargs"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_91

    .line 196
    const-string/jumbo v2, "final"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_94

    .line 199
    const-string/jumbo v2, "native"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_97

    .line 202
    const-string/jumbo v2, "abstract"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-nez v2, :cond_9a

    .line 205
    const-string/jumbo v2, "strictfp"

    invoke-virtual {v1, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z

    move-result v2

    if-eqz v2, :cond_a

    .line 206
    const/16 v2, 0x800

    return v2

    .line 179
    :cond_85
    const/4 v2, 0x1

    return v2

    .line 182
    :cond_87
    const/4 v2, 0x2

    return v2

    .line 185
    :cond_89
    const/4 v2, 0x4

    return v2

    .line 188
    :cond_8b
    const/16 v2, 0x8

    return v2

    .line 191
    :cond_8e
    const/16 v2, 0x20

    return v2

    .line 194
    :cond_91
    const/16 v2, 0x80

    return v2

    .line 197
    :cond_94
    const/16 v2, 0x10

    return v2

    .line 200
    :cond_97
    const/16 v2, 0x100

    return v2

    .line 203
    :cond_9a
    const/16 v2, 0x400

    return v2
.end method

.method public static main([Ljava/lang/String;)V
    .registers 2
    .annotation system Ldalvik/annotation/Throws;
        value = {
            Ljava/lang/Exception;
        }
    .end annotation

    .prologue
    .line 78
    const-string/jumbo v0, "Inf"

    invoke-static {v0}, LMain;->check(Ljava/lang/String;)V

    .line 79
    const-string/jumbo v0, "NonInf"

    invoke-static {v0}, LMain;->check(Ljava/lang/String;)V

    .line 80
    const-string/jumbo v0, "A"

    invoke-static {v0}, LMain;->check(Ljava/lang/String;)V

    .line 81
    const-string/jumbo v0, "A$B"

    invoke-static {v0}, LMain;->check(Ljava/lang/String;)V

    .line 82
    return-void
.end method
