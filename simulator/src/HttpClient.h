#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace routine::sim {
// Asynchronous HTTP(S) POST standing in for the ESP32 HTTPClient worker task:
// start() once, then poll done() from the main loop. On the PC it runs WinHTTP
// on a thread; in the browser (WebAssembly) it uses fetch.
// status(): the HTTP status, or the firmware's negative codes —
// -1 connect/request failure, -2 body larger than maxBytes, -3 read failure.
class HttpRequest {
 public:
  HttpRequest();
  ~HttpRequest();
  HttpRequest(const HttpRequest&) = delete;
  HttpRequest& operator=(const HttpRequest&) = delete;
  void start(const std::string& url,const std::string& extraHeaders,const std::string& body,size_t maxBytes);
  bool done() const;
  int status() const;
  std::string take();   // response body; valid once done()
  struct State;
 private:
  std::shared_ptr<State> state_;
};
}
