#pragma once

#include <string>

// Map file extension to MIME content type.
// Returns "application/octet-stream" for unknown extensions.
std::string get_mime_type(const std::string& filepath);
