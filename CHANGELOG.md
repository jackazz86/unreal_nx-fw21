# Changelog

## Fork, compared to unreal_nx r2

### Performance
- `OpenGLDrv.so` is rebuilt from source (OldUnreal's public Unreal-PubSrc) instead of running the stock closed module.
  Making it load against the 227j engine needed a set of compatibility fixes, described in the Unreal-PubSrc fork
  (`switch/` folder).
- New setting `NXLightUpdateInterval` (in `UnrealLinux.ini`, `[OpenGLDrv.OpenGLRenderDevice]`, default 3): each realtime
  lightmap is re-uploaded at most once every N frames. About 125 ms to about 19 ms per frame in the heaviest scene tested,
  with dynamic lights on.
- The loader skips texture uploads whose contents are identical to the previous one (about 85% of the calls).
- The shipped `UnrealLinux.ini` keeps dynamic lights on, turns volumetric lighting off and sets `NXLightUpdateInterval=3`.

### Controls
- Gyro aiming, read at the sensor's full rate. Tunable in `gyro.cfg`. L3+R3 (held together) switches it on and off.
- Quick save on MINUS (it used to open the on-screen keyboard, which can no longer be brought up).
- A confirms and B goes back in menus, following the Nintendo layout.
- Left stick response curve and dead zone, adjustable in `gyro.cfg`.

### Stability
- Exiting the game no longer crashes the next launch (libsolder fix: module destructors are skipped during process
  shutdown).
- Rebuilt against libnx 4.12.0 for firmware 21.0.0+ / Atmosphère 1.10+.
- A failed `dlopen` prints the real reason, such as a missing symbol.

### Tooling
- Built-in profiler (frame, file and GL call timing over nxlink), compiled out unless `NXP_PROFILE` is set to 1.

## r2 and earlier
See [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx/releases).
