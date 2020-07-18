# typecount

typecount is a JVMTI agent designed to count the instances of various types.
It will print the 100 most common types and the number of instances of each
of them.

# Usage
### Build
>    `m libtypecount libtypecounts`

The libraries will be built for 32-bit, 64-bit, host and target. Below examples
assume you want to use the 64-bit version.

### Command Line

The agent is loaded using -agentpath like normal. It takes no arguments.

#### ART
```shell
art -Xplugin:$ANDROID_HOST_OUT/lib64/libopenjdkjvmti.so '-agentpath:libtypecount.so' -cp tmp/java/helloworld.dex -Xint helloworld
```

* `-Xplugin` and `-agentpath` need to be used, otherwise the agent will fail during init.
* If using `libartd.so`, make sure to use the debug version of jvmti.

```shell
adb shell setenforce 0

adb push $ANDROID_PRODUCT_OUT/system/lib64/libtypecounts.so /data/local/tmp/

adb shell am start-activity --attach-agent '/data/local/tmp/libtypecounts.so' some.debuggable.apps/.the.app.MainActivity
```

#### RI
>    `java '-agentpath:libtypecount.so' -cp tmp/helloworld/classes helloworld`

### Printing the Results
All statistics gathered during the trace are printed automatically when the
program normally exits. In the case of Android applications, they are always
killed, so we need to manually print the results.

>    `kill -SIGQUIT $(pid com.littleinc.orm_benchmark)`

Will initiate a dump of the counts (to logcat).

The dump will look something like this.

```
07-17 18:10:54.835 25720 25720 I droid.twitter_: TYPECOUNT: Printing 100 most common types
07-17 18:10:54.835 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/String;  67637
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [J  19444
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Class;   18061
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [Ljava/lang/Object; 14739
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$Node;    14496
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [I  6698
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/LinkedHashMap$LinkedHashMapEntry;        5428
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/misc/Cleaner;  4690
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/io/File;      4616
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Llibcore/util/NativeAllocationRegistry$CleanerThunk;        4413
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [Ljava/io/File;     4189
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Integer; 3521
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/SoftReference;       3469
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/ArrayList;       3230
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/Hashtable$HashtableEntry;        3099
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/Rect;     2526
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap; 2314
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/util/CaseInsensitiveString;    2293
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [Ljava/util/HashMap$Node;   2199
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Lcom/bumptech/glide/disklrucache/DiskLruCache$Entry;        2094
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/concurrent/ConcurrentHashMap$Node;       1984
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [C  1885
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [B  1563
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Object;  1333
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/WeakReference;       1255
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes$Name;     1252
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes;  1232
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/RenderNode;       1209
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/view/ViewAnimationHostBridge;      1207
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$ServiceKey; 1173
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/FinalizerReference;  1115
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [Ljava/lang/String; 1012
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/util/locale/LocaleObjectCache$CacheEntry;      883
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/util/locale/BaseLocale$Key;    863
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table16;  841
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/Paint;    758
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout$LayoutParams;  744
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/TreeMap$TreeMapEntry;    737
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table1632;        723
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/util/ArrayMap;     612
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/RectF;    590
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: [Landroid/view/View;        587
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/util/LongSparseLongArray;  579
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/view/accessibility/WeakSparseArray$WeakReferenceWithId;    555
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Long;    532
07-17 18:10:54.836 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$4;  531
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$ResourceCache$Level;      455
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1ObjectIdentifier;    441
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/text/TextPaint;    422
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$Service;    407
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatBackgroundHelper;       406
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/Matrix;   386
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: [F  380
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/text/TransliteratorRegistry$ResourceEntry;     379
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/nio/DirectByteBuffer; 357
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout;       353
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/misc/FDBigInteger;     341
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe$FloatKeyframe;  334
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lokio/ByteString;   323
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: [Landroid/graphics/fonts/FontVariationAxis; 311
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/fonts/Font;       309
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/fonts/FontStyle;  309
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/io/FileDescriptor;    309
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ldalvik/system/ClassExt;    307
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/hardware/camera2/impl/CameraMetadataNative$Key;    300
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/hardware/camera2/utils/TypeReference$SpecializedTypeReference;     290
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/nio/DirectByteBuffer$MemoryRef;       288
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Array16;  288
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: [Z  285
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VGroup;   284
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/nio/ch/FileChannelImpl$Unmapper;       276
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/Date;    273
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Byte;    273
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lokhttp3/internal/http2/Huffman$Node;       271
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextHelper;     257
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextViewAutoSizeHelper; 257
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Short;   256
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextClassifierHelper;   254
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/view/View$ListenerInfo;    250
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lcom/android/internal/telephony/MccTable$MccEntry;  241
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lsun/security/util/ObjectIdentifier;        239
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/animation/AnimatorSet$AnimationEvent;      222
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/lang/Character$UnicodeBlock;  221
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$UString;    221
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lcom/android/internal/util/VirtualRefBasePtr;       218
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/Arrays$ArrayList;        216
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lokhttp3/internal/http2/Header;     214
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/concurrent/ConcurrentHashMap;    214
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/util/SparseArray;  207
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$KeySet;  207
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/ColorDrawable;   196
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: [Landroid/animation/PropertyValuesHolder;   190
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/media/ExifInterface$ExifTag;       189
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/animation/PropertyValuesHolder$FloatPropertyValuesHolder;  187
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Lcom/klinker/android/twitter_l/views/widgets/text/FontPrefTextView; 186
07-17 18:10:54.837 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VFullPath;        185
07-17 18:10:54.838 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/widget/FrameLayout$LayoutParams;   185
07-17 18:10:54.838 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/util/PathParser$PathData;  185
07-17 18:10:54.838 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/icu/text/UnicodeSet;       181
07-17 18:10:54.838 25720 25720 I droid.twitter_: TYPECOUNT: Ljava/util/Collections$SynchronizedMap;     176
07-17 18:10:54.838 25720 25720 I droid.twitter_: TYPECOUNT: Landroid/animation/AnimatorSet$Node;        175
```
