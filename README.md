# HLK-LD2450 테스트

ESP32-C3 Super Mini 개발보드와 Arduino framework 사용함. PlatformIO board는 `esp32-c3-devkitm-1` 기반, flash 4MB로 설정함.
USB-C 포트가 native USB(CDC)로 동작하므로 `ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1` 사용함.

## 배선

전원 끈 상태에서 모듈에 표시된 핀 이름 기준으로 연결함.

| HLK-LD2450 | ESP32-C3 Super Mini |
| --- | --- |
| 5V | 5V 전원 |
| GND | GND |
| TX | GPIO4 (ESP32 RX) |
| RX | GPIO5 (ESP32 TX) |

LD2450은 5V 전원과 200mA 초과 공급 능력 필요함. ESP32와 GND 공유함. GPIO4/5는 보드에 표준으로 노출된 핀 기준 임의 선택한 값이므로 실제 보드 실크스크린과 대조 후 연결 필요함. GPIO 변경 시 `platformio.ini`의 `LD2450_RX_PIN`, `LD2450_TX_PIN` 수정함.

## 실행

```powershell
pio run
pio device list
pio run -t upload --upload-port COM번호
pio run -t uploadfs --upload-port COM번호
pio device monitor --port COM번호 --baud 115200
```

업로드 명령은 사용자 승인 후 실행함. 실제 COM 포트로 `COM번호` 대체함.
`uploadfs`는 `data/mapview.png`를 LittleFS에 기록함. 평면도 이미지 교체·최초 설치 시 1회 실행 필요하며, 이후 일반 펌웨어 업데이트(OTA·웹UI 업로드)로는 덮어쓰이지 않음.

센서 UART는 256000 baud, 8N1 사용함. PC Serial Monitor는 115200 baud 사용함.
0.5초마다 수신 byte·frame 수와 최대 3개 타깃의 x/y(mm), 속도(cm/s), 거리 분해능(mm) 출력함.
2초 이상 유효 프레임이 없으면 이전 좌표 대신 수신 오류 안내 출력함.
bytes가 0이면 전원·TX/RX·GND 확인함. bytes만 증가하면 baud 설정과 배선 확인함.
frames가 증가하고 타깃이 없으면 센서 앞에서 움직이며 확인함.
센서 설정 변경 명령은 전송하지 않음.

## 근거 문서

