inline void EventHandler::ExecuteCallback(impl::EventHandlerFunc<kEvent> handler, Args... args) {inline void EventHandler::ExecuteCallback(impl::EventHandlerFunc<kEvent> handler                                          Args... args) {
  handler.ExecuteCallback(jnienv, args...);
}

// Events that need custom logic for if we send the event but are otherwise normal. This includes
// the kBreakpoint, kFramePop, kFieldAccess, and kFieldModification events.

// Need to give custom specializations for Breakpoint since it needs to filter out which particular
// methods/dex_pcs agents get notified on.
template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kBreakpoint>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID jmethod,
    jlocation location) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  art::ArtMethod* method = art::jni::DecodeArtMethod(jmethod);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kBreakpoint>(env, thread) &&
      env->breakpoints.find({method, location}) != env->breakpoints.end();
}

template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFramePop>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID jmethod ATTRIBUTE_UNUSED,
    jboolean is_exception ATTRIBUTE_UNUSED,
    const art::ShadowFrame* frame) const {
  // Search for the frame. Do this before checking if we need to send the event so that we don't
  // have to deal with use-after-free or the frames being reallocated later.
  art::WriterMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return env->notify_frames.erase(frame) != 0 &&
      !frame->GetSkipMethodExitEvents() &&
      ShouldDispatchOnThread<ArtJvmtiEvent::kFramePop>(env, thread);
}

// Need to give custom specializations for FieldAccess and FieldModification since they need to
// filter out which particular fields agents want to get notified on.
// TODO The spec allows us to do shortcuts like only allow one agent to ever set these watches. This
// could make the system more performant.
template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFieldModification>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID method ATTRIBUTE_UNUSED,
    jlocation location ATTRIBUTE_UNUSED,
    jclass field_klass ATTRIBUTE_UNUSED,
    jobject object ATTRIBUTE_UNUSED,
    jfieldID field,
    char type_char ATTRIBUTE_UNUSED,
    jvalue val ATTRIBUTE_UNUSED) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kFieldModification>(env, thread) &&
      env->modify_watched_fields.find(
          art::jni::DecodeArtField(field)) != env->modify_watched_fields.end();
}

template <>
inline bool EventHandler::ShouldDispatch<ArtJvmtiEvent::kFieldAccess>(
    ArtJvmTiEnv* env,
    art::Thread* thread,
    JNIEnv* jnienv ATTRIBUTE_UNUSED,
    jthread jni_thread ATTRIBUTE_UNUSED,
    jmethodID method ATTRIBUTE_UNUSED,
    jlocation location ATTRIBUTE_UNUSED,
    jclass field_klass ATTRIBUTE_UNUSED,
    jobject object ATTRIBUTE_UNUSED,
    jfieldID field) const {
  art::ReaderMutexLock lk(art::Thread::Current(), env->event_info_mutex_);
  return ShouldDispatchOnThread<ArtJvmtiEvent::kFieldAccess>(env, thread) &&
      env->access_watched_fields.find(
          art::jni::DecodeArtField(field)) != env->access_watched_fields.end();
}

// Need to give custom specializations for FramePop since it needs to filter out which particular
// agents get the event. This specialization gets an extra argument so we can determine which (if
// any) environments have the frame pop.
// TODO It might be useful to use more template magic to have this only define ShouldDispatch or
// something.
template <>
inline void EventHandler::ExecuteCallback<ArtJvmtiEvent::kFramePop>(
    impl::EventHandlerFunc<ArtJvmtiEvent::kFramePop> event,
    JNIEnv* jnienv,
    jthread jni_thread,
    jmethodID jmethod,
    jboolean is_exception,
    const art::ShadowFrame* frame ATTRIBUTE_UNUSED) {
  ExecuteCallback<ArtJvmtiEvent::kFramePop>(event, jnienv, jni_thread, jmethod, is_exception);
}

