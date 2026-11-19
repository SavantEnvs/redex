// Known-answer check of libresource's parsers on upstream's own test fixtures:
// prints what the arsc visitor and ResXMLTree saw; test.sh diffs it against a
// committed golden file.
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "androidfw/ResourceTypes.h"
#include "utils/String8.h"
#include "utils/Visitor.h"

static std::vector<uint8_t> slurp(const char* p) {
  std::ifstream f(p, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}

struct Counter : public arsc::ResourceTableVisitor {
  int packages = 0, type_specs = 0, types = 0, entries = 0, maps = 0;
  bool visit_package(android::ResTable_package* p) override {
    packages++;
    return arsc::ResourceTableVisitor::visit_package(p);
  }
  bool visit_type_spec(android::ResTable_package* p,
                       android::ResTable_typeSpec* s) override {
    type_specs++;
    return arsc::ResourceTableVisitor::visit_type_spec(p, s);
  }
  bool visit_type(android::ResTable_package* p, android::ResTable_typeSpec* s,
                  android::ResTable_type* t) override {
    types++;
    return arsc::ResourceTableVisitor::visit_type(p, s, t);
  }
  bool visit_entry(android::ResTable_package* p, android::ResTable_typeSpec* s,
                   android::ResTable_type* t, android::ResTable_entry* e,
                   android::Res_value* v) override {
    entries++;
    return arsc::ResourceTableVisitor::visit_entry(p, s, t, e, v);
  }
  bool visit_map_entry(android::ResTable_package* p,
                       android::ResTable_typeSpec* s, android::ResTable_type* t,
                       android::ResTable_map_entry* e) override {
    maps++;
    return arsc::ResourceTableVisitor::visit_map_entry(p, s, t, e);
  }
};

int main(int argc, char** argv) {
  for (int a = 1; a < argc; a++) {
    std::string path = argv[a];
    auto buf = slurp(argv[a]);
    std::string base = path.substr(path.find_last_of('/') + 1);
    if (path.size() > 5 && path.substr(path.size() - 5) == ".arsc") {
      Counter c;
      bool ok = c.visit(buf.data(), buf.size());
      std::cout << base << ": ok=" << ok << " packages=" << c.packages
                << " type_specs=" << c.type_specs << " types=" << c.types
                << " entries=" << c.entries << " maps=" << c.maps << "\n";
    } else {
      android::ResXMLTree tree;
      if (tree.setTo(buf.data(), buf.size(), true) != android::NO_ERROR) {
        std::cout << base << ": setTo failed\n";
        continue;
      }
      std::cout << base << ":";
      android::ResXMLParser::event_code_t code;
      size_t len;
      while ((code = tree.next()) != android::ResXMLParser::END_DOCUMENT &&
             code != android::ResXMLParser::BAD_DOCUMENT) {
        if (code == android::ResXMLParser::START_TAG) {
          const char16_t* n = tree.getElementName(&len);
          std::cout << " <" << android::String8(n, len).c_str() << " attrs="
                    << tree.getAttributeCount();
          for (size_t i = 0; i < tree.getAttributeCount(); i++) {
            const char16_t* an = tree.getAttributeName(i, &len);
            if (an) std::cout << " " << android::String8(an, len).c_str();
          }
          std::cout << ">";
        }
      }
      std::cout << (code == android::ResXMLParser::END_DOCUMENT ? " END" : " BAD")
                << "\n";
    }
  }
  return 0;
}
