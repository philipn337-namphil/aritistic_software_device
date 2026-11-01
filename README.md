# 예소 루틴 디바이스

ADHD 아동이 하루 루틴을 스스로 해낼 수 있도록 돕는 **키링형 루틴 기기**의 펌웨어와 가상 디바이스입니다.
보호자가 앱에서 루틴을 정하면 기기가 일정을 받아 오고, 아이는 기기 화면의 캐릭터와 함께 루틴을 하나씩 완료합니다. 완료할 때마다 먹이를 받아 캐릭터를 키웁니다.

- **웹 가상 디바이스 (시연용):** https://philipn337-namphil.github.io/aritistic_software_device/
- **3D 기기 화면:** https://philipn337-namphil.github.io/aritistic_software_device/3d.html

> **현재 상태 (2026-11-01)**
> 실물 기기는 고장 나서 쓸 수 없습니다. 그래서 같은 펌웨어 코드를 **WebAssembly로 브라우저에서 실행하는 가상 디바이스**로 시연합니다.
> 가상 디바이스는 화면을 흉내 낸 것이 아니라 기기 펌웨어 C++ 코드를 그대로 컴파일한 것입니다. 그래서 화면, 애니메이션, 루틴 규칙, 보상, 성장, 서버 동기화가 기기와 같습니다.

---

## 목차