struct ScopedDisablePopFrame {
 public:
  explicit ScopedDisablePopFrame(art::Thread* thread) : thread_(thread) {
    art::Locks::mutator_lock_->AssertSharedHeld(thread_);
    art::MutexLock mu(thread_, *art::Locks::thread_list_lock_);
    JvmtiGlobalTLSData* data = ThreadUtil::GetOrCreateGlobalTLSData(thread_);
    current_top_frame_ = art::StackVisitor::ComputeNumFrames(
        thread_, art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
    old_disable_frame_pop_depth_ = data->disable_pop_frame_depth;
    data->disable_pop_frame_depth = current_top_frame_;
    // Check that we cleaned up any old disables. This should only increase (or be equals if we do
    // another ClassLoad/Prepare recursively).
    DCHECK(old_disable_frame_pop_depth_ == JvmtiGlobalTLSData::kNoDisallowedPopFrame ||
           current_top_frame_ >= old_disable_frame_pop_depth_)
        << "old: " << old_disable_frame_pop_depth_ << " current: " << current_top_frame_;
  }

  ~ScopedDisablePopFrame() {
    art::Locks::mutator_lock_->AssertSharedHeld(thread_);
    art::MutexLock mu(thread_, *art::Locks::thread_list_lock_);
    JvmtiGlobalTLSData* data = ThreadUtil::GetGlobalTLSData(thread_);
    DCHECK_EQ(data->disable_pop_frame_depth, current_top_frame_);
    data->disable_pop_frame_depth = old_disable_frame_pop_depth_;
  }

 private:
  art::Thread* thread_;
  size_t current_top_frame_;
  size_t old_disable_frame_pop_depth_;
};
// We want to prevent the use of PopFrame when reporting either of these events.
template <ArtJvmtiEvent kEvent>
inline void EventHandler::DispatchClassLoadOrPrepareEvent(art::Thread* thread,
                                                          JNIEnv* jnienv,
                                                          jthread jni_thread,
                                                          jclass klass) const {
  ScopedDisablePopFrame sdpf(thread);
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  std::vector<impl::EventHandlerFunc<kEvent>> events = CollectEvents<kEvent>(thread,
                                                                             jnienv,
                                                                             jni_thread,
                                                                             klass);

  for (auto event : events) {
    ExecuteCallback<kEvent>(event, jnienv, jni_thread, klass);
  }
}

template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassLoad>(art::Thread* thread,
                                                                   JNIEnv* jnienv,
                                                                   jthread jni_thread,
                                                                   jclass klass) const {
  DispatchClassLoadOrPrepareEvent<ArtJvmtiEvent::kClassLoad>(thread, jnienv, jni_thread, klass);
}
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassPrepare>(art::Thread* thread,
                                                                      JNIEnv* jnienv,
                                                                      jthread jni_thread,
                                                                      jclass klass) const {
  DispatchClassLoadOrPrepareEvent<ArtJvmtiEvent::kClassPrepare>(thread, jnienv, jni_thread, klass);
}

// Need to give a custom specialization for NativeMethodBind since it has to deal with an out
// variable.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kNativeMethodBind>(art::Thread* thread,
                                                                          JNIEnv* jnienv,
                                                                          jthread jni_thread,
                                                                          jmethodID method,
                                                                          void* cur_method,
                                                                          void** new_method) const {
  art::ScopedThreadStateChange stsc(thread, art::ThreadState::kNative);
  std::vector<impl::EventHandlerFunc<ArtJvmtiEvent::kNativeMethodBind>> events =
      CollectEvents<ArtJvmtiEvent::kNativeMethodBind>(thread,
                                                      jnienv,
                                                      jni_thread,
                                                      method,
                                                      cur_method,
                                                      new_method);
  *new_method = cur_method;
  for (auto event : events) {
    *new_method = cur_method;
    ExecuteCallback<ArtJvmtiEvent::kNativeMethodBind>(event,
                                                      jnienv,
                                                      jni_thread,
                                                      method,
                                                      cur_method,
                                                      new_method);
    if (*new_method != nullptr) {
      cur_method = *new_method;
    }
  }
  *new_method = cur_method;
}

