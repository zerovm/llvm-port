#include "llvm/ExecutionEngine/ZMemoryManager.h"

#include <vector>

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
/// TODO: reimplement applyPermissions(), resetPermissions()
/// TODO: consider memory blocks release (do we need that?)
class ZCodeMemoryAllocator {
public:
  ZCodeMemoryAllocator():
    FreeSpaceStart(0) {

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

    uintptr_t RequiredSize = Alignment * ((Size + Alignment - 1)/Alignment + 1);
    uint8_t* start = NULL;

    if (ZCodeSlabs.empty() ||
        RequiredSize > (uintptr_t)ZCodeSlabs.back().base() + (uintptr_t)ZCodeSlabs.back().size() - (uintptr_t)FreeSpaceStart) {
      // allocate slab, add to code slab list
      sys::MemoryBlock mb = allocateNewZSlab((size_t)SlabSize);

      if (!mb.base())
        return start;
      ZCodeSlabs.push_back(mb);


      // find first 64K alignment
      start = (uint8_t*)((uintptr_t)(mb.base() + PageAlignment - 1) & ~(uintptr_t)(PageAlignment - 1));
    } else {
      start = FreeSpaceStart;
    }

    FreeSpaceStart = start + RequiredSize;
    start = (uint8_t*)((uintptr_t)(start + Alignment - 1) & ~(uintptr_t)(Alignment - 1));

    return start;
  }


private:
  sys::MemoryBlock allocateNewZSlab(size_t size) {
    error_code ec;
    sys::MemoryBlock* Near = ZCodeSlabs.empty() ? 0 : &ZCodeSlabs.back();
    sys::MemoryBlock MB = sys::Memory::allocateMappedMemory(size,
                                                            Near,
                                                            sys::Memory::MF_READ |
                                                              sys::Memory::MF_WRITE,
                                                            ec);
    return MB;
  }


  /// vector of allocated memory blocks
  /// can't track client allocations inside them
  std::vector<sys::MemoryBlock> ZCodeSlabs;

  /// free space start in last memory block
  uint8_t*    FreeSpaceStart;

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
}
