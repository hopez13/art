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

/**
 * Utility class to simplify parsing of signatures.
 */
public class StringCursor {

    // Interface to simplify handling of parsing errors.
    public interface ErrorReporter {
        void report(String error);
    }

    private final String mString;
    private int mCursor;
    private final ErrorReporter mErrorReporter;

    public StringCursor(String str, ErrorReporter reporter) {
        mString = str;
        mCursor = 0;
        mErrorReporter = reporter;
    }

    /**
     * Peek current cursor position.
     *
     * @return The character at the current cursor position.
     */
    public char peek() {
        return mString.charAt(mCursor);
    }

    /**
     * Rollback the cursor by one character.
     */
    public void rollback() {
        if (mCursor <= 0) {
            mErrorReporter.report("Trying to roll cursor back beyond start of string");
        }
        --mCursor;
    }

    /**
     * Consume the character at the current cursor position and move the cursor forwards.
     *
     * @return The character at the current cursor position.
     */
    public char consume() {
        return mString.charAt(mCursor++);
    }

    /**
     * Consume several characters at the current cursor position and move the cursor further along.
     *
     * @param x The number of characters to consume.
     * @return A string with x characters from the cursor position.
     */
    public String consume(int x) {
        if (x <= 0) {
            mErrorReporter.report("Cannot consume non-positive number of chars");
        }
        mCursor += x;
        return mString.substring(mCursor - x, mCursor);
    }

    /**
     * Consume characters until a character from {@code str} is encountered.
     *
     * @param str The set of characters that can end consumption.
     * @return A string from the current cursor position until a character from {@code str} is
     * encountered (or the string terminates). If a character from {@code str} is encountered, it is
     * not consumed.
     */
    public String consumeUntil(String str) {
        int firstIdx = mCursor;
        while (!reachedEnd() && str.indexOf(peek()) == -1) {
            ++mCursor;
        }
        return mString.substring(firstIdx, mCursor);
    }

    /**
     * Check if cursor has reached end of string.
     * @return Cursor has reached end of string.
     */
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