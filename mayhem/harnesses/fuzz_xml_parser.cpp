// Fuzzes binary Android XML (AndroidManifest.xml, layouts from an APK) as
// redex reads it: arsc::XmlFileVisitor (tools/xml, ApkResources) and
// android::ResXMLTree, walked to the end of the document.
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
  {
    std::vector<uint8_t> buf = as_mapped(data, size);
    arsc::XmlFileVisitor visitor;
    visitor.visit(buf.data(), buf.size());
  }
  {
    std::vector<uint8_t> buf = as_mapped(data, size);
    android::ResXMLTree tree;
    if (tree.setTo(buf.data(), size, false) != android::NO_ERROR) {
      return 0;
    }
    size_t len;
    android::ResXMLParser::event_code_t code;
    size_t events = 0;
    while ((code = tree.next()) != android::ResXMLParser::END_DOCUMENT &&
           code != android::ResXMLParser::BAD_DOCUMENT && events++ < 100000) {
      if (code == android::ResXMLParser::START_TAG) {
        (void)tree.getElementName(&len);
        (void)tree.getElementNamespace(&len);
        size_t n = tree.getAttributeCount();
        for (size_t i = 0; i < n; i++) {
          (void)tree.getAttributeName(i, &len);
          (void)tree.getAttributeNamespace(i, &len);
          (void)tree.getAttributeStringValue(i, &len);
          (void)tree.getAttributeNameResID(i);
          android::Res_value v;
          (void)tree.getAttributeValue(i, &v);
        }
      } else if (code == android::ResXMLParser::TEXT) {
        (void)tree.getText(&len);
      } else if (code == android::ResXMLParser::START_NAMESPACE) {
        (void)tree.getNamespacePrefix(&len);
        (void)tree.getNamespaceUri(&len);
      }
    }
  }
  return 0;
}
