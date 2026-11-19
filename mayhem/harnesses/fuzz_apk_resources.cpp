// Fuzzes redex's resources.arsc model (libredex/ApkResources.cpp) the way
// redex-all opens an unpacked APK's resource table: ResourcesArscFile(path)
// maps the file, builds the TableSnapshot (TableParser over libresource),
// parses the global / key / type string pools and resolves every resource id
// to its name; the table is then queried like the resource passes do.
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include "ApkResources.h"
#include "RedexContext.h"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  bool ok;
  const std::string& path = stage_input("resources", ".arsc", data, size, &ok);
  if (!ok) return 0;
  g_redex = new RedexContext();
  try {
    ResourcesArscFile arsc(path);
    std::vector<std::string> types;
    arsc.get_type_names(&types);
    (void)arsc.package_count();
    (void)arsc.get_overlayable_id_roots();
    for (uint32_t id : arsc.sorted_res_ids) {
      (void)arsc.resource_value_count(id);
      (void)arsc.get_configs_with_values(id);
    }
  } catch (const std::exception&) {
  }
  delete g_redex;
  g_redex = nullptr;
  return 0;
}
