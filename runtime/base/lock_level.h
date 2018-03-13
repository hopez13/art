/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_LOCK_LEVEL_H_
#define ART_RUNTIME_BASE_LOCK_LEVEL_H_

namespace art {

// LockLevel is used to impose a lock hierarchy [1] where acquisition of a Mutex at a higher or
// equal level to a lock a thread holds is invalid. The lock hierarchy achieves a cycle free
// partial ordering and thereby cause deadlock situations to fail checks.
//
// [1] http://www.drdobbs.com/parallel/use-lock-hierarchies-to-avoid-deadlock/204801163
enum LockLevel {
  kLoggingLock = 0,
  kSwapMutexesLock,
  kUnexpectedSignalLock,
  kThreadSuspendCountLock,
  kAbortLock,
  kNativeDebugInterfaceLock,
  kSignalHandlingLock,
  kJdwpAdbStateLock,
  kJdwpSocketLock,
  kRegionSpaceRegionLock,
  kMarkSweepMarkStackLock,
  kRosAllocGlobalLock,
  kRosAllocBracketLock,
  kRosAllocBulkFreeLock,
  kTaggingLockLevel,
  kTransactionLogLock,
  kJniFunctionTableLock,
  kJniWeakGlobalsLock,
  kJniGlobalsLock,
  kReferenceQueueSoftReferencesLock,
  kReferenceQueuePhantomReferencesLock,
  kReferenceQueueFinalizerReferencesLock,
  kReferenceQueueWeakReferencesLock,
  kReferenceQueueClearedReferencesLock,
  kReferenceProcessorLock,
  kJitDebugInterfaceLock,
  kAllocSpaceLock,
  kBumpPointerSpaceBlockLock,
  kArenaPoolLock,
  kInternTableLock,
  kOatFileSecondaryLookupLock,
  kHostDlOpenHandlesLock,
  kVerifierDepsLock,
  kOatFileManagerLock,
  kTracingUniqueMethodsLock,
  kTracingStreamingLock,
  kDeoptimizedMethodsLock,
  kClassLoaderClassesLock,
  kDefaultMutexLevel,
  kDexLock,
  kMarkSweepLargeObjectLock,
  kJdwpObjectRegistryLock,
  kModifyLdtLock,
  kAllocatedThreadIdsLock,
  kMonitorPoolLock,
  kClassLinkerClassesLock,  // TODO rename.
  kDexToDexCompilerLock,
  kJitCodeCacheLock,
  kCHALock,
  kSubtypeCheckLock,
  kBreakpointLock,
  kMonitorLock,
  kMonitorListLock,
  kJniLoadLibraryLock,
  kThreadListLock,
  kAllocTrackerLock,
  kDeoptimizationLock,
  kProfilerLock,
  kJdwpShutdownLock,
  kJdwpEventListLock,
  kJdwpAttachLock,
  kJdwpStartLock,
  kRuntimeShutdownLock,
  kTraceLock,
  kHeapBitmapLock,
  kMutatorLock,
  kUserCodeSuspensionLock,
  kInstrumentEntrypointsLock,
  kZygoteCreationLock,

  // The highest valid lock level. Use this if there is code that should only be called with no
  // other locks held. Since this is the highest lock level we also allow it to be held even if the
  // runtime or current thread is not fully set-up yet (for example during thread attach). Note that
  // this lock also has special behavior around the mutator_lock_. Since the mutator_lock_ is not
  // really a 'real' lock we allow this to be locked when the mutator_lock_ is held exclusive.
  // Furthermore, the mutator_lock_ may not be acquired in any form when a lock of this level is
  // held. Since the mutator_lock_ being held strong means that all other threads are suspended this
  // will prevent deadlocks while still allowing this lock level to function as a "highest" level.
  kTopLockLevel,

  kLockLevelCount  // Must come last.
};
std::ostream& operator<<(std::ostream& os, const LockLevel& rhs);

}  // namespace art

#endif  // ART_RUNTIME_BASE_LOCK_LEVEL_H_
