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

import com.google.gson.stream.JsonReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.LinkedHashSet;
import java.util.Set;
import java.util.regex.Pattern;

class PresubmitJsonLinter {

    public static void main(String[] args) {
        for (String arg : args) {
            try {
                info("Checking " + arg);
                checkExpectationFile(arg);
            } catch (IOException e) {
                error(e.getMessage());
            }
        }
    }

    private static void info(String message) {
        System.err.println(message);
    }

    private static void error(String message) {
        System.err.println(message);
        System.exit(1);
    }

    private static void checkExpectationFile(String arg) throws IOException {
        JsonReader reader = new JsonReader(new FileReader(arg));
        reader.setLenient(true);
        reader.beginArray();
        while (reader.hasNext()) {
            readExpectation(reader);
        }
        reader.endArray();
    }

    private static void readExpectation(JsonReader reader) throws IOException {
        boolean isFailure = false;
        Pattern pattern = Pattern.compile(".*", Pattern.MULTILINE | Pattern.DOTALL);
        Set<String> names = new LinkedHashSet<String>();
        Set<String> tags = new LinkedHashSet<String>();
        String description = "";

        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            switch (name) {
                case "result":
                    reader.nextString();
                    break;
                case "name":
                    names.add(reader.nextString());
                    break;
                case "names":
                    readStrings(reader, names);
                    break;
                case "failure":
                    isFailure = true;
                    names.add(reader.nextString());
                    break;
                case "pattern":
                    pattern = Pattern.compile(reader.nextString(), Pattern.MULTILINE | Pattern.DOTALL);
                    break;
                case "substring":
                    pattern = Pattern.compile(
                        ".*" + Pattern.quote(reader.nextString()) + ".*", Pattern.MULTILINE | Pattern.DOTALL);
                    break;
                case "tags":
                    readStrings(reader, tags);
                    break;
                case "description":
                    reader.nextString();
                    break;
                case "bug":
                    reader.nextLong();
                    break;
                case "modes":
                    readModes(reader);
                    break;
                case "modes_variants":
                    readModesAndVariants(reader);
                    break;
                default:
                    info("Unhandled name in expectations file: " + name);
                    reader.skipValue();
                    break;
            }
        }
        reader.endObject();

        if (names.isEmpty()) {
            error("Missing 'name' or 'failure' key in " + reader);
        }
    }

    private static void readStrings(JsonReader reader, Set<String> output) throws IOException {
        reader.beginArray();
        while (reader.hasNext()) {
            output.add(reader.nextString());
        }
        reader.endArray();
    }

    private static void readModes(JsonReader reader) throws IOException {
        reader.beginArray();
        while (reader.hasNext()) {
            reader.nextString();
        }
        reader.endArray();
    }

    /**
     * Expected format: mode_variants: [["host", "X32"], ["host", "X64"]]
     */
    private static void readModesAndVariants(JsonReader reader) throws IOException {
        reader.beginArray();
        while (reader.hasNext()) {
            reader.beginArray();
            reader.nextString();
            reader.nextString();
            reader.endArray();
        }
        reader.endArray();
    }
}
