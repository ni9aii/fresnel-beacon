# Wokwi Simulation

The project includes a full simulation setup: an ESP32-S3 with an 8×8 WS2812B NeoPixel matrix on GPIO 39, driven by the same firmware as the real hardware.

| File | Purpose |
|------|---------|
| `wokwi.toml` | Points to the built firmware (`build/fresnel-beacon.bin` + `.elf`) |
| `diagram.json` | Circuit: ESP32-S3 + 8×8 NeoPixel matrix, GPIO 39 → DIN |
| `scenario.yaml` | Automated test: waits for two UART checkpoints, exits 0 on success |

## Prerequisites

You need a built firmware binary. Two options:

**Option A — Download from CI** (no local ESP-IDF required):
```bash
gh run list --repo ni9aii/fresnel-beacon --limit 5
gh run download <run-id> --name firmware --dir build/
```

**Option B — Build locally** (requires [ESP-IDF v5.3.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/)):
```bash
cp main/credentials.h.example main/credentials.h
idf.py build
```

## Interactive simulation (VS Code)

Lets you see the beacon animation in real time.

1. Install the **Wokwi Simulator** extension in VS Code.
2. Get a free token at wokwi.com → Settings → CI Token, then in VS Code: `Ctrl+Shift+P` → `Wokwi: Request License`.
3. Open the `fresnel-beacon` folder in VS Code.
4. `Ctrl+Shift+P` → `Wokwi: Start Simulator`.

The 8×8 matrix will show the rotating amber beacon. The Serial Monitor should print:
```
Fresnel Beacon starting
init RMT on GPIO 39, 64 LEDs
```

## CLI simulation (matches CI exactly)

```bash
# Install wokwi-cli
curl -L https://wokwi.com/ci/install.sh | sh

# Run with the project scenario
export WOKWI_CLI_TOKEN=<your_token>
wokwi-cli --timeout 30000 --scenario scenario.yaml .
```

Exits with code 0 if both UART checkpoints pass, non-zero otherwise. The serial log is printed to stdout.

## What to verify

| Check | Expected |
|-------|----------|
| Beacon rotation | Amber beam sweeps clockwise, ~8 RPM (~7.5 s/revolution) |
| Trail | Quadratic falloff over ~72° behind the leading edge |
| Serpentine wiring | Even rows left→right, odd rows right→left — beam should look continuous, not mirrored |
| UART output | Both lines appear within the 30 s timeout |

## CI integration

The GitHub Actions pipeline builds the firmware and runs `wokwi-cli` automatically on every push. See `.github/workflows/ci.yml` and `scenario.yaml` for the exact invocation.
