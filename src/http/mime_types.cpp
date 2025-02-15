#include "http/mime_types.h"
#include <unordered_map>
#include <algorithm>

// Extract extension from filepath, look up in table
std::string get_mime_type(const std::string& filepath) {
    static const std::unordered_map<std::string, std::string> mime_map = {
        // Text
        {".html", "text/html"},
        {".htm",  "text/html"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".xml",  "application/xml"},
        {".txt",  "text/plain"},
        {".csv",  "text/csv"},
        {".md",   "text/markdown"},

        // Images
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".webp", "image/webp"},
        {".bmp",  "image/bmp"},

        // Fonts
        {".woff",  "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf",   "font/ttf"},
        {".otf",   "font/otf"},
        {".eot",   "application/vnd.ms-fontobject"},

        // Video/Audio
        {".mp4",  "video/mp4"},
        {".webm", "video/webm"},
        {".mp3",  "audio/mpeg"},
        {".wav",  "audio/wav"},
        {".ogg",  "audio/ogg"},

        // Documents
        {".pdf",  "application/pdf"},
        {".zip",  "application/zip"},
        {".gz",   "application/gzip"},
        {".tar",  "application/x-tar"},

        // Web
        {".wasm", "application/wasm"},
        {".map",  "application/json"},
    };

    // Find last dot for extension
    size_t dot = filepath.find_last_of('.');
    if (dot == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = filepath.substr(dot);
    // Lowercase the extension
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    auto it = mime_map.find(ext);
    if (it != mime_map.end()) {
        return it->second;
    }

    return "application/octet-stream";
}
