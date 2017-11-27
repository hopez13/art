.class LTestSync;
.super Ljava/lang/Object;
.source "Main.java"


# direct methods
.method constructor <init>()V
    .registers 1

    .prologue
    .line 6
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public static run()V
    .registers 20

    .prologue
    .line 8
    const-string v1, "First"

    .line 9
    const-string v2, "Second"

    move-object v3, v1
    const v1, 0x1

    .line 10
    monitor-enter v3

    move-object v4, v3
    const v3, 0x3

    move-object v5, v2
    const v2, 0x2

    .line 11
    :try_start_b
    monitor-enter v5
    :try_end_c

    move-object v6, v5
    const v5, 0x5
    move-object v7, v6

    .catchall {:try_start_b .. :try_end_c} :catchall_15

    .line 12
    :try_start_c
    invoke-static {}, LMain;->run()V

    .line 13
    monitor-exit v6
    :try_end_10
    .catchall {:try_start_c .. :try_end_10} :catchall_12

    .line 14
    :try_start_10
    monitor-exit v4
    :try_end_11
    .catchall {:try_start_10 .. :try_end_11} :catchall_15

    .line 15
    return-void

    .line 13
    :catchall_12
    move-exception v0

    :try_start_13
    monitor-exit v6
    :try_end_14
    .catchall {:try_start_13 .. :try_end_14} :catchall_12

    :try_start_14
    throw v0

    .line 14
    :catchall_15
    move-exception v0

    monitor-exit v4
    :try_end_17
    .catchall {:try_start_14 .. :try_end_17} :catchall_15

    throw v0
.end method
