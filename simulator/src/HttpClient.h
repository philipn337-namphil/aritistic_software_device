#pragma once
#include <cstddef>
#include <string>

namespace routine::sim {
// Blocking HTTP(S) POST used from the API worker thread (stands in for the
// ESP32 HTTPClient). Returns the HTTP status, or the firmware's negative codes:
// -1 connect/request failure, -2 body larger than maxBytes, -3 read failure.
int httpPost(const std::string& url,const std::string& extraHeaders,const std::string& body,
             std::string& response,size_t maxBytes);
}
