package com.android.class2greylist;

import java.util.Map;

public abstract class GreylistConsumer {
    public static final String FLAG_WHITELIST = "whitelist";
    public static final String FLAG_GREYLIST = "greylist";
    public static final String FLAG_BLACKLIST = "blacklist";
    public static final String FLAG_BLACKLIST_MAX_O = "blacklist-max-o";

    /**
     * Handle a new greylist entry.
     *
     * @param signature Signature of the member.
     * @param maxTargetSdk maxTargetSdk value from the annotation, or null if none set.
     */
    public void entryWithHiddenapiFlags(String signature, String[] flags) {
    }

    public void entryWithAnnotationProperties(String signature,
            Map<String, String> annotationProperties) {
    }

    public void close() {
    }
}
