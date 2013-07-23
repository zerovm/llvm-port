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

  /// \brief Applies section-specific memory permissions.
  ///
  /// This method is called when object loading is complete and section page
  /// permissions can be applied.  It is up to the memory manager implementation
  /// to decide whether or not to act on this method.  The memory manager will
  /// typically allocate all sections as read-write and then apply specific
  /// permissions when this method is called.  Code sections cannot be executed
  /// until this function has been called.
  ///
  /// \returns true if an error occurred, false otherwise.
  virtual bool applyPermissions(std::string *ErrMsg = 0);

  /// \brief Resets section-specific memory permissions.
  ///
  /// This method is called when compiling new function after applyPermissions()
  /// was called. Memory manager will reset permissions to read-write, so it
  /// would be possible to JIT new functions.
  ///
  /// \returns true if an error occurred, false otherwise.
  virtual bool resetPermissions(std::string *ErrMsg = 0);
private:
  OwningPtr<ZCodeMemoryAllocator> AllocatorHelper;
};
}

#endif // ZMEMORYMANAGER_H
