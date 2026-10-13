#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\FileArchive.cpp"
#include "FileArchive.h"
#include <cstring>
#include <cerrno>
#include <algorithm>

namespace routine::platform {
namespace {
bool matches(FILE* file,const uint8_t* bytes,size_t size){
  uint8_t block[256];size_t offset=0;bool ok=true;
  while(offset<size&&ok){const size_t count=std::min(sizeof(block),size-offset);ok=std::fread(block,1,count,file)==count&&!std::memcmp(block,bytes+offset,count);offset+=count;}
  return ok&&std::fgetc(file)==EOF&&!std::ferror(file);
}
bool verify(const char* path,const uint8_t* bytes,size_t size){
  FILE* file=std::fopen(path,"rb");if(!file)return false;
  const bool ok=matches(file,bytes,size);return std::fclose(file)==0&&ok;
}
}
ArchiveResult writeArchive(const char* path,const char* temporary,const uint8_t* bytes,size_t size,SyncFile sync){
  if(!path||!temporary||!std::strcmp(path,temporary)||!bytes||!size||!sync)return ArchiveResult::IoError;
  FILE* file=std::fopen(path,"rb");
  if(file){const bool same=matches(file,bytes,size);if(std::fclose(file))return ArchiveResult::IoError;return same?ArchiveResult::Existing:ArchiveResult::Conflict;}
  if(errno!=ENOENT)return ArchiveResult::IoError;
  file=std::fopen(temporary,"wb");if(!file)return ArchiveResult::IoError;
  bool ok=std::fwrite(bytes,1,size,file)==size;ok=std::fflush(file)==0&&ok;
  ok=sync(file)&&ok;ok=std::fclose(file)==0&&ok;
  if(!ok||!verify(temporary,bytes,size)){std::remove(temporary);return ArchiveResult::IoError;}
  if(std::rename(temporary,path)){std::remove(temporary);return ArchiveResult::IoError;}
  return verify(path,bytes,size)?ArchiveResult::Saved:ArchiveResult::IoError;
}
bool archiveGeneration(const char* filename,uint64_t& generation){
  if(!filename||std::strncmp(filename,"history-",8))return false;
  const char* p=filename+8;if(*p<'1'||*p>'9')return false;
  uint64_t value=0;
  while(*p>='0'&&*p<='9'){const unsigned digit=unsigned(*p++-'0');if(value>(UINT64_MAX-digit)/10)return false;value=value*10+digit;}
  if(std::strcmp(p,".bin"))return false;
  generation=value;return true;
}
}
