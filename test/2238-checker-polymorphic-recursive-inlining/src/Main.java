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

class CursorWrapper extends Cursor {
  protected final Cursor mCursor;

  public CursorWrapper(Cursor cursor) {
    mCursor = cursor;
  }

  boolean isAfterLast() {
    return mCursor.isAfterLast();
  }
}

class AnotherWrapper extends Cursor {
  protected final Cursor mCursor;

  public AnotherWrapper(Cursor cursor) {
    mCursor = cursor;
  }

  boolean isAfterLast() {
    return mCursor.isAfterLast();
  }
}

class ShallowWrapper extends Cursor {
  protected final Cursor mCursor;

  public ShallowWrapper(Cursor cursor) {
    mCursor = cursor;
  }

  boolean isAfterLast() {
    return false;
  }
}

// TODO: Create different tests with only shallow and so on.

public class Main {
  public static void main(String[] args) {}
}