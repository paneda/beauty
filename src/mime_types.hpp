#pragma once

#include <string>

namespace beauty {
namespace mime_types {

/// Convert a file extension into a MIME type.
std::string extensionToType(const std::string &extension);

}  // namespace mime_types
}  // namespace beauty
