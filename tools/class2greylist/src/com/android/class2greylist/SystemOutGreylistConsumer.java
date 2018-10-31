package com.android.class2greylist;

import java.util.Arrays;
import java.util.Map;

public class SystemOutGreylistConsumer extends GreylistConsumer {
    @Override
    public void entryWithHiddenapiFlags(String signature, String[] flags) {
        if (!Arrays.asList(flags).contains("whitelist")) {
            System.out.println(signature);
        }
    }
}
