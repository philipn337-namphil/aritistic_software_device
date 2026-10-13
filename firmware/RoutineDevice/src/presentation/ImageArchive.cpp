#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\ImageArchive.cpp"
#include "ImageArchive.h"
#include <cstring>
namespace routine::ui {
namespace { const uint8_t* payload=nullptr; size_t payloadSize=0;void* providerContext=nullptr;ImageProvider provider=nullptr;
uint32_t read32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
}
uint32_t imageCrc(const uint8_t* bytes,size_t size) {
  return ~updateImageCrc(0xffffffff,bytes,size);
}
uint32_t updateImageCrc(uint32_t crc,const uint8_t* bytes,size_t size) {
  for(size_t i=0;i<size;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));}
  return crc;
}
bool validImageArchive(const uint8_t* bytes,size_t size,size_t expectedSize,uint32_t expectedCrc) {
  return bytes && size>=16 && size-16==expectedSize && !std::memcmp(bytes,"RDAS0001",8) &&
    read32(bytes+8)==expectedSize && read32(bytes+12)==expectedCrc && imageCrc(bytes+16,size-16)==expectedCrc;
}
bool bindImageArchive(const uint8_t* bytes,size_t size,size_t expectedSize,uint32_t expectedCrc) {
  if(!validImageArchive(bytes,size,expectedSize,expectedCrc))return false;
  payload=bytes+16;payloadSize=size-16;provider=nullptr;providerContext=nullptr;return true;
}
void bindImageProvider(void* context,ImageProvider reader,size_t size){payload=nullptr;payloadSize=size;providerContext=context;provider=reader;}
const uint8_t* archivedImage(size_t offset,size_t size) {
  if(offset>payloadSize||size>payloadSize-offset)return nullptr;
  return provider?provider(providerContext,offset,size):payload?payload+offset:nullptr;
}
}
