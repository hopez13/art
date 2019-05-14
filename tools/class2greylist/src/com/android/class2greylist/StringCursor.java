/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.class2greylist;

public class StringCursor {
    private final String mString;
    private int mCursor;

    public StringCursor(String str) {
        mString = str;
        mCursor = 0;
    }

    public char peek() {
        return mString.charAt(mCursor);
    }

    public void rollback() {
        if (mCursor <= 0) {
            throw new IllegalStateException("Trying to roll cursor back beyond start of " +
                    "string");
        }
        --mCursor;
    }

    public char consume() {
        return mString.charAt(mCursor++);
    }

    public String consume(int x) {
        if (x <= 0) {
            throw new IllegalStateException("Cannot consume non-positive number of chars");
        }
        mCursor += x;
        return mString.substring(mCursor - x, mCursor);
    }

    public String consumeUntil(String str) {
        int firstIdx = mCursor;
        while (!reachedEnd() && str.indexOf(peek()) == -1) {
            ++mCursor;
        }
        return mString.substring(firstIdx, mCursor);
    }

    public boolean reachedEnd() {
        return mCursor >= mString.length();
    }

    @Override
    public String toString() {
        return mString.substring(mCursor);
    }

    public String getOriginalString() {
        return mString;
    }
}