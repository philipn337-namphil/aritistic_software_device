#include "HttpClient.h"
#include <windows.h>
#include <winhttp.h>
#include <vector>

namespace routine::sim {
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
}
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
