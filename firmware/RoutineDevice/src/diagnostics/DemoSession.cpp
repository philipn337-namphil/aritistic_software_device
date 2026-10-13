#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\DemoSession.cpp"
#include "DemoSession.h"
#include <algorithm>
#include <cstring>

namespace routine::diagnostics {
DemoCommand parseDemoCommand(const char* line){
  if(!line||std::strncmp(line,"demo",4)||(line[4]&&line[4]!=' '))return {};
  if(!std::strcmp(line,"demo start"))return {DemoAction::Start,0};
  if(!std::strcmp(line,"demo stop"))return {DemoAction::Stop,0};
  if(!std::strcmp(line,"demo reload"))return {DemoAction::Reload,0};
  if(!std::strcmp(line,"demo status"))return {DemoAction::Status,0};
  if(!std::strncmp(line,"demo advance ",13)){
    const char* p=line+13;uint32_t seconds=0;
    if(!*p)return {DemoAction::Invalid,0};
    for(;*p;++p){if(*p<'0'||*p>'9')return {DemoAction::Invalid,0};seconds=seconds*10+uint32_t(*p-'0');if(seconds>86400)return {DemoAction::Invalid,0};}
    if(seconds)return {DemoAction::Advance,seconds};
  }
  return {DemoAction::Invalid,0};
}
bool demoAllowsCommand(const char* line){
  if(!line)return false;
  for(const char* allowed:{"status","help","reboot","input previous","input next","input confirm","motion on","motion off"})
    if(!std::strcmp(line,allowed))return true;
  return false;
}
domain::TimeSample DemoSession::time(uint32_t now) const {
  const uint32_t elapsed=std::min<uint32_t>(86400,uint32_t(now-started_)/1000+advanced_);
  return {kEpoch+elapsed,active_};
}
bool DemoSession::advance(uint32_t seconds,uint32_t now){
  if(!active_||!seconds||seconds>86400||uint64_t(time(now).unixSeconds-kEpoch)+seconds>86400)return false;
  advanced_+=seconds;return true;
}
void DemoSession::schedule(application::Schedule& output)const{
  output={};output.revision=1;output.count=1;
  auto& record=output.routines[0];
  domain::RunSpec spec{1,kEpoch+3,kEpoch+7200,3,{11,12,13}};
  domain::createRun(spec,{domain::DeadlineRule::DeadlineWins,60},record.run);
  std::strcpy(record.title.data(),"외출 준비");
  std::strcpy(record.stepNames[0].data(),"물 마시기");
  std::strcpy(record.stepNames[1].data(),"가방 챙기기");
  std::strcpy(record.stepNames[2].data(),"신발 신기");
}
application::LoadResult DemoSession::load(application::ProductState& output){
  if(!active_)return live_.load(output);
  if(!size_)return application::LoadResult::Empty;
  return application::decodeState(bytes_,size_,output)==application::DecodeResult::Ok?application::LoadResult::Loaded:application::LoadResult::Corrupt;
}
bool DemoSession::save(const application::ProductState& state){
  if(!active_)return live_.save(state);
  if(!application::validProduct(state))return false;
  const auto size=application::encodeState(state,bytes_,sizeof(bytes_));
  if(!size)return false;
  size_=size;return true;
}
}
