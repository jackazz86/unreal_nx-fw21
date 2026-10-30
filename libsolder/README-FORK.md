# libsolder (copy included in unreal_nx-fw21)

This is a copy of [fgsfdsfgs/libsolder](https://github.com/fgsfdsfgs/libsolder), MIT licensed (see `LICENSE`),
made from its commit `36e5f31`, with one change:

* `source/loader.c`: `solder_unload_all()` sets a `solder_quitting` flag, and while it is set the module destructors
  (`dso_finalize`) are skipped. They were crashing in `free()` during process shutdown and left the next launch
  broken. The modules are still unmapped as before.

Build and install with `make install` (devkitA64 and libnx required).
