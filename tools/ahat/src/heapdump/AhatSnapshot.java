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

import com.android.tools.perflib.captures.MemoryMappedFileBuffer;
import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Heap;
import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.RootObj;
import com.android.tools.perflib.heap.Snapshot;
import com.android.tools.perflib.heap.StackFrame;
import com.android.tools.perflib.heap.StackTrace;

import com.google.common.collect.Lists;

import gnu.trove.TObjectProcedure;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class AhatSnapshot {
  private final Site mRootSite = new Site("ROOT");

  // Collection of objects whose immediate dominator is the SENTINEL_ROOT.
  private final List<AhatInstance> mRooted = new ArrayList<AhatInstance>();

  // Map from instance id to AhatInstance.
  private final Map<Long, AhatInstance> mInstances = new HashMap<Long, AhatInstance>();

  // Map from class name to class object.
  private final Map<String, AhatClassObj> mClasses = new HashMap<String, AhatClassObj>();

  private final List<AhatHeap> mHeaps = new ArrayList<AhatHeap>();

  private final List<NativeAllocation> mNativeAllocations = new ArrayList<NativeAllocation>();

  /**
   * Create an AhatSnapshot from an hprof file.
   */
  public static AhatSnapshot fromHprof(File hprof) throws IOException {
    Snapshot snapshot = Snapshot.createSnapshot(new MemoryMappedFileBuffer(hprof));
    snapshot.computeDominators();
    return new AhatSnapshot(snapshot);
  }

  /**
   * Constructs an AhatSnapshot for the given perflib snapshot.
   * snapshot.computeDominators must have been called before calling this
   * AhatSnapshot constructor.
   */
  private AhatSnapshot(Snapshot snapshot) {
    // Properly label the class of class objects in the perflib snapshot.
    final ClassObj javaLangClass = snapshot.findClass("java.lang.Class");
    if (javaLangClass != null) {
      for (Heap heap : snapshot.getHeaps()) {
        for (ClassObj clsObj  : heap.getClasses()) {
          if (clsObj.getClassObj() == null) {
            clsObj.setClassId(javaLangClass.getId());
          }
        }
      }
    }

    // Create ahat snapshot and instances based on the perflib snapshot and
    // instances.
    final InitializeHelper helper = new InitializeHelper(snapshot, mInstances, mClasses, mHeaps);
    for (Heap heap : snapshot.getHeaps()) {
      final AhatHeap ahatHeap = getHeap(heap.getName());

      TObjectProcedure<Instance> doInitialize = new TObjectProcedure<Instance>() {
        @Override
        public boolean execute(Instance inst) {
          AhatInstance ahat = helper.getInstance(inst.getId());
          ahat.initialize(helper, inst);

          if (inst.getImmediateDominator() == Snapshot.SENTINEL_ROOT) {
            mRooted.add(ahat);
          }

          if (ahat.isStronglyReachable()) {
            ahatHeap.addToSize(inst.getSize());

            // Update sites.
            List<StackFrame> path = Collections.emptyList();
            StackTrace stack = inst.getStack();
            if (stack != null) {
              StackFrame[] frames = stack.getFrames();
              if (frames != null && frames.length > 0) {
                path = Lists.reverse(Arrays.asList(frames));
              }
            }
            Site site = mRootSite.add(path, ahat);
            ahat.setSite(site);
          }
          return true;
        }
      };
      for (Instance instance : heap.getClasses()) {
        doInitialize.execute(instance);
      }
      heap.forEachInstance(doInitialize);
    }

    // Record the roots and their types.
    for (RootObj root : snapshot.getGCRoots()) {
      Instance inst = root.getReferredInstance();
      if (inst != null) {
        findInstance(inst.getId()).addRootType(root.getRootType().toString());
      }
    }

    // Update the native allocations.
    for (AhatInstance ahat : mInstances.values()) {
      NativeAllocation alloc = ahat.getNativeAllocation();
      if (alloc != null) {
        mNativeAllocations.add(alloc);
      }
    }
  }

  /**
   * Return the instance with given id in this snapshot.
   * Returns null if no instance with the given id is found.
   */
  public AhatInstance findInstance(long id) {
    return mInstances.get(id);
  }

  public AhatClassObj findClassObj(long id) {
    AhatInstance inst = findInstance(id);
    return inst == null ? null : inst.asClassObj();
  }

  /**
   * Returns the class object for the class with given name.
   * Returns null if there is no class object for the given name.
   * Note: This method is exposed for testing purposes.
   */
  public AhatClassObj findClass(String name) {
    return mClasses.get(name);
  }

  /**
   * Return the heap with the given name, if any.
   * Returns null if no heap with the given name could be found.
   */
  public AhatHeap getHeap(String name) {
    // We expect a small number of heaps (maybe 3 or 4 total), so a linear
    // search should be acceptable here performance wise.
    for (AhatHeap heap : getHeaps()) {
      if (heap.getName().equals(name)) {
        return heap;
      }
    }
    return null;
  }

  /**
   * Return a list of heaps in the snapshot in canonical order.
   */
  public List<AhatHeap> getHeaps() {
    return mHeaps;
  }

  /**
   * Returns a collection of instances whose immediate dominator is the
   * SENTINEL_ROOT.
   */
  public List<AhatInstance> getRooted() {
    return mRooted;
  }

  /**
   * Return a list of native allocations identified in the heap dump.
   */
  public List<NativeAllocation> getNativeAllocations() {
    return mNativeAllocations;
  }

  // Get the site associated with the given id and depth.
  // Returns the root site if no such site found.
  public Site getSite(int id, int depth) {
    AhatInstance obj = findInstance(id);
    if (obj == null) {
      return mRootSite;
    }

    Site site = obj.getSite();
    for (int i = 0; i < depth && site.getParent() != null; i++) {
      site = site.getParent();
    }
    return site;
  }
}
