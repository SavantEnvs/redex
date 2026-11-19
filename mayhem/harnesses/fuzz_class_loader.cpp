// Fuzzes redex's Java .class parser (libredex/JarLoader.cpp) directly, without
// the zip container: load_class_file() reads the file and runs parse_class()
// (constant pool, fields, methods, attributes). A fresh RedexContext per input
// keeps every input independent.
#include <cstdint>
#include <exception>

#include "DexClass.h"
#include "JarLoader.h"
#include "RedexContext.h"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bool ok;
  const std::string& path = stage_input("class", ".class", data, size, &ok);
  if (!ok) return 0;
  g_redex = new RedexContext();
  try {
    Scope classes;
    (void)load_class_file(path, &classes);
  } catch (const std::exception&) {
  }
  delete g_redex;
  g_redex = nullptr;
  return 0;
}
