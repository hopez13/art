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
 * limitations under the License.
 */

#define LOG_TAG "perfetto_hprof"

#include "perfetto_hprof.h"

#include <fcntl.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <sched.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>

#include <limits>
#include <optional>
#include <thread>
#include <type_traits>

#include "android-base/file.h"
#include "android-base/logging.h"
#include "android-base/properties.h"
#include "base/fast_exit.h"
#include "base/systrace.h"
#include "dex/descriptors_names.h"
#include "gc/heap-visit-objects-inl.h"
#include "gc/heap.h"
#include "gc/scoped_gc_critical_section.h"
#include "mirror/object-refvisitor-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "parse_smaps.h"
#include "perfetto/public/data_source.h"
#include "perfetto/public/pb_decoder.h"
#include "perfetto/public/producer.h"
#include "perfetto/public/protos/config/data_source_config.pzc.h"
#include "perfetto/public/protos/config/profiling/java_hprof_config.pzc.h"
#include "perfetto/public/protos/trace/interned_data/interned_data.pzc.h"
#include "perfetto/public/protos/trace/profiling/heap_graph.pzc.h"
#include "perfetto/public/protos/trace/profiling/profile_common.pzc.h"
#include "perfetto/public/protos/trace/profiling/smaps.pzc.h"
#include "runtime-inl.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread_list.h"
#include "well_known_classes.h"

// There are three threads involved in this:
// * listener thread: this is idle in the background when this plugin gets loaded, and waits
//   for data on on g_signal_pipe_fds.
// * signal thread: an arbitrary thread that handles the signal and writes data to
//   g_signal_pipe_fds.
// * perfetto producer thread: once the signal is received, the app forks. In the newly forked
//   child, the Perfetto Client API spawns a thread to communicate with traced.

namespace perfetto_hprof {

constexpr int kJavaHeapprofdSignal = __SIGRTMIN + 6;
constexpr time_t kWatchdogTimeoutSec = 120;
// This needs to be lower than the maximum acceptable chunk size, because this
// is checked *before* writing another submessage. We conservatively assume
// submessages can be up to 100k here for a 500k chunk size.
// DropBox has a 500k chunk limit, and each chunk needs to parse as a proto.
constexpr uint32_t kPacketSizeThreshold = 400000;
constexpr char kByte[1] = {'x'};
static art::Mutex& GetStateMutex() {
  static art::Mutex state_mutex("perfetto_hprof_state_mutex", art::LockLevel::kGenericBottomLock);
  return state_mutex;
}

static art::ConditionVariable& GetStateCV() {
  static art::ConditionVariable state_cv("perfetto_hprof_state_cv", GetStateMutex());
  return state_cv;
}

static int requested_tracing_session_id = 0;
static State g_state = State::kUninitialized;
static bool g_oome_triggered = false;
static uint32_t g_oome_sessions_pending = 0;

// Pipe to signal from the signal handler into a worker thread that handles the
// dump requests.
int g_signal_pipe_fds[2];
static struct sigaction g_orig_act = {};

template <typename T>
uint64_t FindOrAppend(std::map<T, uint64_t>* m, const T& s) {
  auto it = m->find(s);
  if (it == m->end()) {
    std::tie(it, std::ignore) = m->emplace(s, m->size());
  }
  return it->second;
}

void ArmWatchdogOrDie() {
  timer_t timerid{};
  struct sigevent sev {};
  sev.sigev_notify = SIGEV_SIGNAL;
  sev.sigev_signo = SIGKILL;

  if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
    // This only gets called in the child, so we can fatal without impacting
    // the app.
    PLOG(FATAL) << "failed to create watchdog timer";
  }

  struct itimerspec its {};
  its.it_value.tv_sec = kWatchdogTimeoutSec;

  if (timer_settime(timerid, 0, &its, nullptr) == -1) {
    // This only gets called in the child, so we can fatal without impacting
    // the app.
    PLOG(FATAL) << "failed to arm watchdog timer";
  }
}

bool StartsWith(const std::string& str, const std::string& prefix) {
  return str.compare(0, prefix.length(), prefix) == 0;
}

// Sample entries that match one of the following
// start with /system/
// start with /vendor/
// start with /data/app/
// contains "extracted in memory from Y", where Y matches any of the above
bool ShouldSampleSmapsEntry(const perfetto_hprof::SmapsEntry& e) {
  if (StartsWith(e.pathname, "/system/") || StartsWith(e.pathname, "/vendor/") ||
      StartsWith(e.pathname, "/data/app/")) {
    return true;
  }
  if (StartsWith(e.pathname, "[anon:")) {
    if (e.pathname.find("extracted in memory from /system/") != std::string::npos) {
      return true;
    }
    if (e.pathname.find("extracted in memory from /vendor/") != std::string::npos) {
      return true;
    }
    if (e.pathname.find("extracted in memory from /data/app/") != std::string::npos) {
      return true;
    }
  }
  return false;
}

uint64_t GetCurrentBootClockNs() {
  struct timespec ts = {};
  if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0) {
    LOG(FATAL) << "Failed to get boottime.";
  }
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

bool IsDebugBuild() {
  std::string build_type = android::base::GetProperty("ro.build.type", "");
  return !build_type.empty() && build_type != "user";
}

// Verifies the manifest restrictions are respected.
// For regular heap dumps this is already handled by heapprofd.
bool IsOomeHeapDumpAllowed(perfetto_protos_DataSourceConfig_SessionInitiator session_initiator) {
  if (art::Runtime::Current()->IsJavaDebuggable() || IsDebugBuild()) {
    return true;
  }

  if (session_initiator == perfetto_protos_DataSourceConfig_SESSION_INITIATOR_TRUSTED_SYSTEM) {
    return art::Runtime::Current()->IsProfileable() || art::Runtime::Current()->IsSystemServer();
  } else {
    return art::Runtime::Current()->IsProfileableFromShell();
  }
}

// Round up |size| to a multiple of |alignment| (must be a power of two).
template <size_t alignment>
constexpr size_t AlignUp(size_t size) {
  static_assert((alignment & (alignment - 1)) == 0, "alignment must be a pow2");
  return (size + alignment - 1) & ~(alignment - 1);
}

class VarIntBuffer {
 public:
  VarIntBuffer() { Reset(); }

  // Copy and move are disabled due to pointers to inline buf.
  VarIntBuffer(const VarIntBuffer&) = delete;
  VarIntBuffer(VarIntBuffer&&) = delete;
  VarIntBuffer& operator=(const VarIntBuffer&) = delete;
  VarIntBuffer& operator=(VarIntBuffer&&) = delete;

  void Reset() {
    heap_buf_.reset();
    storage_begin_ = reinterpret_cast<uint8_t*>(&inline_buf_[0]);
    storage_end_ = reinterpret_cast<uint8_t*>(&inline_buf_[0]) + sizeof(inline_buf_);
    write_ptr_ = storage_begin_;
  }

  const void* data() const { return storage_begin_; }
  size_t size() const {
    return static_cast<size_t>(write_ptr_ - storage_begin_);
  }

  void Append(uint64_t val) {
    GrowIfNeeded();
    write_ptr_ = PerfettoPbWriteVarInt(val, write_ptr_);
  }

 private:
  void GrowIfNeeded() {
    if (UNLIKELY(write_ptr_ + PERFETTO_PB_VARINT_MAX_SIZE_64 > storage_end_)) {
      GrowSlowpath();
    }
  }

  void GrowSlowpath() {
    size_t write_off = static_cast<size_t>(write_ptr_ - storage_begin_);
    size_t old_size = static_cast<size_t>(storage_end_ - storage_begin_);
    size_t new_size = old_size * 2;
    new_size = AlignUp<4096>(new_size);
    std::unique_ptr<uint8_t[]> new_buf(new uint8_t[new_size]);
    memcpy(new_buf.get(), storage_begin_, old_size);
    heap_buf_ = std::move(new_buf);
    storage_begin_ = heap_buf_.get();
    storage_end_ = storage_begin_ + new_size;
    write_ptr_ = storage_begin_ + write_off;
  }

