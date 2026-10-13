#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\AssetStorage.cpp"
#include "AssetStorage.h"
#include "../../generated/AssetPack.h"
#include "../../presentation/ImageArchive.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
namespace routine::platform {
namespace { constexpr char temporary[]="/ffat/ui-upload.tmp"; }
bool AssetStorage::load(const char* path,bool activate) {
  FILE* f=std::fopen(path,"rb");if(!f)return false;
  {
    // Verify without loading the entire animation library into RAM.
    uint8_t header[16];bool ok=std::fread(header,1,16,f)==16;
    const auto read32=[](const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);};
    ok=ok&&!std::memcmp(header,"RDAS0001",8)&&read32(header+8)==assets::kAssetPayloadBytes&&read32(header+12)==assets::kAssetCrc;
    size_t remaining=assets::kAssetPayloadBytes;uint32_t crc=0xffffffff;
    while(ok&&remaining){const auto count=std::min(remaining,sizeof(chunk_));ok=std::fread(chunk_,1,count,f)==count;if(ok){crc=ui::updateImageCrc(crc,chunk_,count);remaining-=count;}yield();}
    ok=ok&&~crc==assets::kAssetCrc&&std::fgetc(f)==EOF&&!std::ferror(f);
    ok=std::fclose(f)==0&&ok;if(!ok||!activate||ready())return ok;
  }
  // Two bounded compressed-sprite caches. Growth in library size costs flash,
  // not another full copy in PSRAM; all renderer accesses still remain bounded.
  if(!memory_)memory_=static_cast<uint8_t*>(heap_caps_malloc(2*assets::kLargestImage,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!memory_)return false;
  if(reader_)std::fclose(reader_);reader_=std::fopen(path,"rb");if(!reader_)return false;
  cacheOffset_[0]=cacheOffset_[1]=SIZE_MAX;cacheSize_[0]=cacheSize_[1]=0;cacheNext_=0;readError_=false;
  ui::bindImageProvider(this,readAsset,assets::kAssetPayloadBytes);return true;
}
const uint8_t* AssetStorage::readAsset(void* context,size_t offset,size_t size){
  auto& self=*static_cast<AssetStorage*>(context);
  if(!self.ready()||size>assets::kLargestImage||offset>assets::kAssetPayloadBytes||size>assets::kAssetPayloadBytes-offset)return nullptr;
  for(unsigned slot=0;slot<2;++slot)if(self.cacheOffset_[slot]==offset&&self.cacheSize_[slot]==size){self.cacheNext_=1-slot;return self.memory_+slot*assets::kLargestImage;}
  const unsigned slot=self.cacheNext_;self.cacheNext_=1-slot;auto* bytes=self.memory_+slot*assets::kLargestImage;
  if(std::fseek(self.reader_,long(offset+16),SEEK_SET)||std::fread(bytes,1,size,self.reader_)!=size){self.readError_=true;return nullptr;}
  self.cacheOffset_[slot]=offset;self.cacheSize_[slot]=size;return bytes;
}
bool AssetStorage::begin(bool mounted) { mounted_=mounted;return mounted && load(assets::kAssetPath,true); }
void AssetStorage::info() {
  Serial.printf("ASSETS ready=%d bytes=%u crc=%08lx received=%lu active=%d\n",ready(),unsigned(assets::kAssetPayloadBytes+16),static_cast<unsigned long>(assets::kAssetCrc),static_cast<unsigned long>(received_),active());
}
void AssetStorage::abort(const char* reason) {
  if(file_){std::fclose(file_);file_=nullptr;}
  if(mounted_)std::remove(temporary);
  chunkSize_=chunkAt_=received_=0;
  Serial.printf("ASSETS ERR %s\n",reason);
}
bool AssetStorage::command(const char* line) {
  if(std::strncmp(line,"assets ",7))return false;
  if(!std::strcmp(line,"assets info")){info();return true;}
  if(!std::strcmp(line,"assets abort")){abort("aborted");return true;}
  unsigned size=0,crc=0,offset=0;char extra;
  if(std::sscanf(line,"assets begin %u %x %c",&size,&crc,&extra)==2){
    if(!mounted_||active()||size!=assets::kAssetPayloadBytes+16||crc!=assets::kAssetCrc){Serial.println("ASSETS ERR begin");return true;}
    file_=std::fopen(temporary,"wb");
    if(!file_){Serial.println("ASSETS ERR open");return true;}
    received_=0;lastAt_=millis();Serial.println("ASSETS READY");return true;
  }
  if(std::sscanf(line,"assets chunk %u %u %x %c",&offset,&size,&crc,&extra)==3){
    if(!active()||receiving()||offset!=received_||!size||size>sizeof(chunk_)||received_>assets::kAssetPayloadBytes+16||size>assets::kAssetPayloadBytes+16-received_){Serial.println("ASSETS ERR chunk");return true;}
    chunkSize_=size;chunkAt_=0;chunkCrc_=crc;lastAt_=millis();Serial.println("ASSETS CHUNK READY");return true;
  }
  if(!std::strcmp(line,"assets commit")){
    if(!active()||receiving()||received_!=assets::kAssetPayloadBytes+16){Serial.println("ASSETS ERR incomplete");return true;}
    bool ok=std::fflush(file_)==0;ok=(::fsync(::fileno(file_))==0)&&ok;
    ok=(std::fclose(file_)==0)&&ok;file_=nullptr;
    // Verify the durable candidate before any activation. An existing valid pack
    // is already identical to this firmware's immutable content version.
    if(!ok||!load(temporary,false)){abort("verify");return true;}
    if(load(assets::kAssetPath,false)){std::remove(temporary);}
    else {
      // Removing only an invalid file of this same content version cannot remove
      // another firmware's valid UI pack or any progress record.
      std::remove(assets::kAssetPath);
      if(std::rename(temporary,assets::kAssetPath)!=0){abort("rename");return true;}
    }
    if(!load(assets::kAssetPath,true)){Serial.println("ASSETS ERR activate");return true;}
    Serial.println("ASSETS COMMIT OK");return true;
  }
  Serial.println("ASSETS ERR command");return true;
}
void AssetStorage::tick() {
  if(!active())return;
  if(uint32_t(millis()-lastAt_)>30000){abort("timeout");return;}
  if(!receiving())return;
  const unsigned available=unsigned(Serial.available());
  if(!available)return;
  const unsigned count=std::min<unsigned>(available,unsigned(chunkSize_-chunkAt_));
  const auto got=Serial.read(chunk_+chunkAt_,count);
  chunkAt_+=got;lastAt_=millis();
  if(chunkAt_!=chunkSize_)return;
  if(ui::imageCrc(chunk_,chunkSize_)!=chunkCrc_){abort("chunk-crc");return;}
  if(std::fwrite(chunk_,1,chunkSize_,file_)!=chunkSize_){abort("write");return;}
  received_+=chunkSize_;chunkSize_=chunkAt_=0;
  Serial.printf("ASSETS OK %lu\n",static_cast<unsigned long>(received_));
}
}
