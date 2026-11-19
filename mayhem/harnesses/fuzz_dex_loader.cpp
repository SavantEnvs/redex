// Fuzzes redex's .dex reader the way redex-all loads an APK's classes.dex:
// load_classes_from_dex() parses the header, map list and id tables (DexIdx),
// classes, annotations, encoded values and debug info, and balloons each
// method's code into IR. A fresh RedexContext per input keeps every input
// independent (types, strings and locations are interned there).
#include <cstdint>
#include <cstring>
#include <exception>

#include "DexClass.h"
#include "DexLoader.h"
#include "RedexContext.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  g_redex = new RedexContext();
  try {
    // DataUPtr owns the buffer; the loader keeps it alive while it parses.
    uint8_t* copy = new uint8_t[size ? size : 1];
    if (size) memcpy(copy, data, size);
    DexLoader::DataUPtr buf(copy, [](const uint8_t* p) { delete[] p; });
    const DexLocation* loc = DexLocation::make_location("dex", "fuzz.dex");
    // Malformed input is reported by throwing (always_assert -> RedexException).
    (void)load_classes_from_dex(std::move(buf), size, loc, /*balloon=*/true,
                                /*throw_on_balloon_error=*/true,
                                /*support_dex_version=*/39,
                                DexLoader::Parallel::kNo);
  } catch (const std::exception&) {
  }
  delete g_redex;
  g_redex = nullptr;
  return 0;
}
