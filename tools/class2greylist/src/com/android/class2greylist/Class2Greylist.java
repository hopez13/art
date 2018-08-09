/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.class2greylist;

import com.google.common.collect.Sets;
import com.google.common.io.Files;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.GnuParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * Build time tool for extracting a list of members from jar files that have the @UsedByApps
 * annotation, for building the greylist.
 */
public class Class2Greylist {

    private static final String ANNOTATION_TYPE = "Landroid/annotation/UnsupportedAppUsage;";

    private final Status mStatus;
    private final String mPublicApiListFile;
    private final String[] mPerSdkOutputFiles;
    private final String[] mJarFiles;

    public static void main(String[] args) {
        Options options = new Options();
        options.addOption(OptionBuilder
                .withLongOpt("public-api-list")
                .hasArgs(1)
                .withDescription("Public API list file. Used to de-dupe bridge methods.")
                .create("p"));
        options.addOption(OptionBuilder
                .withLongOpt("write-greylist")
                .hasArgs()
                .withDescription(
                        "Specify file to write greylist to. Can be specified multiple times. " +
                        "Format is either just a filename, or \"int:filename\". If an integer is " +
                        "given, members with a matching maxTargetSdk are written to the file; if " +
                        "no integer is given, members with no maxTargetSdk are written.")
                .create("w"));
        options.addOption(OptionBuilder
                .withLongOpt("debug")
                .hasArgs(0)
                .withDescription("Enable debug")
                .create("d"));
        options.addOption(OptionBuilder
                .withLongOpt("help")
                .hasArgs(0)
                .withDescription("Show this help")
                .create("h"));

        CommandLineParser parser = new GnuParser();
        CommandLine cmd;

        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            help(options);
            return;
        }
        if (cmd.hasOption('h')) {
            help(options);
        }


        String[] jarFiles = cmd.getArgs();
        if (jarFiles.length == 0) {
            System.err.println("Error: no jar files specified.");
            help(options);
        }

        Status status = new Status(cmd.hasOption('d'));
        Class2Greylist c2gl = new Class2Greylist(
                status, cmd.getOptionValue('p', null), cmd.getOptionValues('w'), jarFiles);
        try {
            c2gl.main();
        } catch (IOException e) {
            status.error(e);
        }

        if (status.ok()) {
            System.exit(0);
        } else {
            System.exit(1);
        }

    }

    private Class2Greylist(Status status, String publicApiListFile, String[] perSdkLevelOutputFiles, String[] jarFiles) {
        mStatus = status;
        mPublicApiListFile = publicApiListFile;
        mPerSdkOutputFiles = perSdkLevelOutputFiles;
        mJarFiles = jarFiles;
    }

    private void main() throws IOException {
        Map<Integer, String> outputFiles = readGreylistMap(mPerSdkOutputFiles);

        FileWritingGreylistConsumer output = new FileWritingGreylistConsumer(mStatus, outputFiles);

        Set<String> publicApis;
        if (mPublicApiListFile != null) {
            publicApis = Sets.newHashSet(
                    Files.readLines(new File(mPublicApiListFile), Charset.forName("UTF-8")));
        } else {
            publicApis = Collections.emptySet();
        }
        FileWritingGreylistConsumer consumer = new FileWritingGreylistConsumer(
                mStatus, outputFiles);

        for (String jarFile : mJarFiles) {
            mStatus.debug("Processing jar file %s", jarFile);
            try {
                JarReader reader = new JarReader(mStatus, jarFile);
                reader.stream().forEach(clazz -> new AnnotationVisitor(clazz, ANNOTATION_TYPE,
                        publicApis, outputFiles.keySet(), consumer, mStatus).visit());
                reader.close();
            } catch (IOException e) {
                mStatus.error(e);
            }
        }
    }


    private Map<Integer, String> readGreylistMap(String[] argValues) {
        Map<Integer, String> map = new HashMap<>();
        for (String sdkFile : argValues) {
            Integer maxTargetSdk = null;
            String filename;
            int colonPos = sdkFile.indexOf(':');
            if (colonPos != -1) {
                try {
                    maxTargetSdk = Integer.valueOf(sdkFile.substring(0, colonPos));
                } catch (NumberFormatException nfe) {
                    mStatus.error("Not a valid integer: %s from argument value '%s'",
                            sdkFile.substring(0, colonPos), sdkFile);
                }
                filename = sdkFile.substring(colonPos + 1);
                if (filename.length() == 0) {
                    mStatus.error("Not a valid file name: %s from argument value '%s'",
                            filename, sdkFile);
                }
            } else {
                maxTargetSdk =null;
                filename = sdkFile;
            }
            if (map.containsKey(maxTargetSdk)) {
                mStatus.error("Multiple output files for maxTargetSdk %s", maxTargetSdk);
            } else {
                map.put(maxTargetSdk, filename);
            }
        }
        return map;
    }

    private static void help(Options options) {
        new HelpFormatter().printHelp(
                "class2greylist path/to/classes.jar [classes2.jar ...]",
                "Extracts greylist entries from classes jar files given",
                options, null, true);
        System.exit(1);
    }
}
