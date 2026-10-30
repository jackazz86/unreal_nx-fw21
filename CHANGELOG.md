# Changelog

## unreal_nx-fw21, compared to unreal_nx r2 (unofficial fork, not by fgsfds)

### Firmware
- Rebuilt against libnx 4.12.0, so it works on firmware 21.0.0+ / Atmosphère 1.10+.

### Performance
- New per-texture update limit in the loader (`uploadinterval` in `gyro.cfg`, default 5, range 1 to 8). A texture that was
  just updated is not updated again for N frames; the newest update is kept and sent in time, so no texture stays stale.
  It works with the stock `OpenGLDrv.so` of the 227j patch, so no renderer is replaced.
- In a heavy lit scene (stock renderer): about 41 ms per frame (24 fps) without it, about 29 ms (34 fps) with the default,
  and much lower worst frames. One scene, camera not in exactly the same spot in both runs: indicative only.
- The loader skips texture uploads whose contents are identical to the previous one.
- The shipped `UnrealLinux.ini` keeps dynamic lights on and turns volumetric lighting off.

### Controls
- Gyro aiming, read at the sensor's full rate. Tunable in `gyro.cfg`. L3+R3 (held together) switches it on and off.
- Quick save on MINUS (it used to open the on-screen keyboard, which can no longer be brought up).
- A confirms and B goes back in menus, following the Nintendo layout.
- Left stick response curve and dead zone, adjustable in `gyro.cfg`.

### Stability
- Exiting the game no longer crashes the next launch (libsolder fix, included in `libsolder/`: module destructors are
  skipped during process shutdown).
- A failed `dlopen` prints the real reason, such as a missing symbol.

### Tooling
- `nxstats.txt` in `/switch/unreal/`: average and worst frame time and upload counters, rewritten every 180 frames.
- Built-in profiler (frame, file and GL call timing over nxlink), compiled out unless `NXP_PROFILE` is set to 1.

## r2 and earlier
See [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx/releases).