  uint8_t* storage_begin_;
  uint8_t* storage_end_;
  uint8_t* write_ptr_;
  std::unique_ptr<uint8_t[]> heap_buf_;
  uint64_t inline_buf_[(8192 - (sizeof(uint8_t*) * 4))/sizeof(uint64_t)];
};
static_assert(sizeof(VarIntBuffer) == 8192);

struct PerfettoDs java_hprof_data_source = PERFETTO_DS_INIT();
bool g_is_oome_heap;

class JavaHprofDataSource {
 public:
  static void* StaticOnSetup(struct PerfettoDsImpl*,
                             PerfettoDsInstanceIndex,
                             void* ds_config,
                             size_t ds_config_size,
                             void* user_arg,
                             struct PerfettoDsOnSetupArgs*) {
    auto* inst = new JavaHprofDataSource(*static_cast<bool*>(user_arg));
    inst->OnSetup(ds_config, ds_config_size);
    return inst;
  }

  static void StaticOnStart(struct PerfettoDsImpl* ds_impl,
                            PerfettoDsInstanceIndex inst_idx,
                            void*,
                            void* inst_ctx,
                            struct PerfettoDsOnStartArgs*) {
    PerfettoDsImplGetInstanceLocked(ds_impl, inst_idx);
    auto* inst = reinterpret_cast<JavaHprofDataSource*>(inst_ctx);
    inst->OnStart();
    PerfettoDsImplReleaseInstanceLocked(ds_impl, inst_idx);
  }

  static void StaticOnStop(struct PerfettoDsImpl* ds_impl,
                           PerfettoDsInstanceIndex inst_idx,
                           void*,
                           void* inst_ctx,
                           struct PerfettoDsOnStopArgs* args) {
    PerfettoDsImplGetInstanceLocked(ds_impl, inst_idx);
    auto* inst = reinterpret_cast<JavaHprofDataSource*>(inst_ctx);
    inst->OnStop(args);
    PerfettoDsImplReleaseInstanceLocked(ds_impl, inst_idx);
  }

  static void StaticOnDestroy(struct PerfettoDsImpl*, void*, void* inst_ctx) {
    auto* inst = reinterpret_cast<JavaHprofDataSource*>(inst_ctx);
    delete inst;
  }

  explicit JavaHprofDataSource(bool is_oome_heap) : is_oome_heap_(is_oome_heap) {}

  struct Config {
    uint64_t tracing_session_id = 0;
    uint64_t session_initiator = 0;

    struct JavaHprof {
      std::vector<std::string> ignored_types;
      bool dump_smaps = false;
      std::vector<std::string> process_cmdlines;

      void Parse(const PerfettoPbDecoderField& field) {
        *this = {};
        if (field.wire_type != PERFETTO_PB_WIRE_TYPE_DELIMITED) {
          return;
        }
        for (auto it = PerfettoPbDecoderIterateNestedBegin(field.value.delimited);
             it.field.status == PERFETTO_PB_DECODER_OK;
             PerfettoPbDecoderIterateNext(&it)) {
          switch (it.field.id) {
            case perfetto_protos_JavaHprofConfig_dump_smaps_field_number:
              PerfettoPbDecoderFieldGetBool(&it.field, &dump_smaps);
              break;
            case perfetto_protos_JavaHprofConfig_ignored_types_field_number:
              if (it.field.wire_type == PERFETTO_PB_WIRE_TYPE_DELIMITED) {
                ignored_types.emplace_back(
                    reinterpret_cast<const char*>(it.field.value.delimited.start),
                    it.field.value.delimited.len);
              }
              break;
            case perfetto_protos_JavaHprofConfig_process_cmdline_field_number:
              if (it.field.wire_type == PERFETTO_PB_WIRE_TYPE_DELIMITED) {
                process_cmdlines.emplace_back(
                    reinterpret_cast<const char*>(it.field.value.delimited.start),
                    it.field.value.delimited.len);
              }
              break;
          }
        }
      }
    };
    JavaHprof java_hprof;

    void Parse(void* ds_config, size_t ds_config_size) {
      *this = {};
      for (auto it = PerfettoPbDecoderIterateBegin(ds_config, ds_config_size);
           it.field.status == PERFETTO_PB_DECODER_OK;
           PerfettoPbDecoderIterateNext(&it)) {
        switch (it.field.id) {
          case perfetto_protos_DataSourceConfig_tracing_session_id_field_number:
            PerfettoPbDecoderFieldGetUint64(&it.field, &tracing_session_id);
            break;
          case perfetto_protos_DataSourceConfig_java_hprof_config_field_number:
            java_hprof.Parse(it.field);
            break;
          case perfetto_protos_DataSourceConfig_session_initiator_field_number:
            PerfettoPbDecoderFieldGetUint64(&it.field, &session_initiator);
            break;
        }
      }
    }
  };

  void OnSetup(void* ds_config, size_t ds_config_size) {
    Config config;
    config.Parse(ds_config, ds_config_size);

    if (!is_oome_heap_) {
      uint64_t normalized_tracing_session_id =
          config.tracing_session_id % std::numeric_limits<int32_t>::max();
      if (requested_tracing_session_id < 0) {
        LOG(ERROR) << "invalid requested tracing session id " << requested_tracing_session_id;
        return;
      }
      if (static_cast<uint64_t>(requested_tracing_session_id) != normalized_tracing_session_id) {
        return;
      }
    }

    dump_smaps_ = config.java_hprof.dump_smaps;
    for (const std::string& t : config.java_hprof.ignored_types) {
      ignored_types_.emplace_back(art::InversePrettyDescriptor(t));
    }
    // This tracing session ID matches the requesting tracing session ID, so we know heapprofd
    // has verified it targets this process.
    enabled_ =
        !is_oome_heap_ ||
        (IsOomeHeapDumpAllowed(static_cast<perfetto_protos_DataSourceConfig_SessionInitiator>(
             config.session_initiator)) &&
         IsOomeDumpEnabled(config.java_hprof.process_cmdlines));
  }

  bool dump_smaps() { return dump_smaps_; }

  // Per-DataSource enable bit. Invoked by the PERFETTO_DS_TRACE body.
  bool enabled() { return enabled_; }

  void OnStart() {
    art::MutexLock lk(art_thread(), GetStateMutex());
    // In case there are multiple tracing sessions waiting for an OOME error,
    // there will be a data source instance for each of them. Before the
    // transition to kStart and signaling the dumping thread, we need to make
    // sure all the data sources are ready.
    if (is_oome_heap_ && g_oome_sessions_pending > 0) {
      --g_oome_sessions_pending;
    }
    if (g_state == State::kWaitForStart) {
      // WriteHeapPackets is responsible for checking whether the DataSource is
      // actually enabled.
      if (!is_oome_heap_ || g_oome_sessions_pending == 0) {
        g_state = State::kStart;
        GetStateCV().Broadcast(art_thread());
      }
    }
  }

  // This datasource can be used with a trace config with a short duration_ms
  // but a long datasource_stop_timeout_ms. In that case, OnStop is called (in
  // general) before the dump is done. In that case, we handle the stop
  // asynchronously, and notify the tracing service once we are done.
  // In case OnStop is called after the dump is done (but before the process)
  // has exited, we just acknowledge the request.
  void OnStop(struct PerfettoDsOnStopArgs* args) {
    art::MutexLock lk(art_thread(), finish_mutex_);
    if (is_finished_) {
      return;
    }
    is_stopped_ = true;
    async_stop_ = PerfettoDsOnStopArgsPostpone(args);
  }

