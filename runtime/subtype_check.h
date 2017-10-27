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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_H_
#define ART_RUNTIME_SUBTYPE_CHECK_H_

#include "subtype_check_info.h"

#include "runtime.h"
#include "base/mutex.h"
#include "mirror/class.h" // TODO: move these to instance_oftree.h


/**
 * Any node in a tree can have its path (from the root to the node) represented as a string by
 * concatenating the path of the parent to that of the current node.
 *
 * We can annotate each node with a `sibling-label` which is some value unique amongst all of the
 * node's siblings. As a special case, the root is empty.
 *
 *           (none)
 *        /    |     \
 *       A     B      C
 *     /   \
 *    A’    B’
 *          |
 *          A’’
 *          |
 *          A’’’
 *          |
 *          A’’’’
 *
 * Given these sibling-labels, we can now encode the path from any node to the root by starting at
 * the node and going up to the root, marking each node with this `path-label`. The special
 * character Ø means "end of path".
 *
 *             Ø
 *        /    |      \
 *       AØ    BØ     CØ
 *     /    \
 *   A’AØ   B’AØ
 *           |
 *           A’’B’AØ
 *           |
 *           A’’’A’’B’AØ
 *           |
 *           A’’’’A’’B’AØ
 *
 * Given the above `path-label` we can express if any two nodes are an offspring of the other
 * through a O(1) expression:
 *
 *    x <: y :=
 *      suffix(x, y) == y
 *
 * In the above example suffix(x,y) means the suffix of x that is as long as y (right-padded with
 * Øs if x is shorter than y) :
 *
 *    suffix(x,y) := x(x.length - y.length .. 0]
 *                     + repeat(Ø, max(y.length - x.length, 0))
 *
 * A few generalities here to elaborate:
 *
 * - There can be at most D levels in the tree.
 * - Each level L has an alphabet A, and the maximum number of
 *   nodes is determined by |A|
 * - The alphabet A can be a subset, superset, equal, or unique with respect to the other alphabets
 *   without loss of generality. (In practice it would almost always be a subset of the previous
 *   level’s alphabet as we assume most classes have less children the deeper they are.)
 * - The `sibling-label` doesn’t need to be stored as an explicit value. It can a temporary when
 *   visiting every immediate child of a node. Only the `path-label` needs to be actually stored for
 *   every node.
 *
 * The path can also be reversed, and use a prefix instead of a suffix to define the subchild
 * relation.
 *
 *             Ø
 *        /    |      \    \
 *       AØ    BØ     CØ    DØ
 *     /    \
 *   AA’Ø   AB’Ø
 *            |
 *            AB’A’’Ø
 *            |
 *            AB’A’’A’’’Ø
 *            |
 *            AB’A’’A’’’A’’’’Ø
 *
 *    x <: y :=
 *      prefix(x, y) == y
 *
 *    prefix(x,y) := x[0 .. y.length)
 *                     + repeat(Ø, max(y.length - x.length, 0))
 *
 * In a dynamic tree, new nodes can be inserted at any time. This means if a minimal alphabet is
 * selected to contain the initial tree hierarchy, later node insertions will be illegal because
 * there is no more room to encode the path.
 *
 * In this simple example with an alphabet A,B,C and max level 1:
 *
 *     Level
 *     0:               Ø
 *              /     |     \     \
 *     1:      AØ     BØ     CØ    DØ   (illegal)
 *              |
 *     2:      AAØ  (illegal)
 *
 * Attempting to insert the sibling “D” at Level 1 would be illegal because the Alphabet(1) is
 * {A,B,C} and inserting an extra node would mean the `sibling-label` is no longer unique.
 * Attempting to insert “AAØ” is illegal because the level 2 is more than the max level 1.
 *
 * One solution to this would be to revisit the entire graph, select a larger alphabet to that
 * every `sibling-label` is unique, pick a larger max level count, and then store the updated
 * `path-label` accordingly.
 *
 * The more common approach would instead be to select a set of alphabets and max levels statically,
 * with large enough sizes, for example:
 *
 *     Alphabets = {{A,B,C,D}, {A,B,C}, {A,B}, {A}}
 *     Max Levels = |Alphabets|
 *
 * Which would allow up to 4 levels with each successive level having 1 less max siblings.
 *
 * Attempting to insert a new node into the graph which does not fit into that level’s alphabet
 * would be represented by re-using the `path-label` of the parent. Such a `path_label` would be
 * considered truncated (because it would only have a prefix of the full path from the root to the
 * node).
 *
 *    Level
 *    0:             Ø
 *             /     |     \     \
 *    1:      AØ     BØ     CØ    Ø   (same as parent)
 *             |
 *    2:      AØ (same as parent)
 *
 * The updated relation for offspring is then:
 *
 *    x <: y :=
 *      if !truncated_path(y):
 *        return prefix(x, y) == y               // O(1)
 *      else:
 *        return slow_check_is_offspring(x, y)   // worse than O(1)
 *
 * (Example definition of truncated_path -- any semantically equivalent way to check that the
 *  sibling's `sibling-label` is not unique will do)
 *
 *    truncated_path(y) :=
 *      return y == parent(y)
 *
 * (Example definition. Any slower-than-O(1) definition will do here. This is the traversing
 * superclass hierarchy solution)
 *
 *    slow_check_is_offspring(x, y) :=
 *      if not x: return false
 *      else: return x == y || recursive_is_offspring(parent(x), y)
 *
 * In which case slow_check_is_offspring is some non-O(1) way to check if x and is an offspring of y.
 *
 * In addition, note that it doesn’t matter if the "x" from above is a unique sibling or not; the
 * relation will still be correct.
 *
 * ------------------------------------------------------------------------------------------------
 *
 * Leveraging truncated paths to minimize path lengths.
 *
 * As observed above, for any x <: y, it is sufficient to have a full path only for y,
 * and x can be truncated (to its nearest ancestor's full path).
 *
 * We call a node that stores a full path "Assigned", and a node that stores a truncated path
 * either "Initialized" or "Overflowed."
 *
 * "Initialized" means it is still possible to assign a full path to the node, and "Overflowed"
 * means there is insufficient characters in the alphabet left.
 *
 * In this example, assume that we attempt to "Assign" all non-leafs if possible. Leafs
 * always get truncated (as either Initialized or Overflowed).
 *
 *     Alphabets = {{A,B,C,D}, {A,B}}
 *     Max Levels = |Alphabets|
 *
 *    Level
 *    0:             Ø
 *             /     |     \     \     \
 *    1:      AØ     BØ     CØ    DØ    Ø (Overflowed: Too wide)
 *            |             |
 *    2:     AAØ            CØ (Initialized)
 *            |
 *    3:     AAØ (Overflowed: Too deep)
 *
 * (All un-annotated nodes are "Assigned").
 * Above, the node at level 3 becomes overflowed because it exceeds the max levels. The
 * right-most node at level 1 becomes overflowed because there's no characters in the alphabet
 * left in that level.
 *
 * The "CØ" node is Initialized at level 2, but it can still be promoted to "Assigned" later on
 * if we wanted to.
 *
 * In particular, this is the strategy we use in our implementation
 * (SubtypeCheck::EnsureInitialized, SubtypeCheck::EnsureAssigned).
 *
 * Since the # of characters in our alphabet (BitString) is very limited, we want to avoid
 * allocating a character to a node until its absolutely necessary.
 *
 * All node targets (in `src <: target`) get Assigned, and any parent of an Initialized
 * node also gets Assigned.
 */
