#include "HttpClient.h"
#include <atomic>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <cstring>
#include <vector>
#else
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <vector>
#endif

namespace routine::sim {
struct HttpRequest::State {
  std::atomic<bool> done{true};
  int status=0;
  std::string body;
  size_t maxBytes=0;
#ifdef __EMSCRIPTEN__
  std::string requestBody;
  std::vector<std::string> headerText;
  std::vector<const char*> headers;
#else
  std::thread worker;
#endif
};

#ifdef __EMSCRIPTEN__
namespace {
// The fetch callbacks own a reference to the request state until they run.
void complete(emscripten_fetch_t* fetch){
  auto* holder=static_cast<std::shared_ptr<HttpRequest::State>*>(fetch->userData);
  auto& state=**holder;
  if(!fetch->status)state.status=-1;   // network error, CORS or mixed-content block
  else if(size_t(fetch->numBytes)>state.maxBytes)state.status=-2;
  else {state.status=fetch->status;state.body.assign(fetch->data,size_t(fetch->numBytes));}
  state.done=true;
  emscripten_fetch_close(fetch);
  delete holder;
}
}
HttpRequest::HttpRequest():state_(std::make_shared<State>()){}
HttpRequest::~HttpRequest()=default;
void HttpRequest::start(const std::string& url,const std::string& extraHeaders,const std::string& body,size_t maxBytes){
  state_=std::make_shared<State>();
  auto& s=*state_;
  s.done=false;s.maxBytes=maxBytes;s.requestBody=body;
  // "Name: value\r\n" lines → the name/value list emscripten_fetch expects.
  s.headerText={"Content-Type","application/json"};
  for(size_t at=0;at<extraHeaders.size();){
    const size_t end=extraHeaders.find("\r\n",at),colon=extraHeaders.find(':',at);
    if(end==std::string::npos||colon==std::string::npos||colon>end)break;
    s.headerText.push_back(extraHeaders.substr(at,colon-at));
    size_t value=colon+1;while(value<end&&extraHeaders[value]==' ')++value;
    s.headerText.push_back(extraHeaders.substr(value,end-value));
    at=end+2;
  }
  for(const auto& h:s.headerText)s.headers.push_back(h.c_str());
  s.headers.push_back(nullptr);
  emscripten_fetch_attr_t attr;emscripten_fetch_attr_init(&attr);
  std::strcpy(attr.requestMethod,"POST");
  attr.attributes=EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attr.requestHeaders=s.headers.data();
  attr.requestData=s.requestBody.data();attr.requestDataSize=s.requestBody.size();
  attr.timeoutMSecs=8000;
  attr.userData=new std::shared_ptr<State>(state_);
  attr.onsuccess=complete;attr.onerror=complete;
  emscripten_fetch(&attr,url.c_str());
}
#else
namespace {
std::wstring widen(const std::string& text){
  if(text.empty())return {};
  const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);
  std::wstring out(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),&out[0],n);return out;
}
struct Handle {
  HINTERNET h=nullptr;
  ~Handle(){if(h)WinHttpCloseHandle(h);}
};
int httpPost(const std::string& url,const std::string& extraHeaders,const std::string& body,
             std::string& response,size_t maxBytes){
  response.clear();
  const std::wstring wideUrl=widen(url);
  URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);
  wchar_t host[256]{},path[1024]{};
  parts.lpszHostName=host;parts.dwHostNameLength=256;parts.lpszUrlPath=path;parts.dwUrlPathLength=1024;
  if(!WinHttpCrackUrl(wideUrl.c_str(),0,0,&parts))return -1;
  Handle session{WinHttpOpen(L"YesoVirtualDevice/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
  if(!session.h)return -1;
  // Same 8s connect/read budget as DeviceNetwork::runRequest on the device.
  WinHttpSetTimeouts(session.h,8000,8000,8000,8000);
  Handle connection{WinHttpConnect(session.h,host,parts.nPort,0)};
  if(!connection.h)return -1;
  const bool tls=parts.nScheme==INTERNET_SCHEME_HTTPS;
  Handle request{WinHttpOpenRequest(connection.h,L"POST",path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,tls?WINHTTP_FLAG_SECURE:0)};
  if(!request.h)return -1;
  DWORD noRedirect=WINHTTP_DISABLE_REDIRECTS;
  WinHttpSetOption(request.h,WINHTTP_OPTION_DISABLE_FEATURE,&noRedirect,sizeof(noRedirect));
  const std::wstring headers=widen("Content-Type: application/json\r\n"+extraHeaders);
  if(!WinHttpSendRequest(request.h,headers.c_str(),DWORD(headers.size()),const_cast<char*>(body.data()),DWORD(body.size()),DWORD(body.size()),0))return -1;
  if(!WinHttpReceiveResponse(request.h,nullptr))return -1;
  DWORD status=0,size=sizeof(status);
  if(!WinHttpQueryHeaders(request.h,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return -1;
  for(;;){
    DWORD available=0;if(!WinHttpQueryDataAvailable(request.h,&available))return -3;
    if(!available)break;
    if(available>maxBytes-response.size())return -2;
    std::vector<char> chunk(available);DWORD read=0;
    if(!WinHttpReadData(request.h,chunk.data(),available,&read))return -3;
    response.append(chunk.data(),read);
  }
  return int(status);
}
}
HttpRequest::HttpRequest():state_(std::make_shared<State>()){}
HttpRequest::~HttpRequest(){if(state_->worker.joinable())state_->worker.join();}
void HttpRequest::start(const std::string& url,const std::string& extraHeaders,const std::string& body,size_t maxBytes){
  if(state_->worker.joinable())state_->worker.join();
  state_->done=false;state_->body.clear();state_->maxBytes=maxBytes;
  auto state=state_;
  state_->worker=std::thread([state,url,extraHeaders,body]{
    state->status=httpPost(url,extraHeaders,body,state->body,state->maxBytes);
    state->done.store(true,std::memory_order_release);
  });
}
#endif

bool HttpRequest::done() const { return state_->done.load(std::memory_order_acquire); }
int HttpRequest::status() const { return state_->status; }
std::string HttpRequest::take() {
#ifndef __EMSCRIPTEN__
  if(state_->worker.joinable())state_->worker.join();
#endif
  return std::move(state_->body);
}
}
