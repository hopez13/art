/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.ahat.heapdump;

import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.RootObj;

import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

public abstract class AhatInstance {
  private long mId;
  private long mSize;
  private long mTotalRetainedSize;
  private Map<AhatHeap, Long> mRetainedSizes;
  private AhatHeap mHeap;
  private AhatInstance mImmediateDominator;
  private AhatInstance mNextInstanceToGcRoot;
  private AhatClassObj mClassObj;
  private List<AhatInstance> mHardReverseReferences;
  private List<AhatInstance> mSoftReverseReferences;
  private boolean mIsStronglyReachable;
  private Site mSite;


  // If this instance is a root, mRootTypes contains a set of the root types.
  // If this instance is not a root, mRootTypes is null.
  private Collection<String> mRootTypes;

  // List of instances this instance immediately dominates.
  private List<AhatInstance> mDominated = new ArrayList<AhatInstance>();
  
  void initialize(InitializeHelper helper, Instance inst) {
    mId = inst.getId();
    mSize = inst.getSize();
    mTotalRetainedSize = inst.getTotalRetainedSize();

    mRetainedSizes = new HashMap<AhatHeap, Long>();
    for (AhatHeap heap : helper.getHeaps()) {
      mRetainedSizes.put(heap, inst.getRetainedSize(heap.getIndex()));
    }

    mHeap = helper.getHeap(inst.getHeap());

    Instance dom = inst.getImmediateDominator();
    if (dom == null || dom instanceof RootObj) {
      mImmediateDominator = null;
    } else {
      mImmediateDominator = helper.getInstance(dom.getId());
      mImmediateDominator.addDominated(this);
    }

    Instance parent = inst.getNextInstanceToGcRoot();
    if (parent == null || parent instanceof RootObj) {
      mNextInstanceToGcRoot = null;
    } else {
      mNextInstanceToGcRoot = helper.getInstance(parent.getId());
    }

    ClassObj clsObj = inst.getClassObj();
    if (clsObj != null) {
      mClassObj = helper.getClassObj(clsObj.getId());
    }

    mHardReverseReferences = new ArrayList<AhatInstance>();
    for (Instance ref : inst.getHardReverseReferences()) {
      mHardReverseReferences.add(helper.getInstance(ref.getId()));
    }

    mSoftReverseReferences = new ArrayList<AhatInstance>();
    List<Instance> refs = inst.getSoftReverseReferences();
    if (refs != null) {
      for (Instance ref : refs) {
        mSoftReverseReferences.add(helper.getInstance(ref.getId()));
      }
    }
    
    mIsStronglyReachable = inst.isReachable();
  }

  /**
   * Return a unique identifier for the instance.
   */
  public long getId() {
    return mId;
  }

  /**
   * Return the shallow number of bytes this object takes up.
   */
  public long getSize() {
    return mSize;
  }

  /**
   * Return the number of bytes belonging to the given heap that this instance
   * retains.
   */
  public long getRetainedSize(AhatHeap heap) {
    return mRetainedSizes.get(heap);
  }

  /**
   * Return the total number of bytes this instance retains.
   */
  public long getTotalRetainedSize() {
    return mTotalRetainedSize;
  }

  /**
   * Returns the heap that this instance is allocated on.
   */
  public AhatHeap getHeap() {
    return mHeap;
  }

  /**
   * Returns true if the given instance is marked as a root instance.
   */
  public boolean isRoot() {
    return mRootTypes != null;
  }

  void addRootType(String type) {
    if (mRootTypes == null) {
      mRootTypes = new HashSet<String>();
    }
    mRootTypes.add(type);
  }

  public boolean isStronglyReachable() {
    return mIsStronglyReachable;
  }

  /**
   * Returns the immediate dominator of this instance.
   * Returns null if this is a root instance.
   */
  public AhatInstance getImmediateDominator() {
    return mImmediateDominator;
  }

  /**
   * Returns a list of those objects immediately dominated by the given
   * instance.
   */
  public List<AhatInstance> getDominated() {
    return mDominated;
  }

  void addDominated(AhatInstance inst) {
    mDominated.add(inst);
  }