namespace art {

// This is the base class using the curiously-recurring template pattern to enable
// testability without losing performance. The real class to be used in ART is "SubtypeCheck"
// (see below).
template <typename KlassT, typename ParentT>
struct SubtypeCheckBase {
  // Force this class's SubtypeCheckInfo state into at least Initialized.
  // As a side-effect, all parent classes also become Assigned|Overflow.
  //
  // Cost: O(Depth(Class))
  //
  // Post-condition: State is >= Initialized.
  // Returns: The precise SubtypeCheckInfo::State.
  SubtypeCheckInfo::State EnsureInitialized()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return InitializeOrAssign(/*assign*/false).GetState();
  }

  // Force this class's SubtypeCheckInfo state into at Assigned|Overflowed.
  // As a side-effect, all parent classes also become Assigned|Overflow.
  //
  // Cost: O(Depth(Class))
  //
  // Post-condition: State is Assigned|Overflowed.
  // Returns: The precise SubtypeCheckInfo::State.
  SubtypeCheckInfo::State EnsureAssigned()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckInfo io = InitializeOrAssign(/*assign*/true);
    return io.GetState();
  }

  // Resets the SubtypeCheckInfo into the Uninitialized state.
  //
  // Intended only for the AOT image writer.
  // This is a static function to avoid calling klass.Depth(), which is unsupported
  // in some portions of the image writer.
  //
  // Cost: O(1).
  //
  // Returns: A state that is always Uninitialized.
  static SubtypeCheckInfo::State ForceUninitialize(const KlassT& klass)
    REQUIRES(Locks::subtype_check_lock_)
    REQUIRES_SHARED(Locks::mutator_lock_) {
    // TODO: DCHECK that it's for AOT compilation.

    // Directly read/write the class field here.
    // As this method is used by image_writer on a copy,
    // the Class* there is not a real class and using it for anything
    // more complicated (e.g. ObjPtr or Depth call) will fail dchecks.

    SubtypeCheckBitsAndStatus io_uninitialized = ParentT::ReadField(klass);
      // OK. zero-initializing instance_of_ puts us into the kUninitialized state.
    io_uninitialized.instance_of_ = SubtypeCheckBits{};
    ParentT::WriteField(klass, io_uninitialized);

    // Do not use "SubtypeCheckInfo" API here since that requires Depth()
    // which would cause a dcheck failure.
    return SubtypeCheckInfo::kUninitialized;
  }

  // Resets the SubtypeCheckInfo into the Uninitialized state.
  SubtypeCheckInfo::State ForceUninitialize()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {

    return ForceUninitialize(klass_);
  }

  // Retrieve the path to root bitstring as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the encoded_src value. Must be >= Initialized (EnsureInitialized).
  BitString::StorageType GetEncodedPathToRootForSource()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_NE(SubtypeCheckInfo::kUninitialized, GetSubtypeCheckInfo().GetState());
    return GetSubtypeCheckInfo().GetEncodedPathToRoot();
  }

  // Retrieve the path to root bitstring as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the encoded_target value. Must be Assigned (EnsureAssigned).
  BitString::StorageType GetEncodedPathToRootForTarget()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo().GetState());
    return GetSubtypeCheckInfo().GetEncodedPathToRoot();
  }

  // Retrieve the path to root bitstring mask as a plain uintN_t value that is amenable to
  // be used by a fast check "encoded_src & mask_target == encoded_target".
  //
  // Cost: O(Depth(Class)).
  //
  // Returns the mask_target value. Must be Assigned (EnsureAssigned).
  BitString::StorageType GetEncodedPathToRootMask()
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo().GetState());
    return GetSubtypeCheckInfo().GetEncodedPathToRootMask();
  }

  // Cast the class into an SubtypeCheck.
  // This operation is zero-cost, the tree is just a wrapper around a pointer.
  static ParentT Lookup(const KlassT& klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ParentT tree(klass);
    // TODO: reject interfaces.
    return tree;
  }
  // TODO: Rename "Lookup" to "FromClass" to make it seem cheaper.

  // Is this class a subclass of the target?
  //
  // The current state must be at least Initialized, and the target state
  // must be Assigned, otherwise the result will return kUnknownSubtypeOf.
  //
  // See EnsureInitialized and EnsureAssigned. Ideally,
  // EnsureInitialized will be called previously on all possible sources,
  // and EnsureAssigned will be called previously on all possible targets.
  //
  // Runtime cost: O(Depth(Class)), but would be O(1) if depth was known.
  //
  // If the result is known, return kSubtypeOf or kNotSubtypeOf.
  SubtypeCheckInfo::Result IsSubtypeOf(const KlassT& target)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    ParentT tree = target.Lookup();
    return IsSubtypeOf(tree);
  }

  // See above.
  SubtypeCheckInfo::Result IsSubtypeOf(const ParentT& target_tree)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckInfo io = GetSubtypeCheckInfo();
    SubtypeCheckInfo target_io = target_tree.GetSubtypeCheckInfo();

    return io.IsSubtypeOf(target_io);
  }

  // Print SubtypeCheck bitstring and overflow to a stream (e.g. for oatdump).
  std::ostream& Dump(std::ostream& os) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return os << GetSubtypeCheckInfo();
  }

  static void WriteStatus(const KlassT& klass, ClassStatus status)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return ParentT::WriteStatus(klass, status);
  }

 private:
  // Look up the tree corresponding to the SuperClass.
  // Runtime cost: O(1).
  ParentT LookupParent() const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(HasParent());

    return Lookup(klass_->GetSuperClass());
  }

  SubtypeCheckInfo InitializeOrAssign(bool assign)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    if (UNLIKELY(!HasParent())) {
      // Object root always goes directly from Uninitialized -> Assigned.
      auto func = [](SubtypeCheckInfo io) {
        if (io.GetState() != SubtypeCheckInfo::kUninitialized) {
          return io;  // No change needed.
        }
        return io.NewForRoot();
      };
      // FIXME: assert this class is the Object root, and no other class.
      SubtypeCheckInfo io = Update(func);
      // The object root is always in the Uninitialized|Assigned state.
      DCHECK_EQ(SubtypeCheckInfo::kAssigned, GetSubtypeCheckInfo().GetState())
          << "Invalid object root state, must be Assigned";
      return io;
    }

    // Force all ancestors to Assigned | Overflow.
    LookupParent().EnsureAssigned();
    if (kIsDebugBuild) {
      SubtypeCheckInfo::State parent_state = LookupParent().GetSubtypeCheckInfo().GetState();
      DCHECK(parent_state == SubtypeCheckInfo::kAssigned || parent_state == SubtypeCheckInfo::kOverflowed)
          << "Expected parent Assigned|Overflowed, but was: " << parent_state;
    }

    auto func = [assign](SubtypeCheckInfo io, SubtypeCheckInfo parent_io) {
      SubtypeCheckInfo::State io_state = io.GetState();
      // Skip doing any work if the state is already up-to-date.
      //   - assign == false -> Initialized or higher.
      //   - assign == true  -> Assigned or higher.
      if (io_state == SubtypeCheckInfo::kUninitialized ||
          (io_state == SubtypeCheckInfo::kInitialized && assign)) {
        // Copy parent path into the child.
        //
        // If assign==true, this also appends Parent.Next value to the end.
        // Then the Parent.Next value is incremented to avoid allocating
        // the same value again to another node.
        io = parent_io.NewForChild(assign);  // Note: Parent could be mutated.
      }  // else nothing to do, already >= Initialized.
      return std::make_pair(io, parent_io);
    };
    const std::pair<SubtypeCheckInfo, SubtypeCheckInfo> io_and_parent = UpdateSelfAndParent(func);
    SubtypeCheckInfo io = io_and_parent.first;
    // Post-condition: EnsureAssigned -> Assigned|Overflowed.
    // Post-condition: EnsureInitialized -> Not Uninitialized.
    DCHECK_NE(io.GetState(), SubtypeCheckInfo::kUninitialized);

    if (assign) {
      DCHECK_NE(io.GetState(), SubtypeCheckInfo::kInitialized);
    }

    return io;
  }

  bool HasParent() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return klass_->HasSuperClass();
  }

  // Given a function "SubtypeCheckInfo func(SubtypeCheckInfo)",
  // read the current SubtypeCheckInfo, calling func with it, and then
  // updating the current SubtypeCheckInfo with the return value of func.
  //
  // Returns the return value of calling func.
  template <typename T>
  SubtypeCheckInfo Update(T func)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Read.
    SubtypeCheckBitsAndStatus current_ios;
    const SubtypeCheckInfo current = GetSubtypeCheckInfo(klass_, /*out*/&current_ios);
    // Modify.
    const SubtypeCheckInfo updated = func(current);

    // Write.
    // if (memcmp(&current, &updated, sizeof(current)) != 0) {
      // Avoid dirtying memory when the data hasn't changed.
      SetSubtypeCheckInfo(updated, current_ios);
    // }
    // Return written copy.
    return updated;
  }

  // Given a function "pair<SubtypeCheckInfo,SubtypeCheckInfo> func(SubtypeCheckInfo,SubtypeCheckInfo)",
  // read the current SubtypeCheckInfo and the parent, calling func with them, and then
  // updating the current SubtypeCheckInfo and the parent with the return value of func.
  //
  // Returns the return value of calling func.
  template <typename T>
  std::pair<SubtypeCheckInfo, SubtypeCheckInfo> UpdateSelfAndParent(T func)
        REQUIRES(Locks::subtype_check_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    DCHECK(HasParent());

     // Read.
    SubtypeCheckBitsAndStatus current_ios;
    const SubtypeCheckInfo current = GetSubtypeCheckInfo(klass_, /*out*/&current_ios);

    ParentT parent_tree = LookupParent();
    SubtypeCheckBitsAndStatus current_ios_parent;
    const SubtypeCheckInfo current_parent = GetSubtypeCheckInfo(parent_tree.klass_, /*out*/&current_ios_parent);

    // Modify.
    const std::pair<SubtypeCheckInfo, SubtypeCheckInfo> updated_pair = func(current, current_parent);

    const SubtypeCheckInfo& updated = updated_pair.first; // self
    const SubtypeCheckInfo& updated_parent = updated_pair.second; // parent

    // Write.
    // TODO: with the memcmps to avoid dirtying memory.
    // if (memcmp(&current, &updated, sizeof(current)) != 0) {
      // Avoid dirtying memory when the data hasn't changed.
      SetSubtypeCheckInfo(updated, current_ios);
    // }

    // Write.
    // if (memcmp(&current_parent, &updated_parent, sizeof(current_parent)) != 0) {
      // Avoid dirtying memory when the data hasn't changed.
      parent_tree.SetSubtypeCheckInfo(updated_parent, current_ios_parent);
    // }

    // Return written copies.
    return updated_pair;
  }

  // All of the below get/set are O(depth(class)) because they require the class depth
  // to be read to instantiate an SubtypeCheckInfo label.

  // TODO: clean up all these setter getters etc. Don't need so many.

  static SubtypeCheckInfo GetSubtypeCheckInfo(const KlassT& klass,
                                  /*out*/SubtypeCheckBitsAndStatus* storage)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckBitsAndStatus current_ios = GetStoredSubtypeCheckBitsAndStatus(klass);

    size_t depth = klass->Depth();
    const SubtypeCheckInfo current = SubtypeCheckInfo::Infuse(current_ios.instance_of_, depth);

    *storage = current_ios;
    return current;
  }

  void SetSubtypeCheckInfo(const SubtypeCheckInfo& new_instanceof, const SubtypeCheckInfo& prev_instanceof)
        REQUIRES(Locks::subtype_check_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    UNUSED(prev_instanceof);
    // if (memcmp(&new_instanceof, &prev_instanceof, sizeof(SubtypeCheckInfo)) != 0) {
      // Avoid dirtying memory when the data hasn't changed.
      SetSubtypeCheckInfo(new_instanceof);
    // }
  }

  void SetSubtypeCheckInfo(const SubtypeCheckInfo& new_instanceof)
        REQUIRES(Locks::subtype_check_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    SetSubtypeCheckInfo(new_instanceof, GetStoredSubtypeCheckBitsAndStatus(klass_));
  }

  static SubtypeCheckBitsAndStatus GetStoredSubtypeCheckBitsAndStatus(const KlassT& klass)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    return ParentT::ReadField(klass);
  }

  // Note: Can't use ObjPtr here since image_writer copies are not in low-4gb.
  static SubtypeCheckBitsAndStatus ReadField(const KlassT& klass)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return ParentT::ReadField(klass);
  }

  static void WriteField(const KlassT& klass, const SubtypeCheckBitsAndStatus& new_ios)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return ParentT::WriteField(klass, new_ios);
  }

 protected:
  SubtypeCheckInfo GetSubtypeCheckInfo() const
        REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckBitsAndStatus unused;
    UNUSED(unused);
    return GetSubtypeCheckInfo(klass_, /*out*/&unused);
  }

  void SetSubtypeCheckInfo(const SubtypeCheckInfo& new_instanceof,
                     const SubtypeCheckBitsAndStatus& storage)
        REQUIRES(Locks::subtype_check_lock_)
        REQUIRES_SHARED(Locks::mutator_lock_) {
    SubtypeCheckBitsAndStatus new_ios = storage;
    new_ios.instance_of_ = new_instanceof.Slice();
    ParentT::WriteField(klass_, new_ios);
  }

  KlassT klass_;

  friend std::ostream& operator<<(std::ostream& os, const SubtypeCheckBase& tree)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    os << "(SubtypeCheck io:" << tree.GetSubtypeCheckInfo() << ", class: " << tree.klass_->PrettyClass()
       << ")";
    return os;
  };

  explicit SubtypeCheckBase(const KlassT& klass) : klass_(klass) {}
 public:
  SubtypeCheckBase() = default;
  SubtypeCheckBase(const SubtypeCheckBase& other) = default;
  SubtypeCheckBase(SubtypeCheckBase&& other) = default;
  ~SubtypeCheckBase() = default;
};

