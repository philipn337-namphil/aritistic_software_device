#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\FileArchive.h"
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace routine::platform {
enum class ArchiveResult { Saved, Existing, Conflict, IoError };
using SyncFile=bool (*)(FILE*);
// Single writer. A verified final file is immutable. Interrupted candidates
// remain separate from the final name and can be recreated safely.
ArchiveResult writeArchive(const char* path,const char* temporary,const uint8_t* bytes,size_t size,SyncFile sync);
bool archiveGeneration(const char* filename,uint64_t& generation);
}
