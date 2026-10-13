#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\StorageConsole.cpp"
#include "StorageConsole.h"
#include "../FileArchive.h"
#include <Arduino.h>
#include <FFat.h>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
namespace routine::platform {
bool storageCommand(const char* line,application::ProductStore& store,const application::ProductState& state){
  if(std::strncmp(line,"storage ",8))return false;
  if(!std::strcmp(line,"storage info")){Serial.printf("STORAGE total=%u used=%u\n",unsigned(FFat.totalBytes()),unsigned(FFat.usedBytes()));return true;}
  if(!std::strcmp(line,"storage list")){
    DIR* directory=::opendir("/ffat");if(!directory){Serial.println("STORAGE ERR list");return true;}
    unsigned count=0;bool ok=true;
    for(;;){
      errno=0;const auto* entry=::readdir(directory);
      if(!entry){if(errno)ok=false;break;}
      uint64_t generation=0;if(!archiveGeneration(entry->d_name,generation))continue;
      char path[80];std::snprintf(path,sizeof(path),"/ffat/history-%llu.bin",static_cast<unsigned long long>(generation));
      struct stat info{};if(::stat(path,&info)||!S_ISREG(info.st_mode)||info.st_size<=0||info.st_size>65536){ok=false;break;}
      Serial.printf("STORAGE ARCHIVE %llu %ld\n",static_cast<unsigned long long>(generation),static_cast<long>(info.st_size));++count;
    }
    ok=::closedir(directory)==0&&ok;
    if(ok)Serial.printf("STORAGE LIST END %u\n",count);else Serial.println("STORAGE ERR list");
    return true;
  }
  if(!std::strcmp(line,"storage snapshot")){
    if(!store.archive(state)){Serial.println("STORAGE ERR snapshot");return true;}
    char path[80];std::snprintf(path,sizeof(path),"/ffat/history-%llu.bin",static_cast<unsigned long long>(state.generation));
    FILE* f=std::fopen(path,"rb");if(!f){Serial.println("STORAGE ERR open");return true;}
    const bool ok=std::fseek(f,0,SEEK_END)==0;const long size=std::ftell(f);std::fclose(f);
    if(!ok||size<=0)Serial.println("STORAGE ERR size");
    else Serial.printf("STORAGE SNAPSHOT %llu %ld\n",static_cast<unsigned long long>(state.generation),size);
    return true;
  }
  unsigned long long generation=0;unsigned offset=0,count=0;char extra;
  if(std::sscanf(line,"storage read %llu %u %u %c",&generation,&offset,&count,&extra)==3&&generation&&count&&count<=128&&offset<=65536&&count<=65536-offset){
    char path[80];std::snprintf(path,sizeof(path),"/ffat/history-%llu.bin",generation);
    FILE* f=std::fopen(path,"rb");if(!f){Serial.println("STORAGE ERR open");return true;}
    uint8_t bytes[128];const bool ok=std::fseek(f,long(offset),SEEK_SET)==0&&std::fread(bytes,1,count,f)==count;const bool closed=std::fclose(f)==0;
    if(!ok||!closed){Serial.println("STORAGE ERR read");return true;}
    char hex[257];constexpr char digits[]="0123456789abcdef";for(unsigned i=0;i<count;++i){hex[i*2]=digits[bytes[i]>>4];hex[i*2+1]=digits[bytes[i]&15];}hex[count*2]=0;
    Serial.printf("STORAGE DATA %u %s\n",offset,hex);return true;
  }
  Serial.println("STORAGE ERR command");return true;
}
}
