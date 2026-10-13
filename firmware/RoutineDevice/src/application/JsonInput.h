#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\JsonInput.h"
#pragma once
#include "../vendor/ArduinoJson.h"
#include <cstring>

namespace routine::api {
// ArduinoJson intentionally stops at the first complete object. HTTP/config
// bodies must contain exactly one document, including any trailing bytes.
class JsonInput {
 public:
  JsonInput(const char* data,size_t size):data_(data),size_(size){}
  int read(){return at_<size_?static_cast<unsigned char>(data_[at_++]):-1;}
  size_t readBytes(char* destination,size_t count){size_t n=0;while(n<count&&at_<size_)destination[n++]=data_[at_++];return n;}
  bool finished()const {
    for(size_t i=at_;i<size_;++i)if(data_[i]!=' '&&data_[i]!='\r'&&data_[i]!='\n'&&data_[i]!='\t')return false;
    return true;
  }
 private:
  const char* data_;size_t size_,at_=0;
};
inline bool readJson(JsonDocument& doc,const char* bytes,size_t size,uint8_t nesting){
  if(!bytes||!size||std::memchr(bytes,0,size))return false;
  JsonInput input(bytes,size);
  return !deserializeJson(doc,input,DeserializationOption::NestingLimit(nesting))&&input.finished();
}
// C-string consumers must not silently accept "DONE\u0000other" or a UUID
// prefix. Check JSON's length before passing text into protocol validators.
inline const char* jsonText(JsonVariantConst value){
  if(!value.is<const char*>())return nullptr;
  const auto text=value.as<JsonString>();
  return std::strlen(text.c_str())==text.size()?text.c_str():nullptr;
}
}
