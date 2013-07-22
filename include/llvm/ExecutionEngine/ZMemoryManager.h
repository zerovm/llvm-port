#ifndef ZMEMORYMANAGER_H
#define ZMEMORYMANAGER_H
//===- ZMemoryManager.h - Memory manager for Zerovm MCJIT/RtDyld -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of a section-based memory manager used by
// the MCJIT execution engine and RuntimeDyld run ubder ZeroVM.
//
//===----------------------------------------------------------------------===//


#include "llvm/ExecutionEngine/SectionMemoryManager.h"

namespace llvm {

class ZCodeMemoryAllocator;

/// \brief Manages memory allocation used by MCJIT/RuntimeDyld
///
/// Overrides allocateCodeSection() to get code section aligned with 64K
/// boundary. Such alignment is required by zerovm jail syscall, which enables
/// execute permissions.
class ZMemoryManager : public SectionMemoryManager {
  ZMemoryManager(const ZMemoryManager&) LLVM_DELETED_FUNCTION;
  void operator=(const ZMemoryManager&) LLVM_DELETED_FUNCTION;

public:
  ZMemoryManager();
  virtual ~ZMemoryManager();

  /// \brief Allocates a memory block of (at least) the given size suitable for
  /// executable code.
  ///
  /// The value of \p Alignment must be a power of two.  If \p Alignment is zero
  /// a default alignment of 16 will be used.
  virtual uint8_t* allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                       unsigned SectionID);
private:
  OwningPtr<ZCodeMemoryAllocator> AllocatorHelper;
};
}

#endif // ZMEMORYMANAGER_H
