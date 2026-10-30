#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>

namespace routine::sim {
// Stands in for the ESP32 USB Serial console. Every line is kept so the web
// page can show it, and is also echoed to the host's stdout.
class Console {
 public:
  void printf(const char* format,...) __attribute__((format(printf,2,3))) {
    char line[1024];va_list args;va_start(args,format);
    const int n=std::vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(n>0)write(line);
  }
  void println(const char* text){write(text);write("\n");}
  void write(const char* text){
    std::fputs(text,stdout);std::fflush(stdout);
    log_+=text;total_+=std::char_traits<char>::length(text);
    if(log_.size()>kKeep)log_.erase(0,log_.size()-kKeep);
  }
  // Text written after `since` (a value previously returned by total()).
  std::string since(size_t since) const {
    if(since>=total_)return {};
    const size_t wanted=total_-since;
    return wanted>=log_.size()?log_:log_.substr(log_.size()-wanted);
  }
  size_t total() const { return total_; }
 private:
  static constexpr size_t kKeep=64*1024;
  std::string log_;
  size_t total_=0;
};
}
