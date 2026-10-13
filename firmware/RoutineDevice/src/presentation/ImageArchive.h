#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\ImageArchive.h"
#pragma once
#include <cstdint>
#include <cstddef>
namespace routine::ui {
uint32_t imageCrc(const uint8_t* bytes,size_t size);
// Streaming state starts at 0xffffffff and is complemented after the last chunk.
uint32_t updateImageCrc(uint32_t state,const uint8_t* bytes,size_t size);
bool validImageArchive(const uint8_t* bytes,size_t size,size_t expectedSize,uint32_t expectedCrc);
// Owner retains the memory. A rejected replacement leaves the current pack bound.
bool bindImageArchive(const uint8_t* bytes,size_t size,size_t expectedSize,uint32_t expectedCrc);
using ImageProvider=const uint8_t*(*)(void*,size_t,size_t);
// Platform binds this only after validating the immutable archive's header/CRC.
void bindImageProvider(void* context,ImageProvider provider,size_t size);
const uint8_t* archivedImage(size_t offset,size_t size);
}
