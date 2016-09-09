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

import com.android.tools.perflib.heap.ArrayInstance;
import com.android.tools.perflib.heap.ClassInstance;
import com.android.tools.perflib.heap.ClassObj;
import com.android.tools.perflib.heap.Heap;
import com.android.tools.perflib.heap.Instance;
import com.android.tools.perflib.heap.Snapshot;
import java.util.List;
import java.util.Map;

/**
 * Helper object used when initializing AhatInstances.
 * This keeps track of mappings from instance ids and heap names to
 * corresponding unique AhatInstance and AhatHeap objects when initializing
 * the AhatInstances.
 */
public class InitializeHelper {
  private final Snapshot mSnapshot;
  private final Map<Long, AhatInstance> mInstances;
  private final Map<String, AhatClassObj> mClasses;
  private final List<AhatHeap> mHeaps;

  InitializeHelper(Snapshot snapshot, Map<Long, AhatInstance> instances,
      Map<String, AhatClassObj> classes, List<AhatHeap> heaps) {
    mSnapshot = snapshot;
    mInstances = instances;
    mClasses = classes;
    mHeaps = heaps;
    for (Heap heap : snapshot.getHeaps()) {
      mHeaps.add(new AhatHeap(heap.getName(), snapshot.getHeapIndex(heap)));
    }
  }

  AhatInstance getInstance(long id) {
    AhatInstance ahat = mInstances.get(id);
    if (ahat == null) {
      Instance inst = mSnapshot.findInstance(id);
      if (inst instanceof ClassInstance) {
        ahat = new AhatClassInstance();
        mInstances.put(id, ahat);
      } else if (inst instanceof ArrayInstance) {
        ahat = new AhatArrayInstance();
        mInstances.put(id, ahat);
      } else if (inst instanceof ClassObj) {
        ahat = new AhatClassObj();
        mClasses.put(((ClassObj)inst).getClassName(), ahat.asClassObj());
        mInstances.put(id, ahat);
      }
    }
    return ahat;
  }

  AhatClassObj getClassObj(long id) {
    AhatInstance ahat = getInstance(id);
    return ahat == null ? null : ahat.asClassObj();
  }

  AhatHeap getHeap(Heap heap) {
    return getHeap(heap.getName());
  }

  AhatHeap getHeap(String name) {
    // We expect a small number of heaps (maybe 3 or 4 total), so a linear
    // search should be acceptable here performance wise.
    for (AhatHeap heap : mHeaps) {
      if (heap.getName().equals(name)) {
        return heap;
      }
    }
    return null;
  }

  // Return the Value for the given perflib value object.
  Value getValue(Object value) {
    if (value instanceof Instance) {
      value = getInstance(((Instance)value).getId());
    }
    return value == null ? null : new Value(value);
  }

  List<AhatHeap> getHeaps() {
    return mHeaps;
  }
}
