package com.android.class2greylist;

import org.apache.bcel.classfile.AnnotationEntry;

public interface AnnotationHandler {
    void handleAnnotation(AnnotationEntry annotation, AnnotationContext context);
}
