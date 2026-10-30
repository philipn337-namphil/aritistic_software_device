#include "FileSlots.h"
#include "../../firmware/RoutineDevice/src/platform/FileArchive.h"
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>

namespace routine::sim {
using application::SlotRead;
SlotRead FileSlots::read(unsigned slot,uint8_t* bytes,size_t capacity,size_t& size){
  size=0;if(slot>1)return SlotRead::Error;
  FILE* f=std::fopen(path(slot?"state-b.bin":"state-a.bin").c_str(),"rb");
  if(!f)return errno==ENOENT?SlotRead::Missing:SlotRead::Error;
  size=std::fread(bytes,1,capacity,f);
  const bool extra=std::fgetc(f)!=EOF,error=std::ferror(f)!=0,closed=std::fclose(f)==0;
  return !extra&&!error&&closed?SlotRead::Ok:SlotRead::Error;
}
bool FileSlots::write(unsigned slot,const uint8_t* bytes,size_t size){
  if(slot>1)return false;
  FILE* f=std::fopen(path(slot?"state-b.bin":"state-a.bin").c_str(),"wb");if(!f)return false;
  bool ok=std::fwrite(bytes,1,size,f)==size;ok=std::fflush(f)==0&&ok;
  return std::fclose(f)==0&&ok;
}
bool FileSlots::floor(uint64_t& generation){
  std::string value;
  if(!readSetting(path("store.txt"),"generation",value)){generation=0;return true;}
  generation=std::strtoull(value.c_str(),nullptr,10);return generation!=0;
}
bool FileSlots::seal(uint64_t generation){
  return generation&&writeSetting(path("store.txt"),"generation",std::to_string(generation));
}
bool FileSlots::archive(uint64_t generation,const uint8_t* bytes,size_t size){
  if(!generation||!bytes||!size)return false;
  const auto file=path(("history-"+std::to_string(generation)+".bin").c_str());
  const auto temporary=path(("history-"+std::to_string(generation)+".tmp").c_str());
  const auto result=platform::writeArchive(file.c_str(),temporary.c_str(),bytes,size,[](FILE*){return true;});
  return result==platform::ArchiveResult::Saved||result==platform::ArchiveResult::Existing;
}

namespace {
std::map<std::string,std::string> load(const std::string& file){
  std::map<std::string,std::string> values;std::ifstream in(file);std::string line;
  while(std::getline(in,line)){const auto eq=line.find('=');if(eq!=std::string::npos)values[line.substr(0,eq)]=line.substr(eq+1);}
  return values;
}
}
bool readSetting(const std::string& file,const char* key,std::string& value){
  const auto values=load(file);const auto it=values.find(key);
  if(it==values.end())return false;value=it->second;return true;
}
bool writeSetting(const std::string& file,const char* key,const std::string& value){
  auto values=load(file);values[key]=value;
  const auto temporary=file+".tmp";
  {std::ofstream out(temporary,std::ios::trunc);if(!out)return false;
   for(const auto& [k,v]:values)out<<k<<'='<<v<<'\n';if(!out.flush())return false;}
  std::remove(file.c_str());return std::rename(temporary.c_str(),file.c_str())==0;
}
}
