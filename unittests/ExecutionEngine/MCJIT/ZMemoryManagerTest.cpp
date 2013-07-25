//===- MCJITMemoryManagerTest.cpp - Unit tests for the JIT memory manager -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include <unistd.h>

#include "llvm/ExecutionEngine/ZMemoryManager.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "gtest/gtest.h"

using namespace llvm;

namespace {

TEST(ZMemoryManagerTest, AlignedAllocation) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t *code1 = MemMgr->allocateCodeSection(256, 0, 1);
  uint8_t *data1 = MemMgr->allocateDataSection(256, 0, 2, true);
  uint8_t *code2 = MemMgr->allocateCodeSection(256, 0, 3);
  uint8_t *data2 = MemMgr->allocateDataSection(256, 0, 4, false);

  ASSERT_NE((uint8_t*)0, code1);
  ASSERT_NE((uint8_t*)0, code2);
  ASSERT_NE((uint8_t*)0, data1);
  ASSERT_NE((uint8_t*)0, data2);

  // Initialize the data
  for (unsigned i = 0; i < 256; ++i) {
    code1[i] = 1;
    code2[i] = 2;
    data1[i] = 3;
    data2[i] = 4;
  }

  // Verify the data (this is checking for overlaps in the addresses)
  for (unsigned i = 0; i < 256; ++i) {
    EXPECT_EQ(1, code1[i]);
    EXPECT_EQ(2, code2[i]);
    EXPECT_EQ(3, data1[i]);
    EXPECT_EQ(4, data2[i]);
  }

  // check alignment
  const int boundary = 0xFFFF; // 64K
  ASSERT_FALSE((uintptr_t)code1 & boundary);
}

TEST(ZMemoryManagerTest, NopFillTest) {
#ifdef __native_client__
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t *code = MemMgr->allocateCodeSection(256, 0, 1);

  ASSERT_NE((uint8_t*)0, code);

  for (int i=0;i<246;++i) {
    EXPECT_EQ(code[i], 0x90);
  }

  std::string Error;
  EXPECT_FALSE(MemMgr->applyPermissions(&Error));
#endif
  SUCCEED();
}

TEST(ZMemoryManagerTest, BasicAllocations) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t *code1 = MemMgr->allocateCodeSection(256, 0, 1);
  uint8_t *data1 = MemMgr->allocateDataSection(256, 0, 2, true);
  uint8_t *code2 = MemMgr->allocateCodeSection(256, 0, 3);
  uint8_t *data2 = MemMgr->allocateDataSection(256, 0, 4, false);

  ASSERT_NE((uint8_t*)0, code1);
  ASSERT_NE((uint8_t*)0, code2);
  ASSERT_NE((uint8_t*)0, data1);
  ASSERT_NE((uint8_t*)0, data2);
  ASSERT_NE(code1, code2);
  ASSERT_NE(code1, data1);
  ASSERT_NE(code1, data2);

  // Initialize the data
  for (unsigned i = 0; i < 256; ++i) {
    code1[i] = 1;
    code2[i] = 2;
    data1[i] = 3;
    data2[i] = 4;
  }

  // Verify the data (this is checking for overlaps in the addresses)
  for (unsigned i = 0; i < 256; ++i) {
    EXPECT_EQ(1, code1[i]);
    EXPECT_EQ(2, code2[i]);
    EXPECT_EQ(3, data1[i]);
    EXPECT_EQ(4, data2[i]);
  }

  std::string Error;
  EXPECT_FALSE(MemMgr->applyPermissions(&Error));
}

TEST(ZMemoryManagerTest, LargeAllocations) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t *code1 = MemMgr->allocateCodeSection(0x100000, 0, 1);
  uint8_t *data1 = MemMgr->allocateDataSection(0x100000, 0, 2, true);
  uint8_t *code2 = MemMgr->allocateCodeSection(0x100000, 0, 3);
  uint8_t *data2 = MemMgr->allocateDataSection(0x100000, 0, 4, false);

  ASSERT_NE((uint8_t*)0, code1);
  ASSERT_NE((uint8_t*)0, code2);
  ASSERT_NE((uint8_t*)0, data1);
  ASSERT_NE((uint8_t*)0, data2);

  // Initialize the data
  for (unsigned i = 0; i < 0x100000; ++i) {
    code1[i] = 1;
    code2[i] = 2;
    data1[i] = 3;
    data2[i] = 4;
  }

  // Verify the data (this is checking for overlaps in the addresses)
  for (unsigned i = 0; i < 0x100000; ++i) {
    EXPECT_EQ(1, code1[i]);
    EXPECT_EQ(2, code2[i]);
    EXPECT_EQ(3, data1[i]);
    EXPECT_EQ(4, data2[i]);
  }

  std::string Error;
  EXPECT_FALSE(MemMgr->applyPermissions(&Error));
}

TEST(ZMemoryManagerTest, ContiniousAllocations) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  // doing allocations for total memory = 64 MB
  const int RequiredSize = 0x40000;
  const uint8_t Data = 0xBE;
  for (int Size = 0; Size < 0x4000000; Size += RequiredSize) {
    uint8_t* code = MemMgr->allocateCodeSection(RequiredSize, 0, 0);

    ASSERT_NE(code, (uint8_t*)0);

    // fill with some data
    for (int i = 0; i < RequiredSize; ++i) {
      code[i] = Data;
    }

    // check it
    for (int i = 0; i < RequiredSize; ++i) {
      ASSERT_EQ(code[i], Data);
    }
  }

}

