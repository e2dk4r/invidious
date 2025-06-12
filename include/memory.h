#pragma once

#include "assert.h"
#include "math.h"
#include "type.h"

struct memory_arena {
  u8 *block;
  u64 used;
  u64 total;
};

typedef struct memory_arena memory_arena;

struct memory_temp {
  memory_arena *arena;
  u64 startedAt;
};

typedef struct memory_temp memory_temp;

static memory_arena
MemoryArenaSub(memory_arena *master, u64 size)
{
  debug_assert(master->used + size <= master->total);

  memory_arena sub = (struct memory_arena){
      .total = size,
      .block = master->block + master->used,
  };

  master->used += size;
  return sub;
}

static void *
MemoryArenaPush(memory_arena *mem, u64 size)
{
  debug_assert(mem->used + size <= mem->total);
  u8 *result = mem->block + mem->used;
  mem->used += size;
  return result;
}

static void *
MemoryArenaPushAligned(memory_arena *mem, u64 size, u64 alignment)
{
  debug_assert(IsPowerOfTwo(alignment));

  u8 *block = mem->block + mem->used;

  u64 alignmentMask = alignment - 1;
  u64 alignmentResult = ((u64)block & alignmentMask);
  if (alignmentResult != 0) {
    // if it is not aligned
    u64 alignmentOffset = alignment - alignmentResult;
    size += alignmentOffset;
    block += alignmentOffset;
  }

  debug_assert(mem->used + size <= mem->total);
  mem->used += size;

  return block;
}

static memory_temp
MemoryTempBegin(memory_arena *arena)
{
  return (memory_temp){
      .arena = arena,
      .startedAt = arena->used,
  };
}

static void
MemoryTempEnd(memory_temp *tempMemory)
{
  memory_arena *arena = tempMemory->arena;
  arena->used = tempMemory->startedAt;
}

static void
MemoryCopy(void *dest, void *src, u64 length)
{
  __builtin_memcpy(dest, src, length);
}

static void
MemoryClear(void *dest, u64 length)
{
  __builtin_bzero(dest, length);
}

#define __cleanup_memory_temp__ __attribute__((cleanup(MemoryTempEnd)))
