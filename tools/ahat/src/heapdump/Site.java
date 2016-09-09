/*
 * Copyright (C) 2015 The Android Open Source Project
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

import com.android.tools.perflib.heap.StackFrame;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class Site {
  // The site that this site was directly called from.
  // mParent is null for the root site.
  private Site mParent;

  // A description of the Site. Currently this is used to uniquely identify a
  // site within its parent.
  private String mName;

  // To identify this site, we pick a stack trace that includes the site.
  // mId is the id of an object allocated at that stack trace, and mDepth
  // is the number of calls between this site and the innermost site of
  // allocation of the object with mId.
  // For the root site, mId is 0 and mDepth is 0.
  private long mId;
  private int mDepth;

  // Mapping from heap name to the total size of objects allocated in this
  // site (including child sites) on the given heap.
  private Map<String, Long> mSizesByHeap;

  // Mapping from child site name to child site.
  private Map<String, Site> mChildren;

  // List of all objects allocated in this site (including child sites).
  private List<AhatInstance> mObjects;
  private List<ObjectsInfo> mObjectsInfos;
  private Map<AhatHeap, Map<AhatClassObj, ObjectsInfo>> mObjectsInfoMap;

  public static class ObjectsInfo {
    public AhatHeap heap;
    public AhatClassObj classObj;
    public long numInstances;
    public long numBytes;

    public ObjectsInfo(AhatHeap heap, AhatClassObj classObj, long numInstances, long numBytes) {
      this.heap = heap;
      this.classObj = classObj;
      this.numInstances = numInstances;
      this.numBytes = numBytes;

      if (classObj == null) {
        throw new IllegalArgumentException("classObj must not be null");
      }
    }
  }

  /**
   * Construct a root site.
   */
  public Site(String name) {
    this(null, name, 0, 0);
  }

  public Site(Site parent, String name, long id, int depth) {
    mParent = parent;
    mName = name;
    mId = id;
    mDepth = depth;
    mSizesByHeap = new HashMap<String, Long>();
    mChildren = new HashMap<String, Site>();
    mObjects = new ArrayList<AhatInstance>();
    mObjectsInfos = new ArrayList<ObjectsInfo>();
    mObjectsInfoMap = new HashMap<AhatHeap, Map<AhatClassObj, ObjectsInfo>>();
  }

  /**
   * Add an instance to this site.
   * Returns the site at which the instance was allocated.
   */
  Site add(List<StackFrame> path, AhatInstance inst) {
    mObjects.add(inst);

    String heap = inst.getHeap().getName();
    mSizesByHeap.put(heap, getSize(heap) + inst.getSize());

    Map<AhatClassObj, ObjectsInfo> classToObjectsInfo = mObjectsInfoMap.get(inst.getHeap());
    if (classToObjectsInfo == null) {
      classToObjectsInfo = new HashMap<AhatClassObj, ObjectsInfo>();
      mObjectsInfoMap.put(inst.getHeap(), classToObjectsInfo);
    }

    ObjectsInfo info = classToObjectsInfo.get(inst.getClassObj());
    if (info == null) {
      info = new ObjectsInfo(inst.getHeap(), inst.getClassObj(), 0, 0);
      mObjectsInfos.add(info);
      classToObjectsInfo.put(inst.getClassObj(), info);
    }

    info.numInstances++;
    info.numBytes += inst.getSize();

    if (!path.isEmpty()) {
      String next = path.get(0).toString();
      Site child = mChildren.get(next);
      if (child == null) {
        child = new Site(this, next, inst.getId(), path.size() - 1);
        mChildren.put(next, child);
      }
      return child.add(path.subList(1, path.size()), inst);
    } else {
      return this;
    }
  }

  // Get the size of a site for a specific heap.
  public long getSize(String heap) {
    Long val = mSizesByHeap.get(heap);
    if (val == null) {
      return 0;
    }
    return val;
  }

  /**
   * Get the list of objects allocated under this site. Includes objects
   * allocated in children sites.
   */
  public Collection<AhatInstance> getObjects() {
    return mObjects;
  }

  public List<ObjectsInfo> getObjectsInfos() {
    return mObjectsInfos;
  }

  // Get the combined size of the site for all heaps.
  public long getTotalSize() {
    long size = 0;
    for (Long val : mSizesByHeap.values()) {
      size += val;
    }
    return size;
  }

  /**
   * Return the site this site was called from.
   * Returns null for the root site.
   */
  public Site getParent() {
    return mParent;
  }

  public String getName() {
    return mName;
  }

  // Returns the id of some object allocated in this site.
  public long getId() {
    return mId;
  }

  // Returns the number of frames between this site and the site where the
  // object with id getId() was allocated.
  // by getId().
  public int getDepth() {
    return mDepth;
  }

  public List<Site> getChildren() {
    return new ArrayList<Site>(mChildren.values());
  }

  // Get the child at the given path relative to this site.
  // Returns null if no such child found.
  Site getChild(Iterator<StackFrame> path) {
    if (path.hasNext()) {
      String next = path.next().toString();
      Site child = mChildren.get(next);
      return (child == null) ? null : child.getChild(path);
    } else {
      return this;
    }
  }
}