TEST(ZMemoryManagerTest, NonStaticAllocatorTest) {
  // create several memory managers
  // they should be independent
  for (int i=0;i<5;++i) {
    OwningPtr<SectionMemoryManager> MemMgr1(new ZMemoryManager());

    uint8_t *code1 = MemMgr1->allocateCodeSection(256, 0, 1);

    ASSERT_NE((uint8_t*)0, code1);

    // Initialize the data
    for (unsigned i = 0; i < 256; ++i) {
      code1[i] = 1;
    }

    // Verify the data (this is checking for overlaps in the addresses)
    for (unsigned i = 0; i < 256; ++i) {
      EXPECT_EQ(1, code1[i]);
    }

    // check alignment
    const int boundary = 0xFFFF; // 64K
    ASSERT_FALSE((uintptr_t)code1 & boundary);
  }

}

TEST(ZMemoryManagerTest, ManyAllocations) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t* code[10000];
  uint8_t* data[10000];

  for (unsigned i = 0; i < 10000; ++i) {
    const bool isReadOnly = i % 2 == 0;

    code[i] = MemMgr->allocateCodeSection(32, 0, 1);
    data[i] = MemMgr->allocateDataSection(32, 0, 2, isReadOnly);

    ASSERT_NE((uint8_t *)0, code[i]);
    ASSERT_NE((uint8_t *)0, data[i]);

    for (unsigned j = 0; j < 32; j++) {
      code[i][j] = 1 + (i % 254);
      data[i][j] = 2 + (i % 254);
    }
  }

  // Verify the data (this is checking for overlaps in the addresses)
  for (unsigned i = 0; i < 10000; ++i) {
    for (unsigned j = 0; j < 32;j++ ) {
      uint8_t ExpectedCode = 1 + (i % 254);
      uint8_t ExpectedData = 2 + (i % 254);
      EXPECT_EQ(ExpectedCode, code[i][j]);
      EXPECT_EQ(ExpectedData, data[i][j]);
    }
  }

  std::string Error;
  EXPECT_FALSE(MemMgr->applyPermissions(&Error));
}

TEST(ZMemoryManagerTest, ManyVariedAllocations) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t* code[10000];
  uint8_t* data[10000];

  for (unsigned i = 0; i < 10000; ++i) {
    uintptr_t CodeSize = i % 16 + 1;
    uintptr_t DataSize = i % 8 + 1;

    bool isReadOnly = i % 3 == 0;
    unsigned Align = 8 << (i % 4);

    code[i] = MemMgr->allocateCodeSection(CodeSize, Align, i);
    data[i] = MemMgr->allocateDataSection(DataSize, Align, i + 10000,
                                          isReadOnly);


    ASSERT_NE((uint8_t *)0, code[i]);
    ASSERT_NE((uint8_t *)0, data[i]);

    for (unsigned j = 0; j < CodeSize; j++) {
      code[i][j] = 1 + (i % 254);
    }

    for (unsigned j = 0; j < DataSize; j++) {
      data[i][j] = 2 + (i % 254);
    }

    uintptr_t CodeAlign = Align ? (uintptr_t)code[i] % Align : 0;
    uintptr_t DataAlign = Align ? (uintptr_t)data[i] % Align : 0;

    EXPECT_EQ((uintptr_t)0, CodeAlign);
    EXPECT_EQ((uintptr_t)0, DataAlign);
  }

  for (unsigned i = 0; i < 10000; ++i) {
    uintptr_t CodeSize = i % 16 + 1;
    uintptr_t DataSize = i % 8 + 1;

    for (unsigned j = 0; j < CodeSize; j++) {
      uint8_t ExpectedCode = 1 + (i % 254);
      EXPECT_EQ(ExpectedCode, code[i][j]);
    }

    for (unsigned j = 0; j < DataSize; j++) {
      uint8_t ExpectedData = 2 + (i % 254);
      EXPECT_EQ(ExpectedData, data[i][j]);
    }
  }
}

TEST(ZMemoryManagerTest, PermissionsTest) {
  OwningPtr<SectionMemoryManager> MemMgr(new ZMemoryManager());

  uint8_t *code1 = MemMgr->allocateCodeSection(256, 0, 1);
  uint8_t *code2 = MemMgr->allocateCodeSection(256, 0, 3);

  ASSERT_NE((uint8_t*)0, code1);
  ASSERT_NE((uint8_t*)0, code2);

  // Initialize the data
  for (unsigned i = 0; i < 256; ++i) {
    code1[i] = 1;
    code2[i] = 2;
  }

  // Verify the data (this is checking for overlaps in the addresses)
  for (unsigned i = 0; i < 256; ++i) {
    EXPECT_EQ(1, code1[i]);
    EXPECT_EQ(2, code2[i]);
  }

  std::string Error;
  EXPECT_FALSE(MemMgr->applyPermissions(&Error));
  // Now we can't write into allocated memory
  // But ZMemoryManager allocates huge memory block, alignes it to 64K
  // boundary. Thus there is unused memory regions: in beginning and end.
  // It should be RW, not RX. Try to write into that memory region
  // Writing after code1+code2+PAGE_SIZE
  const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
  uint8_t* tail_region = code2 + PAGE_SIZE;
  *tail_region = 0;
  SUCCEED();
  EXPECT_FALSE(MemMgr->resetPermissions(&Error));
  // we expect memory permissions to be RW now
  *tail_region = 0;
  SUCCEED();
}


} // Namespace


