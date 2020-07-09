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

#include "execution_subgraph.h"

#include <array>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "base/scoped_arena_allocator.h"
#include "class_root.h"
#include "dex/dex_file_types.h"
#include "dex/method_reference.h"
#include "entrypoints/quick/quick_entrypoints_enum.h"
#include "gtest/gtest.h"
#include "handle.h"
#include "handle_scope.h"
#include "nodes.h"
#include "optimizing/data_type.h"
#include "optimizing_unit_test.h"
#include "scoped_thread_state_change.h"

namespace art {

using BlockSet = std::unordered_set<const HBasicBlock*>;

class ExecutionSubgraphTest : public OptimizingUnitTest {
 public:
  ExecutionSubgraphTest() : graph_(CreateGraph()) {}

  AdjacencyListGraph SetupFromAdjacencyList(const std::string_view entry_name,
                                            const std::string_view exit_name,
                                            const std::vector<AdjacencyListGraph::Edge>& adj) {
    return AdjacencyListGraph(graph_, GetAllocator(), entry_name, exit_name, adj);
  }

  bool IsValidSubgraph(const ExecutionSubgraph* esg) {
    return ExecutionSubgraph::CalculateValidity(graph_, esg);
  }

  bool IsValidSubgraph(const ExecutionSubgraph& esg) {
    return ExecutionSubgraph::CalculateValidity(graph_, &esg);
  }

