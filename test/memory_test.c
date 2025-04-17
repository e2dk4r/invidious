#include "memory.h"

// TODO: Show error pretty error message when a test fails
enum memory_test_error {
  MEMORY_TEST_ERROR_NONE = 0,
  MEMORY_TEST_ERROR_MEM_PUSH_ALIGNED_EXPECTED_VALID_ADDRESS_1,
  MEMORY_TEST_ERROR_MEM_PUSH_ALIGNED_EXPECTED_VALID_ADDRESS_2,
  MEMORY_TEST_ERROR_MEM_PUSH_EXPECTED_VALID_ADDRESS_1,
  MEMORY_TEST_ERROR_MEM_PUSH_EXPECTED_VALID_ADDRESS_2,

  // src: https://mesonbuild.com/Unit-tests.html#skipped-tests-and-hard-errors
  // For the default exitcode testing protocol, the GNU standard approach in
  // this case is to exit the program with error code 77. Meson will detect this
  // and report these tests as skipped rather than failed. This behavior was
  // added in version 0.37.0.
  MESON_TEST_SKIP = 77,
  // In addition, sometimes a test fails set up so that it should fail even if
  // it is marked as an expected failure. The GNU standard approach in this case
  // is to exit the program with error code 99. Again, Meson will detect this
  // and report these tests as ERROR, ignoring the setting of should_fail. This
  // behavior was added in version 0.50.0.
  MESON_TEST_FAILED_TO_SET_UP = 99,
};

int
main(void)
{
  enum memory_test_error errorCode = MEMORY_TEST_ERROR_NONE;

  // setup
  u64 KILOBYTES = 1 << 10;
  u8 stackBuffer[8 * KILOBYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };
  bzero(stackMemory.block, stackMemory.total);
  memory_temp tempMemory;

  // MemPush(memory_block *mem, u64 size)
  tempMemory = MemoryTempBegin(&stackMemory);
  {
    void *expected;
    void *value;

    expected = stackMemory.block + 0;
    value = MemoryArenaPushAligned(&stackMemory, 10, 4);
    if (value != expected) {
      errorCode = MEMORY_TEST_ERROR_MEM_PUSH_ALIGNED_EXPECTED_VALID_ADDRESS_1;
      goto end;
    }

    expected = stackMemory.block + 12;
    value = MemoryArenaPushAligned(&stackMemory, 10, 4);
    if (value != expected) {
      errorCode = MEMORY_TEST_ERROR_MEM_PUSH_ALIGNED_EXPECTED_VALID_ADDRESS_2;
      goto end;
    }

    expected = stackMemory.block + 22;
    value = MemoryArenaPush(&stackMemory, 8);
    if (value != expected) {
      errorCode = MEMORY_TEST_ERROR_MEM_PUSH_EXPECTED_VALID_ADDRESS_1;
      goto end;
    }

    expected = stackMemory.block + 30;
    value = MemoryArenaPush(&stackMemory, 10);
    if (value != expected) {
      errorCode = MEMORY_TEST_ERROR_MEM_PUSH_EXPECTED_VALID_ADDRESS_2;
      goto end;
    }
  }

  bzero(stackMemory.block, stackMemory.used);
  MemoryTempEnd(&tempMemory);

end:
  return (int)errorCode;
}
