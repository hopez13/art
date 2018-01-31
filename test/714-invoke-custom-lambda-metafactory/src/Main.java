/*
 * Copyright 2018 Google Inc.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Google designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Google in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */

import java.util.Arrays;
import java.util.List;
import java.util.Optional;

public class Main {
    public static void main(String[] args) {
        int requiredLength = 3;
        List<String> list = Arrays.asList("A", "B", "C", "D", "EEE");
        Optional<String> result = list.stream().filter(x -> x.length() >= requiredLength).findAny();
        if (result.isPresent()) {
            System.out.println("Result is " + result.get());
        } else {
            System.out.println("Result is not there.");
        }
    }
}
