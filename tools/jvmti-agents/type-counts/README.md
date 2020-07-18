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
07-17 18:44:30.234  6727  6727 I droid.twitter_: TYPECOUNT: Printing 100 most common types
07-17 18:44:30.234  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Object;  317863
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/String;  77594
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$Node;    23532
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [J  20221
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/Reference;   18731
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Class;   18048
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Ljava/lang/Object; 16276
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/PhantomReference;    8395
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lsun/misc/Cleaner;  8394
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [I  8196
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Llibcore/util/NativeAllocationRegistry$CleanerThunk;        8115
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractCollection;      6329
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Number;  5796
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/LinkedHashMap$LinkedHashMapEntry;        5741
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractList;    5223
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/ArrayList;       4678
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/io/File;      4628
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/SoftReference;       4519
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Ljava/io/File;     4191
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Integer; 3999
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/Rect;     3996
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/WeakReference;       3771
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractMap;     3556
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap; 3185
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [C  3128
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/Hashtable$HashtableEntry;        3100
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Ljava/util/HashMap$Node;   2878
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/util/CaseInsensitiveString;    2293
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/bumptech/glide/disklrucache/DiskLruCache$Entry;        2095
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/concurrent/ConcurrentHashMap$Node;       2055
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/FinalizerReference;  2046
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/RenderNode;       1956
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/ViewAnimationHostBridge;      1953
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Enum;    1895
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/Paint;    1870
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Container;        1856
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/View; 1803
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [B  1796
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$LayoutParams;       1787
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$MarginLayoutParams; 1733
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table;    1565
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/Drawable;        1398
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/google/android/gms/internal/measurement/zzsm;  1386
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/accessibility/WeakSparseArray$WeakReferenceWithId;    1361
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout$LayoutParams;  1283
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes$Name;     1252
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes;  1232
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Ljava/lang/String; 1226
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$ServiceKey; 1175
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/Drawable$ConstantState;  997
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Landroid/view/View;        953
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/util/ArrayMap;     953
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/RectF;    945
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe;        932
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/Long;    926
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup;    890
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$4;  890
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractSet;     888
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lsun/util/locale/LocaleObjectCache$CacheEntry;      883
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lsun/util/locale/BaseLocale$Key;    866
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VObject;  852
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table16;  841
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/util/LongSparseLongArray;  805
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/TreeMap$TreeMapEntry;    754
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/Date;    747
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table1632;        723
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/text/TextPaint;    672
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatBackgroundHelper;       648
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/Matrix;   609
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout;       607
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ltwitter4j/EntityIndex;     591
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ltwitter4j/TwitterResponseImpl;     556
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [Ltwitter4j/URLEntity;      554
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe$FloatKeyframe;  524
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VGroup;   524
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Llibcore/util/NativeAllocationRegistry$CleanerRunner;       522
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/Arrays$ArrayList;        504
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/animation/PropertyValuesHolder;    486
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$KeySet;  469
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/animation/KeyframeSet;     466
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/AbstractStringBuilder;   461
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$ResourceCache$Level;      455
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1Object;      444
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/lang/StringBuilder;   444
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1Primitive;   444
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1ObjectIdentifier;    441
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: [F  436
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/widget/TextView;   425
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/nio/Buffer;   424
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextViewAutoSizeHelper; 413
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextHelper;     413
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextClassifierHelper;   409
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextView;       408
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe$IntKeyframe;    408
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$Service;    407
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/android/internal/util/VirtualRefBasePtr;       401
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/nio/ByteBuffer;       400
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Landroid/view/View$ListenerInfo;    387
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Lcom/android/icu/util/regex/MatcherNative;  382
07-17 18:44:30.235  6727  6727 I droid.twitter_: TYPECOUNT: Ljava/util/regex/Matcher;   382
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Printing 100 most common types
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Object;  317863
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/String;  77594
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$Node;    23532
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: [J  20221
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/Reference;   18731
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Class;   18048
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: [Ljava/lang/Object; 16276
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/PhantomReference;    8395
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: Lsun/misc/Cleaner;  8394
07-17 18:44:30.999  6727  6737 I droid.twitter_: TYPECOUNT: [I  8196
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Llibcore/util/NativeAllocationRegistry$CleanerThunk;        8115
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractCollection;      6329
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Number;  5796
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/LinkedHashMap$LinkedHashMapEntry;        5741
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractList;    5223
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/ArrayList;       4678
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/io/File;      4628
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/SoftReference;       4519
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [Ljava/io/File;     4191
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Integer; 3999
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/Rect;     3996
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/WeakReference;       3771
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractMap;     3556
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap; 3185
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [C  3128
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/Hashtable$HashtableEntry;        3100
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [Ljava/util/HashMap$Node;   2878
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/util/CaseInsensitiveString;    2293
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/bumptech/glide/disklrucache/DiskLruCache$Entry;        2095
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/concurrent/ConcurrentHashMap$Node;       2055
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/ref/FinalizerReference;  2046
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/RenderNode;       1956
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/ViewAnimationHostBridge;      1953
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Enum;    1895
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/Paint;    1870
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Container;        1856
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/View; 1803
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [B  1796
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$LayoutParams;       1787
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$MarginLayoutParams; 1733
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table;    1565
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/Drawable;        1398
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/google/android/gms/internal/measurement/zzsm;  1386
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/accessibility/WeakSparseArray$WeakReferenceWithId;    1361
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout$LayoutParams;  1283
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes$Name;     1252
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/jar/Attributes;  1232
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [Ljava/lang/String; 1226
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$ServiceKey; 1175
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/Drawable$ConstantState;  997
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [Landroid/view/View;        953
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/util/ArrayMap;     953
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/RectF;    945
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe;        932
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/Long;    926
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup;    890
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/ViewGroup$4;  890
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/AbstractSet;     888
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lsun/util/locale/LocaleObjectCache$CacheEntry;      883
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lsun/util/locale/BaseLocale$Key;    866
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VObject;  852
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table16;  841
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/util/LongSparseLongArray;  805
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/TreeMap$TreeMapEntry;    754
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/Date;    747
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$Table1632;        723
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/text/TextPaint;    672
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatBackgroundHelper;       648
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/Matrix;   609
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/widget/LinearLayout;       607
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ltwitter4j/EntityIndex;     591
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ltwitter4j/TwitterResponseImpl;     556
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [Ltwitter4j/URLEntity;      554
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe$FloatKeyframe;  524
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/graphics/drawable/VectorDrawable$VGroup;   524
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Llibcore/util/NativeAllocationRegistry$CleanerRunner;       522
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/Arrays$ArrayList;        504
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/animation/PropertyValuesHolder;    486
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/HashMap$KeySet;  469
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/animation/KeyframeSet;     466
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/AbstractStringBuilder;   461
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/icu/impl/ICUResourceBundleReader$ResourceCache$Level;      455
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1Object;      444
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/lang/StringBuilder;   444
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1Primitive;   444
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/android/org/bouncycastle/asn1/ASN1ObjectIdentifier;    441
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: [F  436
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/widget/TextView;   425
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/nio/Buffer;   424
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextViewAutoSizeHelper; 413
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextHelper;     413
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextClassifierHelper;   409
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroidx/appcompat/widget/AppCompatTextView;       408
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/animation/Keyframe$IntKeyframe;    408
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/security/Provider$Service;    407
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/android/internal/util/VirtualRefBasePtr;       401
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/nio/ByteBuffer;       400
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Landroid/view/View$ListenerInfo;    387
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Lcom/android/icu/util/regex/MatcherNative;  382
07-17 18:44:31.000  6727  6737 I droid.twitter_: TYPECOUNT: Ljava/util/regex/Matcher;   382
```
