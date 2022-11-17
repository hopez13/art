.class public LB252804549;

.super Ljava/lang/Object;

.method public static compareInstanceOfWithTwo(Ljava/lang/Object;)I
   .registers 2
   instance-of v0, p0, Ljava/lang/String;
   const/4 v1, 0x2
   # Compare instance-of with 2 (i.e. neither 0 nor 1)
   if-eq v0, v1, :Lequal
   const/4 v1, 0x3
   return v1
:Lequal
   return v1
.end method