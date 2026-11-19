// Fuzzes redex's line-map reader (tools/debug-info/PositionMap.cpp), the file
// linemapdump and symbolicate-trace read: read_map() parses the map, then every
// position is printed the way linemapdump does and its inline stack resolved
// the way symbolicate-trace does with get_stack().
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "PositionMap.h"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // read_map() takes a path (it mmaps the file): stage the input in a
  // per-process scratch file in the current directory.
  bool ok;
  const std::string& path = stage_input("linemap", ".map", data, size, &ok);
  if (!ok) return 0;

  auto map = read_map(path.c_str());
  if (map != nullptr) {
    size_t total = 0;
    for (size_t i = 0; i < map->positions_size; ++i) {
      auto pi = map->positions[i];
      total += map->string_pool[pi.class_id].size() +
               map->string_pool[pi.method_id].size() +
               map->string_pool[pi.file_id].size() + pi.line;
      auto stack = get_stack(*map, (int64_t)i);
      for (const auto& p : stack) total += p.cls.size() + p.line;
    }
    if (total == 0xdeadbeef) fputs("", stderr);
  }
  return 0;
}
