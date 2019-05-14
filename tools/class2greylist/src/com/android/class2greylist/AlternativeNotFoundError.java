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

import com.google.common.base.Joiner;

import java.util.List;

public class AlternativeNotFoundError extends Exception {
    public enum AlternativeType {
        CLASS_ALTERNATIVE_NOT_FOUND,
        METHOD_ALTERNATIVE_NOT_FOUND,
        MULTIPLE_METHOD_ALTERNATIVES_FOUND,
        NO_ALTERNATIVE_SPECIFIED
    }
    public final ApiComponents alternative;
    public final List<ApiComponents> almostMatches;
    public final AlternativeType type;

    AlternativeNotFoundError(AlternativeType type, ApiComponents alternative, List<ApiComponents> almostMatches) {
        this.type = type;
        this.alternative = alternative;
        this.almostMatches = almostMatches;
    }

    @Override
    public String toString() {
        switch (type){
            case CLASS_ALTERNATIVE_NOT_FOUND:
                return "Specified class " + alternative.getPackageAndClassName()
                        + " does not exist!";
            case METHOD_ALTERNATIVE_NOT_FOUND:
                return "Could not find public api " + alternative + ".";
            case MULTIPLE_METHOD_ALTERNATIVES_FOUND:
                return "Alternative " + alternative + " returned multiple matches: "
                        + Joiner.on(", ").join(almostMatches);

            case NO_ALTERNATIVE_SPECIFIED:
                return "Hidden API has a public alternative annotation "
                        + "field, but no concrete explanations. Please provide either a "
                        + "reference to an SDK method using javadoc syntax, e.g. "
                        + "{@link foo.bar.Baz#bat}, or a small code snippet if the alternative "
                        + "is part of a support library or third party library, e.g. "
                        + "{@code foo.bar.Baz bat = new foo.bar.Baz(); bat.doSomething();}. "
                        + "If this is too restrictive for your use case, please contact "
                        + "compat-team@.";
        }
        throw new IllegalStateException("Unhandled alternative type!");
    }
}
