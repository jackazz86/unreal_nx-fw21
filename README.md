## unreal_nx-fw21 (unofficial fork)

> **This is an unofficial fork of [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx). It is not an official
> release and it is not made by fgsfds.** The loader, the relocation and patching code and the whole idea are his work;
> this fork builds on top of it. If you want the original, go to his repository.

This is a Switch port/loader for Unreal and Unreal Gold.
It loads the aarch64-linux binaries supplied with the 227j patch for Unreal/Unreal Gold, patches them and runs them,
similar in function to the [Max Payne Mobile loader](https://github.com/fgsfdsfgs/max_nx).

**Unreal and Unreal Gold on the Nintendo Switch for firmware 21+: dynamic lights that run at playable speed with the
stock renderer, gyro aiming, quick save, and no crash when you exit the game.**

### What you need

1. Your own copy of **Unreal** or **Unreal Gold**. No game files are included here.
2. The free **[OldUnreal 227j patch](https://www.oldunreal.com/phpBB3/viewtopic.php?f=51&t=10395)**, installed on that copy.
   It provides the engine binaries and the renderer this loader runs.
3. The **zip from the [latest release](../../releases/latest)** of this repository: `unreal.nro`, `gyro.cfg` and an
   `UnrealLinux.ini` with dynamic lights on.

Step-by-step instructions are in [How to install](#how-to-install).

### What this fork changes

fgsfds' r2 runs the game, but it does not start on current firmware, lit scenes run slowly, and exiting the game
crashes the next launch. This fork fixes those, and adds gyro and quick save.

* **Works on firmware 21.0.0+ / Atmosphère 1.10+.** Rebuilt against libnx 4.12.0. Homebrew built with older libnx
  conflicts with the thread-local storage layout introduced in firmware 21.0.0.
* **Dynamic lights are playable, with the renderer that comes with the patch.** Every light update re-uploads a lightmap
  texture, which costs about 0.5 to 1 ms per call on the Switch whatever its size, and a lit scene does dozens of them
  per frame. The loader now limits how often a texture may be updated (`uploadinterval`, default 5 frames): the newest
  update is kept and sent in time, so a texture never stays stale. Identical uploads are skipped as well. In a heavy
  lit scene, with the stock `OpenGLDrv.so`, the frame time went from about 41 ms (24 fps) to about 29 ms (34 fps), and the
  worst frames dropped a lot. These are measurements on one scene with the camera not in exactly the same spot, so take
  them as indicative. The price is that a moving light updates a few frames late; `uploadinterval` is adjustable.
* **Gyro aiming**, read at the sensor's full rate, tunable in `gyro.cfg`, switched on and off with L3+R3.
* **Quick save** on MINUS.
* **Exiting no longer crashes the next launch**, so there is no need to quit out of hbmenu. This is a fix in
  [libsolder](libsolder/README-FORK.md) (included in this repository): module destructors are skipped while the process
  is shutting down (they were crashing in `free()`); the modules are still unmapped as before.

**Smaller changes**

* Left stick has an adjustable response curve and dead zone, so the middle of its range is controllable.
* A and B follow the Nintendo layout in menus (A confirms, B goes back); they were swapped.
* MINUS no longer opens the on-screen keyboard, which means the on-screen keyboard can no longer be brought up.
* A failed `dlopen` now prints the real reason (for example a missing symbol) instead of only "Can't find file for package".
* `nxstats.txt` (in `/switch/unreal/`) is rewritten every 180 frames with the average frame time, the worst frame and how
  many texture uploads were asked for, skipped, held back and sent. Useful for tuning `uploadinterval` or for bug reports.
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

The zip contains `unreal.nro`, `gyro.cfg` and `SystemARM/UnrealLinux.ini`. The renderer (`OpenGLDrv.so`) is the one
installed by the patch and is not replaced. `UnrealLinux.ini` is replaced, as it was in r2, so if you have changed
renderer settings in it, keep a copy. Your `User.ini` (player, keys, field of view) is not touched.

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

They are on by default, and that is how the shipped `UnrealLinux.ini` is set. The setting that matters is in
`/switch/unreal/gyro.cfg`:

```
uploadinterval = 5
```

`1` updates lightmaps every frame (best looking, slowest, the same as r2). `5` is the default. Lower values look
better when a light moves; higher values are faster in the heaviest scenes. The range is 1 to 8.

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
The upload limit only applies to the default renderer path it was measured on.

If the game crashes, usually there will be a message on the screen telling you the reason.
It will also create logs at `SystemARM/fatal.log` and `SystemARM/UnrealLinux.log`.
Please attach these to issue reports when possible, along with the last Atmosphère crash report.

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
* libsolder: the copy in the [`libsolder`](libsolder) folder of this repository, which has the exit crash fix
  (`make -C libsolder install`). It is a fork of [fgsfdsfgs/libsolder](https://github.com/fgsfdsfgs/libsolder).
* [libiconv-switch](https://github.com/snaiperskaya96/libiconv-switch) (use `./build-switch.sh` to build and install)
* libsndfile
* libxmp
* libalure

See [here](https://gist.github.com/fgsfdsfgs/dfb38bb86188e54f362c450353c8c448) on how to install the last three libraries.
The rest can be obtained using `(dkp-)pacman`.

After you've obtained all the dependencies and ensured devkitA64 is properly installed and the `DEVKITPRO` environment variable is set,
build this repository using the commands:
```
git clone https://github.com/jackazz86/unreal_nx-fw21.git && cd unreal_nx-fw21
source $DEVKITPRO/switchvars.sh
make -C libsolder install
make
```

To turn on the built-in profiler, set `NXP_PROFILE` to `1` near the top of `source/exports.c`, rebuild, and run through `nxlink -s`.

### Credits
* fgsfds for unreal_nx and libsolder, which this fork is built on;
* jackazz86, who maintains this fork and tested every change on a real Switch;
* Claude (Anthropic), an AI coding assistant, which worked with jackazz86 on the changes in this fork (the upload limit,
  gyro, quick save, the libsolder fix and the documentation). The commits carry it as co-author;
* OldUnreal for the 227j patch, which provides the engine this loader runs;
* Switchbrew for libnx;
* devkitPro for devkitA64.

### Third-party libraries
The `.nro` is linked statically against the libraries listed under "How to build". Several of them, such as OpenAL Soft,
libxmp, libsndfile and libiconv, are LGPL; the others use permissive licenses. See each project for its exact terms.
The full source and build instructions for this program are in this repository, so it can be relinked against other
versions of those libraries.

### Legal
This project is an unofficial fork of [fgsfdsfgs/unreal_nx](https://github.com/fgsfdsfgs/unreal_nx), released under the same MIT license.
It has no direct affiliation with Epic Games, OldUnreal or fgsfds.
No game assets and no program code from the original game are included: the engine and the renderer are the ones
installed by the OldUnreal patch, which you provide.
