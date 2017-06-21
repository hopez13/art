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

#include "loop_analysis.h"

#include <iostream>

namespace art {

void HEdge::Dump(std::ostream& stream) const {
  stream << "(" << from_ << "->" << to_ << ")";
}

}  // namespace art

namespace std {
  ostream& operator<<(ostream& os, const art::HEdge& e) {
    e.Dump(os);
    return os;
  }
}  // namespace std
