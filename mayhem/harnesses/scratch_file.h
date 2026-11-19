// Stages a fuzz input as a per-process scratch file in the current directory
// (Mayhem's default cwd is writable) for entry points that take a path.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <unistd.h>

static inline const std::string& stage_input(const char* stem, const char* ext,
                                             const uint8_t* data, size_t size,
                                             bool* ok) {
  static std::string path;
  if (path.empty()) {
    path = std::string("./") + stem + "_fuzz_" + std::to_string(getpid()) + ext;
  }
  FILE* f = fopen(path.c_str(), "wb");
  *ok = f != nullptr;
  if (f) {
    if (size) fwrite(data, 1, size, f);
    fclose(f);
  }
  return path;
}
