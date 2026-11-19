// Fuzzes redex's ProGuard/R8 mapping-file reader (libredex/ProguardMap.cpp),
// the file passed to redex as -Smapping / -m: ProguardMap(std::istream&)
// parses class, field and method lines, then the map is queried both ways the
// way redex's renamers and deobfuscation do.
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

#include "ProguardMap.h"
#include "RedexContext.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string text(reinterpret_cast<const char*>(data), size);
  g_redex = new RedexContext();
  try {
    std::istringstream in(text);
    ProguardMap map(in);
    (void)map.empty();
    (void)map.translate_class("Lcom/foo/Bar;");
    (void)map.deobfuscate_class("La;");
    (void)map.deobfuscate_method("La;.a()V");
    (void)map.deobfuscate_field("La;.a:I");
  } catch (const std::exception&) {
  }
  delete g_redex;
  g_redex = nullptr;
  return 0;
}
