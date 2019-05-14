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
import com.google.common.base.Strings;

import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;

public class ApiResolver {
    private List<ApiComponents> mPotentialPublicAlternatives;
    private Set<PackageAndClassName> mPublicApiClasses;

    private static final Pattern LINK_TAG_PATTERN = Pattern.compile("\\{@link ([^\\}]+)\\}");
    private static final Pattern CODE_TAG_PATTERN = Pattern.compile("\\{@code ([^\\}]+)\\}");

    public ApiResolver(Set<String> publicApis) {
        mPotentialPublicAlternatives = publicApis.stream()
                .map(api -> ApiComponents.fromDexSignature(api))
                .collect(Collectors.toList());
        mPublicApiClasses = mPotentialPublicAlternatives.stream()
                .map(api -> api.getPackageAndClassName())
                .collect(Collectors.toSet());
    }

    /**
     * Verify that all public alternatives are valid.
     *
     * @param publicAlternativesString String containing public alternative explanations.
     * @param signature                Signature of the member that has the annotation.
     * @param reporter                 ErrorReporter instance.
     * @return True if the explanation contains only valid links to public apis or at least one
     * code snippet.
     */
    public boolean resolvePublicAlternatives(String publicAlternativesString, String signature,
            ErrorReporter reporter) {
        if (publicAlternativesString != null && mPotentialPublicAlternatives != null) {
            // Grab all instances of type {@link foo}
            Matcher matcher = LINK_TAG_PATTERN.matcher(publicAlternativesString);
            boolean hasLinkAlternative = false;
            // Validate all link tags
            while (matcher.find()) {
                hasLinkAlternative = true;
                String alternativeString = matcher.group(1);
                ApiComponents alternative = ApiComponents.fromLinkTag(alternativeString,
                        signature, reporter);
                if (alternative.getMemberName().isEmpty()) {
                    // Provided class as alternative
                    if (!mPublicApiClasses.contains(alternative.getPackageAndClassName())) {
                        reporter.reportError("Specified class " + alternative + " does not exist!");
                        return false;
                    }
                } else if (!mPotentialPublicAlternatives.contains(alternative)) {
                    // If the link is not a public alternative, it must because the link does not
                    // contain the method parameter types, e.g. {@link foo.bar.Baz#foo} instead of
                    // {@link foo.bar.Baz#foo(int)}. If the method name is unique within the class,
                    // we can handle it.
                    if (!Strings.isNullOrEmpty(alternative.getMethodParameterTypes())) {
                        reporter.reportError(
                                "Could not find public api " + alternativeString + ".");
                        return false;
                    }
                    List<ApiComponents> almostMatches = mPotentialPublicAlternatives.stream()
                            .filter(api -> api.equalsIgnoringParam(alternative))
                            .collect(Collectors.toList());
                    if (almostMatches.size() == 0) {
                        reporter.reportError("Alternative " + alternativeString
                                + " returned no public API matches.");
                        return false;
                    } else if (almostMatches.size() > 1) {
                        reporter.reportError("Alternative " + alternativeString
                                + " returned multiple matches: " + Joiner.on(", ").join(
                                almostMatches));
                        return false;
                    }
                }
            }
            // No {@link ...} alternatives exist; try looking for {@code ...}
            if (!hasLinkAlternative) {
                if (!CODE_TAG_PATTERN.matcher(publicAlternativesString).find()) {
                    reporter.reportError("Hidden API has a public alternative annotation "
                            + "field, but no concrete explanations. Please provide either a "
                            + "reference to an SDK method using javadoc syntax, e.g. "
                            + "{@link foo.bar.Baz#bat}, or a small code snippet if the alternative "
                            + "is part of a support library or third party library, e.g. "
                            + "{@code foo.bar.Baz bat = new foo.bar.Baz(); bat.doSomething();}. "
                            + "If this is too restrictive for your use case, please contact "
                            + "compat-team@.");
                    return false;
                }
            }
        }
        return true;
    }
}