1. [기기가 하는 일](#1-기기가-하는-일)
2. [하드웨어 구성](#2-하드웨어-구성)
3. [코드 구조](#3-코드-구조)
4. [WebAssembly 가상 디바이스로 전환](#4-webassembly-가상-디바이스로-전환)
5. [지금 어떻게 돌아가는지](#5-지금-어떻게-돌아가는지)
6. [빌드와 실행](#6-빌드와-실행)
7. [화면 자산 (ui.pak)](#7-화면-자산-uipak)
8. [서버 연동](#8-서버-연동)
9. [남은 일과 주의할 점](#9-남은-일과-주의할-점)
10. [폴더 구성](#10-폴더-구성)

---

## 1. 기기가 하는 일

### 루틴 구조
- **큰 루틴:** 시작 시각과 마감 시각이 있는 묶음입니다. 예: "외출 준비 09:00~09:30"
- **작은 루틴:** 큰 루틴 안의 단계들입니다. 순서대로 하나씩 보여 줍니다. 예: "물 마시기 → 가방 챙기기 → 신발 신기"
- 작은 루틴에는 따로 제한 시간이 없고, 큰 루틴의 마감만 적용됩니다.

### 화면 흐름
```
홈 ─▶ 편지 도착(부저) ─▶ 편지 열기 모션 ─▶ 작은 루틴 1개 표시
        ┌──────────────────────────────────────────┘
        ▼
   [완료] ─▶ 먹이 획득 ─▶ 먹이 주기 ─▶ 성장 게이지 ─▶ 다음 작은 루틴 …
   [포기] ─▶ "진짜로…?"(캐릭터 눈물 모션) ─▶ 큰 루틴 종료
                                              │
   모든 루틴이 끝나면 ─▶ 하루 요약 ◀───────────┘
```
- **성장:** 단계마다 먹이 40 / 50 / 60개를 먹으면 다음 단계로 진화합니다. 3단계를 다 키우면 새 캐릭터를 받고 도감에 기록됩니다.
- **캐릭터:** 10종 × 3단계입니다. 몽실이(양), 또비(빨강 로봇), 메에(염소), 저스티스(초록 로봇), 냐요미(고양이), 맘뭉이(강아지), 물범, 파랑 로봇, 북극곰, 깡충이(토끼)이고, 캐릭터마다 배경과 색 테마가 다릅니다.
- **모션 (Figma 기준):**
  - 기본 모션: 홈에서 숨쉬기 2프레임
  - 눈물 모션: 포기할 때 5프레임
  - 편지 모션: 캐릭터마다 다른 편지, 14프레임
  - 먹이 먹기 모션
- **마감:** 마감이 다가오면 경고하고, 마감이 지나면 "시간 초과"로 기록한 뒤 다음 일정으로 넘어갑니다.
- **절전:** 일정 시간 입력이 없으면 화면을 끕니다. 진행 상태는 그대로 유지합니다.
- **기록:** 결과는 기기에 저장해 두었다가 다음 동기화 때 서버로 올립니다. 서버가 받았다고 확인(ack)한 기록만 지워서 중복 집계를 막습니다.

---

## 2. 하드웨어 구성

| 부품 | 내용 |
|---|---|
| 보드 | Seeed **XIAO ESP32-S3 Plus** (16MB 플래시, 8MB PSRAM) |
| 디스플레이 | **ST7789** 240×320 세로, SPI |
| 입력 | 버튼 3개: 이전 / 확인 / 다음 (누르면 GND, active LOW) |
| 소리 | 부저 (NPN 트랜지스터 스위칭, HIGH = 켜짐) |
| 저장 | 내부 플래시 FFat 파티션 9MB (화면 자산과 기록), NVS (설정) |
| 통신 | Wi-Fi (서버 동기화, 시간 동기화), BLE (개발 빌드의 Wi-Fi 설정) |

### 핀 연결 (`firmware/RoutineDevice/src/platform/esp32/HardwareConfig.h` 기준)

| 연결 | XIAO 핀 | GPIO |
|---|---|---|
| LCD SCL/SCK | D9 | 8 |
| LCD SDA/MOSI | D10 | 9 |
| LCD CS | D2 | 3 |
| LCD RST | D1 | 2 |
| LCD DC | D0 | 1 |
| LCD VCC / GND | 3V3 / GND | — |
| 버튼 이전 / 확인 / 다음 | D4 / D5 / D6 | 5 / 6 / 43 |
| 부저 (1kΩ 거쳐 NPN 베이스) | D8 | 7 |

- 부저 회로: `3V3 → 부저(+)`, `부저(−) → NPN C`, `NPN E → GND`. 부저(−)를 GND에 바로 연결하면 안 됩니다.
- 배선도: [`docs/wiring/wiring.png`](docs/wiring/wiring.png)

---

## 3. 코드 구조

펌웨어는 `firmware/RoutineDevice/` 에 있고, Arduino 스케치(`RoutineDevice.ino`)와 `src/` 아래 계층으로 나뉩니다.
**규칙과 화면은 하드웨어를 모르게** 짜여 있습니다. 하드웨어에 닿는 코드는 `platform/esp32/` 에만 있습니다. 덕분에 같은 코드를 PC와 브라우저에서도 그대로 돌릴 수 있습니다(4장).

```
firmware/RoutineDevice/src/
├── domain/          순수 규칙. 하드웨어·화면 모름
│   ├── RoutineEngine     큰/작은 루틴 진행, 완료·포기·마감 판정
│   └── Companion.h       캐릭터 성장(40/50/60), 진화, 새 캐릭터, 도감 수집
├── application/     제품 동작
│   ├── ProductController / ProductInput   화면 전환과 버튼 처리
│   ├── ProductState / ProductView         저장되는 상태 / 지금 보여 줄 화면
│   ├── ProductSync, DeviceApi, JsonInput  서버 claim·sync 요청과 응답 처리
│   ├── RewardLedger, DailySummary         먹이 보상 장부, 하루 요약
│   ├── SnapshotStore, StateCodec          A/B 스냅샷 저장 형식
│   └── WifiProvisioning, ClockSync, ScheduleTransfer
├── presentation/    240×320 화면 그리기
│   ├── HomeRenderer, ProductRenderer, CatalogRenderer, SummaryRenderer
│   ├── Motion            편지·눈물·먹기·기본 모션 타이밍
│   └── Drawing, FontBitmap, ImageRenderer, ImageArchive   압축 이미지와 한글 글꼴
├── content/         도감 목록(캐릭터 10종·먹이 31종, Figma 도감 순서)
├── generated/       화면 자산 설명 파일 (도구로 자동 생성, 직접 수정 금지)
├── input/           버튼 → 이전/확인/다음 의도(Intent) 변환, 길게 누르기 등
├── diagnostics/     데모 세션, 하드웨어 점검, USB 명령 파서
├── platform/esp32/  ★ 실제 하드웨어 계층
│   ├── Board, HardwareConfig   LCD·버튼·부저 핀과 구동
│   ├── ProductRuntime          메인 루프: 입력 → 규칙 → 그리기 → 절전 → 동기화
│   ├── DeviceNetwork, NetworkClock, BleProvisioning   Wi-Fi·서버·시간·BLE
│   ├── FlashSlots, AssetStorage, StorageConsole        FFat 저장과 ui.pak 설치
│   └── PowerSession, UsbConsole, HardwareCheckRuntime
└── vendor/          ArduinoJson (내장)
```

### 빌드 종류 (스케치 정의로 선택)

| 정의 | 결과 |
|---|---|
| (기본) | 제품 펌웨어 `0.9.0`. `ProductRuntime` 을 PSRAM에 올림 |
| `ROUTINE_HARDWARE_CHECK` | 화면·버튼·부저 점검 펌웨어 |
| `ROUTINE_BLE_DEVELOPMENT` | BLE로 Wi-Fi를 설정하는 개발 빌드 `0.9.0-ble-dev` |
| `ROUTINE_SHOWCASE=1` | 시연 빌드. 캐릭터 10종 3단계와 먹이 31개를 처음부터 모두 공개. **웹 가상 디바이스에서만 켬** |

### 기록 저장
- 상태는 `state-a.bin` / `state-b.bin` 두 칸에 번갈아 저장하고, NVS의 세대 번호로 최신 칸을 고릅니다. 저장 중에 전원이 꺼져도 직전 상태가 남습니다.
- 서버 접속 정보(`deviceAccessUuid` 등)는 NVS `routine-api` 에 따로 저장합니다.

---

## 4. WebAssembly 가상 디바이스로 전환

### 왜 바꿨나
- 실물 기기가 고장 나서 시연과 개발 확인을 하드웨어 없이 할 방법이 필요했습니다.
- 휴대폰이나 노트북에서 링크 하나로 바로 보여 줄 수 있어야 했습니다.

### 어떻게 바꿨나
**하드웨어 계층(`platform/esp32/`)만 갈아 끼우고, 나머지 펌웨어 코드는 한 줄도 바꾸지 않고 그대로 컴파일합니다.**

```
            같은 펌웨어 코드 (수정 없음)
   domain · application · presentation · content · generated · DemoSession
                              │
        ┌─────────────────────┼──────────────────────┐
        ▼                     ▼                      ▼
 platform/esp32/        simulator/src/          simulator/src/
 (실물 기기)             + main.cpp (PC)         + wasm_main.cpp (브라우저)
 ST7789, 버튼, 부저       yeso-sim.exe            Emscripten → yeso.wasm
 FFat, Wi-Fi, BLE        HTTP로 화면 전송         GitHub Pages에서 실행
```

| 기기 | 가상 디바이스 |
|---|---|
| ST7789 LCD | 240×320 RGB565 프레임버퍼를 canvas에 그대로 출력 |
| 버튼 D4/D5/D6 | 화면 버튼과 키보드 (← / Space / →) |
| 부저 | Web Audio 비프 + 화면 진동 효과 |
| FFat 파일, NVS | 브라우저 IndexedDB (`/data`) |
| ui.pak (FFat에 설치) | 페이지가 받아서 메모리에 올림 |
| Wi-Fi + HTTPClient | 브라우저 fetch (HTTPS) |
| SNTP 시계 | PC/휴대폰 시계 |
| eFuse MAC 기반 UID | 처음 실행할 때 `YESO-xxxxxxxxxxxx` 무작위 생성 후 저장 |

- **두 가지 실행 방식**
  - PC 호스트(`yeso-sim.exe`): 펌웨어를 PC 프로그램으로 실행하고, 브라우저는 화면만 받습니다.
  - WebAssembly: 브라우저 안에서 펌웨어가 직접 돌아갑니다.
  - 페이지는 PC 호스트가 켜져 있으면 그쪽에 붙고, 아니면 WebAssembly로 실행합니다. `?mode=wasm` / `?mode=host` 로 강제할 수 있습니다.
- **3D 페이지(`3d.html`):** 제품 CAD 모델(`models/device.glb`) 위에 펌웨어 화면을 텍스처로 입혔습니다. 3D 버튼도 실제 입력 경로를 그대로 탑니다. 색상 7종, 스트랩, 인증마크를 지원합니다.

---

## 5. 지금 어떻게 돌아가는지

### 공개 페이지가 뜨기까지
1. GitHub Pages(`gh-pages` 브랜치)에서 `index.html` 또는 `3d.html` 을 엽니다.
2. `device-link.js` 가 `wasm/yeso.js`, `wasm/yeso.wasm`, `assets/ui.pak` 을 받습니다.
   - 세 파일 모두 `?v=<빌드 이름>` 을 붙여서 받기 때문에, 브라우저에 옛 파일이 섞이지 않습니다.
3. IndexedDB의 `/data` 를 불러와 이전 기록과 설정을 복원하고 `sim_begin` 으로 펌웨어를 시작합니다.
4. 4ms마다 `sim_tick` 이 펌웨어 메인 루프를 한 번 돌립니다. 화면이 바뀌면 canvas에 다시 그립니다.

### 시연 방법
- **데모 시작:** 서버 없이 RAM에서만 돌아갑니다. "외출 준비" 루틴 1개와 가상 시계(09:00)로, 북극곰 3단계에서 시작합니다.
  - 흐름: 편지 → 루틴 → 완료/포기 → 하루 요약
  - `+1분 / +10분 / +1시간 / +2시간` 으로 마감 경고와 시간 초과를 바로 볼 수 있습니다.
- **도감:** 홈에서 이전/다음 → 캐릭터 또는 먹이 도감입니다. 시연 빌드라 30단계 캐릭터와 먹이 31개가 모두 열려 있습니다. 캐릭터를 고르면 그 캐릭터와 배경으로 바뀝니다.
- **실서버 연동:** 앱에서 받은 10자리 페어링 코드를 개발자 패널에 넣거나, 이미 등록된 기기는 비밀 링크(`3d.html#bind=…`)로 이어받습니다. 자세한 내용은 [`simulator/README.md`](simulator/README.md) 에 있습니다.

### 실기기와 웹의 차이
| 항목 | 실기기 | 웹 가상 디바이스 |
|---|---|---|
| 도감 공개 | 획득한 것만 | 전부 공개 (`ROUTINE_SHOWCASE=1`) |
| 기록 저장 위치 | 기기 FFat | 그 브라우저의 IndexedDB (기기마다 따로) |
| 서버 주소 | `http://52.79.82.17` | `https://52-79-82-17.sslip.io` (같은 서버, HTTPS) |

---

## 6. 빌드와 실행

### 펌웨어 (실기기)
- Arduino IDE + esp32 코어 **3.3.11**
- 보드 설정(FQBN): `esp32:esp32:XIAO_ESP32S3_Plus:USBMode=hwcdc,CDCOnBoot=default,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi`
- 라이브러리: Adafruit GFX, Adafruit BusIO, Adafruit ST7735/ST7789. ArduinoJson은 `src/vendor` 에 들어 있습니다.
- 스케치: `firmware/RoutineDevice/RoutineDevice.ino`
- 업로드한 뒤에는 `ui.pak` 을 기기 FFat에 설치해야 화면이 나옵니다(7장).

### 가상 디바이스 (PC)
```powershell
.\simulator\run.ps1          # 자동 빌드 후 http://localhost:8080 열기
```
MSYS2 g++ 가 필요합니다. 옵션(`-Mock`, `-Lan`, `-Data` 등)은 [`simulator/README.md`](simulator/README.md) 에 있습니다.

### WebAssembly 빌드와 배포
```bash
bash simulator/scripts/build-wasm.sh    # emsdk 필요. 결과: simulator/web/wasm/, web/assets/ui.pak
```
1. 빌드하기 전에 `simulator/web/device-link.js` 의 `build` 값과 각 페이지의 `?v=` 값을 새 이름으로 바꿉니다. 그래야 방문자 브라우저가 새 파일을 받습니다.
2. **커밋된 코드로만** 빌드합니다. 커밋 안 한 수정이 섞이지 않도록 별도 worktree에서 빌드하는 것을 권장합니다.
3. `simulator/web/` 내용을 `gh-pages` 브랜치에 복사해 커밋하고 푸시하면 1~2분 뒤 공개 주소에 반영됩니다.

---

## 7. 화면 자산 (ui.pak)

- 캐릭터, 배경, 모션, 도감, 먹이 그림은 모두 Figma 원본에서 만듭니다. 묶음 파일 `ui.pak` 은 약 8MB이고 FFat 9MB 안에 들어갑니다.
- 펌웨어에는 `generated/*.cpp` (각 그림의 위치, 크기, 압축 오프셋)와 `AssetPack.h` (전체 CRC)가 들어갑니다. 실제 픽셀은 `ui.pak` 에 있습니다. **CRC가 맞지 않으면 화면 자산을 쓰지 않습니다.**
- 하루 요약 그림과 일부 UI 그림은 `ui.pak` 이 아니라 펌웨어에 직접 들어갑니다(`SummaryAssets.cpp` 등).
- 자산 생성 도구는 **이 저장소 밖**(`Yeso2026-master/tools`)에 있습니다.
  - `prepare_*.py/.cjs`: Figma 이미지를 240×320 위치에 배치
  - `prepare_figma_motions.py`: 기본·눈물·편지 모션과 몽실이·또비 도감, 하루 요약 그림
  - `generate_asset_pack.py`: `generated/` 와 `ui.pak` 생성
  - 결과물 `generated/` 와 `simulator/assets/ui.pak` 을 이 저장소에 복사합니다.
- Figma에서 그림을 받을 때는 **배경이 투명하게 받아야 합니다.** 흰 배경에 합쳐진 이미지를 쓰면 기기 화면에 흰 박스가 생깁니다.

---

## 8. 서버 연동

| 단계 | 요청 | 내용 |
|---|---|---|
| 등록 | `POST /device-api/v1/claim` | 앱에서 발급한 10자리 페어링 코드 + 기기 UID → `deviceAccessUuid` 를 받아 저장 |
| 동기화 | `POST /device-api/v1/sync` | 루틴 일정을 받고 완료·포기 기록과 배터리를 보냄. 서버가 ack 한 기록만 지움 |

- 동기화는 성공하면 5분마다, 루틴 상태가 바뀌면 바로 합니다. 실패하면 10초부터 최대 5분까지 간격을 늘려 다시 시도합니다.
- 배터리는 개발용으로 100%로 고정해서 보냅니다.
- USB 콘솔 명령
  - `api pair <코드>`: 페어링
  - `api reset-uuid`: 서버 접속 정보만 지우기 (Wi-Fi와 기록은 유지)
  - `api endpoint <주소>`: 서버 주소 바꾸기
  - `help`: 전체 명령 목록
- 웹 가상 디바이스는 HTTPS 주소 `https://52-79-82-17.sslip.io` 를 씁니다. 서버의 CORS는 `https://philipn337-namphil.github.io` 만 허용하므로, **실서버 연동은 공개 Pages 주소에서만 됩니다.**

---

## 9. 남은 일과 주의할 점

- **실기기:** 고장이라 최근 변경(모션, 도감, 요약 그림)은 실기기에서 컴파일·업로드하지 못했습니다. 고치면 펌웨어 업로드와 새 `ui.pak` 설치가 필요합니다.
- **정할 규칙**
  - 자동 절전 시간: 시나리오는 10초인데 코드는 60초입니다.
  - 알림 직후 절전 처리
  - 마감 경고 시점
  - 먹이별 성장량
  - 일정이 겹칠 때 처리
- **자산 도구 위치:** 생성 도구가 저장소 밖에 있습니다. 팀원이 다시 만들 수 있도록 저장소로 옮기는 것을 검토해야 합니다.
- **정리할 것:** 일부 `.h/.cpp` 첫 줄에 `#line 1 "C:\Users\...\Yeso2026-master\..."` 가 남아 있습니다. 컴파일에는 문제없습니다.
- **비밀 정보:** 기기 접속 키(`device_access_uuid`)는 비밀번호와 같습니다. 코드, 저장소, 빌드 설정에 넣지 않습니다.

---

## 10. 폴더 구성

```
.
├── firmware/RoutineDevice/   펌웨어 (Arduino 스케치 + src/)
├── simulator/                가상 디바이스
│   ├── src/                  platform/esp32 대체 (PC·브라우저 공용) + main.cpp / wasm_main.cpp
│   ├── web/                  2D 페이지(index.html), 3D 페이지(3d.html), 3D 모델
│   ├── scripts/              build.sh (PC), build-wasm.sh (브라우저), mock_server.py (모의 서버)
│   ├── assets/ui.pak         화면 자산 묶음 (git 제외, 따로 복사)
│   ├── run.ps1               PC 가상 디바이스 실행
│   └── README.md             가상 디바이스 사용법 자세히
├── docs/wiring/              배선도
└── README.md                 이 문서
```
