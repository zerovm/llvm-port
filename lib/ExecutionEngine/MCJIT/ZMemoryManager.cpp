#include "llvm/ExecutionEngine/ZMemoryManager.h"

#include <vector>

#include "llvm/Support/Memory.h"

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

    uintptr_t RequiredSize = (Size % Alignment) ?
                               Alignment * ((Size + Alignment - 1)/Alignment + 1):
                               Size;
    uint8_t* start = NULL;

    if (ZCodeSlabs.empty() ||
        RequiredSize > (uintptr_t)ZCodeSlabs.back().base() + (uintptr_t)ZCodeSlabs.back().size() - (uintptr_t)FreeSpaceStart) {
      // allocate slab, add to code slab list
      sys::MemoryBlock mb = allocateNewZSlab((size_t)SlabSize);

      if (!mb.base())
        return start;
      ZCodeSlabs.push_back(mb);
      ZCodeSlabsEnd.push_back((uintptr_t)mb.base());


      // find first 64K alignment
      start = (uint8_t*)(((uintptr_t)mb.base() + PageAlignment - 1) & ~(uintptr_t)(PageAlignment - 1));
    } else {
      start = FreeSpaceStart;
    }

    FreeSpaceStart = start + RequiredSize;
    ZCodeSlabsEnd.back() = (uintptr_t)FreeSpaceStart;
    start = (uint8_t*)((uintptr_t)(start + Alignment - 1) & ~(uintptr_t)(Alignment - 1));

    return start;
  }

  /// Sets allocated memory ready for execution
  bool setMemoryExecutable() {
    return applyMemoryPermissions(sys::Memory::MF_READ | sys::Memory::MF_EXEC);
  }
  /// Sets allocated memory ready for write
  bool setMemoryWritable() {
    return applyMemoryPermissions(sys::Memory::MF_READ | sys::Memory::MF_WRITE);
  }

private:
  /// \brief Allocating code slab
  ///
  /// Try to allocate memory block of given size. On failure it will return
  /// non-valid memory block: base address and size will be null
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
      uint8_t* base = (uint8_t*)((((uintptr_t)ZCodeSlabs[i].base() + PageAlignment - 1) & ~(uintptr_t)(PageAlignment - 1)));
      size_t size = ZCodeSlabsEnd[i] - (uintptr_t)base;
      ec = sys::Memory::protectMappedMemory(sys::MemoryBlock(base, size),
                                            Permissions);
      if (ec)
        return true;
    }

    return false;
  }


  /// vector of allocated memory blocks
  /// can't track client allocations inside them
  std::vector<sys::MemoryBlock> ZCodeSlabs;
  std::vector<uintptr_t>        ZCodeSlabsEnd;


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

bool ZMemoryManager::applyPermissions(std::string* ErrMsg)
{
  // apply permissions
  bool ret = SectionMemoryManager::applyPermissions(ErrMsg);
  if (ret)
    return ret;
  return AllocatorHelper->setMemoryExecutable();
}

bool ZMemoryManager::resetPermissions(std::string* ErrMsg)
{
  bool ret = SectionMemoryManager::resetPermissions(ErrMsg);
  if (ret)
    return ret;
  return AllocatorHelper->setMemoryWritable();
}

}
