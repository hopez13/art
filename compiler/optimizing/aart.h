/*
 * Copyright (C) 2016 Aart Bik
 */

#ifndef ART_COMPILER_OPTIMIZING_AART_H_
#define ART_COMPILER_OPTIMIZING_AART_H_

#undef AART
#undef AART2

#include "runtime.h"

#ifdef AART

#include <iostream>

#include "base/arena_allocator.h"
#include "base/dumpable.h"
#include "graph_checker.h"
#include "pretty_printer.h"

namespace art {

// proper stats needs
//  kArenaAllocatorCountAllocations = true in art/runtime/base/arena_allocator.h
// which, in turn, needs larger frame value, like
//  deviceFrameSizeLimit := 6000
//  hostFrameSizeLimit := 6000 in art/build/art.go
static inline void DumpArena(std::string mess, ArenaAllocator* arena) {
  std::cout << "\n-- " << mess << " #bytes-alloc "
            << arena->BytesAllocated() << " #bytes-used "
            << arena->BytesUsed() << std::endl;
  arena->GetMemStats().Dump(std::cout);
  std::cout << "--\n" << std::endl;
}

static inline void DumpGraph(std::string mess, HGraph* graph) {
  StringPrettyPrinter printer(graph);
  printer.VisitInsertionOrder();
  std::cout << "\n* " << mess << "\n\n" << printer.str() << std::endl;
}

static inline void DumpLoop(HLoopInformation* loop) {
  std::cout << "\nLoop B" << loop->GetHeader()->GetBlockId() << " {";
  for (HBlocksInLoopIterator it(*loop); !it.Done(); it.Advance()) {
    std::cout << " B" <<  it.Current()->GetBlockId();
    if (it.Current()->GetLoopInformation() != loop) {
      std::cout << "!";
    }
    if (!loop->Contains(*it.Current())) {
      std::cout << "?";
    }
  }
  size_t lifetime = loop->GetLifetimeEnd();
  std::cout << " }" << std::endl;
  std::cout << "   #blocks    : "  << loop->GetBlocks().NumSetBits() << std::endl;
  std::cout << "   preheader  : B" << loop->GetPreHeader()->GetBlockId() << std::endl;
  std::cout << "   header     : B" << loop->GetHeader()->GetBlockId()    << std::endl;
  std::cout << "   irred/cont : "  << loop->IsIrreducible()
                                   << "/" << loop->ContainsIrreducibleLoop() << std::endl;
  std::cout << "   lifetime   : "  << (lifetime == kNoLifetime ? 0 : lifetime) << std::endl;
  std::cout << "   populated  : "  << loop->IsPopulated() << std::endl;
  std::cout << "   back edges {";
  for (HBasicBlock* back : loop->GetBackEdges()) {
    std::cout << " B" <<  back->GetBlockId();
  }
  std::cout << " }\n   outer      {";
  for (HLoopInformationOutwardIterator it(*loop->GetHeader()); !it.Done(); it.Advance()) {
    std::cout << " B" << it.Current()->GetHeader()->GetBlockId();
  }
  std::cout << " }\n" << std::endl;
#ifdef AART2
  loop->Dump(std::cout);
#endif
}

}  // namespace art

#endif

#endif  // ART_COMPILER_OPTIMIZING_AART_H_
