.class public Landroid/test/anno/SimplyNoted;
.super Ljava/lang/Object;
.source "SimplyNoted.java"


# annotations
.annotation runtime Landroid/test/anno/AnnoSimpleType2;
.end annotation

.annotation runtime Landroid/test/anno/AnnoSimpleType;
.end annotation

.annotation build Landroid/test/anno/AnnoSimpleTypeInvis;
.end annotation

.annotation runtime Landroid/test/anno/MissingAnnotation;
.end annotation


# static fields
.field public static mOneFoo:I
    .annotation runtime Landroid/test/anno/AnnoSimpleField;
    .end annotation

    .annotation runtime Landroid/test/anno/MissingAnnotation;
    .end annotation
.end field


# instance fields
.field public mFoo:I
    .annotation runtime Landroid/test/anno/AnnoSimpleField;
    .end annotation

    .annotation runtime Landroid/test/anno/MissingAnnotation;
    .end annotation
.end field


# direct methods
.method constructor <init>()V
    .registers 2
    .annotation runtime Landroid/test/anno/AnnoSimpleConstructor;
    .end annotation

    .annotation runtime Landroid/test/anno/MissingAnnotation;
    .end annotation

    .line 18
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 19
    const/4 v0, 0x0

    iput v0, p0, Landroid/test/anno/SimplyNoted;->mFoo:I

    .line 20
    return-void
.end method

.method constructor <init>(I)V
    .registers 2
    .param p1, "xyzzy"    # I
        .annotation runtime Landroid/test/anno/AnnoSimpleParameter;
        .end annotation
    .end param
    .annotation runtime Landroid/test/anno/AnnoSimpleConstructor;
    .end annotation

    .annotation runtime Landroid/test/anno/MissingAnnotation;
    .end annotation

    .line 24
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    .line 25
    iput p1, p0, Landroid/test/anno/SimplyNoted;->mFoo:I

    .line 26
    return-void
.end method


# virtual methods
.method public foo()I
    .registers 2
    .annotation runtime Landroid/test/anno/AnnoSimpleMethod;
    .end annotation

    .annotation runtime Landroid/test/anno/MissingAnnotation;
    .end annotation

    .line 32
    const/4 v0, 0x5

    .line 34
    .local v0, "bar":I
    return v0
.end method
