// Fuzzes redex's .jar reader (libredex/JarLoader.cpp) the way redex-all loads
// library jars (-j): load_jar_file() maps the file, finds the zip central
// directory, inflates or copies each .class entry and parses it with
// parse_class() (constant pool, fields, methods, attributes). A fresh
// RedexContext per input keeps every input independent (classes, types and
// strings are interned there).
#include <cstdint>
#include <exception>

#include "DexClass.h"
#include "JarLoader.h"
#include "RedexContext.h"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bool ok;
  const std::string& path = stage_input("jar", ".jar", data, size, &ok);
  if (!ok) return 0;
  g_redex = new RedexContext();
  try {
    Scope classes;
    const DexLocation* loc = DexLocation::make_location("", path);
    // Malformed input is reported by throwing (always_assert -> RedexException).
    (void)load_jar_file(loc, &classes);
  } catch (const std::exception&) {
  }
  delete g_redex;
  g_redex = nullptr;
  return 0;
}
