#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\StorageConsole.h"
#pragma once
#include "../../application/ProductState.h"
namespace routine::platform {
// Read-only access to immutable user-progress archives. Never exposes Wi-Fi/API NVS.
bool storageCommand(const char* line,application::ProductStore& store,const application::ProductState& state);
}