// C++ does not allow partial template function specialization. The dispatch for our separated
// ClassFileLoadHook event types is the same, and in the DispatchClassFileLoadHookEvent helper.
// The following two DispatchEvent specializations dispatch to it.
template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kClassFileLoadHookNonRetransformable>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <>
inline void EventHandler::DispatchEvent<ArtJvmtiEvent::kStructuralDexFileLoadHook>(
    art::Thread* thread,
    JNIEnv* jnienv,
    jclass class_being_redefined,
    jobject loader,
    const char* name,
    jobject protection_domain,
    jint class_data_len,
    const unsigned char* class_data,
    jint* new_class_data_len,
    unsigned char** new_class_data) const {
  return DispatchClassFileLoadHookEvent<ArtJvmtiEvent::kStructuralDexFileLoadHook>(
      thread,
      jnienv,
      class_being_redefined,
      loader,
      name,
      protection_domain,
      class_data_len,
      class_data,
      new_class_data_len,
      new_class_data);
}

template <ArtJvmtiEvent kEvent>
inline bool EventHandler::ShouldDispatchOnThread(ArtJvmTiEnv* env, art::Thread* thread) const {
  bool dispatch = env->event_masks.global_event_mask.Test(kEvent);

  if (!dispatch && thread != nullptr && env->event_masks.unioned_thread_event_mask.Test(kEvent)) {
    EventMask* mask = env->event_masks.GetEventMaskOrNull(thread);
    dispatch = mask != nullptr && mask->Test(kEvent);
  }
  return dispatch;
}

template <ArtJvmtiEvent kEvent, typename ...Args>
inline bool EventHandler::ShouldDispatch(ArtJvmTiEnv* env,
                                         art::Thread* thread,
                                         Args... args ATTRIBUTE_UNUSED) const {
  static_assert(std::is_same<typename impl::EventFnType<kEvent>::type,
                             void(*)(jvmtiEnv*, Args...)>::value,
                "Unexpected different type of shouldDispatch");

  return ShouldDispatchOnThread<kEvent>(env, thread);
}

inline void EventHandler::RecalculateGlobalEventMask(ArtJvmtiEvent event) {
  art::WriterMutexLock mu(art::Thread::Current(), envs_lock_);
  RecalculateGlobalEventMaskLocked(event);
}

inline void EventHandler::RecalculateGlobalEventMaskLocked(ArtJvmtiEvent event) {
  bool union_value = false;
  for (const ArtJvmTiEnv* stored_env : envs) {
    if (stored_env == nullptr) {
      continue;
    }
    union_value |= stored_env->event_masks.global_event_mask.Test(event);
    union_value |= stored_env->event_masks.unioned_thread_event_mask.Test(event);
    if (union_value) {
      break;
    }
  }
  global_mask.Set(event, union_value);
}

inline bool EventHandler::NeedsEventUpdate(ArtJvmTiEnv* env,
                                           const jvmtiCapabilities& caps,
                                           bool added) {
  ArtJvmtiEvent event = added ? ArtJvmtiEvent::kClassFileLoadHookNonRetransformable
                              : ArtJvmtiEvent::kClassFileLoadHookRetransformable;
  return (added && caps.can_access_local_variables == 1) ||
      caps.can_generate_breakpoint_events == 1 ||
      caps.can_pop_frame == 1 ||
      caps.can_force_early_return == 1 ||
      (caps.can_retransform_classes == 1 &&
       IsEventEnabledAnywhere(event) &&
       env->event_masks.IsEnabledAnywhere(event));
}

inline void EventHandler::HandleChangedCapabilities(ArtJvmTiEnv* env,
                                                    const jvmtiCapabilities& caps,
                                                    bool added) {
  if (UNLIKELY(NeedsEventUpdate(env, caps, added))) {
    env->event_masks.HandleChangedCapabilities(caps, added);
    if (caps.can_retransform_classes == 1) {
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookRetransformable);
      RecalculateGlobalEventMask(ArtJvmtiEvent::kClassFileLoadHookNonRetransformable);
    }
    if (added && caps.can_access_local_variables == 1) {
      HandleLocalAccessCapabilityAdded();
    }
    if (caps.can_generate_breakpoint_events == 1) {
      HandleBreakpointEventsChanged(added);
    }
    if ((caps.can_pop_frame == 1 || caps.can_force_early_return == 1) && added) {
      // TODO We should keep track of how many of these have been enabled and remove it if there are
      // no more possible users. This isn't expected to be too common.
      art::Runtime::Current()->SetNonStandardExitsEnabled();
    }
  }
}

}  // namespace openjdkjvmti

#endif  // ART_OPENJDKJVMTI_EVENTS_INL_H_
