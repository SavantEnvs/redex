// Fuzzes redex's ProGuard keep-rule parser (libredex/ProguardLexer.cpp,
// ProguardParser.cpp), which reads the -P/--proguard-config files given to
// redex: keep_rules::proguard_parser::parse() over an istream of the input.
#include <cstdint>
#include <exception>
#include <sstream>
#include <string>

#include "ProguardConfiguration.h"
#include "ProguardParser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::istringstream in(std::string(reinterpret_cast<const char*>(data), size));
  keep_rules::ProguardConfiguration config;
  try {
    (void)keep_rules::proguard_parser::parse(in, &config, "fuzz.pro");
  } catch (const std::exception&) {
  }
  return 0;
}
