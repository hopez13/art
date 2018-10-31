package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.Map;

public class FileWritingGreylistConsumer extends GreylistConsumer {

    private final Status mStatus;
    private final PrintStream mFlagsCsvStream;

    public FileWritingGreylistConsumer(Status status, String csvFlagsFile)
            throws FileNotFoundException {
        mStatus = status;
        mFlagsCsvStream = new PrintStream(new FileOutputStream(new File(csvFlagsFile)));
    }

    @Override
    public void entryWithHiddenapiFlags(String signature, String[] flags) {
        if (flags.length == 0) {
          mFlagsCsvStream.println(signature);
        } else {
          mFlagsCsvStream.println(signature + "," + String.join(",", flags));
        }
    }

    @Override
    public void close() {
        if (mFlagsCsvStream != null) {
            mFlagsCsvStream.close();
        }
    }
}
