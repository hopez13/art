package com.android.class2greylist;

import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.JavaClass;

import java.util.Locale;

/**
 * Represents a member of a class file (a field or method).
 */
public class Member {

    /**
     * Signature of this member.
     */
    public final String signature;
    /**
     * Indicates if this is a synthetic bridge method.
     */
    public final boolean bridge;

    public Member(String signature, boolean bridge) {
        this.signature = signature;
        this.bridge = bridge;
    }
}
