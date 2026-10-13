#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\StateCodec.h"
#pragma once
#include "ProductState.h"

namespace routine::application {
// Stable little-endian wire format, never the compiler's struct layout.
constexpr size_t kSnapshotBytes = 65536;
enum class DecodeResult { Ok, Invalid, Unsupported };
uint32_t crc32(const uint8_t* bytes, size_t size);
size_t encodeState(const ProductState& state, uint8_t* bytes, size_t capacity);
DecodeResult decodeState(const uint8_t* bytes, size_t size, ProductState& state);
}
