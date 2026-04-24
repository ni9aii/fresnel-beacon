# Fresnel Beacon

A desktop lighthouse lamp: ESP32-S3 drives an 8×8 WS2812B LED matrix mounted inside a Fresnel lens to simulate a rotating lighthouse beam — no moving parts.

## Concept

A real lighthouse rotates its optics around a fixed light source. This project inverts that: the lens is static, and LEDs fire in sequence to synthesize the sweeping beam effect electronically.

## Hardware

| Component | Part | Notes |
|-----------|------|-------|
| MCU + LED panel | Waveshare ESP32-S3-Matrix | ESP32-S3 + 8×8 WS2812B, USB-C |
| Optics | Fresnel lens | Mounted over the LED matrix |
| Power | USB-C 5V | Via onboard connector |
| Enclosure | TBD | Lighthouse-shaped, 3D printed |

## Firmware Stack

- **Framework**: ESP-IDF (FreeRTOS)
- **LED driver**: RMT peripheral → WS2812B
- **Language**: C

## Planned Features

- [ ] Core beacon animation (rotating beam sweep)
- [ ] Configurable rotation speed
- [ ] Multiple light modes (beacon, strobe, ambient)
- [ ] Brightness control
- [ ] Web UI over WiFi for runtime configuration

## Firmware Architecture

```
main_task
├── led_task        — RMT-based WS2812B frame renderer
├── animation_task  — beacon pattern generator, pushes frames to led_task
└── wifi_task       — HTTP server for web UI (phase 2)
```

## Project Structure (planned)

```
fresnel-beacon/
├── main/
│   ├── main.c
│   ├── led/          — WS2812B RMT driver
│   ├── animation/    — beacon patterns
│   └── wifi/         — web UI (phase 2)
├── CMakeLists.txt
└── sdkconfig.defaults
```

## Development Environment

- ESP-IDF v5.x
- Target: `esp32s3`
- Flash via USB-C (native USB, no UART adapter needed)
