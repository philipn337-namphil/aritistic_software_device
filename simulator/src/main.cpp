// Virtual device host: runs the firmware's product logic in SimRuntime and
// serves the web page plus a small HTTP API on one thread (no locking needed).
// PC only; the browser build uses wasm_main.cpp instead.
#ifndef __EMSCRIPTEN__
#include "SimRuntime.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>

namespace {
struct Request { std::string method,path,query,body; };
struct Options { std::string host="127.0.0.1",data="simulator/data",pak="simulator/assets/ui.pak",web="simulator/web";int port=8080; };

bool readRequest(SOCKET client,Request& request){
  std::string raw;char buffer[8192];size_t headerEnd=std::string::npos,length=0;
  while(true){
    const int n=recv(client,buffer,sizeof(buffer),0);if(n<=0)return false;
    raw.append(buffer,size_t(n));
    if(headerEnd==std::string::npos){
      headerEnd=raw.find("\r\n\r\n");
      if(headerEnd==std::string::npos){if(raw.size()>65536)return false;continue;}
      std::string lower=raw.substr(0,headerEnd);for(auto& c:lower)c=char(tolower(c));
      const auto at=lower.find("content-length:");
      if(at!=std::string::npos)length=size_t(std::strtoul(lower.c_str()+at+15,nullptr,10));
      if(length>1<<20)return false;
    }
    if(raw.size()>=headerEnd+4+length)break;
  }
  const auto firstSpace=raw.find(' '),secondSpace=raw.find(' ',firstSpace+1);
  if(firstSpace==std::string::npos||secondSpace==std::string::npos)return false;
  request.method=raw.substr(0,firstSpace);
  std::string target=raw.substr(firstSpace+1,secondSpace-firstSpace-1);
  const auto q=target.find('?');
  request.path=target.substr(0,q);request.query=q==std::string::npos?"":target.substr(q+1);
  request.body=raw.substr(headerEnd+4,length);
  return true;
}
void sendAll(SOCKET client,const char* data,size_t size){
  while(size){const int n=send(client,data,int(std::min<size_t>(size,1<<16)),0);if(n<=0)return;data+=n;size-=size_t(n);}
}
void respond(SOCKET client,int status,const char* type,const std::string& body,const std::string& headers=""){
  const char* reason=status==200?"OK":status==204?"No Content":status==404?"Not Found":"Bad Request";
  std::string head="HTTP/1.1 "+std::to_string(status)+" "+reason+"\r\nContent-Type: "+type+"\r\nContent-Length: "+std::to_string(body.size())
    +"\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Expose-Headers: X-Frame, X-Screen-On, X-Console-Total\r\nConnection: close\r\n"+headers+"\r\n";
  sendAll(client,head.data(),head.size());sendAll(client,body.data(),body.size());
}
std::string queryValue(const std::string& query,const char* key){
  const std::string prefix=std::string(key)+"=";size_t at=0;
  while(at<query.size()){
    const auto end=query.find('&',at);const auto part=query.substr(at,end==std::string::npos?std::string::npos:end-at);
    if(!part.compare(0,prefix.size(),prefix))return part.substr(prefix.size());
    if(end==std::string::npos)break;at=end+1;
  }
  return {};
}
const char* contentType(const std::string& path){
  auto ends=[&](const char* suffix){const size_t n=std::strlen(suffix);return path.size()>=n&&!path.compare(path.size()-n,n,suffix);};
  if(ends(".html"))return "text/html; charset=utf-8";
  if(ends(".js"))return "text/javascript; charset=utf-8";
  if(ends(".css"))return "text/css; charset=utf-8";
  if(ends(".svg"))return "image/svg+xml";
  if(ends(".png"))return "image/png";
  return "application/octet-stream";
}
void handle(SOCKET client,routine::sim::SimRuntime& device,const Options& options){
  Request request;if(!readRequest(client,request))return;
  if(request.method=="OPTIONS"){respond(client,204,"text/plain","","Access-Control-Allow-Methods: GET, POST\r\nAccess-Control-Allow-Headers: Content-Type\r\n");return;}
  if(request.path=="/api/frame"){
    const auto since=std::strtoul(queryValue(request.query,"since").c_str(),nullptr,10);
    const std::string headers="X-Frame: "+std::to_string(device.frames())+"\r\nX-Screen-On: "+(device.screenOn()?"1":"0")+"\r\n";
    if(since==device.frames()&&queryValue(request.query,"force").empty()){respond(client,204,"application/octet-stream","",headers);return;}
    respond(client,200,"application/octet-stream",std::string(reinterpret_cast<const char*>(device.pixels()),240*320*2),headers);return;
  }
  if(request.path=="/api/status"){respond(client,200,"application/json",device.statusJson());return;}
  if(request.path=="/api/console"){
    const size_t since=size_t(std::strtoull(queryValue(request.query,"since").c_str(),nullptr,10));
    respond(client,200,"text/plain; charset=utf-8",device.console().since(since),"X-Console-Total: "+std::to_string(device.console().total())+"\r\n");return;
  }
  if(request.path=="/api/buttons"&&request.method=="POST"){
    device.buttons(uint8_t(std::strtoul(request.body.c_str(),nullptr,10)&7));respond(client,200,"text/plain","OK");return;
  }
  if(request.path=="/api/command"&&request.method=="POST"){
    std::string line=request.body;while(!line.empty()&&(line.back()=='\n'||line.back()=='\r'))line.pop_back();
    if(line.empty()||line.size()>223||line.find('\n')!=std::string::npos){respond(client,400,"text/plain","ERR line\n");return;}
    const size_t before=device.console().total();device.command(line.c_str());
    respond(client,200,"text/plain; charset=utf-8",device.console().since(before));return;
  }
  if(request.method!="GET"||request.path.find("..")!=std::string::npos){respond(client,404,"text/plain","not found");return;}
  const std::string file=options.web+(request.path=="/"?"/index.html":request.path);
  std::ifstream in(file,std::ios::binary);
  if(!in){respond(client,404,"text/plain","not found");return;}
  respond(client,200,contentType(file),std::string(std::istreambuf_iterator<char>(in),{}));
}
}

