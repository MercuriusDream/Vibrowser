#include <clever/url/url.h>
#include <algorithm>
#include <string>

namespace clever::url {

bool URL::is_special() const {
    return scheme == "http" || scheme == "https" ||
           scheme == "ftp" || scheme == "ws" ||
           scheme == "wss" || scheme == "file";
}

std::string URL::serialize() const {
    std::string result;
    result += scheme;
    result += ':';

    // If we have a host (or it's a special scheme), we include the authority
    if (!host.empty() || scheme == "file") {
        result += "//";

        if (!username.empty() || !password.empty()) {
            result += username;
            if (!password.empty()) {
                result += ':';
                result += password;
            }
            result += '@';
        }

        result += host;

        if (port.has_value()) {
            result += ':';
            result += std::to_string(port.value());
        }
    } else if (is_special()) {
        // Special schemes always get //
        result += "//";
    }

    result += path;

    if (!query.empty()) {
        result += '?';
        result += query;
    }

    if (!fragment.empty()) {
        result += '#';
        result += fragment;
    }

    return result;
}

std::string URL::origin() const {
    // For special schemes (except file), origin is scheme://host[:port]
    // For file and non-special, origin is opaque ("null")
    if (scheme == "file") {
        return "null";
    }

    if (!is_special()) {
        return "null";
    }

    std::string result;
    result += scheme;
    result += "://";
    result += host;

    if (port.has_value()) {
        result += ':';
        result += std::to_string(port.value());
    }

    return result;
}

bool urls_same_origin(const URL& a, const URL& b) {
    if (a.scheme != b.scheme) return false;
    if (a.host != b.host) return false;
    if (a.port != b.port) return false;
    return true;
}

} // namespace clever::url
