#include <algorithm>
#include <unordered_map>

#include "beauty/mime_types.hpp"

namespace beauty {
namespace mime_types {

const std::unordered_map<std::string, std::string> mime_map = {
    {"css", "text/css"},
    {"gif", "image/gif"},
    {"htm", "text/html"},
    {"html", "text/html"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"js", "application/javascript"},
    {"json", "application/json"},
    {"png", "image/png"},
    {"bmp", "image/bmp"},
    {"webp", "image/webp"},
    {"ico", "image/x-icon"},
    {"svg", "image/svg+xml"},
    {"txt", "text/plain"},
    {"csv", "text/csv"},
    {"md", "text/markdown"},
    {"xml", "application/xml"},
    {"pdf", "application/pdf"},
    {"zip", "application/zip"},
    {"gz", "application/gzip"},
    {"tar", "application/x-tar"},
    {"rtf", "application/rtf"},
    {"mp3", "audio/mpeg"},
    {"m4a", "audio/mp4"},
    {"mp4", "video/mp4"},
    {"mpeg", "video/mpeg"},
    {"avi", "video/x-msvideo"},
    {"mov", "video/quicktime"},
    {"webm", "video/webm"},
    {"ogg", "application/ogg"},
    {"ogv", "video/ogg"},
    {"wav", "audio/wav"},
    {"flac", "audio/flac"},
    {"woff", "font/woff"},
    {"woff2", "font/woff2"},
    {"ttf", "font/ttf"},
    {"eot", "application/vnd.ms-fontobject"},
    {"wasm", "application/wasm"},
    {"sh", "application/x-sh"},
    {"c", "text/x-c"},
    {"cpp", "text/x-c"},
    {"h", "text/x-c"},
    {"hpp", "text/x-c"},
    {"py", "text/x-python"},
    {"ts", "application/typescript"},
    {"jsx", "text/jsx"},
    {"tsx", "text/tsx"},
    {"yaml", "text/yaml"},
    {"yml", "text/yaml"},
    {"apk", "application/vnd.android.package-archive"},
    {"3gp", "video/3gpp"}};

std::string extensionToType(const std::string &extension) {
    std::string ext = extension;
    std::transform(
        ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });

    auto it = mime_map.find(ext);
    if (it != mime_map.end()) {
        return it->second;
    }
    return "text/plain";
}

}  // namespace mime_types
}  // namespace beauty
