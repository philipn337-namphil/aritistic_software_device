# Yeso Routine Device

ESP32-S3 firmware for the Yeso routine device.

Today's device integration changes:

- Development API endpoint support for `http://52.79.82.17`.
- Battery reporting fixed at 100% for development sync/status payloads.
- USB diagnostic command `api reset-uuid` to clear the locally stored
  `deviceAccessUuid` and `deviceId` while preserving Wi-Fi and product data.
- Claim flow continues to use `POST /device-api/v1/claim` and stores the
  returned `deviceAccessUuid` for subsequent sync requests.

The reset command is intentionally a physical USB-console operation. After
resetting, issue a newly generated 10-digit pairing code with `api pair`.