  static art::Thread* art_thread() {
    // TODO(fmayer): Attach the Perfetto producer thread to ART and give it a name. This is
    // not trivial, we cannot just attach the first time this method is called, because
    // AttachCurrentThread deadlocks with the ConditionVariable::Wait in WaitForDataSource.
    //
    // We should attach the thread as soon as the Client API spawns it, but that needs more
    // complicated plumbing.
    return nullptr;
  }

  std::vector<std::string> ignored_types() { return ignored_types_; }

  void Finish() {
    art::MutexLock lk(art_thread(), finish_mutex_);
    if (is_stopped_) {
      PerfettoDsStopDone(async_stop_);
    } else {
      is_finished_ = true;
    }
  }

 private:
  static bool IsOomeDumpEnabled(const std::vector<std::string>& process_cmdlines) {
    std::string cmdline;
    if (!android::base::ReadFileToString("/proc/self/cmdline", &cmdline)) {
      return false;
    }
    const char* argv0 = cmdline.c_str();

    for (const std::string& pattern : process_cmdlines) {
      if (fnmatch(pattern.c_str(), argv0, FNM_NOESCAPE) == 0) {
        return true;
      }
    }
    return false;
  }

  bool is_oome_heap_ = false;
  bool enabled_ = false;
  bool dump_smaps_ = false;
  std::vector<std::string> ignored_types_;

  art::Mutex finish_mutex_{"perfetto_hprof_ds_mutex", art::LockLevel::kGenericBottomLock};
  bool is_finished_ = false;
  bool is_stopped_ = false;
  PerfettoDsAsyncStopper* async_stop_ = nullptr;
};

void SetupDataSource(const char* ds_name, bool is_oome_heap) {
  // XXX We're in the forked process. In case the perfetto library was already
  // initialized in the parent (this is not possible now, because nobody else
  // uses the shared library), we should destroy it somehow, with something like
  // perfetto::Tracing::Shutdown().
  struct PerfettoProducerInitArgs args = {0};
  args.backends = PERFETTO_BACKEND_SYSTEM;
  PerfettoProducerInit(args);

  PerfettoDsParams params = PerfettoDsParamsDefault();
  params.on_setup_cb = JavaHprofDataSource::StaticOnSetup;
  params.on_start_cb = JavaHprofDataSource::StaticOnStart;
  params.on_stop_cb = JavaHprofDataSource::StaticOnStop;
  params.on_destroy_cb = JavaHprofDataSource::StaticOnDestroy;
  g_is_oome_heap = is_oome_heap;
  params.user_arg = &g_is_oome_heap;
  params.buffer_exhausted_policy = PERFETTO_DS_BUFFER_EXHAUSTED_POLICY_STALL_AND_ABORT;

  PerfettoDsRegister(&java_hprof_data_source, ds_name, params);

  LOG(INFO) << "registered data source " << ds_name;
}

// Waits for the data source OnStart
void WaitForDataSource(art::Thread* self) {
  art::MutexLock lk(self, GetStateMutex());
  while (g_state != State::kStart) {
    GetStateCV().Wait(self);
  }
}

// Waits for the data source OnStart with a timeout. Returns false on timeout.
bool TimedWaitForDataSource(art::Thread* self, int64_t timeout_ms) {
  const uint64_t cutoff_ns = GetCurrentBootClockNs() + timeout_ms * 1000000;
  art::MutexLock lk(self, GetStateMutex());
  while (g_state != State::kStart) {
    const uint64_t current_ns = GetCurrentBootClockNs();
    if (current_ns >= cutoff_ns) {
      return false;
    }
    GetStateCV().TimedWait(self, (cutoff_ns - current_ns) / 1000000, 0);
  }
  return true;
}

// Helper class to write Java heap dumps to `ctx`. The whole heap dump can be
// split into more perfetto.protos.HeapGraph messages, to avoid making each
// message too big.
class Writer {
 public:
  Writer(pid_t pid, PerfettoDsTracerIterator* ctx, uint64_t timestamp)
      : pid_(pid), ctx_(ctx), timestamp_(timestamp) {}

  // Return whether the next call to GetHeapGraph will create a new TracePacket.
  bool will_create_new_packet() const {
    return !packets_ || PerfettoStreamWriterGetWrittenSize(&packets_->trace_packet.writer.writer) -
                                packets_->last_written >
                            kPacketSizeThreshold;
  }

  perfetto_protos_HeapGraph* GetHeapGraph() {
    if (will_create_new_packet()) {
      CreateNewHeapGraph();
    }
    return &packets_->heap_graph;
  }

  void Finalize() {
    if (packets_) {
      perfetto_protos_TracePacket_end_heap_graph(&packets_->trace_packet.msg,
                                                 &packets_->heap_graph);
      PerfettoDsTracerPacketEnd(ctx_, &packets_->trace_packet);
    }
    packets_.reset();
  }

  ~Writer() { Finalize(); }

 private:
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;
  Writer(Writer&&) = delete;
  Writer& operator=(Writer&&) = delete;

  void CreateNewHeapGraph() {
    if (packets_) {
      perfetto_protos_HeapGraph_set_continued(&packets_->heap_graph, true);
    }
    Finalize();

    packets_.emplace();

    PerfettoDsTracerPacketBegin(ctx_, &packets_->trace_packet);
    size_t written = PerfettoStreamWriterGetWrittenSize(&packets_->trace_packet.writer.writer);
    perfetto_protos_TracePacket_set_timestamp(&packets_->trace_packet.msg, timestamp_);
    perfetto_protos_TracePacket_begin_heap_graph(&packets_->trace_packet.msg,
                                                 &packets_->heap_graph);
    perfetto_protos_HeapGraph_set_pid(&packets_->heap_graph, pid_);
    perfetto_protos_HeapGraph_set_index(&packets_->heap_graph, index_++);

    packets_->last_written = written;
  }

  const pid_t pid_;
  PerfettoDsTracerIterator* const ctx_;
  const uint64_t timestamp_;

  struct Packets {
    PerfettoDsRootTracePacket trace_packet;
    perfetto_protos_HeapGraph heap_graph;
    size_t last_written;
  };
  std::optional<Packets> packets_;

  uint64_t index_ = 0;
};

class ReferredObjectsFinder {
 public:
  explicit ReferredObjectsFinder(
      std::vector<std::pair<std::string, art::mirror::Object*>>* referred_objects,
      bool emit_field_ids)
      : referred_objects_(referred_objects), emit_field_ids_(emit_field_ids) {}

  // For art::mirror::Object::VisitReferences.
  void operator()(art::ObjPtr<art::mirror::Object> obj, art::MemberOffset offset,
                  bool is_static) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (offset.Uint32Value() == art::mirror::Object::ClassOffset().Uint32Value()) {
      // Skip shadow$klass pointer.
      return;
    }
    art::mirror::Object* ref = obj->GetFieldObject<art::mirror::Object>(offset);
    art::ArtField* field;
    if (is_static) {
      field = art::ArtField::FindStaticFieldWithOffset(obj->AsClass(), offset.Uint32Value());
    } else {
      field = art::ArtField::FindInstanceFieldWithOffset(obj->GetClass(), offset.Uint32Value());
    }
    std::string field_name = "";
    if (field != nullptr && emit_field_ids_) {
      field_name = field->PrettyField(/*with_type=*/true);
    }
    referred_objects_->emplace_back(std::move(field_name), ref);
  }

  void VisitRootIfNonNull(
      [[maybe_unused]] art::mirror::CompressedReference<art::mirror::Object>* root) const {}
  void VisitRoot(
      [[maybe_unused]] art::mirror::CompressedReference<art::mirror::Object>* root) const {}

 private:
  // We can use a raw Object* pointer here, because there are no concurrent GC threads after the
  // fork.
  std::vector<std::pair<std::string, art::mirror::Object*>>* referred_objects_;
  // Prettifying field names is expensive; avoid if field name will not be used.
  bool emit_field_ids_;
};

