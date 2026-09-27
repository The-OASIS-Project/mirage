# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project Overview

MIRAGE (Multi-Input Reconnaissance and Guidance Environment) is a heads-up display system for the OASIS Project, targeting NVIDIA Jetson and Raspberry Pi. It renders real-time camera feeds with overlaid HUD elements to dual displays (one per eye), with object detection, video recording, and streaming.

See @README.md for features and @GETTING_STARTED.md for setup.

## ⚠️ THE PRIME DIRECTIVE: WE ALWAYS DO IT RIGHT. WE DON'T DO STOP-GAPS. ⚠️

**This rule outranks convenience, speed, and scope. It governs every other rule in this file.**

- **Do it right. Don't be lazy.** Every fix is the real fix, designed properly and phased properly.
  There is no "quick fix now, real fix later." Never propose a stop-gap, a band-aid, a workaround
  dressed up as a fix, or a degrade-instead-of-fix path as the plan. If the proper fix is large,
  **phase the proper fix**: every phase must be a piece of the final design, not something to rip
  out later.
- **MIRAGE is a public project.** Judge correctness against **every user's** accounts, configs,
  platforms, and hardware — never only the developer's. "It works on my setup" or "my account isn't
  affected" is not an argument. Other people run this.
- **Verify, don't assume.** Check claims against the code and against authoritative vendor
  documentation or live behavior, and reproduce a bug before calling it fixed. If you haven't
  verified something, say so plainly.
- **When a real tradeoff exists,** lay the options out honestly, say what each one actually fixes,
  and recommend the proper one even when it's more work. Never quietly pick the easy option.

## Critical Rules — Always Follow

- **NEVER delete files.** Tell the developer which files to delete.
- **NEVER run `git add`, `git commit`, or `git push`.** Suggest the command and message; let the developer run it.
- **Feedback before implementation.** Provide analysis, trade-offs, and a recommendation *first*. Wait for explicit confirmation ("go ahead", "do it", "yes") before coding.
- **Format before committing.** Every change must pass `./format_code.sh --check`. Pre-commit hook enforces this.
- **GPL header on every new `.c`/`.cpp`/`.h`.** Template in @CODING_STYLE_GUIDE.md.
- **Design doc commit policy**: commit design docs only when they describe shipped or in-flight code. Docs for planned-but-unstarted work stay untracked.

## Build & Test

Default build directory is `build-debug/` (created by the `debug` CMake
preset).  Earlier `build/` layouts are legacy — prefer the preset.

```bash
# Standard build
cmake --preset debug
make -C build-debug -j$(nproc)

# With optional features
cmake --preset debug -DUSE_JETSON_INFERENCE=ON -DUSE_CUDA=ON

# Run
./build-debug/mirage

# Format
./format_code.sh                 # fix all
./format_code.sh --changed       # only changed files (fast)
./format_code.sh --check         # CI mode
```

- Dependencies and install steps: see @README.md and @GETTING_STARTED.md.
- Pre-commit hook: `./install-git-hooks.sh` (one-time).
- Manual testing on device required for camera, HUD rendering, MQTT, recording.

## Code Standards

Full standards in @CODING_STYLE_GUIDE.md. Critical gotchas:

- **Return codes**: `SUCCESS` (0) / `FAILURE` (1) — never negative.
- **Naming**: `snake_case` functions/vars, `UPPER_CASE` constants, `_t` suffix on types.
- **Logging**: `LOG_INFO` / `LOG_WARNING` / `LOG_ERROR` macros.
- **Formatting**: 3-space indent, 100-char lines, K&R braces, right-aligned pointers. Enforced by `.clang-format`.
- **Memory**: prefer static allocation for embedded; check malloc; free and NULL.

## Threading Model

Multiple threads for concurrent processing:

- `video_proc_thread` — camera input, SDL render
- `vid_out_thread` — video output (disk + streaming)
- `command_proc_thread` — USB/serial input
- `thread_handles[0..7]` — audio playback (NUM_AUDIO_THREADS=8)
- `od_L_thread`, `od_R_thread` — object detection per eye (when enabled)
- `cpu_util_thread` / `map_download_thread` — monitoring / map tiles
- Mosquitto loop — MQTT message handling

**Thread-safety rules:**

