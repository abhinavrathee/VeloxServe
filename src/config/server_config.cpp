#include "config/server_config.h"
#include <algorithm>

bool LocationConfig::is_method_allowed(const std::string& method) const {
    if (methods.empty()) return true;  // empty = all methods allowed
    return std::find(methods.begin(), methods.end(), method) != methods.end();
}
