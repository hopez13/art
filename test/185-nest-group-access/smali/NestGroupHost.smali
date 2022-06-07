.class public Llibcore/java/lang/nestgroup/NestGroupHost;
.super Ljava/lang/Object;
.source "NestGroupHost.java"


# annotations
.annotation system Ldalvik/annotation/NestMembers;
    classes = {
        Llibcore/java/lang/nestgroup/NestGroupInnerA;,
        Llibcore/java/lang/nestgroup/NestGroupInnerB;
    }
.end annotation


# instance fields
.field private innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

.field private innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

.field private val:I


# direct methods
.method public constructor <init>()V
    .registers 2

    .line 7
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 8
    const/4 v0, 0x0

    iput-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    .line 9
    iput-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    .line 10
    const/4 v0, 0x0

    iput v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    return-void
.end method


# virtual methods
.method private inc()V
    .registers 2

    .line 27
    iget v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    add-int/lit8 v0, v0, 0x1

    iput v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    .line 28
    return-void
.end method

.method public reset()V
    .registers 3

    .line 21
    const/4 v0, 0x0

    iput v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    .line 22
    iget-object v1, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    iput v0, v1, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    .line 23
    iget-object v1, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    iput v0, v1, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    .line 24
    return-void
.end method

.method public testInnerAccess()I
    .registers 3

    .line 13
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    iget v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    .line 14
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    invoke-virtual {v0}, Llibcore/java/lang/nestgroup/NestGroupInnerA;->inc()V

    .line 15
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    iget v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    .line 16
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    invoke-virtual {v0}, Llibcore/java/lang/nestgroup/NestGroupInnerB;->inc()V

    .line 17
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    iget v0, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    iget-object v1, p0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    iget v1, v1, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    add-int/2addr v0, v1

    return v0
.end method