class RootFinder : public art::SingleRootVisitor {
 public:
  explicit RootFinder(
    std::map<art::RootType, std::vector<art::mirror::Object*>>* root_objects)
      : root_objects_(root_objects) {}

  void VisitRoot(art::mirror::Object* root, const art::RootInfo& info) override {
    (*root_objects_)[info.GetType()].emplace_back(root);
  }

 private:
  // We can use a raw Object* pointer here, because there are no concurrent GC threads after the
  // fork.
  std::map<art::RootType, std::vector<art::mirror::Object*>>* root_objects_;
};

perfetto_protos_HeapGraphRoot_Type ToProtoType(art::RootType art_type) {
  switch (art_type) {
    case art::kRootUnknown:
      return perfetto_protos_HeapGraphRoot_ROOT_UNKNOWN;
    case art::kRootJNIGlobal:
      return perfetto_protos_HeapGraphRoot_ROOT_JNI_GLOBAL;
    case art::kRootJNILocal:
      return perfetto_protos_HeapGraphRoot_ROOT_JNI_LOCAL;
    case art::kRootJavaFrame:
      return perfetto_protos_HeapGraphRoot_ROOT_JAVA_FRAME;
    case art::kRootNativeStack:
      return perfetto_protos_HeapGraphRoot_ROOT_NATIVE_STACK;
    case art::kRootStickyClass:
      return perfetto_protos_HeapGraphRoot_ROOT_STICKY_CLASS;
    case art::kRootThreadBlock:
      return perfetto_protos_HeapGraphRoot_ROOT_THREAD_BLOCK;
    case art::kRootMonitorUsed:
      return perfetto_protos_HeapGraphRoot_ROOT_MONITOR_USED;
    case art::kRootThreadObject:
      return perfetto_protos_HeapGraphRoot_ROOT_THREAD_OBJECT;
    case art::kRootInternedString:
      return perfetto_protos_HeapGraphRoot_ROOT_INTERNED_STRING;
    case art::kRootFinalizing:
      return perfetto_protos_HeapGraphRoot_ROOT_FINALIZING;
    case art::kRootDebugger:
      return perfetto_protos_HeapGraphRoot_ROOT_DEBUGGER;
    case art::kRootReferenceCleanup:
      return perfetto_protos_HeapGraphRoot_ROOT_REFERENCE_CLEANUP;
    case art::kRootVMInternal:
      return perfetto_protos_HeapGraphRoot_ROOT_VM_INTERNAL;
    case art::kRootJNIMonitor:
      return perfetto_protos_HeapGraphRoot_ROOT_JNI_MONITOR;
  }
}

perfetto_protos_HeapGraphType_Kind ProtoClassKind(uint32_t class_flags) {
  switch (class_flags) {
    case art::mirror::kClassFlagNormal:
    case art::mirror::kClassFlagRecord:
      return perfetto_protos_HeapGraphType_KIND_NORMAL;
    case art::mirror::kClassFlagNoReferenceFields:
    case art::mirror::kClassFlagNoReferenceFields | art::mirror::kClassFlagRecord:
      return perfetto_protos_HeapGraphType_KIND_NOREFERENCES;
    case art::mirror::kClassFlagString | art::mirror::kClassFlagNoReferenceFields:
      return perfetto_protos_HeapGraphType_KIND_STRING;
    case art::mirror::kClassFlagObjectArray:
      return perfetto_protos_HeapGraphType_KIND_ARRAY;
    case art::mirror::kClassFlagClass:
      return perfetto_protos_HeapGraphType_KIND_CLASS;
    case art::mirror::kClassFlagClassLoader:
      return perfetto_protos_HeapGraphType_KIND_CLASSLOADER;
    case art::mirror::kClassFlagDexCache:
      return perfetto_protos_HeapGraphType_KIND_DEXCACHE;
    case art::mirror::kClassFlagSoftReference:
      return perfetto_protos_HeapGraphType_KIND_SOFT_REFERENCE;
    case art::mirror::kClassFlagWeakReference:
      return perfetto_protos_HeapGraphType_KIND_WEAK_REFERENCE;
    case art::mirror::kClassFlagFinalizerReference:
      return perfetto_protos_HeapGraphType_KIND_FINALIZER_REFERENCE;
    case art::mirror::kClassFlagPhantomReference:
      return perfetto_protos_HeapGraphType_KIND_PHANTOM_REFERENCE;
    default:
      return perfetto_protos_HeapGraphType_KIND_UNKNOWN;
  }
}

std::string PrettyType(art::mirror::Class* klass) NO_THREAD_SAFETY_ANALYSIS {
  if (klass == nullptr) {
    return "(raw)";
  }
  std::string temp;
  std::string result(art::PrettyDescriptor(klass->GetDescriptor(&temp)));
  return result;
}

void DumpSmaps(PerfettoDsTracerIterator* ctx) {
  FILE* smaps = fopen("/proc/self/smaps", "re");
  if (smaps != nullptr) {
    struct PerfettoDsRootTracePacket root;
    PerfettoDsTracerPacketBegin(ctx, &root);
    {
      struct perfetto_protos_SmapsPacket smaps_packet;
      perfetto_protos_TracePacket_begin_smaps_packet(&root.msg, &smaps_packet);
      perfetto_protos_SmapsPacket_set_pid(&smaps_packet, getpid());

      perfetto_hprof::ParseSmaps(smaps, [&smaps_packet](const perfetto_hprof::SmapsEntry& e) {
        if (ShouldSampleSmapsEntry(e)) {
          struct perfetto_protos_SmapsEntry smaps_entry;
          perfetto_protos_SmapsPacket_begin_entries(&smaps_packet, &smaps_entry);
          perfetto_protos_SmapsEntry_set_path(&smaps_entry, e.pathname.data(), e.pathname.size());
          perfetto_protos_SmapsEntry_set_size_kb(&smaps_entry, e.size_kb);
          perfetto_protos_SmapsEntry_set_private_dirty_kb(&smaps_entry, e.private_dirty_kb);
          perfetto_protos_SmapsEntry_set_swap_kb(&smaps_entry, e.swap_kb);
          perfetto_protos_SmapsPacket_end_entries(&smaps_packet, &smaps_entry);
        }
      });
      perfetto_protos_TracePacket_end_smaps_packet(&root.msg, &smaps_packet);
    }
    PerfettoDsTracerPacketEnd(ctx, &root);
    fclose(smaps);
  } else {
    PLOG(ERROR) << "failed to open smaps";
  }
}

uint64_t GetObjectId(const art::mirror::Object* obj) {
  return reinterpret_cast<uint64_t>(obj) / std::alignment_of<art::mirror::Object>::value;
}

template <typename F>
void ForInstanceReferenceField(art::mirror::Class* klass, F fn) NO_THREAD_SAFETY_ANALYSIS {
  for (art::ArtField& af : klass->GetIFields()) {
    if (af.IsPrimitiveType() ||
        af.GetOffset().Uint32Value() == art::mirror::Object::ClassOffset().Uint32Value()) {
      continue;
    }
    fn(af.GetOffset());
  }
}

size_t EncodedSize(uint64_t n) {
  if (n == 0) return 1;
  return 1 + static_cast<size_t>(art::MostSignificantBit(n)) / 7;
}

