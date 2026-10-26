## UnrealNX (fork)

This is a Switch port/loader for Unreal and Unreal Gold.
It loads the aarch64-linux binaries supplied with the 227j patch for Unreal/Unreal Gold, patches them and runs them,
similar in function to the [Max Payne Mobile loader](https://github.com/fgsfdsfgs/max_nx).

This is a fork of [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx). The loader, the relocation and patching
code and the whole idea are fgsfds' work; this fork builds on top of it. Everything that changed is listed below.

**Unreal and Unreal Gold on the Nintendo Switch, with gyro aiming, dynamic lights that run at playable speed,
quick save, and no crash when you exit the game.**

### What you need

1. Your own copy of **Unreal** or **Unreal Gold**. No game files are included here.
2. The free **[OldUnreal 227j patch](https://www.oldunreal.com/phpBB3/viewtopic.php?f=51&t=10395)**, installed on that copy.
   It provides the engine binaries this loader runs.
3. The **zip from the [latest release](../../releases/latest)** of this repository. It has everything else:
   `unreal.nro`, `gyro.cfg`, a rebuilt `OpenGLDrv.so` that replaces the one from the patch, and an `UnrealLinux.ini`
   with dynamic lights on and the settings used for testing.

Step-by-step instructions are in [How to install](#how-to-install).

### What this fork changes

r2 runs the game, but lit scenes run slowly, exiting the game crashes the next launch, and builds made with older libnx
conflict with current firmware. This fork is not only a rebuild: it goes into the renderer, which r2
could only run as a closed binary, and fixes the performance problem at its source.

**Headlines**

* **Works on current firmware.** Rebuilt against libnx 4.12.0. Homebrew built with older libnx conflicts with the
  thread-local storage layout of firmware 21.0.0+ / Atmosphère 1.10+.
* **Dynamic lights are playable.** r2 suggested turning them off. The cost was never the lighting itself but re-uploading
  lightmap textures: about half a millisecond for every texture touched, and a lit scene touched around a hundred per
  frame. The renderer module, `OpenGLDrv.so`, is now **rebuilt from source** (OldUnreal's public
  [Unreal-PubSrc](https://github.com/OldUnreal/Unreal-PubSrc)) with a setting, `NXLightUpdateInterval`, that refreshes
  each lightmap at most once every N frames. In the heaviest scene tested a frame went from about 125 ms to about 19 ms,
  with the lights still on. On top of that the loader skips identical texture uploads (about 85% of the calls).
* **Gyro aiming**, read at the sensor's full rate, tunable in `gyro.cfg`, switched on and off with L3+R3.
* **Quick save** on MINUS.
* **Exiting no longer crashes the next launch**, so there is no need to quit out of hbmenu. This is a fix in
  [libsolder](https://github.com/jackazz86/libsolder): module destructors are skipped while the process is shutting
  down (they were crashing in `free()`); the modules are still unmapped as before.

**Smaller changes**

* Left stick has an adjustable response curve and dead zone, so the middle of its range is controllable.
* A and B follow the Nintendo layout in menus (A confirms, B goes back); they were swapped.
* MINUS no longer opens the on-screen keyboard, which means the on-screen keyboard can no longer be brought up.
* A failed `dlopen` now prints the real reason (for example a missing symbol) instead of only "Can't find file for package".
* A built-in profiler (frame, file and GL call timing over nxlink), compiled out by default.

See [CHANGELOG.md](CHANGELOG.md) for the same list in release-notes form.

### How to install

You're going to need an Unreal or Unreal Gold installation patched with the [OldUnreal 227j patch](https://www.oldunreal.com/phpBB3/viewtopic.php?f=51&t=10395).
Unfortunately oldunreal.com seems to disallow direct linking to files, so click the link at the bottom of that post to get it.

Ensure that the patch is installed correctly: when installing **make sure to tick every box, but only tick "Return to Na Pali support" if you have Unreal Gold**.
If you don't have a folder called `SystemARM` in the Unreal folder, you need to reinstall the patch. Simply run the installer again to do that.

To install:
1. Create a folder called `unreal` in the `switch` folder on your SD card.
2. Copy the contents of your `Unreal` or `UnrealGold` folder into `/switch/unreal/`.
3. Extract the contents of the `.zip` file from the latest release into `/switch/unreal/`. Replace everything.

The zip contains `unreal.nro`, `gyro.cfg`, `SystemARM/OpenGLDrv.so` and `SystemARM/UnrealLinux.ini`.
`OpenGLDrv.so` **replaces** the one that came with the 227j patch; if you ever want the stock one back, rerun the patch
installer. `UnrealLinux.ini` is replaced too, as it was in r2, so if you have changed renderer settings in it, keep a copy.
Your `User.ini` (player, keys, field of view) is not touched.

No game assets and no closed engine binaries are included in the zip.

### Controls

| Input | Action |
|---|---|
| Left stick | Move |
| Right stick | Look |
| Gyro | Aim, added on top of the right stick |
| L3 + R3 (hold both) | Gyro on / off |
| A / B in menus | Confirm / back |
| MINUS | Quick save |
| PLUS | Menu |

### Gyro

Two gestures drive turning and both work at once: tilting the console (or controller) sideways, and sweeping the whole
controller left and right. They land on different sensor axes, so whichever you naturally do is the one that works,
handheld or with a Pro Controller.

Everything is tuned in `/switch/unreal/gyro.cfg` (next to `unreal.nro`, not inside `SystemARM`). It is read at every
start; change a number, relaunch, try. Every setting is documented inside the file.

* `yaw`, `pitch`, `roll`: sensitivity per axis. They are signed, so flip the sign if an axis goes the wrong way. Larger
  magnitude is faster.
* `enabled = 0`: start with the gyro off (L3+R3 still toggles it).
* `movecurve`, `deadzone`: left stick response.

### Dynamic lights

They are on by default, and that is how the shipped `UnrealLinux.ini` is set. The only related setting you may want to
change is in `SystemARM/UnrealLinux.ini`, section `[OpenGLDrv.OpenGLRenderDevice]`:

```
NXLightUpdateInterval=3
```

`1` updates lightmaps every frame (best looking, slowest). `3` is the default. Higher values are faster, at the cost of
lights reacting a little later to a moving light source.

Volumetric lighting is turned off in the shipped file because it costs speed for a small visual gain. To get it back, set
`VolumetricLighting` to `True` (it appears twice in the file).

A small CPU overclock helps in the heaviest scenes. This is optional and entirely at your own risk; this program does not
change clocks itself.

### Notes

This will only work with Unreal/UnrealGold 227j right now. If 227k is released, the loader will most likely have to be updated to support it.

This will likely not work in applet/album mode. Use a proper game override.

Trying to join a multiplayer game or opening the server browser will likely cause the game to crash.
Hosting a multiplayer game or playing Botmatch seems to work, though.

If you see any `appThrowf` errors, first try to re-extract the `.zip` file again, replacing everything.

Return to Na Pali may crash on new game with an error about `UPakFonts`.
To fix that get the original 3 MB `UPakFonts.utx` file from the `Textures` folder on your Unreal Gold CD
or unpatched Unreal Gold install and replace the one in `/switch/unreal/Textures`.

You can enable the advanced XOpenGL renderer, though it hasn't been tested much, does not seem to improve performance
and will probably explode. To do that, in `SystemARM/UnrealLinux.ini` change `GameRenderDevice=OpenGLDrv.OpenGLRenderDevice` to `GameRenderDevice=XOpenGLDrv.XOpenGLRenderDevice`.
The rebuilt `OpenGLDrv.so` only replaces the default renderer.

If the game crashes, usually there will be a message on the screen telling you the reason.
It will also create logs at `SystemARM/fatal.log` and `SystemARM/UnrealLinux.log`.
Please attach these to issue reports when possible, along with the last Atmosphere crash report.

### How to build

You're going to need devkitA64, libnx and the following libraries:
* switch-mesa
* switch-libdrm_nouveau
* switch-sdl2
* switch-libogg
* switch-libvorbis
* switch-flac
* switch-opus
* switch-libsodium
* switch-openal
* devkitpro-pkgbuild-helpers
* [libsolder](https://github.com/jackazz86/libsolder) (this fork, not the original: it has the exit crash fix; use `make install` to build and install)
* [libiconv-switch](https://github.com/snaiperskaya96/libiconv-switch) (use `./build-switch.sh` to build and install)
* libsndfile
* libxmp
* libalure

See [here](https://gist.github.com/fgsfdsfgs/dfb38bb86188e54f362c450353c8c448) on how to install the last three libraries.
The rest can be obtained using `(dkp-)pacman`.

After you've obtained all the dependencies and ensured devkitA64 is properly installed and the `DEVKITPRO` environment variable is set,
build this repository using the commands:
```
git clone https://github.com/jackazz86/unreal_nx.git && cd unreal_nx
source $DEVKITPRO/switchvars.sh
make
```

To turn on the built-in profiler, set `NXP_PROFILE` to `1` near the top of `source/exports.c`, rebuild, and run through `nxlink -s`.

`OpenGLDrv.so` is built separately from a fork of OldUnreal's public source:
[jackazz86/Unreal-PubSrc](https://github.com/jackazz86/Unreal-PubSrc), see the `switch/` folder there.

### Credits
* fgsfds for unreal_nx and libsolder, which this fork is built on;
* OldUnreal for the 227j patch and for publishing the source of the renderer;
* Smirftsch of OldUnreal, who kindly took the time to answer by email about redistributing the rebuilt renderer;
* Chris Dohnal, author of UTGLR, which `OpenGLDrv` is derived from;
* Switchbrew for libnx;
* devkitPro for devkitA64.

### Third-party libraries
The `.nro` is linked statically against the libraries listed under "How to build". Several of them, such as OpenAL Soft,
libxmp, libsndfile and libiconv, are LGPL; the others use permissive licenses. See each project for its exact terms.
The full source and build instructions for this program are in this repository, so it can be relinked against other
versions of those libraries.

### Legal
This project is a fork of [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx), released under the same MIT license.
It has no direct affiliation with Epic Games or OldUnreal.
No game assets and no closed program code from the original game are included. `OpenGLDrv.so` is compiled from source code
publicly released by OldUnreal ([Unreal-PubSrc](https://github.com/OldUnreal/Unreal-PubSrc)), a modified version of Chris Dohnal's UTGLR;
OldUnreal confirmed they place no restriction on it.
