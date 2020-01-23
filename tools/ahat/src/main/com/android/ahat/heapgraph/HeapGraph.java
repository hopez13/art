/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.ahat.heapgraph;

import com.android.ahat.heapdump.AhatClassInstance;
import com.android.ahat.heapdump.AhatClassObj;
import com.android.ahat.heapdump.AhatHeap;
import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Field;
import com.android.ahat.heapdump.FieldValue;
import com.android.ahat.heapdump.Instances;
import com.android.ahat.heapdump.Reachability;
import com.android.ahat.heapdump.RootType;
import com.android.ahat.heapdump.Site;
import com.android.ahat.heapdump.SuperRoot;
import com.android.ahat.heapdump.Type;
import com.android.ahat.heapdump.Value;
import com.android.ahat.progress.Progress;

import com.google.perfetto.protos.TraceProto;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.DataFormatException;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

public class HeapGraph {

    private static final String DESCRIPTION = "Loading heap graph";
    private static final String HEAP_NAME = "default";
    private static final String SITE_NAME = "ROOT";
    private static final String CLASS_METACLASS_NAME = "java.lang.Class<java.lang.Object>";
    private static final String CLASS_NAME_PREFIX = "java.lang.Class";
    private static final String CLASS_FIELD_NAME = "java.lang.Object.shadow$_klass_";

