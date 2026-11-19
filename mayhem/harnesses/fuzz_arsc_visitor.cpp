// Fuzzes the resources.arsc parser in libresource the way tools/arsc/verbose_parse
// and redex's ApkResources read a table: arsc::ResourceTableVisitor walks the
// table/package/type/entry/string-pool chunks; android::ResStringPool parses
// a top-level string pool chunk.
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "androidfw/ResourceTypes.h"
#include "utils/Visitor.h"

// redex's callers (ApkResources, tools/arsc, tools/xml) hand visit() an mmap'd
// file and read the first ResChunk_header before checking the length, so a
// file shorter than that header reads the zero-filled tail of its page, never
// unmapped memory. Model that: the buffer holds at least one zeroed header, and
// the visitor is told the buffer's length (an input shorter than a header is
// seen as that many bytes plus zero padding, like the mapped page); every read
// past max(size, header) bytes still overflows the heap allocation.
static std::vector<uint8_t> as_mapped(const uint8_t* data, size_t size) {
  std::vector<uint8_t> buf(std::max(size, sizeof(android::ResChunk_header)), 0);
  if (size) memcpy(buf.data(), data, size);
  return buf;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // The visitors take a mutable void* (they can edit in place): parse a copy.
  std::vector<uint8_t> buf = as_mapped(data, size);
  {
    arsc::ResourceTableVisitor visitor;
    visitor.visit(buf.data(), buf.size());
  }
  {
    std::vector<uint8_t> copy(data, data + size);
    android::ResStringPool pool;
    if (pool.setTo(copy.data(), copy.size(), true) == android::NO_ERROR) {
      size_t n = pool.size();
      for (size_t i = 0; i < n && i < 4096; i++) {
        size_t len = 0;
        if (pool.isUTF8()) {
          (void)pool.string8At(i, &len);
        } else {
          (void)pool.stringAt(i, &len);
        }
        (void)pool.styleAt(i);
      }
    }
  }
  return 0;
}
