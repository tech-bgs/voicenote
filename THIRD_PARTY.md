# 다른 사람이 만든 것 (Third-party notices)

이 저장소의 코드는 MIT 라이선스(`LICENSE`)지만, 아래 파일은 각자의 라이선스를 따릅니다.

| 파일 | 만든 곳 | 라이선스 |
|---|---|---|
| `firmware/voicenote/es8311.cpp`, `es8311.h`, `es8311_reg.h` | Espressif Systems (오디오 칩 ES8311 드라이버) | Apache-2.0 (각 파일 머리말 참고) |
| `tools/fonts/NotoSansKR[wght].ttf` | Google · Adobe (Noto Sans KR) | SIL Open Font License 1.1 (`tools/fonts/NotoSansKR-OFL.txt`) |
| `firmware/voicenote/font_ko.h` | 위 Noto Sans KR 글꼴을 `tools/make_font.py`로 점 글꼴로 바꾼 것 | SIL Open Font License 1.1 |
| `firmware/voicenote/certs.h` | 공개 루트 인증서 (Google Trust Services, GlobalSign, Amazon, Starfield, ISRG) | 공개 인증서 |

빌드에 쓰는 라이브러리 (저장소에는 들어 있지 않음):

- Arduino core for ESP32 (Espressif) — LGPL-2.1
- U8g2 (olikraus) — BSD-2-Clause
- ArduinoJson (Benoit Blanchon) — MIT