// Returns all the references that `*obj` (an object of type `*klass`) is holding.
std::vector<std::pair<std::string, art::mirror::Object*>> GetReferences(art::mirror::Object* obj,
                                                                        art::mirror::Class* klass,
                                                                        bool emit_field_ids)
    REQUIRES_SHARED(art::Locks::mutator_lock_) {
  std::vector<std::pair<std::string, art::mirror::Object*>> referred_objects;
  ReferredObjectsFinder objf(&referred_objects, emit_field_ids);

  uint32_t klass_flags = klass->GetClassFlags();
  if (klass_flags != art::mirror::kClassFlagNormal &&
      klass_flags != art::mirror::kClassFlagSoftReference &&
      klass_flags != art::mirror::kClassFlagWeakReference &&
      klass_flags != art::mirror::kClassFlagFinalizerReference &&
      klass_flags != art::mirror::kClassFlagPhantomReference) {
    obj->VisitReferences(objf, art::VoidFunctor());
  } else {
    for (art::mirror::Class* cls = klass; cls != nullptr; cls = cls->GetSuperClass().Ptr()) {
      ForInstanceReferenceField(cls,
                                [obj, objf](art::MemberOffset offset) NO_THREAD_SAFETY_ANALYSIS {
                                  objf(art::ObjPtr<art::mirror::Object>(obj),
                                       offset,
                                       /*is_static=*/false);
                                });
    }
  }
  return referred_objects;
}

// Returns the base for delta encoding all the `referred_objects`. If delta
// encoding would waste space, returns 0.
uint64_t EncodeBaseObjId(
    const std::vector<std::pair<std::string, art::mirror::Object*>>& referred_objects,
    const art::mirror::Object* min_nonnull_ptr) REQUIRES_SHARED(art::Locks::mutator_lock_) {
  uint64_t base_obj_id = GetObjectId(min_nonnull_ptr);
  if (base_obj_id <= 1) {
    return 0;
  }

  // We need to decrement the base for object ids so that we can tell apart
  // null references.
  base_obj_id--;
  uint64_t bytes_saved = 0;
  for (const auto& p : referred_objects) {
    art::mirror::Object* referred_obj = p.second;
    if (!referred_obj) {
      continue;
    }
    uint64_t referred_obj_id = GetObjectId(referred_obj);
    bytes_saved += EncodedSize(referred_obj_id) - EncodedSize(referred_obj_id - base_obj_id);
  }

  // +1 for storing the field id.
  if (bytes_saved <= EncodedSize(base_obj_id) + 1) {
    // Subtracting the base ptr gains fewer bytes than it takes to store it.
    return 0;
  }
  return base_obj_id;
}

// Helper to keep intermediate state while dumping objects and classes from ART into
// perfetto.protos.HeapGraph.
class HeapGraphDumper {
 public:
  // Instances of classes whose name is in `ignored_types` will be ignored.
  explicit HeapGraphDumper(const std::vector<std::string>& ignored_types)
      : ignored_types_(ignored_types) {}

  // Dumps a heap graph from `*runtime` and writes it to `writer`.
  void Dump(art::Runtime* runtime, Writer& writer) REQUIRES(art::Locks::mutator_lock_) {
    DumpRootObjects(runtime, writer);

    DumpObjects(runtime, writer);

    WriteInternedData(writer);
  }

