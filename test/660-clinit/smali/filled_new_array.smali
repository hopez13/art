.class public LFilledNewArray;

.super Ljava/lang/Object;

.method public static constructor <clinit>()V
    .registers 4
    new-instance v1, LPair;
    invoke-direct {v1}, LPair;-><init>()V
    new-instance v2, LPair;
    invoke-direct {v2}, LPair;-><init>()V
    new-instance v3, LPair;
    invoke-direct {v3}, LPair;-><init>()V
    filled-new-array {v1, v2, v3 }, [LPair;
    move-result-object v0
    return-void
.end method

