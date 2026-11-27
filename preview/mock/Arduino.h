#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
inline long random(long n) { return rand() % n; }
template <class T, class A, class B> T constrain(T v, A lo, B hi) { return v < lo ? lo : v > hi ? hi : v; }
#if !(defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 38)))   // newer glibc has its own
inline size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
#endif
