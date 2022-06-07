.class public Llibcore/java/lang/nestgroup/NestGroupInnerB;
.super Ljava/lang/Object;
.source "NestGroupInnerB.java"


# annotations
.annotation system Ldalvik/annotation/NestHost;
    host = Llibcore/java/lang/nestgroup/NestGroupHost;
.end annotation


# instance fields
.field private final host:Llibcore/java/lang/nestgroup/NestGroupHost;

.field private val:I


# direct methods
.method public constructor <init>(Llibcore/java/lang/nestgroup/NestGroupHost;)V
    .registers 3

    .line 11
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 9
    const/4 v0, 0x0

    iput v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    .line 12
    iput-object p1, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    .line 13
    iput-object p0, p1, Llibcore/java/lang/nestgroup/NestGroupHost;->innerB:Llibcore/java/lang/nestgroup/NestGroupInnerB;

    .line 14
    return-void
.end method

.method private inc()V
    .registers 2

    .line 29
    iget v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    add-int/lit8 v0, v0, 0x1

    iput v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->val:I

    .line 30
    return-void
.end method


# virtual methods
.method public testHostAccess()I
    .registers 3

    .line 23
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    iget v1, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    .line 24
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    invoke-direct {v0}, Llibcore/java/lang/nestgroup/NestGroupHost;->inc()V

    .line 25
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    iget v0, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->val:I

    return v0
.end method

.method public testMemberAccess()I
    .registers 3

    .line 17
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    iget-object v0, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    iget v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    add-int/lit8 v1, v1, 0x1

    iput v1, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    .line 18
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    iget-object v0, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    invoke-direct {v0}, Llibcore/java/lang/nestgroup/NestGroupInnerA;->inc()V

    .line 19
    iget-object v0, p0, Llibcore/java/lang/nestgroup/NestGroupInnerB;->host:Llibcore/java/lang/nestgroup/NestGroupHost;

    iget-object v0, v0, Llibcore/java/lang/nestgroup/NestGroupHost;->innerA:Llibcore/java/lang/nestgroup/NestGroupInnerA;

    iget v0, v0, Llibcore/java/lang/nestgroup/NestGroupInnerA;->val:I

    return v0
.end method