    public static AhatSnapshot load(File proto, Progress progress, Reachability retained)
            throws IOException, DataFormatException {

        progress.start(DESCRIPTION, 1);
        progress.advance();
        progress.done();

        SuperRoot superRoot = new SuperRoot();

        AhatHeap theHeap = new AhatHeap(HEAP_NAME, 0);
        List<AhatHeap> heaps = new ArrayList<AhatHeap>();
        heaps.add(theHeap);

        Site rootSite = new Site(SITE_NAME);

        List<AhatInstance> inst = new ArrayList<>();
        Map<Long, String> classNames = new HashMap<>();
        Map<Long, String> fieldNames = new HashMap<>();
        Map<Long, TraceProto.HeapGraphObject> objects = new HashMap<>();
        List<TraceProto.HeapGraphRoot> roots = new ArrayList<>();
        Map<Long, AhatClassObj> ahatClasses = new HashMap<>();
        Map<Long, AhatClassInstance> ahatInstances = new HashMap<>();

        AhatClassObj javaLangClass = null;

        TraceProto.Trace trace = TraceProto.Trace.parseFrom(
                new GZIPInputStream(new FileInputStream(proto)));

        // Damn you COMPRESSED_PACKETS!
        List<TraceProto.HeapGraph> heapGraphs = new ArrayList<>();
        extractHeapGraphPackets(heapGraphs, trace);

        for (TraceProto.HeapGraph graph : heapGraphs) {

            for (TraceProto.HeapGraphObject object : graph.getObjectsList()) {
                final long objectId = object.getId();
                objects.put(objectId, object);
            }

            roots.addAll(graph.getRootsList());

            for (TraceProto.InternedString s : graph.getTypeNamesList()) {
                final String name = s.getStr().toStringUtf8();
                classNames.put(s.getIid(), name);
            }

            for (TraceProto.InternedString s : graph.getFieldNamesList()) {
                final String name = s.getStr().toStringUtf8();
                fieldNames.put(s.getIid(), name);
            }
        }

        // Create all of the classes first
        for (TraceProto.HeapGraphObject object : objects.values()) {
            final String className = classNames.get(object.getTypeId());
            if (className.startsWith("java.lang.Class")) {
                int begin = className.indexOf('<');
                int end = className.indexOf('>', begin);
                String typeName = null;
                if (begin == -1 || end == -1) {
                    typeName = className;
                } else {
                    typeName = className.substring(begin + 1, end);
                }
                AhatClassObj classObj = new AhatClassObj(object.getId(), typeName);
                classObj.initialize(null, new FieldValue[0]);
                ahatClasses.put(object.getId(), classObj);
                inst.add(classObj);

                if (className.equals(CLASS_METACLASS_NAME)) {
                    javaLangClass = classObj;
                }
            }
        }

        if (javaLangClass == null) {
            throw new RuntimeException("couldn't find java.lang.Class object");
        }
        for (AhatClassObj classObj : ahatClasses.values()) {
            classObj.initialize(theHeap, rootSite, javaLangClass);
        }

        // Create all of the instances next
        for (TraceProto.HeapGraphObject object : objects.values()) {
            final String className = classNames.get(object.getTypeId());
            if (className.startsWith(CLASS_NAME_PREFIX)) {
                // skip classes
                continue;
            }
            AhatClassInstance instance = new AhatClassInstance(object.getId());
            long classId = findClassId(object, classNames, fieldNames);
            AhatClassObj type = ahatClasses.get(classId);
            if (type == null) {
                System.err.println(
                        "Couldn't find class object for class " + className);
            }
            instance.initialize(theHeap, rootSite, type);
            ahatInstances.put(object.getId(), instance);
            inst.add(instance);
        }

        // Set Roots
        for (TraceProto.HeapGraphRoot root : roots) {
            for (Long id : root.getObjectIdsList()) {
                AhatInstance rootObj = findAhatInstance(id, ahatInstances, ahatClasses);
                superRoot.addRoot(rootObj);
                rootObj.addRootType(toRootType(root.getRootType()));
            }
        }

        // Initialize links in the graph
        Set<Long> seenClass = new HashSet<>();
        for (AhatClassInstance instance : ahatInstances.values()) {
            long id = instance.getId();
            TraceProto.HeapGraphObject object = objects.get(id);
            long clsId = instance.getClassObj().getId();

            // The fields can be set on the class once the first instance of that class has been
            // seen.
            if (!seenClass.contains(clsId)) {
                AhatClassObj classObj = instance.getClassObj();
                assert classObj.getId() == clsId;

                Field[] fields = new Field[object.getReferenceFieldIdCount()];
                assert object.getReferenceObjectIdCount() == object.getReferenceFieldIdCount();
                for (int i = 0; i < fields.length; ++i) {
                    String name = fieldNames.get(object.getReferenceFieldId(i));
                    int begin = name.lastIndexOf(".");
                    fields[i] = new Field(name.substring(begin + 1), Type.OBJECT);
                }
                TraceProto.HeapGraphObject cls = objects.get(clsId);
                int staticFieldCount = cls.getReferenceFieldIdCount();
                AhatClassObj superClass = ahatClasses.get(
                        findSuperClassId(objects.get(clsId), fieldNames));
                classObj.initialize(superClass, object.getSelfSize(), fields, staticFieldCount);
                FieldValue[] staticFields = new FieldValue[staticFieldCount];
                for (int i = 0; i < staticFields.length; ++i) {
                    String name = fieldNames.get(cls.getReferenceFieldId(i));
                    int begin = name.lastIndexOf(".");
                    long fieldId = cls.getReferenceObjectId(i);
                    Value v = fieldId == 0 ? Value.pack(null) : Value.pack(
                            findAhatInstance(fieldId, ahatInstances,
                                    ahatClasses));
                    staticFields[i] = new FieldValue(name.substring(begin + 1), Type.OBJECT, v);
                }
                classObj.initialize(null, staticFields);

                seenClass.add(clsId);
            }

            Value[] values = new Value[object.getReferenceFieldIdCount()];
            for (int i = 0; i < values.length; ++i) {
                long fieldId = object.getReferenceObjectId(i);
                values[i] = fieldId == 0 ? Value.pack(null) : Value.pack(
                        findAhatInstance(fieldId, ahatInstances, ahatClasses));
            }
            instance.initialize(values);

            // Fix up size of the instance.
            instance.setExtraJavaSize(object.getSelfSize() - instance.getSize().getSize());
        }

        // If a class hasn't been seen (because there aren't any instances), at least add the super
        // class.
        for (AhatClassObj classObj : ahatClasses.values()) {
            final long clsId = classObj.getId();
            if (seenClass.contains(clsId)) {
                continue;
            }
            AhatClassObj superClass = ahatClasses.get(
                    findSuperClassId(objects.get(clsId), fieldNames));
            classObj.initialize(superClass, 0, new Field[0], 0);
        }

        // Fix up size of classes
        for (AhatClassObj classObj : ahatClasses.values()) {
            TraceProto.HeapGraphObject obj = objects.get(classObj.getId());
            classObj.setExtraJavaSize(obj.getSelfSize() - classObj.getSize().getSize());
        }

        // Check size
        long totalSize = 0;
        for (TraceProto.HeapGraphObject object : objects.values()) {
            totalSize += object.getSelfSize();
        }

        long instanceSize = 0;
        for (AhatClassInstance instance : ahatInstances.values()) {
            instanceSize += instance.getSize().getSize();
        }

        long classSize = 0;
        for (AhatClassObj obj : ahatClasses.values()) {
            classSize += obj.getSize().getSize();
        }
        assert totalSize == instanceSize + classSize;

        return new AhatSnapshot(superRoot, new Instances<AhatInstance>(inst), heaps, rootSite,
                progress, retained);
    }