 private:
  // Dumps the root objects from `*runtime` to `writer`.
  void DumpRootObjects(art::Runtime* runtime, Writer& writer)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    std::map<art::RootType, std::vector<art::mirror::Object*>> root_objects;
    RootFinder rcf(&root_objects);
    runtime->VisitRoots(&rcf);
    for (const auto& [root_type, children] : root_objects) {
      perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
      perfetto_protos_HeapGraphRoot root_proto;
      perfetto_protos_HeapGraph_begin_roots(hg, &root_proto);
      perfetto_protos_HeapGraphRoot_set_root_type(&root_proto, ToProtoType(root_type));
      PerfettoPbPackedMsgUint64 objects_proto;
      perfetto_protos_HeapGraphRoot_begin_object_ids(&root_proto, &objects_proto);
      for (art::mirror::Object* obj : children) {
        if (writer.will_create_new_packet()) {
          perfetto_protos_HeapGraphRoot_end_object_ids(&root_proto, &objects_proto);
          perfetto_protos_HeapGraph_end_roots(hg, &root_proto);
          hg = writer.GetHeapGraph();
          perfetto_protos_HeapGraph_begin_roots(hg, &root_proto);
          perfetto_protos_HeapGraphRoot_set_root_type(&root_proto, ToProtoType(root_type));
          perfetto_protos_HeapGraphRoot_begin_object_ids(&root_proto, &objects_proto);
        }
        PerfettoPbPackedMsgUint64Append(&objects_proto, GetObjectId(obj));
      }
      perfetto_protos_HeapGraphRoot_end_object_ids(&root_proto, &objects_proto);
      perfetto_protos_HeapGraph_end_roots(hg, &root_proto);
    }
  }

  // Dumps all the objects from `*runtime` to `writer`.
  void DumpObjects(art::Runtime* runtime, Writer& writer) REQUIRES(art::Locks::mutator_lock_) {
    runtime->GetHeap()->VisitObjectsPaused(
        [this, &writer](art::mirror::Object* obj)
            REQUIRES_SHARED(art::Locks::mutator_lock_) { WriteOneObject(obj, writer); });
  }

  // Writes all the previously accumulated (while dumping objects and roots) interned data to
  // `writer`.
  void WriteInternedData(Writer& writer) {
    for (const auto& [str, id] : interned_locations_) {
      perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
      perfetto_protos_InternedString location_proto;
      perfetto_protos_HeapGraph_begin_location_names(hg, &location_proto);
      perfetto_protos_InternedString_set_iid(&location_proto, id);
      perfetto_protos_InternedString_set_str(
          &location_proto, reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
      perfetto_protos_HeapGraph_end_location_names(hg, &location_proto);
    }
    for (const auto& [str, id] : interned_fields_) {
      perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
      perfetto_protos_InternedString field_proto;
      perfetto_protos_HeapGraph_begin_field_names(hg, &field_proto);
      perfetto_protos_InternedString_set_iid(&field_proto, id);
      perfetto_protos_InternedString_set_str(
          &field_proto, reinterpret_cast<const uint8_t*>(str.c_str()), str.size());
      perfetto_protos_HeapGraph_end_field_names(hg, &field_proto);
    }
  }

  // Writes `*obj` into `writer`.
  void WriteOneObject(art::mirror::Object* obj, Writer& writer)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (obj->IsClass()) {
      WriteClass(obj->AsClass().Ptr(), writer);
    }

    art::mirror::Class* klass = obj->GetClass();
    uintptr_t class_ptr = reinterpret_cast<uintptr_t>(klass);
    // We need to synethesize a new type for Class<Foo>, which does not exist
    // in the runtime. Otherwise, all the static members of all classes would be
    // attributed to java.lang.Class.
    if (klass->IsClassClass()) {
      class_ptr = WriteSyntheticClassFromObj(obj, writer);
    }

    if (IsIgnored(obj)) {
      return;
    }

    auto class_id = FindOrAppend(&interned_classes_, class_ptr);

    uint64_t object_id = GetObjectId(obj);
    perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
    perfetto_protos_HeapGraphObject object_proto;

    perfetto_protos_HeapGraph_begin_objects(hg, &object_proto);
    if (prev_object_id_ && prev_object_id_ < object_id) {
      perfetto_protos_HeapGraphObject_set_id_delta(&object_proto, object_id - prev_object_id_);
    } else {
      perfetto_protos_HeapGraphObject_set_id(&object_proto, object_id);
    }
    prev_object_id_ = object_id;
    perfetto_protos_HeapGraphObject_set_type_id(&object_proto, class_id);

    // Arrays / strings are magic and have an instance dependent size.
    if (obj->SizeOf() != klass->GetObjectSize()) {
      perfetto_protos_HeapGraphObject_set_self_size(&object_proto, obj->SizeOf());
    }

    FillReferences(obj, klass, &object_proto);

    FillFieldValues(obj, klass, &object_proto);
    perfetto_protos_HeapGraph_end_objects(hg, &object_proto);
  }

  // Writes `*klass` into `writer`.
  void WriteClass(art::mirror::Class* klass, Writer& writer)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
    perfetto_protos_HeapGraphType type_proto;
    perfetto_protos_HeapGraph_begin_types(hg, &type_proto);
    perfetto_protos_HeapGraphType_set_id(
        &type_proto, FindOrAppend(&interned_classes_, reinterpret_cast<uintptr_t>(klass)));
    std::string class_name = PrettyType(klass);
    perfetto_protos_HeapGraphType_set_class_name(&type_proto, class_name.data(), class_name.size());
    perfetto_protos_HeapGraphType_set_location_id(
        &type_proto, FindOrAppend(&interned_locations_, klass->GetLocation()));
    perfetto_protos_HeapGraphType_set_object_size(&type_proto, klass->GetObjectSize());
    perfetto_protos_HeapGraphType_set_kind(&type_proto, ProtoClassKind(klass->GetClassFlags()));
    perfetto_protos_HeapGraphType_set_classloader_id(&type_proto,
                                                     GetObjectId(klass->GetClassLoader().Ptr()));
    if (klass->GetSuperClass().Ptr()) {
      perfetto_protos_HeapGraphType_set_superclass_id(
          &type_proto,
          FindOrAppend(&interned_classes_,
                       reinterpret_cast<uintptr_t>(klass->GetSuperClass().Ptr())));
    }
    ForInstanceReferenceField(
        klass, [klass, this](art::MemberOffset offset) NO_THREAD_SAFETY_ANALYSIS {
          auto art_field = art::ArtField::FindInstanceFieldWithOffset(klass, offset.Uint32Value());
          reference_field_ids_.Append(
              FindOrAppend(&interned_fields_, art_field->PrettyField(true)));
        });
    if (reference_field_ids_.size() != 0) {
      perfetto_protos_HeapGraphType_set_reference_field_id(
          &type_proto, reference_field_ids_.data(), reference_field_ids_.size());
      reference_field_ids_.Reset();
    }
    perfetto_protos_HeapGraph_end_types(hg, &type_proto);
  }

  // Creates a fake class that represents a type only used by `*obj` into `writer`.
  uintptr_t WriteSyntheticClassFromObj(art::mirror::Object* obj, Writer& writer)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    CHECK(obj->IsClass());
    perfetto_protos_HeapGraph* hg = writer.GetHeapGraph();
    perfetto_protos_HeapGraphType type_proto;
    perfetto_protos_HeapGraph_begin_types(hg, &type_proto);
    // All pointers are at least multiples of two, so this way we can make sure
    // we are not colliding with a real class.
    uintptr_t class_ptr = reinterpret_cast<uintptr_t>(obj) | 1;
    auto class_id = FindOrAppend(&interned_classes_, class_ptr);
    perfetto_protos_HeapGraphType_set_id(&type_proto, class_id);
    std::string class_name = obj->PrettyTypeOf();
    perfetto_protos_HeapGraphType_set_class_name(&type_proto, class_name.data(), class_name.size());
    perfetto_protos_HeapGraphType_set_location_id(
        &type_proto, FindOrAppend(&interned_locations_, obj->AsClass()->GetLocation()));
    perfetto_protos_HeapGraph_end_types(hg, &type_proto);
    return class_ptr;
  }

  // Fills `*object_proto` with all the references held by `*obj` (an object of type `*klass`).
  void FillReferences(art::mirror::Object* obj,
                      art::mirror::Class* klass,
                      perfetto_protos_HeapGraphObject* object_proto)
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    const uint32_t klass_flags = klass->GetClassFlags();
    const bool emit_field_ids = klass_flags != art::mirror::kClassFlagObjectArray &&
                                klass_flags != art::mirror::kClassFlagNormal &&
                                klass_flags != art::mirror::kClassFlagSoftReference &&
                                klass_flags != art::mirror::kClassFlagWeakReference &&
                                klass_flags != art::mirror::kClassFlagFinalizerReference &&
                                klass_flags != art::mirror::kClassFlagPhantomReference;
    std::vector<std::pair<std::string, art::mirror::Object*>> referred_objects =
        GetReferences(obj, klass, emit_field_ids);

    art::mirror::Object* min_nonnull_ptr = FilterIgnoredReferencesAndFindMin(referred_objects);

    uint64_t base_obj_id = EncodeBaseObjId(referred_objects, min_nonnull_ptr);

    for (const auto& [field_name, referred_obj] : referred_objects) {
      if (emit_field_ids) {
        reference_field_ids_.Append(FindOrAppend(&interned_fields_, field_name));
      }
      uint64_t referred_obj_id = GetObjectId(referred_obj);
      if (referred_obj_id) {
        referred_obj_id -= base_obj_id;
      }
      reference_object_ids_.Append(referred_obj_id);
    }
    if (emit_field_ids && reference_field_ids_.size() != 0) {
      perfetto_protos_HeapGraphObject_set_reference_field_id(
          object_proto, reference_field_ids_.data(), reference_field_ids_.size());
      reference_field_ids_.Reset();
    }
    if (base_obj_id) {
      // The field is called `reference_field_id_base`, but it has always been used as a base for
      // `reference_object_id`. It should be called `reference_object_id_base`.
      perfetto_protos_HeapGraphObject_set_reference_field_id_base(object_proto, base_obj_id);
    }
    if (reference_object_ids_.size() != 0) {
      perfetto_protos_HeapGraphObject_set_reference_object_id(
          object_proto, reference_object_ids_.data(), reference_object_ids_.size());
      reference_object_ids_.Reset();
    }
  }

  // Iterates all the `referred_objects` and sets all the objects that are supposed to be ignored
  // to nullptr. Returns the object with the smallest address (ignoring nullptr).
  art::mirror::Object* FilterIgnoredReferencesAndFindMin(
      std::vector<std::pair<std::string, art::mirror::Object*>>& referred_objects) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    art::mirror::Object* min_nonnull_ptr = nullptr;
    for (auto& p : referred_objects) {
      art::mirror::Object*& referred_obj = p.second;
      if (referred_obj == nullptr)
        continue;
      if (IsIgnored(referred_obj)) {
        referred_obj = nullptr;
        continue;
      }
      if (min_nonnull_ptr == nullptr || min_nonnull_ptr > referred_obj) {
        min_nonnull_ptr = referred_obj;
      }
    }
    return min_nonnull_ptr;
  }

  // Fills `*object_proto` with the value of a subset of potentially interesting fields of `*obj`
  // (an object of type `*klass`).
  void FillFieldValues(art::mirror::Object* obj,
                       art::mirror::Class* klass,
                       perfetto_protos_HeapGraphObject* object_proto) const
      REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (obj->IsClass() || klass->IsClassClass()) {
      return;
    }

    for (art::mirror::Class* cls = klass; cls != nullptr; cls = cls->GetSuperClass().Ptr()) {
      if (cls->IsArrayClass()) {
        continue;
      }

      if (cls->DescriptorEquals("Llibcore/util/NativeAllocationRegistry;")) {
        art::ArtField* af = cls->FindDeclaredInstanceField(
            "size", art::Primitive::Descriptor(art::Primitive::kPrimLong));
        if (af) {
          perfetto_protos_HeapGraphObject_set_native_allocation_registry_size_field(
              object_proto, af->GetLong(obj));
        }
      }
    }
  }

  // Returns true if `*obj` has a type that's supposed to be ignored.
  bool IsIgnored(art::mirror::Object* obj) const REQUIRES_SHARED(art::Locks::mutator_lock_) {
    if (obj->IsClass()) {
      return false;
    }
    art::mirror::Class* klass = obj->GetClass();
    std::string temp;
    std::string_view name(klass->GetDescriptor(&temp));
    return std::find(ignored_types_.begin(), ignored_types_.end(), name) != ignored_types_.end();
  }

  // Name of classes whose instances should be ignored.
  const std::vector<std::string> ignored_types_;

  // Make sure that intern ID 0 (default proto value for a uint64_t) always maps to ""
  // (default proto value for a string) or to 0 (default proto value for a uint64).

  // Map from string (the field name) to its index in perfetto.protos.HeapGraph.field_names
  std::map<std::string, uint64_t> interned_fields_{{"", 0}};
  // Map from string (the location name) to its index in perfetto.protos.HeapGraph.location_names
  std::map<std::string, uint64_t> interned_locations_{{"", 0}};
  // Map from addr (the class pointer) to its id in perfetto.protos.HeapGraph.types
  std::map<uintptr_t, uint64_t> interned_classes_{{0, 0}};

  // Temporary buffers: used locally in some methods and then cleared.
  VarIntBuffer reference_field_ids_;
  VarIntBuffer reference_object_ids_;

  // Id of the previous object that was dumped. Used for delta encoding.
  uint64_t prev_object_id_ = 0;
};

