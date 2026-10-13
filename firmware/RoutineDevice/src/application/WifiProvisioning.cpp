#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\WifiProvisioning.cpp"
#include "WifiProvisioning.h"
#include "JsonInput.h"
#include <cstring>

namespace routine::provisioning {
void erase(void* data,size_t size){auto* p=static_cast<volatile unsigned char*>(data);while(size--)*p++=0;}
namespace {
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool unicodeEscapes(const char* bytes,size_t size){
  for(size_t i=0;i<size;++i){
    if(bytes[i]!='\\')continue;
    if(++i==size)return false;
    if(bytes[i]!='u')continue;
    if(size-i<=4)return false;
    unsigned value=0;for(unsigned n=0;n<4;++n){const int digit=hex(bytes[++i]);if(digit<0)return false;value=value*16+unsigned(digit);}
    if(value>=0xdc00&&value<=0xdfff)return false;
    if(value>=0xd800&&value<=0xdbff){
      if(size-i<=6||bytes[i+1]!='\\'||bytes[i+2]!='u')return false;
      i+=2;value=0;for(unsigned n=0;n<4;++n){const int digit=hex(bytes[++i]);if(digit<0)return false;value=value*16+unsigned(digit);}
      if(value<0xdc00||value>0xdfff)return false;
    }
  }
  return true;
}
bool utf8(const char* text){
  const auto* p=reinterpret_cast<const unsigned char*>(text);
  while(*p){
    uint32_t cp=*p++;unsigned remaining=0;uint32_t minimum=0;
    if(cp<0x80){if(cp<0x20||cp==0x7f)return false;continue;}
    if(cp>=0xc2&&cp<=0xdf){cp&=0x1f;remaining=1;minimum=0x80;}
    else if(cp>=0xe0&&cp<=0xef){cp&=0xf;remaining=2;minimum=0x800;}
    else if(cp>=0xf0&&cp<=0xf4){cp&=7;remaining=3;minimum=0x10000;}
    else return false;
    while(remaining--){if((*p&0xc0)!=0x80)return false;cp=(cp<<6)|(*p++&0x3f);}
    if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
  }
  return true;
}
bool privateIpv4(const char* first,const char* last){
  unsigned parts[4]{};
  for(unsigned i=0;i<4;++i){
    const char* start=first;if(first==last)return false;
    while(first!=last&&*first>='0'&&*first<='9'){parts[i]=parts[i]*10+unsigned(*first++-'0');if(parts[i]>255||first-start>3)return false;}
    if(first==start||(first-start>1&&*start=='0'))return false;
    if(i<3){if(first==last||*first++!='.')return false;}else if(first!=last)return false;
  }
  return parts[0]==10||(parts[0]==172&&parts[1]>=16&&parts[1]<=31)||(parts[0]==192&&parts[1]==168);
}
}
bool validServerUrl(const char* url,bool allowLocalHttp){
  if(!url||std::strlen(url)>180)return false;
  const bool https=std::strncmp(url,"https://",8)==0;
  const bool http=std::strncmp(url,"http://",7)==0;
  if(!https&&!(allowLocalHttp&&http))return false;
  const char* host=url+(https?8:7);const char* end=host+std::strlen(host);
  if(end!=host&&end[-1]=='/')--end;
  if(end==host)return false;
  const char* colon=nullptr;
  for(const char* p=host;p!=end;++p){
    const unsigned char c=*p;
    if(c==':'){if(colon)return false;colon=p;}
    else if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))return false;
  }
  const char* hostEnd=colon?colon:end;
  if(hostEnd==host||*host=='.'||*host=='-'||hostEnd[-1]=='.'||hostEnd[-1]=='-')return false;
  if(colon){unsigned port=0;if(colon+1==end||end-colon>6)return false;for(const char* p=colon+1;p!=end;++p){if(*p<'0'||*p>'9')return false;port=port*10+unsigned(*p-'0');}if(!port||port>65535)return false;}
  return https||privateIpv4(host,hostEnd);
}
bool parseConfig(const char* bytes,size_t size,Config& out,bool allowLocalHttp){
  out.clear();if(!bytes||size>Frame::kCapacity||!unicodeEscapes(bytes,size))return false;
  JsonDocument doc;if(!api::readJson(doc,bytes,size,2)||!doc.is<JsonObject>())return false;
  const char* ssid=api::jsonText(doc["ssid"]);const char* password=api::jsonText(doc["password"]);
  const char* code=api::jsonText(doc["pairingCode"]);const char* url=api::jsonText(doc["serverUrl"]);
  if(!ssid||!*ssid||std::strlen(ssid)>32||!utf8(ssid)||!password||!code||std::strlen(code)!=10||!validServerUrl(url,allowLocalHttp))return false;
  const size_t n=std::strlen(password);if(n&&(n<8||n>63))return false;
  for(size_t i=0;i<n;++i)if(static_cast<unsigned char>(password[i])<32||static_cast<unsigned char>(password[i])>126)return false;
  for(unsigned i=0;i<10;++i)if(code[i]<'0'||code[i]>'9')return false;
  std::strcpy(out.ssid,ssid);std::strcpy(out.password,password);std::strcpy(out.pairingCode,code);std::strcpy(out.serverUrl,url);
  const size_t length=std::strlen(out.serverUrl);if(out.serverUrl[length-1]=='/')out.serverUrl[length-1]=0;
  return true;
}
void Frame::clear(){erase(bytes_,sizeof(bytes_));size_=0;lastAt_=0;dropping_=complete_=false;}
bool Frame::expire(uint32_t now){if((size_||dropping_)&&uint32_t(now-lastAt_)>=kTimeoutMs){clear();return true;}return false;}
FrameResult Frame::push(const uint8_t* data,size_t size,uint32_t now){
  if(complete_||!data||!size||size>20)return FrameResult::Invalid;
  if(expire(now))return FrameResult::Invalid;
  lastAt_=now;bool invalid=false;
  for(size_t i=0;i<size;++i){
    const uint8_t c=data[i];
    if(dropping_){if(c=='\n')clear();invalid=true;continue;}
    if(c=='\n'){
      if(!size_||i+1!=size){clear();return FrameResult::Invalid;}
      complete_=true;return invalid?FrameResult::Invalid:FrameResult::Complete;
    }
    if(!c||size_==kCapacity){clear();dropping_=true;lastAt_=now;invalid=true;continue;}
    bytes_[size_++]=char(c);
  }
  return invalid?FrameResult::Invalid:FrameResult::Waiting;
}
bool Join::start(WifiPort& port,const Config& config,uint32_t now){
  if(state_==JoinState::Connecting)return false;
  beganAt_=now;state_=JoinState::Connecting;
  if(!port.tryCandidate(config.ssid,config.password)){port.restorePrevious();state_=JoinState::Failed;return false;}return true;
}
void Join::tick(WifiPort& port,uint32_t now){
  if(state_!=JoinState::Connecting)return;
  if(port.hasCandidateIp()){
    if(port.persistCandidate()){state_=JoinState::Committed;return;}
    port.restorePrevious();state_=JoinState::Failed;
  }else if(uint32_t(now-beganAt_)>=kTimeoutMs){port.restorePrevious();state_=JoinState::Failed;}
}
void Join::cancel(WifiPort& port){if(state_==JoinState::Connecting)port.restorePrevious();state_=JoinState::Idle;}
}
