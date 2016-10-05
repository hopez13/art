/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_
#define ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_

#include "nodes.h"

namespace art {

// Linearizes the 'graph' such that:
// (1): a block is always after its dominator,
// (2): blocks of loops are contiguous.
// Storage is obtained through 'allocator' and the linear
// order it computed into 'linear_order'.
void LinearizeGraph(const HGraph* graph,
                    ArenaAllocator* allocator,
                    ArenaVector<HBasicBlock*>* linear_order);

class HLinearPostOrderIterator : public ValueObject {
 public:
  explicit HLinearPostOrderIterator(const ArenaVector<HBasicBlock*>& order)
      : order_(order),
        index_(order.size()) { }

  bool Done() const { return index_ == 0; }

  HBasicBlock* Current() const { return order_[index_ - 1u]; }

  void Advance() {
    --index_;
    DCHECK_GE(index_, 0U);
  }

 private:
  const ArenaVector<HBasicBlock*>& order_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HLinearPostOrderIterator);
};

class HLinearOrderIterator : public ValueObject {
 public:
  explicit HLinearOrderIterator(const ArenaVector<HBasicBlock*>& order)
      : order_(order),
        index_(0) { }

  bool Done() const { return index_ == order_.size(); }

  HBasicBlock* Current() const { return order_[index_]; }

  void Advance() {
    ++index_;
    DCHECK_LE(index_, order_.size());
  }

 private:
  const ArenaVector<HBasicBlock*>& order_;
  size_t index_;

  DISALLOW_COPY_AND_ASSIGN(HLinearOrderIterator);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_LINEAR_ORDER_H_