// A zero-cost wrapper around a mirror::Class* that abstracts out access to SubtypeCheckInfo operations.
// See also above of this file for the documentation.
struct SubtypeCheck : public SubtypeCheckBase<mirror::Class*, SubtypeCheck> {
  using ClassT = mirror::Class*;
  using BaseT = SubtypeCheckBase<ClassT, SubtypeCheck>;
  using BaseT::Lookup;

  // Convenience wrapper to lookup ObjPtr<mirror::Class> as an SubtypeCheck.
  static SubtypeCheck Lookup(ObjPtr<mirror::Class> klass) REQUIRES_SHARED(Locks::mutator_lock_) {
    return BaseT::Lookup(klass.Ptr());
  }

  explicit SubtypeCheck(const ClassT& klass) : BaseT(klass) {}

  //////////////////////////////////////////////////////////////////////////////////////////////////
  // The rest of this public definition is the SubtypeCheckBase static interface implementation:
  //////////////////////////////////////////////////////////////////////////////////////////////////

  static SubtypeCheckBitsAndStatus ReadField(const ClassT&)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static void WriteField(const ClassT&, const SubtypeCheckBitsAndStatus& new_ios)
      REQUIRES(Locks::subtype_check_lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static void WriteStatus(const ClassT&, ClassStatus status)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // TODO: ScopedAssertNoThreadSuspension needs to support std::move
  // ScopedAssertNoThreadSuspension sants_;
};

}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_H_
