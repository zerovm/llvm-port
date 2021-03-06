#include "llvm/ExecutionEngine/ZMemoryManager.h"

#include <vector>
#include <iostream>

#include "llvm/Support/Memory.h"
#include "llvm/Support/Debug.h"

#ifdef __native_client__
#include "zvm.h"
#endif

namespace llvm {


/// \brief Allocator class. Allocates memory blocks aligned with 64K boundary.
///
/// It allocates big enough memory blocks, align them with 64KB boundary.
/// Every client allocation via allocate() method 'takes' piece of memory inside
/// such block.
///
/// It is suited for ZeroVM jail system call. Thus we have a set of memory blocks
/// with code data with proper alignment.
///
/// TODO: consider memory blocks release (do we need that?)
class ZCodeMemoryAllocator {
public:
  ZCodeMemoryAllocator() {
  }
  ~ZCodeMemoryAllocator() {
    error_code ec;
    for (unsigned i=0;i<ZCodeSlabs.size();++i) {
      ec = sys::Memory::releaseMappedMemory(ZCodeSlabs[i]);
    }
  }

  /// \brief Allocates memory of given size with alignment
  ///
  /// \p Size size of memory
  /// \p Alignment alignment
  /// \returns pointer to memory on success, or NULL on error
  uint8_t* allocate(uintptr_t Size,
                    unsigned Alignment) {
    if (!Alignment)
      Alignment = 16;

    // calculate required size, it could be greater due to alignment
    uintptr_t RequiredSize = (Size % Alignment) ?
                               Alignment * ((Size + Alignment - 1)/Alignment + 1):
                               Size;
    uint8_t* start = NULL;

    // if no memory were allocated OR there no enough free space if block left
    if (ZCodeSlabs.empty() ||
        RequiredSize > (uintptr_t)ZCodeSlabs.back().base() + (uintptr_t)ZCodeSlabs.back().size() - (uintptr_t)ZCodeSlabsEnd.back()) {
      // allocate slab, add to code slab list
      sys::MemoryBlock mb = allocateNewZSlab((size_t)SlabSize);

      if (!mb.base())
        return start;
      ZCodeSlabs.push_back(mb);

      // find first 64K alignment
      start = (uint8_t*)(((uintptr_t)mb.base() + PageAlignment - 1) & ~(uintptr_t)(PageAlignment - 1));
      // save initial start position
      ZCodeSlabsEnd.push_back((uintptr_t)start);
    } else {
      // just take start position, no allocation needed
      start = (uint8_t*)ZCodeSlabsEnd.back();
    }

    // move free space start position
    ZCodeSlabsEnd.back() += RequiredSize;
    start = (uint8_t*)((uintptr_t)(start + Alignment - 1) & ~(uintptr_t)(Alignment - 1));

    DEBUG(dbgs() << "allocating " << RequiredSize << " bytes at 0x");
    DEBUG(dbgs().write_hex((uintptr_t)start));
    DEBUG(dbgs() << "\n");
    return start;
  }

  /// \brief Sets allocated memory ready for execution
  /// \returns false on success
  bool setMemoryExecutable() {
    return applyMemoryPermissions(sys::Memory::MF_READ | sys::Memory::MF_EXEC);
  }
  /// \brief Sets allocated memory ready for write
  /// \returns false on success
  bool setMemoryWritable() {
    return applyMemoryPermissions(sys::Memory::MF_READ | sys::Memory::MF_WRITE);
  }

private:
  /// \brief Allocating code slab
  ///
  /// Try to allocate memory block of given size. On failure it will return
  /// non-valid memory block: base address and size will be null. Under ZeroVM
  /// it fills memory block with valid NOPs, which is valid instruction.
  ///
  /// \p size size of memory block to allocate
  /// \returns allocated memory block
  sys::MemoryBlock allocateNewZSlab(size_t size) {
    error_code ec;
    sys::MemoryBlock* Near = ZCodeSlabs.empty() ? 0 : &ZCodeSlabs.back();
    sys::MemoryBlock MB = sys::Memory::allocateMappedMemory(size,
                                                            Near,
                                                            sys::Memory::MF_READ |
                                                              sys::Memory::MF_WRITE,
                                                            ec);
#ifdef __native_client__
    memset(MB.base(), 0x90, MB.size());
#endif
    return MB;
  }

  /// \brief Apply permissions to allocated memory
  ///
  /// Try to apply given permissions to memory we've already allocated. It will
  /// return false on succes (due to llvm semantics, it also return false on
  /// succesfull permission apply).
  /// TODO: apply permissions only to allocated memory (exclude non-usable
  /// chunks of code)
  ///
  /// \p Permissions permissions to apply
  /// \returns false on success
  bool applyMemoryPermissions(unsigned int Permissions) {

    error_code ec;
    for (unsigned i=0;i<ZCodeSlabs.size();++i) {
      // calculate base address of memory block (64K alignment)
      uint8_t* base = (uint8_t*)((((uintptr_t)ZCodeSlabs[i].base() + PageAlignment - 1) & ~(uintptr_t)(PageAlignment - 1)));
      // and its size
      size_t size = ZCodeSlabsEnd[i] - (uintptr_t)base;
#ifdef __native_client__
      int ret = (Permissions & sys::Memory::MF_EXEC) ? zvm_jail(base, size) : zvm_unjail(base, size);
      if (ret) {
        std::cout << "Error during (un)jail system call (Addr=" << std::hex << (uintptr_t)base << " size=" << std::dec << size << "). Error code is " << ret << std::endl;
        return true;
      }
      return false;
#else
      ec = sys::Memory::protectMappedMemory(sys::MemoryBlock(base, size),
                                            Permissions);
#endif
      if (ec)
        return true;
    }

    return false;
  }

  // vector of allocated memory blocks
  // can't track client allocations inside them
  std::vector<sys::MemoryBlock> ZCodeSlabs;
  // vector of free space position start for each memory block
  std::vector<uintptr_t>        ZCodeSlabsEnd;

  const static int SlabSize            = 0x1000000;    // 16 MB
  const static int PageAlignment       = 0x10000;      // 64 K
};


ZMemoryManager::ZMemoryManager():
  AllocatorHelper(new ZCodeMemoryAllocator()) {
}
ZMemoryManager::~ZMemoryManager() {
}

uint8_t* ZMemoryManager::allocateCodeSection(uintptr_t Size, unsigned Alignment, unsigned SectionID) {
  return AllocatorHelper->allocate(Size, Alignment);
}

bool ZMemoryManager::applyPermissions(std::string* ErrMsg)
{
  return AllocatorHelper->setMemoryExecutable();
}

bool ZMemoryManager::resetPermissions(std::string* ErrMsg)
{
  return AllocatorHelper->setMemoryWritable();
}

}
