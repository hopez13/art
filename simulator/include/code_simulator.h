
  static CodeSimulator* CreateCodeSimulator(InstructionSet target_isa);

  virtual void RunFrom(intptr_t code_buffer) = 0;

  // Get return value according to C ABI.
  virtual bool GetCReturnBool() const = 0;
  virtual int32_t GetCReturnInt32() const = 0;
  virtual int64_t GetCReturnInt64() const = 0;
  DISALLOW_COPY_AND_ASSIGN(CodeSimulator#endif  // ART_SIMULATOR_INCLUDE_CODE_SIMULATOR_H