  HGraph* graph_;
};

// Some comparators used by these tests to avoid having to deal with various set types.
template <typename BLKS, typename = std::enable_if_t<!std::is_same_v<BlockSet, BLKS>>>
bool operator==(const BlockSet& bs, const BLKS& sas) {
  std::unordered_set<const HBasicBlock*> us(sas.begin(), sas.end());
  return bs == us;
}
template <typename BLKS, typename = std::enable_if_t<!std::is_same_v<BlockSet, BLKS>>>
bool operator==(const BLKS& sas, const BlockSet& bs) {
  return bs == sas;
}
template <typename BLKS, typename = std::enable_if_t<!std::is_same_v<BlockSet, BLKS>>>
bool operator!=(const BlockSet& bs, const BLKS& sas) {
  return !(bs == sas);
}
template <typename BLKS, typename = std::enable_if_t<!std::is_same_v<BlockSet, BLKS>>>
bool operator!=(const BLKS& sas, const BlockSet& bs) {
  return !(bs == sas);
}

// +-------+       +-------+
// | right | <--   | entry |
// +-------+       +-------+
//   |               |
//   |               |
//   |               v
//   |           + - - - - - +
//   |           '  removed  '
//   |           '           '
//   |           ' +-------+ '
//   |           ' | left  | '
//   |           ' +-------+ '
//   |           '           '
//   |           + - - - - - +
//   |               |
//   |               |
//   |               v
//   |             +-------+
//   +--------->   | exit  |
//                 +-------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphBasic) {
  AdjacencyListGraph blks(SetupFromAdjacencyList(
      "entry",
      "exit",
      { { "entry", "left" }, { "entry", "right" }, { "left", "exit" }, { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("left"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 3u);
  ASSERT_TRUE(contents.find(blks.Get("left")) == contents.end());

  ASSERT_TRUE(contents.find(blks.Get("right")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
  esg.RemoveBlock(blks.Get("right"));
  esg.Finalize();
  std::unordered_set<const HBasicBlock*> contents_2(esg.ReachableBlocks().begin(),
                                                    esg.ReachableBlocks().end());
  ASSERT_EQ(contents_2.size(), 0u);
}

//                   +-------+         +-------+
//                   | right |   <--   | entry |
//                   +-------+         +-------+
//                     |                 |
//                     |                 |
//                     |                 v
//                     |             + - - - - - - - - - - - - - - - - - - - -+
//                     |             '             indirectly_removed         '
//                     |             '                                        '
//                     |             ' +-------+                      +-----+ '
//                     |             ' |  l1   | -------------------> | l1r | '
//                     |             ' +-------+                      +-----+ '
//                     |             '   |                              |     '
//                     |             '   |                              |     '
//                     |             '   v                              |     '
//                     |             ' +-------+                        |     '
//                     |             ' |  l1l  |                        |     '
//                     |             ' +-------+                        |     '
//                     |             '   |                              |     '
//                     |             '   |                              |     '
//                     |             '   |                              |     '
// + - - - - - - - -+  |      +- - -     |                              |     '
// '                '  |      +-         v                              |     '
// ' +-----+           |               +----------------+               |     '
// ' | l2r | <---------+-------------- |  l2 (removed)  | <-------------+     '
// ' +-----+           |               +----------------+                     '
// '   |            '  |      +-         |                                    '
// '   |       - - -+  |      +- - -     |         - - - - - - - - - - - - - -+
// '   |     '         |             '   |       '
// '   |     '         |             '   |       '
// '   |     '         |             '   v       '
// '   |     '         |             ' +-------+ '
// '   |     '         |             ' |  l2l  | '
// '   |     '         |             ' +-------+ '
// '   |     '         |             '   |       '
// '   |     '         |             '   |       '
// '   |     '         |             '   |       '
// '   |       - - -+  |      +- - -     |       '
// '   |            '  |      +-         v       '
// '   |               |               +-------+ '
// '   +---------------+-------------> |  l3   | '
// '                   |               +-------+ '
// '                '  |      +-                 '
// + - - - - - - - -+  |      +- - - - - - - - - +
//                     |                 |
//                     |                 |
//                     |                 v
//                     |               +-------+
//                     +----------->   | exit  |
//                                     +-------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphPropagation) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "l1" },
                                                   { "l1", "l1l" },
                                                   { "l1", "l1r" },
                                                   { "l1l", "l2" },
                                                   { "l1r", "l2" },
                                                   { "l2", "l2l" },
                                                   { "l2", "l2r" },
                                                   { "l2l", "l3" },
                                                   { "l2r", "l3" },
                                                   { "l3", "exit" },
                                                   { "entry", "right" },
                                                   { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("l2"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  // ASSERT_EQ(contents.size(), 3u);
  // Not present, no path through.
  ASSERT_TRUE(contents.find(blks.Get("l1")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l2")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l3")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l1l")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l1r")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l2l")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l2r")) == contents.end());

  // present, path through.
  ASSERT_TRUE(contents.find(blks.Get("right")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
}

// +------------------------------------+
// |                                    |
// |  +-------+       +-------+         |
// |  | right | <--   | entry |         |
// |  +-------+       +-------+         |
// |    |               |               |
// |    |               |               |
// |    |               v               |
// |    |             +-------+       +--------+
// +----+--------->   |  l1   |   --> | l1loop |
//      |             +-------+       +--------+
//      |               |
//      |               |
//      |               v
//      |           +- - - - - -+
//      |           '  removed  '
//      |           '           '
//      |           ' +-------+ '
//      |           ' |  l2   | '
//      |           ' +-------+ '
//      |           '           '
//      |           +- - - - - -+
//      |               |
//      |               |
//      |               v
//      |             +-------+
//      +--------->   | exit  |
//                    +-------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphPropagationLoop) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "l1" },
                                                   { "l1", "l2" },
                                                   { "l1", "l1loop" },
                                                   { "l1loop", "l1" },
                                                   { "l2", "exit" },
                                                   { "entry", "right" },
                                                   { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("l2"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 5u);

  // Not present, no path through.
  ASSERT_TRUE(contents.find(blks.Get("l2")) == contents.end());

  // present, path through.
  // Since the loop can diverge we should leave it in the execution subgraph.
  ASSERT_TRUE(contents.find(blks.Get("l1")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l1loop")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("right")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
}

// +--------------------------------+
// |                                |
// |  +-------+     +-------+       |
// |  | right | <-- | entry |       |
// |  +-------+     +-------+       |
// |    |             |             |
// |    |             |             |
// |    |             v             |
// |    |           +-------+     +--------+
// +----+---------> |  l1   | --> | l1loop |
//      |           +-------+     +--------+
//      |             |
//      |             |
//      |             v
//      |           +-------+
//      |           |  l2   |
//      |           +-------+
//      |             |
//      |             |
//      |             v
//      |           +-------+
//      +---------> | exit  |
//                  +-------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphPropagationLoop2) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "l1" },
                                                   { "l1", "l2" },
                                                   { "l1", "l1loop" },
                                                   { "l1loop", "l1" },
                                                   { "l2", "exit" },
                                                   { "entry", "right" },
                                                   { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("l1"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 3u);

  // Not present, no path through.
  ASSERT_TRUE(contents.find(blks.Get("l1")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l1loop")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l2")) == contents.end());

  // present, path through.
  ASSERT_TRUE(contents.find(blks.Get("right")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
}

// +--------------------------------+
// |                                |
// |  +-------+     +-------+       |
// |  | right | <-- | entry |       |
// |  +-------+     +-------+       |
// |    |             |             |
// |    |             |             |
// |    |             v             |
// |    |           +-------+     +--------+
// +----+---------> |  l1   | --> | l1loop |
//      |           +-------+     +--------+
//      |             |
//      |             |
//      |             v
//      |           +-------+
//      |           |  l2   |
//      |           +-------+
//      |             |
//      |             |
//      |             v
//      |           +-------+
//      +---------> | exit  |
//                  +-------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphPropagationLoop3) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "l1" },
                                                   { "l1", "l2" },
                                                   { "l1", "l1loop" },
                                                   { "l1loop", "l1" },
                                                   { "l2", "exit" },
                                                   { "entry", "right" },
                                                   { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("l1loop"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 3u);

  // Not present, no path through. If we got to l1 loop then we must merge back
  // with l1 and l2 so they're bad too.
  ASSERT_TRUE(contents.find(blks.Get("l1loop")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l1")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("l2")) == contents.end());

  // present, path through.
  ASSERT_TRUE(contents.find(blks.Get("right")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
}

TEST_F(ExecutionSubgraphTest, ExecutionSubgraphInvalid) {
  AdjacencyListGraph blks(SetupFromAdjacencyList(
      "entry",
      "exit",
      { { "entry", "left" }, { "entry", "right" }, { "left", "exit" }, { "right", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("left"));
  esg.RemoveBlock(blks.Get("right"));
  esg.Finalize();

  ASSERT_FALSE(esg.IsValid());
  ASSERT_FALSE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 0u);
}
// Sibling branches are disconnected.
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphExclusions) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "a" },
                                                   { "entry", "b" },
                                                   { "entry", "c" },
                                                   { "a", "exit" },
                                                   { "b", "exit" },
                                                   { "c", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("a"));
  esg.RemoveBlock(blks.Get("c"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 3u);
  // Not present, no path through.
  ASSERT_TRUE(contents.find(blks.Get("a")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c")) == contents.end());

  // present, path through.
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("b")) != contents.end());

  ArrayRef<const ExecutionSubgraph::ExcludedCohort> exclusions(esg.GetExcludedCohorts());
  ASSERT_EQ(exclusions.size(), 2u);
  std::unordered_set<const HBasicBlock*> exclude_a({ blks.Get("a") });
  std::unordered_set<const HBasicBlock*> exclude_c({ blks.Get("c") });
  ASSERT_TRUE(std::find_if(exclusions.cbegin(),
                           exclusions.cend(),
                           [&](const ExecutionSubgraph::ExcludedCohort& it) {
                             return it.Blocks() == exclude_a;
                           }) != exclusions.cend());
  ASSERT_TRUE(std::find_if(exclusions.cbegin(),
                           exclusions.cend(),
                           [&](const ExecutionSubgraph::ExcludedCohort& it) {
                             return it.Blocks() == exclude_c;
                           }) != exclusions.cend());
}

// Sibling branches are disconnected.
//                                      +- - - - - - - - - - - - - - - - - - - - - - +
//                                      '                      remove_c              '
//                                      '                                            '
//                                      ' +-----------+                              '
//                                      ' | c_begin_2 | -------------------------+   '
//                                      ' +-----------+                          |   '
//                                      '                                        |   '
//                                      +- - - - - - - - - - - - - - - - - -     |   '
//                                          ^                                '   |   '
//                                          |                                '   |   '
//                                          |                                '   |   '
//                   + - - - - - -+                                          '   |   '
//                   '  remove_a  '                                          '   |   '
//                   '            '                                          '   |   '
//                   ' +--------+ '       +-----------+                 +---+'   |   '
//                   ' | **a**  | ' <--   |   entry   |   -->           | b |'   |   '
//                   ' +--------+ '       +-----------+                 +---+'   |   '
//                   '            '                                          '   |   '
//                   + - - - - - -+                                          '   |   '
//                       |                  |                             |  '   |   '
//                       |                  |                             |  '   |   '
//                       |                  v                             |  '   |   '
//                       |              +- - - - - - - -+                 |  '   |   '
//                       |              '               '                 |  '   |   '
//                       |              ' +-----------+ '                 |  '   |   '
//                       |              ' | c_begin_1 | '                 |  '   |   '
//                       |              ' +-----------+ '                 |  '   |   '
//                       |              '   |           '                 |  '   |   '
//                       |              '   |           '                 |  '   |   '
//                       |              '   |           '                 |  '   |   '
// + - - - - - - - - -+  |       + - - -    |            - - - - - - - +  |  '   |   '
// '                  '  |       +          v                          '  |  +   |   '
// ' +---------+         |                +-----------+                   |      |   '
// ' | c_end_2 | <-------+--------------- | **c_mid** | <-----------------+------+   '
// ' +---------+         |                +-----------+                   |          '
// '                  '  |       +          |                          '  |  +       '
// + - - - - - - - - -+  |       + - - -    |            - - - - - - - +  |  + - - - +
//     |                 |              '   |           '                 |
//     |                 |              '   |           '                 |
//     |                 |              '   v           '                 |
//     |                 |              ' +-----------+ '                 |
//     |                 |              ' |  c_end_1  | '                 |
//     |                 |              ' +-----------+ '                 |
//     |                 |              '               '                 |
//     |                 |              +- - - - - - - -+                 |
//     |                 |                  |                             |
//     |                 |                  |                             |
//     |                 |                  v                             v
//     |                 |                +---------------------------------+
//     |                 +------------>   |              exit               |
//     |                                  +---------------------------------+
//     |                                    ^
//     +------------------------------------+
TEST_F(ExecutionSubgraphTest, ExecutionSubgraphExclusionExtended) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "a" },
                                                   { "entry", "b" },
                                                   { "entry", "c_begin_1" },
                                                   { "entry", "c_begin_2" },
                                                   { "c_begin_1", "c_mid" },
                                                   { "c_begin_2", "c_mid" },
                                                   { "c_mid", "c_end_1" },
                                                   { "c_mid", "c_end_2" },
                                                   { "a", "exit" },
                                                   { "b", "exit" },
                                                   { "c_end_1", "exit" },
                                                   { "c_end_2", "exit" } }));
  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("a"));
  esg.RemoveBlock(blks.Get("c_mid"));
  esg.Finalize();
  ASSERT_TRUE(esg.IsValid());
  ASSERT_TRUE(IsValidSubgraph(esg));
  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());

  ASSERT_EQ(contents.size(), 3u);
  // Not present, no path through.
  ASSERT_TRUE(contents.find(blks.Get("a")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c_begin_1")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c_begin_2")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c_mid")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c_end_1")) == contents.end());
  ASSERT_TRUE(contents.find(blks.Get("c_end_2")) == contents.end());

  // present, path through.
  ASSERT_TRUE(contents.find(blks.Get("entry")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("exit")) != contents.end());
  ASSERT_TRUE(contents.find(blks.Get("b")) != contents.end());

  ArrayRef<const ExecutionSubgraph::ExcludedCohort> exclusions(esg.GetExcludedCohorts());
  ASSERT_EQ(exclusions.size(), 2u);
  BlockSet exclude_a({ blks.Get("a") });
  BlockSet exclude_c({ blks.Get("c_begin_1"),
                       blks.Get("c_begin_2"),
                       blks.Get("c_mid"),
                       blks.Get("c_end_1"),
                       blks.Get("c_end_2") });
  ASSERT_TRUE(std::find_if(exclusions.cbegin(),
                           exclusions.cend(),
                           [&](const ExecutionSubgraph::ExcludedCohort& it) {
                             return it.Blocks() == exclude_a;
                           }) != exclusions.cend());
  ASSERT_TRUE(
      std::find_if(
          exclusions.cbegin(), exclusions.cend(), [&](const ExecutionSubgraph::ExcludedCohort& it) {
            return it.Blocks() == exclude_c &&
                   BlockSet({ blks.Get("c_begin_1"), blks.Get("c_begin_2") }) == it.EntryBlocks() &&
                   BlockSet({ blks.Get("c_end_1"), blks.Get("c_end_2") }) == it.ExitBlocks();
          }) != exclusions.cend());
}

//    ┌───────┐     ┌────────────┐
// ┌─ │ right │ ◀── │   entry    │
// │  └───────┘     └────────────┘
// │                  │
// │                  │
// │                  ▼
// │                ┌────────────┐
// │                │  esc_top   │
// │                └────────────┘
// │                  │
// │                  │
// │                  ▼
// │                ┌────────────┐
// └──────────────▶ │   middle   │ ─┐
//                  └────────────┘  │
//                    │             │
//                    │             │
//                    ▼             │
//                  ┌────────────┐  │
//                  │ esc_bottom │  │
//                  └────────────┘  │
//                    │             │
//                    │             │
//                    ▼             │
//                  ┌────────────┐  │
//                  │    exit    │ ◀┘
//                  └────────────┘
TEST_F(ExecutionSubgraphTest, InAndOutEscape) {
  AdjacencyListGraph blks(SetupFromAdjacencyList("entry",
                                                 "exit",
                                                 { { "entry", "esc_top" },
                                                   { "entry", "right" },
                                                   { "esc_top", "middle" },
                                                   { "right", "middle" },
                                                   { "middle", "exit" },
                                                   { "middle", "esc_bottom" },
                                                   { "esc_bottom", "exit" } }));

  ExecutionSubgraph esg(graph_, /*valid=*/true, GetScopedAllocator());
  esg.RemoveBlock(blks.Get("esc_top"));
  esg.RemoveBlock(blks.Get("esc_bottom"));
  esg.Finalize();

  std::unordered_set<const HBasicBlock*> contents(esg.ReachableBlocks().begin(),
                                                  esg.ReachableBlocks().end());
  ASSERT_EQ(contents.size(), 0u);
  ASSERT_FALSE(esg.IsValid());
  ASSERT_FALSE(IsValidSubgraph(esg));

  ASSERT_EQ(contents.size(), 0u);
}

}  // namespace art