- **SDL renderer**: main thread only — never call `SDL_Render*` from other threads.
- **GStreamer pipeline**: recording thread (`vid_out_thread`) only.
- **Config reload**: mutex-protected in `config_manager.c` (5s polling).
- **MQTT callbacks**: run in mosquitto's loop thread; route via topic matching.
- **Audio playback**: isolated via POSIX message queues (8 threads, no shared state).
- **Atomic flags**: `quit`, `detect_enabled`, `active_alerts`, `averageFrameRate` are `_Atomic`.
- **AI state**: `aiName`/`aiState` use double-buffering with atomic index swap (lock-free).
- **Video buffers**: `v_mutex` protects frame rotation; triple-buffered pre-allocated pool.
- **Shutdown order**: MQTT loop stopped **before** element list freed (prevents use-after-free in `registerArmor`).

Not yet synchronized (benign in practice): sensor structs (`motion`, `enviro`, `gps`, `system_metrics`) show torn reads momentarily but don't crash. Detection arrays fine while detection disabled; need double-buffering when re-enabled.

## MQTT Integration

MIRAGE communicates with other OASIS components:
- **AURA** (helmet sensors) → MIRAGE: motion, orientation, environmental, GPS
- **SPARK** (armor sensors) → MIRAGE: component status, audio commands
- **DAWN** (AI assistant) ↔ MIRAGE: AI state, TTS notifications, image capture requests

All messages conform to OCP v1.4 (ms timestamps, `msg_type` field, `clock_gettime(CLOCK_REALTIME)`).

## Platform Support

Controlled via `PLATFORM` CMake variable (AUTO, JETSON, RPI):

- `PLATFORM_JETSON` — nvv4l2h264enc hardware encoding, nvarguscamerasrc
- `PLATFORM_RPI` — libcamerasrc
- Generic ARM — USB cameras only, software encoding

## Configuration Files

- `config.json` — runtime HUD config; hot-reloads every 5 seconds.
- `config-720p.json` — alternative 720p resolution.
- `secrets.json` — API keys and MQTT credentials (gitignored; copy from `secrets.json.example`).
- `defines.h` — compile-time settings (camera resolution, GStreamer pipelines, feature flags).
- `version.h` — manually maintained.

MQTT auth and TLS are optional: empty `mqtt_username` logs a warning; `tls: false` connects unencrypted. Mirrors DAWN's split of connection settings in `config.json` and credentials in `secrets.json`.

**Change camera resolution** by uncommenting ONE option in `defines.h`:

```c
//#define USE_720P_30FPS
#define USE_720P_60FPS      // Default
//#define USE_1080P_30FPS
//#define USE_1080P_60FPS
```

## Patterns

### MQTT Callbacks (`mosquitto_comms.c`)

```c
void on_message(struct mosquitto *mosq, void *obj,
                const struct mosquitto_message *msg) {
   if (strncmp(msg->topic, "hud", 3) == 0) { ... }
}
```

### Config Access (via `config_parser.h` getters)

```c
element *elements = get_elements();
int count = get_element_count();
// Hot-reloads every 5s via config_manager.c polling
```

### HUD Element Registration (JSON-driven)

Elements are defined in `config.json` with type, position, size, layer. Bitmask-based HUD membership (up to 16 named screens). No code changes needed to add/remove elements.

## File Size Discipline

- **1,500+ lines (C)**: flag as getting large.
- **2,500+ lines**: recommend splitting before adding features.
- **New feature in a large file**: propose a separate module instead.
- **Refactoring large files**: never full rewrites. Incremental extraction — one feature at a time, keep original working, test after each step.

Current large files (monitor):

| File | Lines | Status |
|------|-------|--------|
| `mirage.c` | ~2,550 | At splitting threshold — needs modular extraction (god module, extern coupling) |
| `element_renderer.c` | ~2,200 | Approaching threshold — split by renderer type |
| `config_parser.c` | ~1,600 | Above monitoring threshold |
| `command_processing.c` | ~1,530 | Above monitoring threshold |

## Development Lifecycle

1. **Plan** (non-trivial only) — plan mode + Explore agents for multi-module work.
2. **Implement** — build + format check after each chunk: `make -C build-debug -j$(nproc)` + `./format_code.sh --check`.
3. **Review** — run review agents on the diff for non-trivial changes.
4. **Test** — manual on device (camera, HUD, MQTT, recording). No automated tests yet.
5. **Commit** — final `./format_code.sh --check`, provide `git add` + commit message; **developer runs git commands**.

## License

GPLv3 or later. Every new source file includes the GPL header block.