  /**
   * Returns the site where this instance was allocated.
   */
  public Site getSite() {
    return mSite;
  }

  void setSite(Site site) {
    mSite = site;
  }

  /**
   * Returns a list of string descriptions of the root types of this object.
   * Returns null if this object is not a root.
   */
  public Collection<String> getRootTypes() {
    return mRootTypes;
  }

  /**
   * Returns true if the given instance is a class object
   */
  public boolean isClassObj() {
    // AhatClassObj overrides this method to return true. Otherwise this isn't
    // a class object.
    return false;
  }

  /**
   * Returns this as an AhatClassObj if this is an AhatClassObj.
   * Returns null if this is not an AhatClassObj.
   */
  public AhatClassObj asClassObj() {
    // AhatClassObj overrides this method to return something useful.
    // Otherwise this isn't a class object.
    return null;
  }

  /**
   * Returns the class object instance for the class of this object.
   */
  public AhatClassObj getClassObj() {
    return mClassObj;
  }

  /**
   * Returns true if the given instance is an array instance
   */
  public boolean isArrayInstance() {
    // AhatArrayInstance overrides this method to return true. Otherwise this
    // isn't an array instance.
    return false;
  }

  /**
   * Returns this as an AhatArrayInstance if this is an AhatArrayInstance.
   * Returns null if this is not an AhatArrayInstance.
   */
  public AhatArrayInstance asArrayInstance() {
    // AhatArrayInstance overrides this method to return something useful.
    // Otherwise this isn't an array instance.
    return null;
  }

  /**
   * Assuming this instance represents a NativeAllocation, return information
   * about the native allocation. Returns null if the given instance does not
   * represent a native allocation.
   */
  public NativeAllocation getNativeAllocation() {
    // Note: The main implementation is in AhatClassInstance.
    return null;
  }

  /**
   * Returns true if the given instance is a class instance
   */
  public boolean isClassInstance() {
    return false;
  }

  /**
   * Returns this as an AhatClassInstance if this is an AhatClassInstance.
   * Returns null if this is not an AhatClassInstance.
   */
  public AhatClassInstance asClassInstance() {
    return null;
  }

  /**
   * Return the referent associated with this instance.
   * This is relevent for instances of java.lang.ref.Reference.
   * Returns null if the instance has no referent associated with it.
   */
  public AhatInstance getReferent() {
    // Overridden by AhatClassInstance.
    return null;
  }

  /**
   * Returns a list of objects with hard references to this object.
   */
  public List<AhatInstance> getHardReverseReferences() {
    return mHardReverseReferences;
  }

  /**
   * Returns a list of objects with soft references to this object.
   */
  public List<AhatInstance> getSoftReverseReferences() {
    return mSoftReverseReferences;
  }

  /**
   * Returns the value of a field of an instance.
   * Returns null if the field value is null, the field couldn't be read, or
   * there are multiple fields with the same name.
   *
   * TODO: Move this to AhatClassInstance?
   */
  public Value getField(String fieldName) {
    // AhatClassInstance overrides this method. If this isn't an
    // AhatClassInstance, then you can't get a field.
    return null;
  }

  /**
   * Read an int field of an instance.
   * The field is assumed to be an int type.
   * Returns <code>def</code> if the field value is not an int or could not be
   * read.
   */
  Integer getIntField(String fieldName, Integer def) {
    Value value = getField(fieldName);
    if (value == null || !value.isInteger()) {
      return def;
    }
    return value.asInteger();
  }

  /**
   * Read a long field of this instance.
   * The field is assumed to be a long type.
   * Returns <code>def</code> if the field value is not an long or could not
   * be read.
   */
  Long getLongField(String fieldName, Long def) {
    Value value = getField(fieldName);
    if (value == null || !value.isLong()) {
      return def;
    }
    return value.asLong();
  }

  /**
   * Assuming inst represents a DexCache object, return the dex location for
   * that dex cache. Returns null if the given instance doesn't represent a
   * DexCache object or the location could not be found.
   * If maxChars is non-negative, the returned location is truncated to
   * maxChars in length.
   */
  public String getDexCacheLocation(int maxChars) {
    return null;
  }