// waitpid with a timeout implemented by ~busy-waiting
// See b/181031512 for rationale.
void BusyWaitpid(pid_t pid, uint32_t timeout_ms) {
  for (size_t i = 0;; ++i) {
    if (i == timeout_ms) {
      // The child hasn't exited.
      // Give up and SIGKILL it. The next waitpid should succeed.
      LOG(ERROR) << "perfetto_hprof child timed out. Sending SIGKILL.";
      kill(pid, SIGKILL);
    }
    int stat_loc;
    pid_t wait_result = waitpid(pid, &stat_loc, WNOHANG);
    if (wait_result == -1 && errno != EINTR) {
      if (errno != ECHILD) {
        // This hopefully never happens (should only be EINVAL).
        PLOG(FATAL_WITHOUT_ABORT) << "waitpid";
      }
      // If we get ECHILD, the parent process was handling SIGCHLD, or did a wildcard wait.
      // The child is no longer here either way, so that's good enough for us.
      break;
    } else if (wait_result > 0) {
      break;
    } else {  // wait_result == 0 || errno == EINTR.
      usleep(1000);
    }
  }
}

enum class ResumeParentPolicy {
  IMMEDIATELY,
  DEFERRED
};

void ForkAndRun(art::Thread* self,
                ResumeParentPolicy resume_parent_policy,
                const std::function<void(pid_t child)>& parent_runnable,
                const std::function<void(pid_t parent, uint64_t timestamp)>& child_runnable) {
  pid_t parent_pid = getpid();
  LOG(INFO) << "forking for " << parent_pid;
  // Need to take a heap dump while GC isn't running. See the comment in
  // Heap::VisitObjects(). Also we need the critical section to avoid visiting
  // the same object twice. See b/34967844.
  //
  // We need to do this before the fork, because otherwise it can deadlock
  // waiting for the GC, as all other threads get terminated by the clone, but
  // their locks are not released.
  // This does not perfectly solve all fork-related issues, as there could still be threads that
  // are unaffected by ScopedSuspendAll and in a non-fork-friendly situation
  // (e.g. inside a malloc holding a lock). This situation is quite rare, and in that case we will
  // hit the watchdog in the grand-child process if it gets stuck.
  std::optional<art::gc::ScopedGCCriticalSection> gcs(std::in_place, self, art::gc::kGcCauseHprof,
                                                      art::gc::kCollectorTypeHprof);

  std::optional<art::ScopedSuspendAll> ssa(std::in_place, __FUNCTION__, /* long_suspend=*/ true);

  pid_t pid = fork();
  if (pid == -1) {
    // Fork error.
    PLOG(ERROR) << "fork";
    return;
  }
  if (pid != 0) {
    // Parent
    if (resume_parent_policy == ResumeParentPolicy::IMMEDIATELY) {
      // Stop the thread suspension as soon as possible to allow the rest of the application to
      // continue while we waitpid here.
      ssa.reset();
      gcs.reset();
    }
    parent_runnable(pid);
    if (resume_parent_policy != ResumeParentPolicy::IMMEDIATELY) {
      ssa.reset();
      gcs.reset();
    }
    return;
  }
  // The following code is only executed by the child of the original process.
  // Uninstall signal handler, so we don't trigger a profile on it.
  if (sigaction(kJavaHeapprofdSignal, &g_orig_act, nullptr) != 0) {
    close(g_signal_pipe_fds[0]);
    close(g_signal_pipe_fds[1]);
    PLOG(FATAL) << "Failed to sigaction";
    return;
  }

  uint64_t ts = GetCurrentBootClockNs();
  child_runnable(parent_pid, ts);
  // Prevent the `atexit` handlers from running. We do not want to call cleanup
  // functions the parent process has registered.
  art::FastExit(0);
}

void WriteHeapPackets(pid_t parent_pid, uint64_t timestamp) NO_THREAD_SAFETY_ANALYSIS {
  PERFETTO_DS_TRACE(java_hprof_data_source, ctx) {
    bool dump_smaps;
    std::vector<std::string> ignored_types;
    {
      void* inst = PerfettoDsImplGetInstanceLocked(java_hprof_data_source.impl, ctx.impl.inst_id);
      auto* ds = reinterpret_cast<JavaHprofDataSource*>(inst);
      if (!ds || !ds->enabled()) {
        if (ds) {
          ds->Finish();
          PerfettoDsImplReleaseInstanceLocked(java_hprof_data_source.impl, ctx.impl.inst_id);
        }
        LOG(INFO) << "skipping irrelevant data source.";
        continue;
      }
      dump_smaps = ds->dump_smaps();
      ignored_types = ds->ignored_types();
      PerfettoDsImplReleaseInstanceLocked(java_hprof_data_source.impl, ctx.impl.inst_id);
    }

    LOG(INFO) << "dumping heap for " << parent_pid;
    if (dump_smaps) {
      DumpSmaps(&ctx);
    }
    Writer writer(parent_pid, &ctx, timestamp);
    // Too big to be on the stack (-Wframe-larger-than).
    auto dumper = std::make_unique<HeapGraphDumper>(ignored_types);

    dumper->Dump(art::Runtime::Current(), writer);

    writer.Finalize();
    PerfettoDsTracerFlush(
        &ctx,
        [](void*) {
          art::MutexLock lk(JavaHprofDataSource::art_thread(), GetStateMutex());
          g_state = State::kEnd;
          GetStateCV().Broadcast(JavaHprofDataSource::art_thread());
        },
        nullptr);
    // Wait for the Flush that will happen on the Perfetto thread.
    {
      art::MutexLock lk(JavaHprofDataSource::art_thread(), GetStateMutex());
      while (g_state != State::kEnd) {
        GetStateCV().Wait(JavaHprofDataSource::art_thread());
      }
    }
    {
      void* inst = PerfettoDsImplGetInstanceLocked(java_hprof_data_source.impl, ctx.impl.inst_id);
      auto* ds = reinterpret_cast<JavaHprofDataSource*>(inst);
      if (ds) {
        ds->Finish();
      } else {
        LOG(ERROR) << "datasource timed out (duration_ms + datasource_stop_timeout_ms) "
                      "before dump finished";
      }
      PerfettoDsImplReleaseInstanceLocked(java_hprof_data_source.impl, ctx.impl.inst_id);
    }
  }
}

