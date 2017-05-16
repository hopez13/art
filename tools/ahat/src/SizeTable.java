/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.ahat;

import com.android.ahat.heapdump.Size;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Class for rendering a table that includes all categories of size as the
 * first set of columns.
 */
class SizeTable {

  /**
   * Start a table with columns in the order of: left, sizes, columns.
   *
   * This should be followed by calls to the 'row' method to fill in the table
   * contents and the 'end' method to end the table.
   *
   * Set showDiff to true if size diffs should be shown.
   */
  static void table(Doc doc, Column left, boolean showDiff, Column... columns) {
    List<Column> cols = new ArrayList<Column>();
    cols.add(left);
    cols.add(new Column("Java Size", Column.Align.RIGHT));
    cols.add(new Column("Δ", Column.Align.RIGHT, showDiff));
    cols.add(new Column("Registered Native Size", Column.Align.RIGHT));
    cols.add(new Column("Δ", Column.Align.RIGHT, showDiff));
    cols.add(new Column("Total Size", Column.Align.RIGHT));
    cols.add(new Column("Δ", Column.Align.RIGHT, showDiff));
    cols.addAll(Arrays.asList(columns));
    doc.table(cols.toArray(new Column[cols.size()]));
  }

  /**
   * Start a table with columns in the order of: sizes, columns.
   *
   * This should be followed by calls to the 'row' method to fill in the table
   * contents and the 'end' method to end the table.
   *
   * Set showDiff to true if size diffs should be shown.
   */
  static void table(Doc doc, boolean showDiff, Column... columns) {
    // Make an invisible dummy column.
    table(doc, new Column("", Column.Align.LEFT, false), showDiff, columns);
  }

  /**
   * Add a row to the currently active table.
   * The number of values must match the number of columns provided for the
   * currently active table.
   */
  static void row(Doc doc, DocString left, Size size, Size base, DocString... values) {
    List<DocString> vals = new ArrayList<DocString>();
    vals.add(left);
    vals.add(DocString.size(size.getJavaSize(), false));
    vals.add(DocString.delta(false, false, size.getJavaSize(), base.getJavaSize()));
    vals.add(DocString.size(size.getRegisteredNativeSize(), false));
    vals.add(DocString.delta(false, false,
          size.getRegisteredNativeSize(), base.getRegisteredNativeSize()));
    vals.add(DocString.size(size.getSize(), false));
    vals.add(DocString.delta(false, false, size.getSize(), base.getSize()));
    vals.addAll(Arrays.asList(values));
    doc.row(vals.toArray(new DocString[vals.size()]));
  }

  /**
   * Add a row to the currently active table.
   * The number of values must match the number of columns provided for the
   * currently active table.
   */
  static void row(Doc doc, Size size, Size base, DocString... values) {
    row(doc, new DocString(), size, base, values);
  }

  /**
   * End the currently active table.
   */
  static void end(Doc doc) {
    doc.end();
  }

}