int main(int argc,char** argv){
  SetConsoleOutputCP(CP_UTF8);
  Options options;
  for(int i=1;i+1<argc;i+=2){
    const std::string key=argv[i],value=argv[i+1];
    if(key=="--port")options.port=std::atoi(value.c_str());
    else if(key=="--host")options.host=value;
    else if(key=="--data")options.data=value;
    else if(key=="--pak")options.pak=value;
    else if(key=="--web")options.web=value;
  }
  CreateDirectoryA(options.data.c_str(),nullptr);
  WSADATA wsa;if(WSAStartup(MAKEWORD(2,2),&wsa)){std::puts("FATAL winsock");return 1;}
  SOCKET listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  BOOL reuse=TRUE;setsockopt(listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&reuse),sizeof(reuse));
  sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(uint16_t(options.port));
  inet_pton(AF_INET,options.host.c_str(),&address.sin_addr);
  if(bind(listener,reinterpret_cast<sockaddr*>(&address),sizeof(address))||listen(listener,32)){std::printf("FATAL cannot listen on %s:%d\n",options.host.c_str(),options.port);return 1;}
  // ProductController/SnapshotStore hold several large snapshots: keep them off the stack.
  auto device=std::make_unique<routine::sim::SimRuntime>(options.data,options.pak);
  device->begin();
  // "localhost" resolves to ::1 first on Windows; without this second
  // listener every request from such a client waits for the IPv4 fallback.
  SOCKET listener6=INVALID_SOCKET;
  if(options.host=="127.0.0.1"){
    listener6=socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);
    sockaddr_in6 address6{};address6.sin6_family=AF_INET6;address6.sin6_port=htons(uint16_t(options.port));address6.sin6_addr=in6addr_loopback;
    if(listener6!=INVALID_SOCKET&&(bind(listener6,reinterpret_cast<sockaddr*>(&address6),sizeof(address6))||listen(listener6,32))){closesocket(listener6);listener6=INVALID_SOCKET;}
  }
  std::printf("Virtual device: http://%s:%d/\n",options.host=="0.0.0.0"?"localhost":options.host.c_str(),options.port);
  std::fflush(stdout);
  for(;;){
    fd_set readable;FD_ZERO(&readable);FD_SET(listener,&readable);
    if(listener6!=INVALID_SOCKET)FD_SET(listener6,&readable);
    timeval wait{0,2000};
    if(select(0,&readable,nullptr,nullptr,&wait)>0){
      for(SOCKET source:{listener,listener6}){
        if(source==INVALID_SOCKET||!FD_ISSET(source,&readable))continue;
        SOCKET client=accept(source,nullptr,nullptr);
        if(client==INVALID_SOCKET)continue;
        DWORD timeout=2000;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout));
        handle(client,*device,options);shutdown(client,SD_SEND);closesocket(client);
      }
    }
    device->tick();
  }
}
#endif
