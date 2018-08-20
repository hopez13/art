package com.android.class2greylist;

public class SystemOutGreylistConsumer implements GreylistConsumer {
    @Override
    public void greylistEntry(String signature, Integer maxTargetSdk) {
        System.out.println(signature);
    }

    @Override
    public void whitelistEntry(String signature) {
        // ignore.
    }

    @Override
    public void close() {
    }
}
