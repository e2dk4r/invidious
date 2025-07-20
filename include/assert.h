#pragma once

static inline void
runtime_assert(int condition)
{
  if (condition)
    return;

#if defined(__has_builtin) && __has_builtin(__builtin_debugtrap)
  __builtin_debugtrap();
#elif defined(_MSC_VER)
  __debugbreak();
#else
  __asm__("int3; nop");
#endif
}

static inline void
debug_assert(int condition)
{
#if IS_BUILD_DEBUG
  runtime_assert(condition);
#endif
}

static inline void
breakpoint(void)
{
  debug_assert(0);
}

#define static_assert(expression)                                                                                      \
  do {                                                                                                                 \
    enum { STATIC_ASSERT__ = 1 / (expression) };                                                                       \
  } while (0)
