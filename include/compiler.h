#pragma once

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define swap(x, y)                                                                                                     \
  {                                                                                                                    \
    __typeof__(x) temp = x;                                                                                            \
    x = y;                                                                                                             \
    y = temp;                                                                                                          \
  }
