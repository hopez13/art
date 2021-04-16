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

import java.util.*;

public class CollectionToArrayBenchmark {
    static final Collection<Object> ARRAY_LIST_1 = new ArrayList<Object>(mkCollection(1));
    static final Collection<Object> ARRAY_LIST_100 = new ArrayList<Object>(mkCollection(100));
    static final Collection<Object> ARRAY_LIST_10_000 = new ArrayList<Object>(mkCollection(10000));
    static final Collection<Object> LINKED_LIST_1 = new LinkedList<Object>(mkCollection(1));
    static final Collection<Object> LINKED_LIST_100 = new LinkedList<Object>(mkCollection(100));
    static final Collection<Object> LINKED_LIST_1000 = new LinkedList<Object>(mkCollection(1000));
    static final Collection<Object> ARRAYS_AS_LIST_LIST_1 = Arrays.asList(mkCollection(1).toArray());
    static final Collection<Object> ARRAYS_AS_LIST_LIST_100 = Arrays.asList(mkCollection(100).toArray());
    static final Collection<Object> ARRAYS_AS_LIST_LIST_10_000 = Arrays.asList(mkCollection(10000).toArray());

    public static Collection<Object> mkCollection(int len) {
      Collection<Object> out = new ArrayList<>(len);
      for (int i= 0; i < len; i++) {
        out.add(new Object());
      }
      return out;
    }

    // ARRAY LIST
    public void timeNew0ArrayList1(int cnt) {
      Collection<Object> col = ARRAY_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0ArrayList100(int cnt) {
      Collection<Object> col = ARRAY_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0ArrayList10_000(int cnt) {
      Collection<Object> col = ARRAY_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeArrayList1(int cnt) {
      Collection<Object> col = ARRAY_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeArrayList100(int cnt) {
      Collection<Object> col = ARRAY_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeArrayList10_000(int cnt) {
      Collection<Object> col = ARRAY_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeCallSizeArrayList1(int cnt) {
      Collection<Object> col = ARRAY_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeArrayList100(int cnt) {
      Collection<Object> col = ARRAY_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeArrayList10_000(int cnt) {
      Collection<Object> col = ARRAY_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timePassSizeArrayList1(int cnt) {
      Collection<Object> col = ARRAY_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 1);
      }
    }
    public void timePassSizeArrayList100(int cnt) {
      Collection<Object> col = ARRAY_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 100);
      }
    }
    public void timePassSizeArrayList10_000(int cnt) {
      Collection<Object> col = ARRAY_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 10000);
      }
    }

    // Linked List
    public void timeNew0LinkedList1(int cnt) {
      Collection<Object> col = LINKED_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0LinkedList100(int cnt) {
      Collection<Object> col = LINKED_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0LinkedList10_000(int cnt) {
      Collection<Object> col = LINKED_LIST_1000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeLinkedList1(int cnt) {
      Collection<Object> col = LINKED_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeLinkedList100(int cnt) {
      Collection<Object> col = LINKED_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeLinkedList10_000(int cnt) {
      Collection<Object> col = LINKED_LIST_1000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeCallSizeLinkedList1(int cnt) {
      Collection<Object> col = LINKED_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeLinkedList100(int cnt) {
      Collection<Object> col = LINKED_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeLinkedList10_000(int cnt) {
      Collection<Object> col = LINKED_LIST_1000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timePassSizeLinkedList1(int cnt) {
      Collection<Object> col = LINKED_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 1);
      }
    }
    public void timePassSizeLinkedList100(int cnt) {
      Collection<Object> col = LINKED_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 100);
      }
    }
    public void timePassSizeLinkedList10_000(int cnt) {
      Collection<Object> col = LINKED_LIST_1000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 1000);
      }
    }


    // Arrays.asList LIST
    public void timeNew0ArraysAsListList1(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0ArraysAsListList100(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeNew0ArraysAsListList10_000(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArrayNew0(col);
      }
    }
    public void timeArraysAsListList1(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeArraysAsListList100(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeArraysAsListList10_000(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArray(col);
      }
    }
    public void timeCallSizeArraysAsListList1(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeArraysAsListList100(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timeCallSizeArraysAsListList10_000(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizeCall(col);
      }
    }
    public void timePassSizeArraysAsListList1(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_1;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 1);
      }
    }
    public void timePassSizeArraysAsListList100(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_100;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 100);
      }
    }
    public void timePassSizeArraysAsListList10_000(int cnt) {
      Collection<Object> col = ARRAYS_AS_LIST_LIST_10_000;
      for(int i = 0; i < cnt; i++) {
        $noinline$toArraySizePass(col, 10000);
      }
    }

    static Object[] $noinline$toArrayNew0(Collection<Object> col) {
      return col.toArray(new Object[0]);
    }
    static Object[] $noinline$toArray(Collection<Object> col) {
      return col.toArray();
    }
    static Object[] $noinline$toArraySizePass(Collection<Object> col, int size) {
      return col.toArray(new Object[size]);
    }
    static Object[] $noinline$toArraySizeCall(Collection<Object> col) {
      return col.toArray(new Object[col.size()]);
    }
}
