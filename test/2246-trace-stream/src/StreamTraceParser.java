/*
 * Copyright (C) 2022 The Android Open Source Project
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

import java.io.File;
import java.io.IOException;

<<<<<<< PATCH SET (772ef4 Collect timestamps directly from the JITed code)
public class StreamTraceParser {
    public static final int MAGIC_NUMBER = 0x574f4c53;
    public static final int DUAL_CLOCK_VERSION = 3;
    public static final int TRACE_VERSION_WALL_CLOCK = 0xF2;
=======
public class StreamTraceParser extends BaseTraceParser {
>>>>>>> BASE      (85752e Add fence to allocations in nterp.)

<<<<<<< PATCH SET (772ef4 Collect timestamps directly from the JITed code)
    public StreamTraceParser(File file) throws IOException {
        dataStream = new DataInputStream(new FileInputStream(file));
        method_id_map = new HashMap<Integer, String>();
        thread_id_map = new HashMap<Integer, String>();
        thread_to_timestamp_map = new HashMap<Integer, Integer>();
    }
=======
    public void CheckTraceFileFormat(File file,
        int expectedVersion, String threadName) throws Exception {
        InitializeParser(file);
>>>>>>> BASE      (85752e Add fence to allocations in nterp.)

        validateTraceHeader(expectedVersion);
        boolean hasEntries = true;
        boolean seenStopTracingMethod = false;
        while (hasEntries) {
            int threadId = GetThreadID();
            if (threadId != 0) {
              String eventString = ProcessEventEntry(threadId);
              if (!ShouldCheckThread(threadId, threadName)) {
                continue;
              }
              // Ignore events after method tracing was stopped. The code that is executed
              // later could be non-deterministic.
              if (!seenStopTracingMethod) {
                UpdateThreadEvents(threadId, eventString);
              }
              if (eventString.contains("Main$VMDebug $noinline$stopMethodTracing")) {
                seenStopTracingMethod = true;
              }
            } else {
              int headerType = GetEntryHeader();
              switch (headerType) {
                case 1:
                  ProcessMethodInfoEntry();
                  break;
                case 2:
                  ProcessThreadInfoEntry();
                  break;
                case 3:
                  // TODO(mythria): Add test to also check format of trace summary.
                  hasEntries = false;
                  break;
                default:
                  System.out.println("Unexpected header in the trace " + headerType);
              }
            }
        }
        closeFile();

        // Printout the events.
        for (String str : threadEventsMap.values()) {
            System.out.println(str);
        }
    }
<<<<<<< PATCH SET (772ef4 Collect timestamps directly from the JITed code)

    public int GetEntryHeader() throws IOException {
        // Read 2-byte thread-id. On host thread-ids can be greater than 16-bit.
        int thread_id = readNumber(2);
        if (thread_id != 0) {
            return thread_id;
        }
        // Read 1-byte header type
        return readNumber(1);
    }

    public void ProcessMethodInfoEntry() throws IOException {
        // Read 2-byte method info size
        int header_length = readNumber(2);
        // Read header_size data.
        String method_info = readString(header_length);
        String[] tokens = method_info.split("\t", 2);
        // Get method_id and record method_id -> method_name map.
        int method_id = Integer.decode(tokens[0]);
        String method_line = tokens[1].replace('\t', ' ');
        method_line = method_line.substring(0, method_line.length() - 1);
        method_id_map.put(method_id, method_line);
    }

    public void ProcessThreadInfoEntry() throws IOException {
        // Read 2-byte thread id
        int thread_id = readNumber(2);
        // Read 2-byte thread info size
        int header_length = readNumber(2);
        // Read header_size data.
        String thread_info = readString(header_length);
        thread_id_map.put(thread_id, thread_info);
    }

    public String eventTypeToString(int event_type) {
        String str = "";
        for (int i = 0; i < nesting_level; i++) {
            str += ".";
        }
        switch (event_type) {
            case 0:
                nesting_level++;
                str += ".>>";
                break;
            case 1:
                nesting_level--;
                str += "<<";
                break;
            case 2:
                nesting_level--;
                str += "<<E";
                break;
            default:
                str += "??";
        }
        return str;
    }

    public String ProcessEventEntry(int thread_id) throws IOException, Exception {
        // Read 4-byte method value
        int method_and_event = readNumber(4);
        int method_id = method_and_event & ~0x3;
        int event_type = method_and_event & 0x3;

        String str = eventTypeToString(event_type) + " " + thread_id_map.get(thread_id) + " "
                + method_id_map.get(method_id);
        // Depending on the version read one or two timestamps.
        int timestamp = readNumber(4);
        if (thread_to_timestamp_map.containsKey(thread_id)) {
            int old_timestamp = thread_to_timestamp_map.get(thread_id);
            if (timestamp < old_timestamp) {
                throw new Exception("timestamps are not increasing current: " + timestamp
                        + "  earlier: " + old_timestamp);
            }
        }
        thread_to_timestamp_map.put(thread_id, timestamp);
        if (trace_format_version != 2) {
            // Skip the second timestamp.
            dataStream.skipBytes(4);
        }
        return str;
    }

    DataInputStream dataStream;
    HashMap<Integer, String> method_id_map;
    HashMap<Integer, String> thread_id_map;
    HashMap<Integer, Integer> thread_to_timestamp_map;
    int record_size = 0;
    int trace_format_version = 0;
    int nesting_level = 0;
=======
>>>>>>> BASE      (85752e Add fence to allocations in nterp.)
}