- [PlatformIO Espressif ESP32-C3-DevKitM-1](https://docs.platformio.org/en/stable/boards/espressif32/esp32-c3-devkitm-1.html) (ESP32-C3 Super Mini는 PlatformIO 전용 board 정의가 없어 이 generic board를 사용함)
- [Hi-Link LD2450 설명서](https://h.hlktech.com/download/HLK-LD2450-24G/1/HLK%20LD2450%201T2R%E8%BF%90%E5%8A%A8%E7%9B%AE%E6%A0%87%E6%A3%80%E6%B5%8B%E8%BF%BD%E8%B8%AA%E6%A8%A1%E7%BB%84%E8%AF%B4%E6%98%8E%E4%B9%A6%20V1.02%20.pdf)

제조사 설명서의 30-byte 프레임 구조와 최상위 비트 1=양수 규칙 적용함.

## 네트워크 및 웹UI

부팅 시 저장된 Wi-Fi 설정으로 각 10초씩 총 5회 연결 시도함. 연결이 끊기면 동일한 재시도 수행함.
모두 실패하면 비밀번호 없는 `ESP32-Sensor` AP 시작함. AP 접속 후 `http://192.168.4.1` 열면 됨.
Wi-Fi 연결 성공 시 Serial Monitor에 출력된 IP로 접속함.

대시보드에서 타깃 위치·좌표·속도·거리 분해능·이동 궤적·프레임 수신률 확인 가능함.
장치 이름, 좌표 평활화 α, 표시 거리, 수신 만료 시간은 NVS에 저장됨.
타깃 데이터 표는 원래 수신 좌표, 위치 화면은 평활화된 좌표 사용함.
센서 내부 설정은 변경하지 않으며 RF 파형은 제공하지 않음.

"지도" 탭에서 `data/mapview.png` 평면도 위에 타깃 위치를 겹쳐 표시함. LittleFS(`uploadfs`)로 이미지 제공함.
보정값(센서 위치 픽셀, 방향, mm/px 축척)은 웹UI에서 설정하고 NVS에 저장함. 기본값은 미보정 상태이며 위치가 정확하지 않을 수 있음.
보정 절차: "센서 위치 지정"으로 평면도에서 센서 설치 지점 클릭 → 방향(도) 입력 → "축척 측정"으로 실제 거리를 아는 두 지점 클릭 후 거리(mm) 입력해 계산 → "보정값 저장".
방향은 센서 정면이 이미지에서 가리키는 각도(시계방향, 0=이미지 위쪽)임.

시스템에서 Wi-Fi 설정, `.bin` 펌웨어 설치, 자동업데이트 설정 및 재시작 가능함.
Wi-Fi 비밀번호는 API 응답에 포함하지 않음. 비밀번호 입력을 비워두면 기존 비밀번호 유지함.
Wi-Fi 설정은 `sensor` namespace의 `wifiCfg` 단일 NVS 항목으로 관리함.
부팅 시 정상 NVS 설정을 우선 사용하고, 기존 `ssid`·`pass` 설정은 유효할 때 이관함.
저장 설정이 없거나 유효하지 않으면 코드 기본값으로 NVS 초기화함.
웹UI 저장 시 NVS 쓰기와 재읽기 검증이 성공한 경우에만 재연결함. 저장 실패는 HTTP 500으로 표시함.
Serial Monitor에 설정 출처·저장 검증 결과 표시함. 비밀번호는 출력하지 않음.
Serial 로그는 `ansi_color.h`를 사용하여 경과 시간·태그·색상 표시함.
성공은 초록, 경고는 노랑, 오류는 빨강, 네트워크 IP는 밝은 청록 사용함.
타깃 1·2·3은 각각 초록·파랑·자홍으로 표시함. 각 로그 끝에서 색상 초기화함.
ANSI 지원 터미널에서 `pio device monitor --port COM번호 --baud 115200 --raw`로 확인 가능함.
비밀번호 없는 Wi-Fi 사용 시 별도 체크박스 선택함.
AP와 웹UI는 공개 모드이며 접속자는 설정 변경·업데이트 가능함.

## 펌웨어 업데이트

초기 펌웨어는 USB로 업로드해야 함. 이후 웹UI에서 `.pio/build/esp32-c3-super-mini/firmware.bin` 설치 가능함.
성공하면 재부팅하고 NVS 설정 유지함. 이 프로젝트의 기본 partition table은 OTA 슬롯 2개 제공함.

자동업데이트는 기본 비활성화됨. 시스템 화면에서 다음 설정 입력 후 활성화함.

- 버전 확인 URL: `1.0.1`처럼 `major.minor.patch` 한 줄 제공하는 텍스트 파일 주소임. Content-Length는 1~64 byte 필요함.
- 펌웨어 URL: 동일 버전으로 빌드한 `firmware.bin` 주소임. Content-Length 필요함.
- 확인 주기: 5~10080분 설정 가능함. 기본 60분임.
- HTTPS: 신뢰할 루트 CA 인증서 PEM 입력 필요함. Wi-Fi 연결 후 NTP 시간 동기화 수행함.

현재보다 높은 버전만 설치함. 수동 확인 버튼도 새 버전 발견 시 즉시 설치함.
배포 시 `sensor_web.cpp`의 기본 `FIRMWARE_VERSION` 또는 build flag를 새 버전으로 지정해야 함.
버전 텍스트와 binary를 같은 릴리스로 게시해야 함. 배포 서버 구성·게시 작업은 이 프로젝트에 포함하지 않음.
다운로드 및 OTA 쓰기 중에는 웹 응답과 센서 처리가 일시 정지될 수 있음.

구현은 Arduino ESP32 내장 WiFi, WebServer, Preferences, Update, HTTPUpdate, LittleFS 사용함. 외부 라이브러리 추가하지 않았음.
참고: [Espressif Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html), [OTA Web Update](https://docs.espressif.com/projects/arduino-esp32/en/latest/ota_web_update.html).