    private static void extractHeapGraphPackets(List<TraceProto.HeapGraph> graphs,
            TraceProto.Trace trace) throws IOException, DataFormatException {
        for (TraceProto.TracePacket packet : trace.getPacketList()) {
            switch (packet.getDataCase()) {
                case COMPRESSED_PACKETS:
                    TraceProto.Trace trace_ = TraceProto.Trace.parseFrom(
                            new InflaterInputStream(new ByteArrayInputStream(
                                    packet.getCompressedPackets().toByteArray())));
                    extractHeapGraphPackets(graphs, trace_);
                    break;
                case HEAP_GRAPH:
                    graphs.add(packet.getHeapGraph());
                    break;
            }
        }
    }

    private static long findClassId(TraceProto.HeapGraphObject obj, Map<Long, String> classNames,
            Map<Long, String> fieldNames) {
        int index = 0;
        for (Long id : obj.getReferenceFieldIdList()) {
            String name = fieldNames.get(id);
            if (CLASS_FIELD_NAME.equals(name)) {
                return obj.getReferenceObjectId(index);
            }
            index++;
        }
        throw new RuntimeException(
                "Couldn't find class ID for object of type " + classNames.get(obj.getTypeId()));
    }

    private static long findSuperClassId(TraceProto.HeapGraphObject cls,
            Map<Long, String> fieldNames) {
        for (int i=0; i<cls.getReferenceFieldIdCount(); ++i) {
            String name = fieldNames.get(cls.getReferenceFieldId(i));
            if (name.endsWith("superClass")) {
                return cls.getReferenceObjectId(i);
            }
        }
        return 0;
    }

    private static AhatInstance findAhatInstance(long id, Map<Long, AhatClassInstance> instances,
            Map<Long, AhatClassObj> classes) {
        if (instances.containsKey(id)) {
            return instances.get(id);
        } else if (classes.containsKey(id)) {
            return classes.get(id);
        } else {
            throw new RuntimeException("couldn't find ahat instance " + id);
        }
    }

    private static RootType toRootType(TraceProto.HeapGraphRoot.Type type) {
        switch (type) {
            case ROOT_DEBUGGER:
                return RootType.DEBUGGER;
            case ROOT_FINALIZING:
                return RootType.FINALIZING;
            case ROOT_INTERNED_STRING:
                return RootType.INTERNED_STRING;
            case ROOT_JAVA_FRAME:
                return RootType.JAVA_FRAME;
            case ROOT_JNI_GLOBAL:
                return RootType.JNI_GLOBAL;
            case ROOT_JNI_LOCAL:
                return RootType.JNI_LOCAL;
            case ROOT_JNI_MONITOR:
                return RootType.JNI_MONITOR;
            case ROOT_MONITOR_USED:
                return RootType.MONITOR;
            case ROOT_NATIVE_STACK:
                return RootType.NATIVE_STACK;
            case ROOT_REFERENCE_CLEANUP:
                return RootType.UNKNOWN;
            case ROOT_STICKY_CLASS:
                return RootType.STICKY_CLASS;
            case ROOT_THREAD_BLOCK:
                return RootType.THREAD_BLOCK;
            case ROOT_THREAD_OBJECT:
                return RootType.THREAD;
            case ROOT_UNKNOWN:
                return RootType.UNKNOWN;
            case ROOT_VM_INTERNAL:
                return RootType.VM_INTERNAL;
            default:
                return RootType.UNKNOWN;
        }
    }
}
