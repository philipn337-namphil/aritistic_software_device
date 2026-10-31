#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\ProvisioningBuild.h"
#pragma once
// Never infer insecure provisioning from a debug log level or the saved URL.
#ifndef ROUTINE_BLE_DEVELOPMENT
#define ROUTINE_BLE_DEVELOPMENT 0
#endif
namespace routine::platform {
inline constexpr bool kBleDevelopment=ROUTINE_BLE_DEVELOPMENT!=0;
// Plain-HTTP integration server; trusted only in the BLE development build.
inline constexpr char kDevelopmentServerUrl[]="http://52.79.82.17";
}