void DumpPerfetto(art::Thread* self) {
  ForkAndRun(
    self,
    ResumeParentPolicy::IMMEDIATELY,
    // parent thread
    [](pid_t child) {
      // Busy waiting here will introduce some extra latency, but that is okay because we have
      // already unsuspended all other threads. This runs on the perfetto_hprof_listener, which
      // is not needed for progress of the app itself.
      // We daemonize the child process, so effectively we only need to wait
      // for it to fork and exit.
      BusyWaitpid(child, 1000);
    },
    // child thread
    [self](pid_t dumped_pid, uint64_t timestamp) {
      // Daemon creates a new process that is the grand-child of the original process, and exits.
      if (daemon(0, 0) == -1) {
        PLOG(FATAL) << "daemon";
      }
      // The following code is only executed by the grand-child of the original process.

      // Make sure that this is the first thing we do after forking, so if anything
      // below hangs, the fork will go away from the watchdog.
      ArmWatchdogOrDie();
      SetupDataSource("android.java_hprof", false);
      WaitForDataSource(self);
      WriteHeapPackets(dumped_pid, timestamp);
      LOG(INFO) << "finished dumping heap for " << dumped_pid;
    });
}

void DumpPerfettoOutOfMemory() REQUIRES_SHARED(art::Locks::mutator_lock_) {
  art::Thread* self = art::Thread::Current();
  if (!self) {
    LOG(FATAL_WITHOUT_ABORT) << "no thread in DumpPerfettoOutOfMemory";
    return;
  }

  // Ensure that there is an active, armed tracing session
  uint32_t session_cnt =
      android::base::GetUintProperty<uint32_t>("traced.oome_heap_session.count", 0);
  if (session_cnt == 0) {
    return;
  }
  {
    // OutOfMemoryErrors are reentrant, make sure we do not fork and process
    // more than once.
    art::MutexLock lk(self, GetStateMutex());
    if (g_oome_triggered) {
      return;
    }
    g_oome_triggered = true;
    g_oome_sessions_pending = session_cnt;
  }

  art::ScopedThreadSuspension sts(self, art::ThreadState::kSuspended);
  // If we fork & resume the original process execution it will most likely exit
  // ~immediately due to the OOME error thrown. When the system detects that
  // that, it will cleanup by killing all processes in the cgroup (including
  // the process we just forked).
  // We need to avoid the race between the heap dump and the process group
  // cleanup, and the only way to do this is to avoid resuming the original
  // process until the heap dump is complete.
  // Given we are already about to crash anyway, the diagnostic data we get
  // outweighs the cost of introducing some latency.
  ForkAndRun(
    self,
    ResumeParentPolicy::DEFERRED,
    // parent process
    [](pid_t child) {
      // waitpid to reap the zombie
      // we are explicitly waiting for the child to exit
      // The reason for the timeout on top of the watchdog is that it is
      // possible (albeit unlikely) that even the watchdog will fail to be
      // activated in the case of an atfork handler.
      BusyWaitpid(child, kWatchdogTimeoutSec * 1000);
    },
    // child process
    [self](pid_t dumped_pid, uint64_t timestamp) {
      ArmWatchdogOrDie();
      art::ScopedTrace trace("perfetto_hprof oome");
      SetupDataSource("android.java_hprof.oom", true);
      PerfettoProducerActivateTrigger("com.android.telemetry.art-outofmemory", 500);

      // A pre-armed tracing session might not exist, so we should wait for a
      // limited amount of time before we decide to let the execution continue.
      if (!TimedWaitForDataSource(self, 1000)) {
        LOG(INFO) << "OOME hprof timeout (state " << g_state << ")";
        return;
      }
      WriteHeapPackets(dumped_pid, timestamp);
      LOG(INFO) << "OOME hprof complete for " << dumped_pid;
    });
}

// The plugin initialization function.
extern "C" bool ArtPlugin_Initialize() {
  if (art::Runtime::Current() == nullptr) {
    return false;
  }
  art::Thread* self = art::Thread::Current();
  {
    art::MutexLock lk(self, GetStateMutex());
    if (g_state != State::kUninitialized) {
      LOG(ERROR) << "perfetto_hprof already initialized. state: " << g_state;
      return false;
    }
    g_state = State::kWaitForListener;
  }

  if (pipe2(g_signal_pipe_fds, O_CLOEXEC) == -1) {
    PLOG(ERROR) << "Failed to pipe";
    return false;
  }

  struct sigaction act = {};
  act.sa_flags = SA_SIGINFO | SA_RESTART;
  act.sa_sigaction = [](int, siginfo_t* si, void*) {
    requested_tracing_session_id = si->si_value.sival_int;
    if (write(g_signal_pipe_fds[1], kByte, sizeof(kByte)) == -1) {
      PLOG(ERROR) << "Failed to trigger heap dump";
    }
  };

  // TODO(fmayer): We can probably use the SignalCatcher thread here to not
  // have an idle thread.
  if (sigaction(kJavaHeapprofdSignal, &act, &g_orig_act) != 0) {
    close(g_signal_pipe_fds[0]);
    close(g_signal_pipe_fds[1]);
    PLOG(ERROR) << "Failed to sigaction";
    return false;
  }

  std::thread th([] {
    art::Runtime* runtime = art::Runtime::Current();
    if (!runtime) {
      LOG(FATAL_WITHOUT_ABORT) << "no runtime in perfetto_hprof_listener";
      return;
    }
    if (!runtime->AttachCurrentThread("perfetto_hprof_listener", /*as_daemon=*/ true,
                                      runtime->GetSystemThreadGroup(), /*create_peer=*/ false)) {
      LOG(ERROR) << "failed to attach thread.";
      {
        art::MutexLock lk(nullptr, GetStateMutex());
        g_state = State::kUninitialized;
        GetStateCV().Broadcast(nullptr);
      }

      return;
    }
    art::Thread* self = art::Thread::Current();
    if (!self) {
      LOG(FATAL_WITHOUT_ABORT) << "no thread in perfetto_hprof_listener";
      return;
    }
    {
      art::MutexLock lk(self, GetStateMutex());
      if (g_state == State::kWaitForListener) {
        g_state = State::kWaitForStart;
        GetStateCV().Broadcast(self);
      }
    }
    char buf[1];
    for (;;) {
      int res;
      do {
        res = read(g_signal_pipe_fds[0], buf, sizeof(buf));
      } while (res == -1 && errno == EINTR);

      if (res <= 0) {
        if (res == -1) {
          PLOG(ERROR) << "failed to read";
        }
        close(g_signal_pipe_fds[0]);
        return;
      }

      perfetto_hprof::DumpPerfetto(self);
    }
  });
  th.detach();

  // Register the OOM error handler.
  art::Runtime::Current()->SetOutOfMemoryErrorHook(perfetto_hprof::DumpPerfettoOutOfMemory);

  return true;
}

extern "C" bool ArtPlugin_Deinitialize() {
  art::Runtime::Current()->SetOutOfMemoryErrorHook(nullptr);

  if (sigaction(kJavaHeapprofdSignal, &g_orig_act, nullptr) != 0) {
    PLOG(ERROR) << "failed to reset signal handler";
    // We cannot close the pipe if the signal handler wasn't unregistered,
    // to avoid receiving SIGPIPE.
    return false;
  }
  close(g_signal_pipe_fds[1]);

  art::Thread* self = art::Thread::Current();
  art::MutexLock lk(self, GetStateMutex());
  // Wait until after the thread was registered to the runtime. This is so
  // we do not attempt to register it with the runtime after it had been torn
  // down (ArtPlugin_Deinitialize gets called in the Runtime dtor).
  while (g_state == State::kWaitForListener) {
    GetStateCV().Wait(art::Thread::Current());
  }
  g_state = State::kUninitialized;
  GetStateCV().Broadcast(self);
  return true;
}

}  // namespace perfetto_hprof