  /**
   * Return the bitmap instance associated with this object, or null if there
   * is none. This works for android.graphics.Bitmap instances and their
   * underlying Byte[] instances.
   */
  public AhatInstance getAssociatedBitmapInstance() {
    return null;
  }

  /**
   * Read the string value from this instance.
   * Returns null if this object can't be interpreted as a string.
   * The returned string is truncated to maxChars characters.
   * If maxChars is negative, the returned string is not truncated.
   */
  public String asString(int maxChars) {
    // By default instances can't be interpreted as a string. This method is
    // overridden by AhatClassInstance and AhatArrayInstance for those cases
    // when an instance can be interpreted as a string.
    return null;
  }

  /**
   * Reads the string value from an hprof Instance.
   * Returns null if the object can't be interpreted as a string.
   */
  public String asString() {
    return asString(-1);
  }

  /**
   * Return the bitmap associated with the given instance, if any.
   * This is relevant for instances of android.graphics.Bitmap and byte[].
   * Returns null if there is no bitmap associated with the given instance.
   */
  public BufferedImage asBitmap() {
    return null;
  }

  /**
   * Returns a sample path from a GC root to this instance.
   * This instance is included as the last element of the path with an empty
   * field description.
   */
  public List<PathElement> getPathFromGcRoot() {
    List<PathElement> path = new ArrayList<PathElement>();

    AhatInstance dom = this;
    for (PathElement elem = new PathElement(this, ""); elem != null;
        elem = getNextPathElementToGcRoot(elem.instance)) {
      if (elem.instance.equals(dom)) {
        elem.isDominator = true;
        dom = dom.getImmediateDominator();
      }
      path.add(elem);
    }
    Collections.reverse(path);
    return path;
  }

  /**
   * Returns the next instance to GC root from this object and a string
   * description of which field of that object refers to the given instance.
   * Returns null if the given instance has no next instance to the gc root.
   */
  private static PathElement getNextPathElementToGcRoot(AhatInstance inst) {
    AhatInstance parent = inst.mNextInstanceToGcRoot;
    if (parent == null) {
      return null;
    }

    // Search the parent for the reference to the child.
    // TODO: This seems terribly inefficient. Can we use data structures to
    // help us here? Perhaps a single heap traversal that records the
    // information in one pass would be good.
    String description = ".???";
    if (parent.isArrayInstance()) {
      AhatArrayInstance array = parent.asArrayInstance();
      Value[] values = array.getValues();
      for (int i = 0; i < values.length; i++) {
        if (values[i] != null && values[i].isAhatInstance()) {
          AhatInstance ref = values[i].asAhatInstance();
          if (ref == inst) {
            description = String.format("[%d]", i);
            break;
          }
        }
      }
    } else if (parent.isClassObj()) {
      AhatClassObj cls = parent.asClassObj();
      for (FieldValue field : cls.getStaticFieldValues()) {
        Value value = field.getValue();
        if (value != null && value.isAhatInstance()) {
          AhatInstance ref = field.getValue().asAhatInstance();
          if (ref == inst) {
            description = "." + field.getName();
            break;
          }
        }
      }
    } else if (parent.isClassInstance()) {
      AhatClassInstance obj = parent.asClassInstance();
      for (FieldValue fields : obj.getValues()) {
        Value value = fields.getValue();
        if (value != null && value.isAhatInstance()) {
          AhatInstance ref = fields.getValue().asAhatInstance();
          if (ref == inst) {
            description = "." + fields.getName();
            break;
          }
        }
      }
    }
    return new PathElement(parent, description);
  }

  /** Returns a human-readable identifier for this object.
   * For class objects, the string is the class name.
   * For class instances, the string is the class name followed by '@' and the
   * hex id of the instance.
   * For array instances, the string is the array type followed by the size in
   * square brackets, followed by '@' and the hex id of the instance.
   */
  @Override public abstract String toString();

  /**
   * Read the byte[] value from an hprof Instance.
   * Returns null if the instance is not a byte array.
   */
  byte[] asByteArray() {
    return null;
  }
}
