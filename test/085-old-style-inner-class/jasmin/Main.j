; Copyright (C) 2017 The Android Open Source Project
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

.source                  Main.java
.class                   public Main
.super                   java/lang/Object

.field                   private static theRunnable Ljava/lang/Runnable;

.method                  public <init>()V
   .limit stack          1
   .limit locals         1
   .line                 22
   aload_0               
   invokespecial         java/lang/Object/<init>()V
   return                
.end method              

.method                  private static create()Ljava/lang/Runnable;
   .limit stack          2
   .limit locals         0
   .line                 28
   new                   Main$2
   dup                   
   invokespecial         Main$2/<init>()V
   areturn               
.end method              

.method                  private static nameOf(Ljava/lang/Class;)Ljava/lang/String;
   .limit stack          1
   .limit locals         1
   .line                 34
   aload_0               
   ifnonnull             LABEL0x9
   ldc                   "(null)"
   goto                  LABEL0xd
LABEL0x9:
   aload_0               
   invokevirtual         java/lang/Class/getName()Ljava/lang/String;
LABEL0xd:
   areturn               
.end method              

.method                  private static nameOf(Ljava/lang/reflect/Method;)Ljava/lang/String;
   .limit stack          1
   .limit locals         1
   .line                 38
   aload_0               
   ifnonnull             LABEL0x9
   ldc                   "(null)"
   goto                  LABEL0xd
LABEL0x9:
   aload_0               
   invokevirtual         java/lang/reflect/Method/toString()Ljava/lang/String;
LABEL0xd:
   areturn               
.end method              

.method                  private static infoFor(Ljava/lang/Class;)V
   .limit stack          3
   .limit locals         1
   .line                 42
   getstatic             java/lang/System/out Ljava/io/PrintStream;
   new                   java/lang/StringBuffer
   dup                   
   invokespecial         java/lang/StringBuffer/<init>()V
   ldc                   "Class: "
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   aload_0               
   invokestatic          Main/nameOf(Ljava/lang/Class;)Ljava/lang/String;
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   ldc                   "\012  getDeclaringClass(): "
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   aload_0               
   .line                 44
   invokevirtual         java/lang/Class/getDeclaringClass()Ljava/lang/Class;
   invokestatic          Main/nameOf(Ljava/lang/Class;)Ljava/lang/String;
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   ldc                   "\012  getEnclosingClass(): "
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   aload_0               
   .line                 46
   invokevirtual         java/lang/Class/getEnclosingClass()Ljava/lang/Class;
   invokestatic          Main/nameOf(Ljava/lang/Class;)Ljava/lang/String;
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   ldc                   "\012  getEnclosingMethod(): "
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   aload_0               
   .line                 48
   invokevirtual         java/lang/Class/getEnclosingMethod()Ljava/lang/reflect/Method;
   invokestatic          Main/nameOf(Ljava/lang/reflect/Method;)Ljava/lang/String;
   invokevirtual         java/lang/StringBuffer/append(Ljava/lang/String;)Ljava/lang/StringBuffer;
   invokevirtual         java/lang/StringBuffer/toString()Ljava/lang/String;
   .line                 42
   invokevirtual         java/io/PrintStream/println(Ljava/lang/String;)V
   .line                 49
   return                
.end method              

.method                  public static main([Ljava/lang/String;)V
   .limit stack          1
   .limit locals         1
   .line                 52
   getstatic             Main/theRunnable Ljava/lang/Runnable;
   invokevirtual         java/lang/Object/getClass()Ljava/lang/Class;
   invokestatic          Main/infoFor(Ljava/lang/Class;)V
   .line                 53
   invokestatic          Main/create()Ljava/lang/Runnable;
   invokevirtual         java/lang/Object/getClass()Ljava/lang/Class;
   invokestatic          Main/infoFor(Ljava/lang/Class;)V
   .line                 54
   return                
.end method              

.method                  static <clinit>()V
   .limit stack          2
   .limit locals         0
   .line                 23
   new                   Main$1
   dup                   
   invokespecial         Main$1/<init>()V
   putstatic             Main/theRunnable Ljava/lang/Runnable;
   return                
.end method              

