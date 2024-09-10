#include "mime_types.hpp"

namespace beauty {
namespace mime_types {

struct Mapping {
    const char *extension;
    const char *mime_type;
} mappings[] = {{"css", "text/css"},
                {"gif", "image/gif"},
                {"htm", "text/html"},
                {"html", "text/html"},
                {"jpg", "image/jpeg"},
                {"js", "application/javascript"},
                {"json", "application/json"},
                {"png", "image/png"}};

std::string extensionToType(const std::string &extension) {
    for (Mapping m : mappings) {
        if (m.extension == extension) {
            return m.mime_type;
        }
    }

    return "text/plain";
}

}  // namespace mime_types
}  // namespace beauty
