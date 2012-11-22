#pragma once

#include <cstdint>
#include <cstring>

inline uint64_t readfixnum(const void* buf, size_t width) 
{
  uint64_t num = 0;
  std::memcpy(&num, buf, width);
  return be64toh(num) >> ((sizeof(num) - width) * 8);
}

inline void writefixnum(void* buf, uint64_t num, size_t width) 
{
  num = htobe64(num);
  std::memcpy(buf, (const char*)&num + sizeof(num) - width, width);
}

template <typename T>
inline T atoi (const char *s, int off, int len) 
{
  T n = 0;
  s += off;
  const char *t = s + len;
  for (; s != t; ++s) {
    if (*s != ' ')
      n = n * 10 + *s - '0';
  }
  return n;
}

template <typename T, char pad = ' '>
inline void itoa (char *s, int w, T n)
{
  char *t = s + w;
  do {
    *--t = n % 10 + '0';
  } while ((n /= 10) > 0 && t > s);
  do {
    *--t = pad;
  } while (t > s);      
}
