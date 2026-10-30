#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <setjmp.h>
#include <wchar.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <iconv.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <malloc.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <switch.h>
#include <solder.h>

#include <SDL2/SDL.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/alure.h>

#include <xmp.h>

#include "utils.h"
#include "exports.h"

// Built-in frame/IO/GL-call profiler, compiled out by default.  Set to 1 and
// rebuild to get per-frame timing ([nxp]/[nxr]/[slow]/[dip]/[nxflush]) over
// nxlink - useful for diagnosing a report of stutter or slowdown without
// needing a special debug build first.  Off by default so a normal build
// pays no per-call armGetSystemTick() cost.
#ifndef NXP_PROFILE
#define NXP_PROFILE 0
#endif

#define TIMER_FD 0x56435643
#define TRIGGER_THRESHOLD 0x6000
#define AXIS_DEADZONE 0x400

static u64 start_tick;

static SDL_GameController *sdl_ctrl;
static u8 sdl_keys[SDL_NUM_SCANCODES];

FILE *export_stdin;
FILE *export_stdout;
FILE *export_stderr;

char *fake_argv[MAX_FAKE_ARGC];
int fake_argc;

// local libstdc++ crap that needs to be exported
extern int __cxa_atexit(void *, void *, void *) __attribute__((used));
extern void _Unwind_Resume(void *) __attribute__((used));

// these are not visible by default for some reason
extern int usleep(useconds_t u) __attribute__((used));
extern unsigned sleep(unsigned int s) __attribute__((used));

// can't be arsed with GNU macros
extern void sincos(double, double *, double *);
extern void sincosf(float, float *, float *);

/* general libc stuff */

// general purpose dummy function
static void dummy_fn(void) { }

// Report why a module failed to load.  solder makes dlopen fail if ANY symbol
// stays unresolved, and the engine turns that NULL into the far less useful
// "Can't find file for package X", so the real reason never reaches the log.
static void *wrap_dlopen(const char *fname, int flags) {
  void *h = solder_dlopen(fname, flags);
  if (!h) {
    const char *e = solder_dlerror();
    printf("[nxdl] dlopen(\"%s\") FAILED: %s\n", fname ? fname : "(null)", e ? e : "(no error)");
    fflush(stdout);
  }
  return h;
}

// exception handling is completely fucked anyway
void wrap_cxa_throw(void *exdata, void *exinfo, void (*exdtor)(void *)) {
  fatal_error("throw called:\n%ls\n", exdata ? (const wchar_t *)exdata : L"NULL");
}

void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func) {
  fatal_error("ASSERTION FAILED AT %s:%u (%s):\n`%s`", file, line, func, expr);
}

// not actually used
int pthread_setaffinity_np(pthread_t thr, size_t cssize, const void *cs) { return ENOSYS; }

// unsupported
int pthread_kill(pthread_t thread, int sig) { return -1; }

// this is pretty much a no-op for our purposes
char *realpath(const char *restrict path, char *restrict resolved_path) {
  printf("realpath(%s) called\n", path);
  if (!path || !resolved_path) return NULL;
  strcpy(resolved_path, path);
  return resolved_path;
}

const unsigned short **__ctype_b_loc(void) {
  static const unsigned short tab[384] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
    0x200, 0x320, 0x220, 0x220, 0x220, 0x220, 0x200, 0x200,
    0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
    0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200, 0x200,
    0x160, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
    0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
    0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8, 0x8d8,
    0x8d8, 0x8d8, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
    0x4c0, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8d5, 0x8c5,
    0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5,
    0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5, 0x8c5,
    0x8c5, 0x8c5, 0x8c5, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x4c0,
    0x4c0, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8d6, 0x8c6,
    0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6,
    0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6, 0x8c6,
    0x8c6, 0x8c6, 0x8c6, 0x4c0, 0x4c0, 0x4c0, 0x4c0, 0x200,
  };
  static const unsigned short *tabptr = tab + 128;
  return &tabptr;
}

// TODO: maybe return username
char *getlogin(void) { 
  static char username[] = "Player";
  return username;
}

// probably don't need any real values
long pathconf(const char *path, int name) {
  printf("pathconf(%s, %d) called\n", path, name);
  return -1;
}

// only 2 config vars are actually read
long sysconf(int name) {
  printf("sysconf(%d) called\n", name);
  switch (name) {
    case 30: // PAGESIZE
      return 0x1000;
    case 83: // NPROCESSORS
      return 3; // as far as I can tell this is only used for setaffinity
    default:
      errno = EINVAL;
      return -1;
  }
}

// this is used just to get the total memory size
int sysinfo(unsigned char *info) {
  extern char *fake_heap_start;
  extern char *fake_heap_end;
  const u64 memunit = 1024; // KB
  const u64 memsize = (fake_heap_end - fake_heap_start) / memunit;
  *(u64 *)(info + 0x20) = memsize; // info->totalram
  *(u64 *)(info + 0x68) = memunit; // info->mem_unit
  return 0;
}

// this is used for tracking max rss only
int getrusage(int who, struct rusage *ru) {
  static size_t max_rss = 0;
  if (who == RUSAGE_SELF) {
    // feed it max malloc'd space instead
    struct mallinfo mi = mallinfo();
    if (mi.uordblks > max_rss)
      max_rss = mi.uordblks;
    // the field we need should be right after the fields we have
    *(long long *)&ru[1] = max_rss;
    return 0;
  }
  return -1;
}

// all the multi-process crap is not supported
pid_t fork(void) { errno = ENOSYS; return -1; }
int execve(const char *filename, char *const argv [], char *const envp[]) { errno = ENOSYS; return -1; }
int execl(const char *path, const char *arg, ...) { errno = ENOSYS; return -1; }
pid_t waitpid(pid_t pid, int *status, int options) { errno = ENOSYS; return -1; }
int system(const char *str) { errno = ENOSYS; return -1; }

// neither is most of syscall(), but
long syscall(long id, ...) {
  printf("syscall(%ld) called\n", id);
  if (id == 241 /* SYS_perf_event_open */) {
    // it's going to use this event for the perf timer
    start_tick = armGetSystemTick();
    printf(" - timer started at %lu\n", start_tick);
    return TIMER_FD;
  }
  errno = ENOSYS;
  return -1;
}

// signals can go fuck themselves tbh but we actually do have limited support for them
int sigaction(int sig, const struct sigaction *ap, struct sigaction *op) {
  // we wanna catch SIGILL and SIGSEGV ourselves
  if (ap && ap->sa_handler && sig != SIGILL && sig != SIGSEGV) {
    void *old_handler = signal(sig, ap->sa_handler);
    if (old_handler == SIG_ERR)
      return -1;
    return 0;
  }
  errno = EINVAL;
  return -1;
}

// used in appGetLocalIP, which falls back to 0.0.0.0
int getifaddrs(void **pp) { errno = ENOSYS; return -1; }
void freeifaddrs(void *p) { }

// long doubles are handled via soft floats for some reason
int __eqtf2(long double a, long double b) { return a != b; }
int __unordtf2(long double a, long double b) { return a - b; }
long double __addtf3(long double a, long double b) { return a + b; }
long double __subtf3(long double a, long double b) { return a - b; }
long double __multf3(long double a, long double b) { return a * b; }
long double __floatsitf(int i) { return (long double)i; }
long double __floatunsitf(unsigned int i) { return (long double)i; }
unsigned int __fixunstfsi(long double x) { return (unsigned int)x; }
int __fixtfsi(long double x) { return (int)x; }
long double __extenddftf2(double x) { return (long double)x; }
long double __netf2(long double a, long double b) { return a != b; }

// can't backtrace from here
int backtrace(void **buffer, int size) { return 0; }
char **backtrace_symbols(void *const *buffer, int size) { return NULL; }

// don't have these
void *getpwent(void) { return NULL; }
uid_t getuid(void) { return 1; }

// h_errno is threadlocal here so I wonder if this will work correctly
int *__h_errno_location(void) { return &h_errno; }

// these are macros in our libc
static int tolower_fn(int c) { return tolower(c); }
static int toupper_fn(int c) { return toupper(c); }
static int sigemptyset_fn(sigset_t *set) { return sigemptyset(set); }

// their timespec is different
struct timespec64 {
  s64 tv_sec;
  s64 tv_nsec;
};
static int wrap_clock_gettime(clockid_t id, struct timespec64 *tp) {
  struct timespec tv;
  const int ret = clock_gettime(CLOCK_REALTIME, &tv);
  tp->tv_sec = tv.tv_sec;
  tp->tv_nsec = tv.tv_nsec;
  return ret;
}

// their struct stat is different
struct stat64 {
  s64 st_dev;
  s64 st_ino;
  s64 st_nlink;
  s32 st_mode;
  s32 st_uid;
  s32 st_gid;
  s32 __pad0;
  s64 st_rdev;
  u64 st_size;
  u64 st_blksize;
  u64 st_blocks;
  struct timespec64 st_atim;
  struct timespec64 st_mtim;
  struct timespec64 st_ctim;
  u64 __pad1[3];
};
// some weird fucking posix versioning thing
int __xstat(int ver, char *restrict path, struct stat64 *sbuf) { 
  struct stat s;
  const int ret = stat(path, &s);
  if (ret == 0) {
    sbuf->st_dev = s.st_dev;
    sbuf->st_ino = s.st_ino;
    sbuf->st_nlink = s.st_nlink;
    sbuf->st_mode = s.st_mode;
    sbuf->st_uid = s.st_uid;
    sbuf->st_gid = s.st_gid;
    sbuf->st_rdev = s.st_rdev;
    sbuf->st_size = s.st_size;
    sbuf->st_blksize = s.st_blksize;
    sbuf->st_blocks = s.st_blocks;
    sbuf->st_atim.tv_sec = s.st_atim.tv_sec;
    sbuf->st_atim.tv_nsec = s.st_atim.tv_nsec;
    sbuf->st_mtim.tv_sec = s.st_atim.tv_sec;
    sbuf->st_mtim.tv_nsec = s.st_atim.tv_nsec;
    sbuf->st_ctim.tv_sec = s.st_atim.tv_sec;
    sbuf->st_ctim.tv_nsec = s.st_atim.tv_nsec;
  }
  return ret;
}

// their struct dirent is different
struct dirent64 {
  s64 d_ino;
  u64 d_off;
  u16 d_reclen;
  u8 d_type;
  char d_name[256 + 1];
};
static struct dirent64 *wrap_readdir(DIR *dirp) {
  // statically allocating this is allowed by spec I think
  static struct dirent64 udent = { .d_reclen = sizeof(udent) };
  struct dirent *dent = readdir(dirp);
  if (dent) {
    udent.d_ino = dent->d_ino;
    udent.d_type = dent->d_type;
    strncpy(udent.d_name, dent->d_name, 256);
    return &udent;
  } else {
    return NULL;
  }
}

#if NXP_PROFILE
static u64 nxp_io_calls, nxp_io_ticks, nxp_io_worst;
static u64 nxp_op_calls, nxp_op_ticks, nxp_op_worst;
// per-frame snapshots, for the slow-frame report below
static u64 nxp_pf_io_t, nxp_pf_io_c, nxp_pf_up_t, nxp_pf_draw_t, nxp_pf_bind_t, nxp_pf_swap_t;
static u64 nxp_pf_op_t, nxp_pf_op_c;
// A drop from 60fps to 45 is a 22ms frame.  The old 80ms threshold never saw
// one, and a dip shorter than the 2s reporting window vanishes into its
// average - which is why days of "it stutters" kept coming back as "16.7ms".
static unsigned nxp_dip_printed;
#endif

// nxu: the engine's file access does not have to go through the read() we
// export.  A module calling fread() gets the NRO's own stdio, which reaches
// newlib's _read_r directly and never touches our wrapper - so "read 0 calls"
// during a 4-second freeze proves nothing until stdio is covered too.  These
// land in the same bucket as read(), and fopen/fseek get their own, since an
// open can block on its own.
static FILE *wrap_fopen(const char *path, const char *mode) {
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  FILE *f = fopen(path, mode);
  const u64 dt = armGetSystemTick() - t0;
  nxp_op_ticks += dt; nxp_op_calls++;
  if (dt > nxp_op_worst) nxp_op_worst = dt;
  return f;
#else
  return fopen(path, mode);
#endif
}

static size_t wrap_fread(void *buf, size_t sz, size_t n, FILE *f) {
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  const size_t r = fread(buf, sz, n, f);
  const u64 dt = armGetSystemTick() - t0;
  nxp_io_ticks += dt; nxp_io_calls++;
  if (dt > nxp_io_worst) nxp_io_worst = dt;
  return r;
#else
  return fread(buf, sz, n, f);
#endif
}

static int wrap_fseek(FILE *f, long off, int whence) {
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  const int r = fseek(f, off, whence);
  const u64 dt = armGetSystemTick() - t0;
  nxp_op_ticks += dt; nxp_op_calls++;
  if (dt > nxp_op_worst) nxp_op_worst = dt;
  return r;
#else
  return fseek(f, off, whence);
#endif
}

static off_t wrap_lseek(int fd, off_t off, int whence) {
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  const off_t r = lseek(fd, off, whence);
  const u64 dt = armGetSystemTick() - t0;
  nxp_op_ticks += dt; nxp_op_calls++;
  if (dt > nxp_op_worst) nxp_op_worst = dt;
  return r;
#else
  return lseek(fd, off, whence);
#endif
}

static size_t wrap_read(int fd, void *buf, size_t count) {
  if (fd == TIMER_FD) {
    // it's trying to read from the perf event fd, give it the time
    *(u64 *)buf = armGetSystemTick() - start_tick;
    return count;
  }
  // normal read
#if NXP_PROFILE
  {
    const u64 t0 = armGetSystemTick();
    const size_t r = read(fd, buf, count);
    const u64 dt = armGetSystemTick() - t0;
    nxp_io_ticks += dt;
    nxp_io_calls++;
    if (dt > nxp_io_worst) nxp_io_worst = dt;
    return r;
  }
#else
  return read(fd, buf, count);
#endif
}

/* socket stuff: have to wrap these because of differing structs and constants */

struct sockaddr_linux {
  u16 sa_family;
  u8 sa_data[14];
};

struct sockaddr_in_linux {
  u16 sin_family;
  u16 sin_port;
  struct in_addr sin_addr;
  u8 sin_zero[8];
};

static inline int sockopt_cvt(const int opt) {
  switch (opt) {
    case 2:  return SO_REUSEADDR;
    case 3:  return SO_TYPE;
    case 4:  return SO_ERROR;
    case 5:  return SO_DONTROUTE;
    case 6:  return SO_BROADCAST;
    case 7:  return SO_SNDBUF;
    case 8:  return SO_RCVBUF;
    case 9:  return SO_KEEPALIVE;
    case 10: return SO_OOBINLINE;
    default:
      printf("sockopt_cvt(): unknown option %d\n", opt);
      return -1;
  }
}

static inline void laddr_to_saddr(struct sockaddr *dst, const struct sockaddr_linux *src) {
  if (dst && src) {
    // it only uses sockaddr_in anyway
    struct sockaddr_in *saddr_in = (struct sockaddr_in *)dst; 
    const struct sockaddr_in_linux *laddr_in = (const struct sockaddr_in_linux *)src;
    saddr_in->sin_len = sizeof(*saddr_in);
    saddr_in->sin_family = laddr_in->sin_family;
    saddr_in->sin_port = laddr_in->sin_port;
    saddr_in->sin_addr = laddr_in->sin_addr;
  }
}

static inline void saddr_to_laddr(struct sockaddr_linux *dst, const struct sockaddr *src) {
  if (dst && src) {
    // it only uses sockaddr_in anyway
    struct sockaddr_in_linux *laddr_in = (struct sockaddr_in_linux *)dst; 
    const struct sockaddr_in *saddr_in = (const struct sockaddr_in *)src;
    laddr_in->sin_family = saddr_in->sin_family;
    laddr_in->sin_port = saddr_in->sin_port;
    laddr_in->sin_addr = saddr_in->sin_addr;
  }
}

static int wrap_connect(int fd, const struct sockaddr_linux *addr, socklen_t len) {
  struct sockaddr saddr = { 0 };
  laddr_to_saddr(&saddr, addr);
  return connect(fd, &saddr, len);
}

static int wrap_bind(int fd, const struct sockaddr_linux *addr, socklen_t len) {
  struct sockaddr saddr = { 0 };
  laddr_to_saddr(&saddr, addr);
  return bind(fd, &saddr, len);
}

static ssize_t wrap_sendto(int fd, const void *buf, size_t n, int flags, const struct sockaddr_linux *addr, socklen_t addr_len) {
  struct sockaddr saddr = { 0 };
  laddr_to_saddr(&saddr, addr);
  return sendto(fd, buf, n, flags, &saddr, addr_len);
}

static ssize_t wrap_recvfrom(int fd, void *buf, size_t n, int flags, struct sockaddr_linux *addr, socklen_t *addr_len) {
  struct sockaddr saddr = { 0 };
  const int rc = recvfrom(fd, buf, n, flags, &saddr, addr_len);
  saddr_to_laddr(addr, &saddr);
  return rc;
}

static int wrap_accept(int fd, struct sockaddr_linux *addr, socklen_t *addr_len) {
  struct sockaddr saddr = { 0 };
  laddr_to_saddr(&saddr, addr);
  return accept(fd, &saddr, addr_len);
}

static int wrap_getsockname(int fd, struct sockaddr_linux *addr, socklen_t *len) {
  struct sockaddr saddr = { 0 };
  const int rc = getsockname(fd, &saddr, len);
  saddr_to_laddr(addr, &saddr);
  return rc;
}

static int wrap_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
  if (level == 1) level = SOL_SOCKET; // this is also different for some reason
  optname = sockopt_cvt(optname);
  if (optname < 0) return -1;
  return setsockopt(sockfd, level, optname, optval, optlen);
}

static int wrap_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
  if (level == 1) level = SOL_SOCKET; // this is also different for some reason
  optname = sockopt_cvt(optname);
  if (optname < 0) return -1;
  return getsockopt(sockfd, level, optname, optval, optlen);
}

static int wrap_fcntl(int fd, int cmd, int flag, ...) {
  if (cmd == F_SETFL && (flag & 0x800)) {
    flag &= ~0x800;
    flag |= O_NONBLOCK;
    return fcntl(fd, cmd, flag);
  }
  return fcntl(fd, cmd, flag);
}

/* SDL/GL stuff */

static int wrap_SDL_GL_SetAttribute(SDL_GLattr attr, int val) {
  int ret = -1;
  switch (attr) {
    case SDL_GL_MULTISAMPLEBUFFERS:
    case SDL_GL_MULTISAMPLESAMPLES:
    case SDL_GL_DOUBLEBUFFER:
      printf("SDL_GL_SetAttribute(%x, %d) - inhibited\n", attr, val);
      break;
    default:
      ret = SDL_GL_SetAttribute(attr, val);
      break;
  }
  return ret;
}

static SDL_Window *wrap_SDL_CreateWindow(const char *title, int x, int y, int w, int h, u32 flags) {
  // drop any extra flags it might be passing in
  SDL_Window *win = SDL_CreateWindow(title, 0, 0, w, h, flags & SDL_WINDOW_OPENGL);
  printf("SDL_CreateWindow(`%s`, %d, %d, %d, %d, 0x%08x) -> %p\n", title, x, y, w, h, flags, win);
  if (!win) printf(" - failed: %s\n", SDL_GetError());
  return win;
}

static SDL_GLContext wrap_SDL_GL_CreateContext(SDL_Window *win) {
  // ensure we have proper GL
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
  printf("SDL_GL_CreateContext(%p) -> %p\n", win, ctx);
  if (!ctx) printf(" - failed: %s\n", SDL_GetError());
  return ctx;
}

static const char *(*real_glGetString)(u32) = NULL;
static const char *wrap_glGetString(u32 id) {
  if (id == 0x1F02)
    return "4.6"; // return nonsense so that glad loads every available function
  if (real_glGetString)
    return real_glGetString(id);
  return NULL;
}

static void (*real_glShaderSource)(u32, int, char **, int *);
static void wrap_glShaderSource(u32 shader, int num, char **strs, int *lens) {
  if (!strs) return;
  // patch #version to say "430     " instead of "4*0 core", for XOpenGLDrv support
  for (int i = 0; i < num; ++i) {
    char *p = strstr(strs[i], "#version 4");
    if (p) memcpy(p + 10, "30     ", 7);
  }
  if (real_glShaderSource)
    real_glShaderSource(shader, num, strs, lens);
}

// ===========================================================================
// GL upload profiling (Switch)
//
// The README blames dynamic lights for the poor performance, and the same
// claim on Serious Engine 1 turned out to be true about the cause but wrong
// about the remedy: on this driver each glTexImage2D call costs a fixed few
// milliseconds *regardless of texture size*, so re-uploading a mip chain every
// frame is what actually hurts. Cutting the number of calls there took a scene
// from 2.5 to 58 fps with the lights still on.
//
// Before assuming the same thing here, measure it. Two rules learned the hard
// way on that engine:
//   - a time without a count is ambiguous. A big total means either many cheap
//     calls (batch them) or few expensive ones (something else entirely), and
//     guessing between those wasted several rounds.
//   - averages cannot see hitches. A 2s window holds ~120 frames, so one 100ms
//     stall moves the mean by under 1ms. Track the worst single frame and what
//     happened inside it.
// ===========================================================================

// --- file I/O profiling ----------------------------------------------------
// The worst frames measured so far (905ms, 391ms) contained almost no texture
// uploads, so the time is being spent outside the GL path. With the GPU idle
// at ~10% the process is not computing either - it is waiting. Reading a
// package off the SD card for the first time is the obvious suspect, so time
// open() and read() and keep the worst single call.

#if NXP_PROFILE
static u64 nxg_bind_calls, nxg_bind_ticks, nxg_win_bind, nxg_win_bind_t;
static u64 nxg_draw_calls, nxg_draw_ticks, nxg_win_draw, nxg_win_draw_t;
static u64 nxg_swap_ticks, nxp_win_swap_t2;
#endif

// --- redundant upload elimination -----------------------------------------
// Measured: each glTexSubImage2D costs ~1ms in the driver regardless of size,
// and a bad scene issues ~144 of them per frame - 144ms of a 195ms frame, with
// the GPU idle. Cutting the COUNT is therefore the only lever that matters.
//
// Lightmaps for static geometry lit by a stationary light get rewritten with
// identical bytes every frame. Skipping a write of bytes that are already
// there cannot change what is rendered, so this is exact rather than an
// approximation - no visual trade-off, just work that was never needed.
//
// Safety: the comparison is a full memcmp against the previous contents, not a
// hash, so a match really is a match. Uploads whose memory layout we cannot be
// sure of (non-default row length, or a format we do not know the stride for)
// are passed straight through untouched.
#define NXD_MAX_BYTES 16384
#define NXD_SLOTS     1024

typedef struct {
  unsigned tex, lvl, x, y, w, h, fmt, typ, size;
  unsigned char *data;
} nxd_slot_t;

static nxd_slot_t nxd_cache[NXD_SLOTS];
static unsigned   nxd_bound_tex = 0;
static int        nxd_enabled = 1;   // 0 = deduplicazione disattivata
static u64        nxd_skipped = 0;
#if NXP_PROFILE
static u64        nxd_win_skipped = 0;
#endif

static void (*real_glBindTexture)(unsigned, unsigned);
static void (*real_glGetIntegerv)(unsigned, int *);

static void wrap_glBindTexture(unsigned target, unsigned tex) {
  if (target == 0x0DE1 /* GL_TEXTURE_2D */) nxd_bound_tex = tex;
  if (!real_glBindTexture) return;
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  real_glBindTexture(target, tex);
  nxg_bind_ticks += armGetSystemTick() - t0;
  nxg_bind_calls++;
#else
  real_glBindTexture(target, tex);
#endif
}

// bytes per pixel for the combinations this engine actually uses; 0 = unknown
static unsigned nxd_bpp(unsigned fmt, unsigned typ) {
  switch (typ) {
    case 0x1401: break;              // GL_UNSIGNED_BYTE
    case 0x8033: return 4;           // GL_UNSIGNED_INT_8_8_8_8
    case 0x8367: return 4;           // GL_UNSIGNED_INT_8_8_8_8_REV
    case 0x8363: return 2;           // GL_UNSIGNED_SHORT_5_6_5
    case 0x8034: return 2;           // GL_UNSIGNED_SHORT_5_5_5_1
    default: return 0;
  }
  switch (fmt) {
    case 0x1903: return 1;           // GL_RED
    case 0x1909: return 1;           // GL_LUMINANCE
    case 0x190A: return 2;           // GL_LUMINANCE_ALPHA
    case 0x1907: return 3;           // GL_RGB
    case 0x80E0: return 3;           // GL_BGR
    case 0x1908: return 4;           // GL_RGBA
    case 0x80E1: return 4;           // GL_BGRA
    default: return 0;
  }
}

// true when the upload was identical to the previous one for this exact region
static int nxd_is_duplicate(unsigned tex, unsigned lvl, unsigned x, unsigned y,
                            unsigned w, unsigned h, unsigned fmt, unsigned typ,
                            const void *px) {
  if (!nxd_enabled) return 0;
  if (!px || !tex) return 0;

  const unsigned bpp = nxd_bpp(fmt, typ);
  if (!bpp) return 0;

  const unsigned size = w * h * bpp;
  if (size == 0 || size > NXD_MAX_BYTES) return 0;

  // Rows must be tightly packed, or w*h*bpp is not what the driver will read.
  if (real_glGetIntegerv) {
    int row = 0, align = 4;
    real_glGetIntegerv(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, &row);
    real_glGetIntegerv(0x0CF5 /* GL_UNPACK_ALIGNMENT  */, &align);
    if (row != 0 && (unsigned)row != w) return 0;
    if (align > 1 && ((w * bpp) % (unsigned)align) != 0) return 0;
  }

  unsigned idx = (tex * 2654435761u) ^ (lvl << 24) ^ (x << 12) ^ y;
  idx %= NXD_SLOTS;
  nxd_slot_t *s = &nxd_cache[idx];

  if (s->tex == tex && s->lvl == lvl && s->x == x && s->y == y &&
      s->w == w && s->h == h && s->fmt == fmt && s->typ == typ &&
      s->size == size && s->data && memcmp(s->data, px, size) == 0)
    return 1;

  if (!s->data || s->size < size) {
    unsigned char *nd = realloc(s->data, size);
    if (!nd) return 0;
    s->data = nd;
  }
  memcpy(s->data, px, size);
  s->tex = tex; s->lvl = lvl; s->x = x; s->y = y;
  s->w = w; s->h = h; s->fmt = fmt; s->typ = typ; s->size = size;
  return 0;
}
// --- draw / bind / swap profiling -----------------------------------------
// The uploads account for about two thirds of a bad frame. This measures what
// the rest is: the number and cost of draw calls and texture binds, plus the
// time spent inside the buffer swap, where a vsync wait or a driver flush
// would show up. A previous attempt at this crashed the game because a new
// dispatcher branch returned NULL on a failed lookup and so skipped the
// original ARB/EXT-suffix fallback, taking away a function the engine had
// been finding. nxg_getproc reproduces that fallback.

static void (*real_glDrawArrays)(unsigned, int, int);
static void (*real_glDrawElements)(unsigned, int, unsigned, const void *);

static void *nxg_getproc(const char *name) {
  void *p = SDL_GL_GetProcAddress(name);
  if (p) return p;
  static const char *sfx[] = { "ARB", "EXT" };
  char tmp[128];
  for (size_t i = 0; i < sizeof(sfx) / sizeof(*sfx); ++i) {
    snprintf(tmp, sizeof(tmp), "%s%s", name, sfx[i]);
    p = SDL_GL_GetProcAddress(tmp);
    if (p) return p;
  }
  return NULL;
}

static void wrap_glDrawArrays(unsigned mode, int first, int count) {
  if (!real_glDrawArrays) return;
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  real_glDrawArrays(mode, first, count);
  nxg_draw_ticks += armGetSystemTick() - t0;
  nxg_draw_calls++;
#else
  real_glDrawArrays(mode, first, count);
#endif
}

static void wrap_glDrawElements(unsigned mode, int count, unsigned type, const void *idx) {
  if (!real_glDrawElements) return;
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  real_glDrawElements(mode, count, type, idx);
  nxg_draw_ticks += armGetSystemTick() - t0;
  nxg_draw_calls++;
#else
  real_glDrawElements(mode, count, type, idx);
#endif
}
static void (*real_glTexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);
static void (*real_glTexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void *);

#if NXP_PROFILE
static u64 nxp_tex_calls, nxp_tex_ticks;   // glTexImage2D
static u64 nxp_sub_calls, nxp_sub_ticks;   // glTexSubImage2D
#endif

static u64 nxp_frame_mark;                 // tick at previous swap - also drives gyro's per-frame dt

// --- per-texture update rate limit -----------------------------------------
// The stock renderer re-uploads a lightmap every frame in which a dynamic light
// touches it, and each upload costs about half a millisecond here whatever its
// size. Same remedy as NXLightUpdateInterval in the rebuilt renderer, applied
// from outside so the stock OpenGLDrv.so can be used: a texture that has just
// been updated is not updated again for N frames. The update the renderer
// issues in between is kept (the newest one per region) and, if the renderer
// does not issue another when the texture is due, sent at the end of that
// frame, so a texture never stays stale after its light has stopped changing.
//
// Safety: only 2D sub-uploads of tightly packed, known-format data are held
// back; anything else goes straight through. The first upload of a texture, and
// every further upload within the same frame, passes untouched.
// gyro.cfg: uploadinterval = N (1 = off).
#define NXT_PENDING_MAX 384
#define NXT_MAX_BYTES   65536
#define NXT_TEX_SLOTS   8192

typedef struct { unsigned tex; unsigned last; } nxt_state_t;
typedef struct {
  int used;
  unsigned tex, unit, lvl, x, y, w, h, fmt, typ, size;
  unsigned char *data;
} nxt_pend_t;

static nxt_state_t nxt_state[NXT_TEX_SLOTS];
static nxt_pend_t  nxt_pend[NXT_PENDING_MAX];
static int         nxt_npend = 0;
static unsigned    nxt_frame = 1;
static int         nxt_interval = 3;
static int         nxt_ready = 0;    // 1 ready, -1 unavailable
static u64         nxt_deferred = 0, nxt_flushed = 0;
static u64         nxs_calls = 0, nxs_passed = 0, nxs_dedup0 = 0;
static u64         nxs_win_calls = 0, nxs_win_passed = 0, nxs_win_def = 0, nxs_win_flush = 0, nxs_win_dedup = 0;
static u64         nxs_win_ticks = 0, nxs_win_worst = 0;
static unsigned    nxs_win_frames = 0;

static void (*real_glActiveTexture)(unsigned);
static void (*real_glPixelStorei)(unsigned, int);
static void (*real_glDeleteTextures)(int, const unsigned *);

static int nxt_init(void) {
  if (nxt_ready) return nxt_ready > 0;
  if (!real_glGetIntegerv)    real_glGetIntegerv    = nxg_getproc("glGetIntegerv");
  if (!real_glActiveTexture)  real_glActiveTexture  = nxg_getproc("glActiveTexture");
  if (!real_glPixelStorei)    real_glPixelStorei    = nxg_getproc("glPixelStorei");
  nxt_ready = (real_glGetIntegerv && real_glActiveTexture && real_glPixelStorei &&
               real_glBindTexture) ? 1 : -1;
  printf("[uploads] rate limit every %d frames: %s\n", nxt_interval,
         nxt_ready > 0 ? "active" : "unavailable");
  fflush(stdout);
  return nxt_ready > 0;
}

static void nxt_drop(unsigned tex, int lvl /* -1 = every level */) {
  if (!nxt_npend) return;
  for (int i = 0; i < NXT_PENDING_MAX; ++i) {
    nxt_pend_t *e = &nxt_pend[i];
    if (e->used && e->tex == tex && (lvl < 0 || e->lvl == (unsigned)lvl)) {
      e->used = 0;
      nxt_npend--;
    }
  }
}

// Returns 1 if the upload was kept for later and must not be sent now.
static int nxt_defer(unsigned tgt, int lvl, int x, int y, int w, int h,
                     unsigned fmt, unsigned typ, const void *px) {
  if (nxt_interval <= 1 || !px || tgt != 0x0DE1 /* GL_TEXTURE_2D */) return 0;
  const unsigned tex = nxd_bound_tex;
  if (!tex || w <= 0 || h <= 0) return 0;
  if (!nxt_init()) return 0;

  nxt_state_t *st = &nxt_state[tex % NXT_TEX_SLOTS];
  if (st->tex != tex) {                 // first sight of this texture
    st->tex = tex;
    st->last = nxt_frame;
    return 0;
  }
  const int due = (st->last == nxt_frame) ||
                  (nxt_frame - st->last >= (unsigned)nxt_interval);
  if (due) {
    st->last = nxt_frame;
    // a fresh upload for this region supersedes any held one
    for (int i = 0; nxt_npend && i < NXT_PENDING_MAX; ++i) {
      nxt_pend_t *e = &nxt_pend[i];
      if (e->used && e->tex == tex && e->lvl == (unsigned)lvl &&
          e->x == (unsigned)x && e->y == (unsigned)y &&
          e->w == (unsigned)w && e->h == (unsigned)h) { e->used = 0; nxt_npend--; }
    }
    return 0;
  }

  const unsigned bpp = nxd_bpp(fmt, typ);
  if (!bpp) return 0;
  const unsigned size = (unsigned)w * (unsigned)h * bpp;
  if (!size || size > NXT_MAX_BYTES) return 0;
  int row = 0, align = 4, unit = 0;
  real_glGetIntegerv(0x0CF2, &row);
  real_glGetIntegerv(0x0CF5, &align);
  if (row != 0 && row != w) return 0;
  if (align > 1 && ((unsigned)w * bpp) % (unsigned)align != 0) return 0;
  real_glGetIntegerv(0x84E0 /* GL_ACTIVE_TEXTURE */, &unit);
  if (unit < 0x84C0) return 0;

  nxt_pend_t *slot = NULL;
  for (int i = 0; i < NXT_PENDING_MAX; ++i) {
    nxt_pend_t *e = &nxt_pend[i];
    if (e->used) {
      if (e->tex == tex && e->lvl == (unsigned)lvl && e->x == (unsigned)x &&
          e->y == (unsigned)y && e->w == (unsigned)w && e->h == (unsigned)h) { slot = e; break; }
    } else if (!slot) {
      slot = e;
    }
  }
  if (!slot) return 0;                  // table full: send it now
  if (!slot->data || slot->size < size) {
    unsigned char *nd = realloc(slot->data, size > 4096 ? size : 4096);
    if (!nd) return 0;
    slot->data = nd;
  }
  if (!slot->used) { slot->used = 1; nxt_npend++; }
  memcpy(slot->data, px, size);
  slot->tex = tex; slot->unit = (unsigned)unit; slot->lvl = (unsigned)lvl;
  slot->x = (unsigned)x; slot->y = (unsigned)y; slot->w = (unsigned)w; slot->h = (unsigned)h;
  slot->fmt = fmt; slot->typ = typ; slot->size = size;
  nxt_deferred++;
  return 1;
}

// End of frame: send what is due and was not superseded by a fresh upload.
static void nxt_flush(void) {
  if (!nxt_npend || nxt_ready <= 0 || !real_glTexSubImage2D) return;
  int row = 0, align = 4, unit0 = 0;
  real_glGetIntegerv(0x0CF2, &row);
  real_glGetIntegerv(0x0CF5, &align);
  real_glGetIntegerv(0x84E0, &unit0);
  int unit_cur = unit0;
  int changed_store = 0;

  for (int i = 0; i < NXT_PENDING_MAX; ++i) {
    nxt_pend_t *e = &nxt_pend[i];
    if (!e->used) continue;
    nxt_state_t *st = &nxt_state[e->tex % NXT_TEX_SLOTS];
    if (st->tex == e->tex && nxt_frame - st->last < (unsigned)nxt_interval) continue;

    if (!changed_store) {
      if (row != 0) real_glPixelStorei(0x0CF2, 0);
      if (align != 1) real_glPixelStorei(0x0CF5, 1);
      changed_store = 1;
    }
    if ((int)e->unit != unit_cur) { real_glActiveTexture(e->unit); unit_cur = (int)e->unit; }
    int old = 0;
    real_glGetIntegerv(0x8069 /* GL_TEXTURE_BINDING_2D */, &old);
    real_glBindTexture(0x0DE1, e->tex);
    real_glTexSubImage2D(0x0DE1, (int)e->lvl, (int)e->x, (int)e->y, (int)e->w, (int)e->h,
                         e->fmt, e->typ, e->data);
    real_glBindTexture(0x0DE1, (unsigned)old);
    st->tex = e->tex;
    st->last = nxt_frame;
    e->used = 0;
    nxt_npend--;
    nxt_flushed++;
  }
  if (unit_cur != unit0) real_glActiveTexture((unsigned)unit0);
  if (changed_store) {
    if (row != 0) real_glPixelStorei(0x0CF2, row);
    if (align != 1) real_glPixelStorei(0x0CF5, align);
  }
}

static void wrap_glDeleteTextures(int n, const unsigned *ids) {
  if (!real_glDeleteTextures) return;
  for (int i = 0; ids && i < n; ++i) {
    nxt_drop(ids[i], -1);
    nxt_state_t *st = &nxt_state[ids[i] % NXT_TEX_SLOTS];
    if (st->tex == ids[i]) st->tex = 0;
  }
  real_glDeleteTextures(n, ids);
}

#if NXP_PROFILE
static u64 nxp_seen_tex, nxp_seen_sub;     // counter snapshot at previous swap

static u64 nxp_worst_ticks, nxp_worst_tex, nxp_worst_sub;
static u32 nxp_frames, nxp_o25, nxp_o50, nxp_o100;
static u64 nxp_win_mark;                   // tick at start of report window
static u64 nxp_win_tex, nxp_win_sub, nxp_win_tex_t, nxp_win_sub_t;
static u64 nxp_win_io, nxp_win_io_t, nxp_win_op, nxp_win_op_t;
#endif

static inline double nxp_ms(u64 ticks) {
  return (double)ticks * 1000.0 / (double)armGetSystemTickFreq();
}

static void wrap_glTexImage2D(unsigned tgt, int lvl, int ifmt, int w, int h,
                              int bdr, unsigned fmt, unsigned typ, const void *px) {
  if (!real_glTexImage2D) return;
  if (tgt == 0x0DE1 && nxd_bound_tex) nxt_drop(nxd_bound_tex, lvl);
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  real_glTexImage2D(tgt, lvl, ifmt, w, h, bdr, fmt, typ, px);
  nxp_tex_ticks += armGetSystemTick() - t0;
  nxp_tex_calls++;
#else
  real_glTexImage2D(tgt, lvl, ifmt, w, h, bdr, fmt, typ, px);
#endif
}

static int nxpbo_enabled = 1;   // 0 = plain direct uploads (gyro.cfg: pbo = 0)

// --- asynchronous uploads through a pixel buffer object --------------------
// Measured: glTexSubImage2D costs 0.55ms here while a texture bind costs
// 0.001ms and a draw 0.031ms. The cost is fixed, unrelated to size, and the
// GPU is idle - so it is not work, it is waiting. The likely reason is that the
// texture being written is still referenced by queued drawing, forcing the
// driver to flush and synchronise before it may touch it. That happens ~126
// times a frame.
//
// Passing the pixels through a buffer object removes the reason to wait: the
// data is copied into GPU-owned storage and the texture update reads from
// there, so the driver can schedule it instead of blocking. glBufferData with
// a NULL pointer first ("orphaning") tells it we do not care about the old
// contents, so it need not wait for any previous use of the buffer either.
//
// This trades one call for four, which is only sensible because the other
// calls are three orders of magnitude cheaper than the one we are avoiding.
// Whether that trade actually pays here is exactly what the next measurement
// answers - hence the runtime switch.
#define NXPBO_COUNT 4
#define NXPBO_MAX   65536

static void (*real_glGenBuffers)(int, unsigned *);
static void (*real_glBindBuffer)(unsigned, unsigned);
static void (*real_glBufferData)(unsigned, long, const void *, unsigned);
static void (*real_glBufferSubData)(unsigned, long, long, const void *);

static unsigned nxpbo_id[NXPBO_COUNT];
static int      nxpbo_ready = 0;    // 1 ready, -1 unavailable
static int      nxpbo_next  = 0;
static u64      nxpbo_used  = 0;
#if NXP_PROFILE
static u64      nxpbo_win_used = 0;
#endif

static int nxpbo_init(void) {
  if (nxpbo_ready) return nxpbo_ready > 0;
  // Resolve these ourselves. The dispatcher only sees what the engine asks
  // for, and a 1998 renderer never requests buffer objects - so waiting to be
  // handed them means never getting them.
  if (!real_glGenBuffers)    real_glGenBuffers    = nxg_getproc("glGenBuffers");
  if (!real_glBindBuffer)    real_glBindBuffer    = nxg_getproc("glBindBuffer");
  if (!real_glBufferData)    real_glBufferData    = nxg_getproc("glBufferData");
  if (!real_glBufferSubData) real_glBufferSubData = nxg_getproc("glBufferSubData");

  if (!real_glGenBuffers || !real_glBindBuffer || !real_glBufferData || !real_glBufferSubData) {
    nxpbo_ready = -1;
    printf("[pbo] buffer functions unavailable, staying on direct uploads\n");
    fflush(stdout);
    return 0;
  }
  real_glGenBuffers(NXPBO_COUNT, nxpbo_id);
  for (int i = 0; i < NXPBO_COUNT; ++i) {
    if (!nxpbo_id[i]) { nxpbo_ready = -1; return 0; }
    real_glBindBuffer(0x88EC /* GL_PIXEL_UNPACK_BUFFER */, nxpbo_id[i]);
    real_glBufferData(0x88EC, NXPBO_MAX, NULL, 0x88E0 /* GL_STREAM_DRAW */);
  }
  real_glBindBuffer(0x88EC, 0);
  nxpbo_ready = 1;
  printf("[pbo] %d buffers ready\n", NXPBO_COUNT);
  fflush(stdout);
  return 1;
}

// Returns 1 if the upload was performed through a buffer object.
static int nxpbo_upload(unsigned tgt, int lvl, int x, int y, int w, int h,
                        unsigned fmt, unsigned typ, const void *px) {
  if (!nxpbo_enabled || !px) return 0;
  if (!nxpbo_init()) return 0;

  const unsigned bpp = nxd_bpp(fmt, typ);
  if (!bpp) return 0;
  const unsigned size = (unsigned)w * (unsigned)h * bpp;
  if (!size || size > NXPBO_MAX) return 0;

  // Same layout guard as the duplicate check: if rows are not tightly packed,
  // w*h*bpp is not the number of bytes the driver would read.
  if (real_glGetIntegerv) {
    int row = 0, align = 4;
    real_glGetIntegerv(0x0CF2, &row);
    real_glGetIntegerv(0x0CF5, &align);
    if (row != 0 && (unsigned)row != (unsigned)w) return 0;
    if (align > 1 && (((unsigned)w * bpp) % (unsigned)align) != 0) return 0;
  }

  const unsigned id = nxpbo_id[nxpbo_next];
  nxpbo_next = (nxpbo_next + 1) % NXPBO_COUNT;

  real_glBindBuffer(0x88EC, id);
  real_glBufferData(0x88EC, NXPBO_MAX, NULL, 0x88E0);   // orphan
  real_glBufferSubData(0x88EC, 0, size, px);
  real_glTexSubImage2D(tgt, lvl, x, y, w, h, fmt, typ, (const void *)0);
  real_glBindBuffer(0x88EC, 0);

  nxpbo_used++;
  return 1;
}
static void wrap_glTexSubImage2D(unsigned tgt, int lvl, int x, int y, int w, int h,
                                 unsigned fmt, unsigned typ, const void *px) {
  if (!real_glTexSubImage2D) return;
  nxs_calls++;
  if (nxd_is_duplicate(nxd_bound_tex, (unsigned)lvl, (unsigned)x, (unsigned)y,
                       (unsigned)w, (unsigned)h, fmt, typ, px)) {
    nxd_skipped++;
    return;
  }
  if (nxt_defer(tgt, lvl, x, y, w, h, fmt, typ, px)) return;
  nxs_passed++;
#if NXP_PROFILE
  const u64 t0 = armGetSystemTick();
  if (!nxpbo_upload(tgt, lvl, x, y, w, h, fmt, typ, px))
    real_glTexSubImage2D(tgt, lvl, x, y, w, h, fmt, typ, px);
  nxp_sub_ticks += armGetSystemTick() - t0;
  nxp_sub_calls++;
#else
  if (!nxpbo_upload(tgt, lvl, x, y, w, h, fmt, typ, px))
    real_glTexSubImage2D(tgt, lvl, x, y, w, h, fmt, typ, px);
#endif
}

static void nxgy_frame(float dt);

// L3+R3 held together toggles gyro on/off at runtime, on top of gyro.cfg's
// "enabled" default.  Checked every frame regardless of nxgy_on's current
// value - otherwise turning the gyro back on would have nowhere to run from,
// since nxgy_frame() returns immediately when it is off.
static int nxgy_on; // tentative decl; the real definition with its default is below

static void nxgy_check_toggle(void) {
  const int l3 = SDL_GameControllerGetButton(sdl_ctrl, SDL_CONTROLLER_BUTTON_LEFTSTICK);
  const int r3 = SDL_GameControllerGetButton(sdl_ctrl, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
  static int held = 0;
  const int now = l3 && r3;
  if (now && !held) {
    nxgy_on = !nxgy_on;
    printf("[gyro] %s (L3+R3)\n", nxgy_on ? "on" : "off");
  }
  held = now;
}

static void wrap_SDL_GL_SwapWindow(SDL_Window *win) {
  nxt_flush();
#if NXP_PROFILE
  {
    const u64 ts = armGetSystemTick();
    SDL_GL_SwapWindow(win);
    nxg_swap_ticks += armGetSystemTick() - ts;
  }
#else
  SDL_GL_SwapWindow(win);
#endif

  nxt_frame++;

  const u64 now = armGetSystemTick();
  if (nxp_frame_mark) {
    const u64 d = now - nxp_frame_mark;
    nxs_win_ticks += d;
    if (d > nxs_win_worst) nxs_win_worst = d;
    if (++nxs_win_frames >= 180) {
      FILE *sf = fopen("../nxstats.txt", "w");
      if (sf) {
        const double fq = (double)armGetSystemTickFreq();
        const double n = (double)nxs_win_frames;
        fprintf(sf, "interval=%d active=%d\n", nxt_interval, nxt_ready);
        fprintf(sf, "last %u frames: avg %.1f ms (%.1f fps), worst %.1f ms\n", nxs_win_frames,
                1000.0 * (double)nxs_win_ticks / fq / n, n * fq / (double)nxs_win_ticks,
                1000.0 * (double)nxs_win_worst / fq);
        fprintf(sf, "per frame: sub-uploads asked %.1f | identical skipped %.1f | held back %.1f | sent late %.1f | sent now %.1f\n",
                (double)(nxs_calls - nxs_win_calls) / n,
                (double)(nxd_skipped - nxs_win_dedup) / n,
                (double)(nxt_deferred - nxs_win_def) / n,
                (double)(nxt_flushed - nxs_win_flush) / n,
                (double)(nxs_passed - nxs_win_passed) / n);
        fprintf(sf, "pending now: %d\n", nxt_npend);
        fclose(sf);
      }
      nxs_win_calls = nxs_calls; nxs_win_passed = nxs_passed; nxs_win_def = nxt_deferred;
      nxs_win_flush = nxt_flushed; nxs_win_dedup = nxd_skipped;
      nxs_win_ticks = 0; nxs_win_worst = 0; nxs_win_frames = 0;
    }
  }

  if (nxp_frame_mark) {
    const u64 dt  = now - nxp_frame_mark;
    nxgy_frame((float)dt / (float)armGetSystemTickFreq());
#if NXP_PROFILE
    const u64 dtx = nxp_tex_calls - nxp_seen_tex;
    const u64 dsb = nxp_sub_calls - nxp_seen_sub;

    if (dt > nxp_worst_ticks) {
      nxp_worst_ticks = dt;
      nxp_worst_tex   = dtx;
      nxp_worst_sub   = dsb;
    }

    const double ms = nxp_ms(dt);
    if (ms > 100.0) nxp_o100++;
    else if (ms > 50.0) nxp_o50++;
    else if (ms > 25.0) nxp_o25++;


    // A 4-second frame averaged into a 2-second window becomes "avg 186ms" and
    // tells you nothing.  Report the frame itself, the moment it happens, split
    // into everything we can time.  Whatever lands in "rest" is engine CPU we
    // cannot see from out here - but knowing it IS the rest is the point.
    if (ms > 20.0 && nxp_dip_printed < 8) {
      nxp_dip_printed++;
      const double io_ms   = nxp_ms(nxp_io_ticks   - nxp_pf_io_t);
      const double up_ms   = nxp_ms((nxp_tex_ticks + nxp_sub_ticks) - nxp_pf_up_t);
      const double draw_ms = nxp_ms(nxg_draw_ticks - nxp_pf_draw_t);
      const double bind_ms = nxp_ms(nxg_bind_ticks - nxp_pf_bind_t);
      const double swap_ms = nxp_ms(nxg_swap_ticks - nxp_pf_swap_t);
      const double op_ms = nxp_ms(nxp_op_ticks - nxp_pf_op_t);
      printf("[dip] %.0fms | read %.0fms/%llu (worst %.0fms) | open/seek %.0fms/%llu | "
             "upload %.0fms | draw %.0fms | bind %.0fms | swap %.0fms | REST %.0fms\n",
             ms, io_ms,
             (unsigned long long)(nxp_io_calls - nxp_pf_io_c),
             nxp_ms(nxp_io_worst),
             op_ms, (unsigned long long)(nxp_op_calls - nxp_pf_op_c),
             up_ms, draw_ms, bind_ms, swap_ms,
             ms - io_ms - op_ms - up_ms - draw_ms - bind_ms - swap_ms);
      fflush(stdout);
    }
    nxp_io_worst = 0;
    nxp_op_worst = 0;

    nxp_frames++;
#endif
  }

  nxp_frame_mark = now;
#if NXP_PROFILE
  nxp_seen_tex   = nxp_tex_calls;
  nxp_seen_sub   = nxp_sub_calls;
  nxp_pf_io_t   = nxp_io_ticks;   nxp_pf_io_c = nxp_io_calls;
  nxp_pf_op_t   = nxp_op_ticks;   nxp_pf_op_c = nxp_op_calls;
  nxp_pf_up_t   = nxp_tex_ticks + nxp_sub_ticks;
  nxp_pf_draw_t = nxg_draw_ticks; nxp_pf_bind_t = nxg_bind_ticks;
  nxp_pf_swap_t = nxg_swap_ticks;

  if (!nxp_win_mark) nxp_win_mark = now;

  // Report every ~2 seconds.
  if (now - nxp_win_mark >= armGetSystemTickFreq() * 2) {
    const double win_ms = nxp_ms(now - nxp_win_mark);
    const u64 tex = nxp_tex_calls - nxp_win_tex;
    const u64 sub = nxp_sub_calls - nxp_win_sub;
    const u64 sub_t = nxp_sub_ticks - nxp_win_sub_t;

    printf("[nxp] %uf avg %.1fms WORST %.1fms | sub %llu %.2fea (%.0fms/f) | SWAP %.0fms/f | BIND %llu %.3fea (%.0fms/f) | DRAW %llu %.3fea (%.0fms/f) | tex %llu | SKIP %llu | PBO %llu | >25:%u >50:%u >100:%u\n",
           nxp_frames,
           nxp_frames ? win_ms / nxp_frames : 0.0,
           nxp_ms(nxp_worst_ticks),
           (unsigned long long)sub, sub ? nxp_ms(sub_t) / sub : 0.0,
           nxp_frames ? nxp_ms(sub_t) / nxp_frames : 0.0,
           nxp_frames ? nxp_ms(nxg_swap_ticks - nxp_win_swap_t2) / nxp_frames : 0.0,
           (unsigned long long)(nxg_bind_calls - nxg_win_bind),
           (nxg_bind_calls - nxg_win_bind) ? nxp_ms(nxg_bind_ticks - nxg_win_bind_t) / (nxg_bind_calls - nxg_win_bind) : 0.0,
           nxp_frames ? nxp_ms(nxg_bind_ticks - nxg_win_bind_t) / nxp_frames : 0.0,
           (unsigned long long)(nxg_draw_calls - nxg_win_draw),
           (nxg_draw_calls - nxg_win_draw) ? nxp_ms(nxg_draw_ticks - nxg_win_draw_t) / (nxg_draw_calls - nxg_win_draw) : 0.0,
           nxp_frames ? nxp_ms(nxg_draw_ticks - nxg_win_draw_t) / nxp_frames : 0.0,
           (unsigned long long)tex,
           (unsigned long long)(nxd_skipped - nxd_win_skipped),
           (unsigned long long)(nxpbo_used - nxpbo_win_used),
           nxp_o25, nxp_o50, nxp_o100);
    nxd_win_skipped = nxd_skipped;
    nxpbo_win_used  = nxpbo_used;
    nxg_win_bind = nxg_bind_calls; nxg_win_bind_t = nxg_bind_ticks;
    nxg_win_draw = nxg_draw_calls; nxg_win_draw_t = nxg_draw_ticks;
    nxp_win_swap_t2 = nxg_swap_ticks;
    fflush(stdout);




    nxp_dip_printed = 0;
    nxp_frames = nxp_o25 = nxp_o50 = nxp_o100 = 0;
    nxp_worst_ticks = nxp_worst_tex = nxp_worst_sub = 0;
    nxp_win_mark = now;
    nxp_win_tex = nxp_tex_calls;   nxp_win_sub = nxp_sub_calls;
    nxp_win_tex_t = nxp_tex_ticks; nxp_win_sub_t = nxp_sub_ticks;
    nxp_win_io = nxp_io_calls;   nxp_win_io_t = nxp_io_ticks;   nxp_io_worst = 0;
    nxp_win_op = nxp_op_calls;   nxp_win_op_t = nxp_op_ticks;   nxp_op_worst = 0;
  }
#endif
}

// ===========================================================================
// Gyro aiming, injected as mouse motion
//
// Unreal binds the two input paths differently, and the difference is the
// whole point here:
//
//     MouseX = Axis aMouseX Speed=6.0        relative, 1:1, no curve
//     JoyZ   = Axis aTurn   SpeedBase=250.0  a turn RATE, like a stick
//
// Feeding the gyro through the joystick axes would put it behind the engine's
// rate model and its acceleration - exactly the "buttery" feel we are trying to
// avoid. Synthesising mouse motion instead lands on aMouseX/aMouseY, which is
// plain relative displacement.
//
// Sampling is the other half of "precise". hidGetSixAxisSensorStates(h,&s,1)
// returns only the newest sample, and the sensor runs far faster than 60Hz -
// its LIFO holds 17 - so one read per frame throws away the motion between
// frames. That loss reads as latency or over-smoothing and no amount of
// sensitivity tuning fixes it. So: read all 17, average only the samples that
// are actually new since last frame, and multiply that mean by the frame time.
// If nothing new arrived, emit zero rather than repeating the last sample.
// ===========================================================================

static HidSixAxisSensorHandle nxgy_h[4];
static int   nxgy_ready = 0;
static u64   nxgy_seen = 0;          // newest sampling_number consumed
static float nxgy_pend_x = 0.0f;     // sub-pixel mouse delta carried over
static float nxgy_pend_y = 0.0f;

// Tunables, read from /switch/unreal/gyro.cfg so trying a value is a file edit
// rather than a rebuild. Defaults are the ones Serious Sam settled on after the
// sampling fix.
static float nxgy_yaw   = -6.7f;      // wrist swing left/right  (sensor Y)
static float nxgy_pitch = 4.0f;     // tilt up/down            (sensor X)
static float nxgy_roll  = -6.7f;      // "steering wheel"        (sensor Z)
static float nxgy_curve = 3.5f;      // left stick softening, 1 = off
static float nxgy_dead  = 0.10f;
// left stick dead zone
static int   nxgy_on    = 1;
// HidGyroscopeZeroDriftMode: 0=Loose, 1=Standard (the system default), 2=Tight.
// The console keeps its own estimate of the gyro's zero point, subtracts it from
// every reading, and refreshes it whenever it believes the pad is at rest - the
// same judgement hidIsSixAxisSensorAtRest reports. A slow, small, deliberate
// rotation does not move the accelerometer enough to leave that state, so it is
// absorbed into the bias and cancelled before the game ever sees it.
//
// hid.h documents Standard as the default, and neither this loader nor the two
// real ports (iortcw, Serious Sam) ever wrote it. That is the only thing all
// three had in common: they differ in output type, sampling, calibration and
// axis handling, and every one of those was ruled out by a port that does it
// differently and still shows the same dead feel at small amplitudes.
static int   nxgy_drift = 0;
static int   nxgy_dbg   = 0;   // 1 = log raw axes and the system's at-rest bit

static void nxgy_load_cfg(void) {
  FILE *f = fopen("../gyro.cfg", "r");   // cwd is SystemARM
  if (!f) return;
  char line[128], key[64];
  float val;
  while (fgets(line, sizeof(line), f)) {
    if (line[0] == '#' || line[0] == ';') continue;
    if (sscanf(line, "%63[^= \t] = %f", key, &val) != 2 &&
        sscanf(line, "%63[^= \t]=%f", key, &val) != 2) continue;
    if      (!strcmp(key, "yaw"))       nxgy_yaw   = val;
    else if (!strcmp(key, "pitch"))     nxgy_pitch = val;
    else if (!strcmp(key, "roll"))      nxgy_roll  = val;
    else if (!strcmp(key, "movecurve")) nxgy_curve = val;
    else if (!strcmp(key, "deadzone"))  nxgy_dead  = val;
    else if (!strcmp(key, "pbo"))       nxpbo_enabled = (int)val;
    else if (!strcmp(key, "uploadinterval")) { nxt_interval = (int)val; if (nxt_interval < 1) nxt_interval = 1; if (nxt_interval > 8) nxt_interval = 8; }
    else if (!strcmp(key, "enabled"))   nxgy_on    = (int)val;
    else if (!strcmp(key, "driftmode")) nxgy_drift = (int)val;
    else if (!strcmp(key, "debug"))     nxgy_dbg   = (int)val;
  }
  fclose(f);
  printf("[gyro] yaw=%.2f pitch=%.2f roll=%.2f curve=%.2f dead=%.2f on=%d drift=%d\n",
         nxgy_yaw, nxgy_pitch, nxgy_roll, nxgy_curve, nxgy_dead, nxgy_on, nxgy_drift);
  fflush(stdout);
}

static void nxgy_init(void) {
  if (nxgy_ready) return;
  nxgy_load_cfg();
  hidGetSixAxisSensorHandles(&nxgy_h[0], 2, HidNpadIdType_No1, HidNpadStyleTag_NpadJoyDual);
  hidGetSixAxisSensorHandles(&nxgy_h[2], 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey);
  hidGetSixAxisSensorHandles(&nxgy_h[3], 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
  for (int i = 0; i < 4; ++i) hidStartSixAxisSensor(nxgy_h[i]);

  // Apply the zero-drift mode to every handle, then read one back. A silent
  // failure and a successful write are indistinguishable from the game's side,
  // and that ambiguity is what let this setting go unexamined for so long.
  {
    int want = nxgy_drift < 0 ? 0 : (nxgy_drift > 2 ? 2 : nxgy_drift);
    HidGyroscopeZeroDriftMode got = HidGyroscopeZeroDriftMode_Standard;
    for (int i = 0; i < 4; ++i) {
      const Result rc = hidSetGyroscopeZeroDriftMode(nxgy_h[i],
                          (HidGyroscopeZeroDriftMode)want);
      if (R_FAILED(rc))
        printf("[gyro] zero-drift set failed on handle %d (rc=0x%x)\n",
               i, (unsigned)rc);
    }
    if (R_SUCCEEDED(hidGetGyroscopeZeroDriftMode(nxgy_h[3], &got))) {
      printf("[gyro] zero-drift mode = %d (%s), requested %d\n", (int)got,
             got == HidGyroscopeZeroDriftMode_Loose ? "Loose" :
             got == HidGyroscopeZeroDriftMode_Tight ? "Tight" : "Standard",
             want);
    } else {
      printf("[gyro] zero-drift readback failed\n");
    }
  }

  nxgy_ready = 1;
  printf("[gyro] sensors started\n");
  fflush(stdout);
}

// Pick the sensor actually in the player's hands. An explicitly paired
// controller wins over ambient handheld status: with a Pro Controller on an
// undocked console both bits are set, and testing handheld first reads the
// console's own sensor while the player waves the pad.
static HidSixAxisSensorHandle *nxgy_pick(void) {
  const u64 pad = hidGetNpadStyleSet(HidNpadIdType_No1);
  const u64 hh  = hidGetNpadStyleSet(HidNpadIdType_Handheld);
  if (pad & HidNpadStyleTag_NpadFullKey)  return &nxgy_h[2];
  if (hh  & HidNpadStyleTag_NpadHandheld) return &nxgy_h[3];
  if (pad & HidNpadStyleTag_NpadJoyDual)  return &nxgy_h[1];
  return NULL;
}

// Called once per frame from the swap wrapper; dt is the frame time in seconds.
static void nxgy_frame(float dt) {
  if (!nxgy_on) return;
  nxgy_init();

  HidSixAxisSensorHandle *h = nxgy_pick();
  if (!h) return;

  HidSixAxisSensorState st[17];
  const size_t n = hidGetSixAxisSensorStates(*h, st, 17);
  if (!n) return;

  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  u64 newest = nxgy_seen;
  int fresh = 0;

  // hidGetSixAxisSensorStates returns newest-first.
  for (size_t i = 0; i < n; ++i) {
    if (st[i].sampling_number <= nxgy_seen) continue;
    if (st[i].sampling_number > newest) newest = st[i].sampling_number;
    ax += st[i].angular_velocity.x;
    ay += st[i].angular_velocity.y;
    az += st[i].angular_velocity.z;
    fresh++;
  }

  nxgy_seen = newest;
  if (!fresh) return;              // nothing new: emit no rotation at all

  ax /= (float)fresh;
  ay /= (float)fresh;
  az /= (float)fresh;

  // The system's OWN at-rest bit - the same judgement that gates the zero-drift
  // correction. If this stays 1 through a slow deliberate wrist movement, the
  // console is calling that movement stillness and subtracting it away. It is
  // the first probe here that sits ABOVE the sensor instead of below it.
  if (nxgy_dbg) {
    static u32 tick = 0;
    if ((tick++ % 30) == 0) {
      bool rest = false;
      hidIsSixAxisSensorAtRest(*h, &rest);
      printf("[gyro] fresh=%d x=%+.5f y=%+.5f z=%+.5f atRest=%d\n",
             fresh, ax, ay, az, (int)rest);
      fflush(stdout);
    }
  }

  // Mean rate over the frame times the frame duration is the true rotation for
  // evenly spaced samples, and avoids depending on the unit of delta_time.
  float turn = ay * nxgy_yaw;
  if (nxgy_roll != 0.0f) turn += az * nxgy_roll;

  nxgy_pend_x += turn * dt * 1000.0f;
  nxgy_pend_y -= ax * nxgy_pitch * dt * 1000.0f;
}

// Hand the accumulated motion to the engine as a relative mouse event. Whole
// pixels only; the remainder is carried so slow movement is not quantised away.
static int nxgy_pop_motion(SDL_Event *ev) {
  const int dx = (int)nxgy_pend_x;
  const int dy = (int)nxgy_pend_y;
  if (dx == 0 && dy == 0) return 0;
  nxgy_pend_x -= (float)dx;
  nxgy_pend_y -= (float)dy;

  SDL_zerop(ev);
  ev->type = SDL_MOUSEMOTION;
  ev->motion.timestamp = SDL_GetTicks();
  ev->motion.state = 0;
  ev->motion.xrel = dx;
  ev->motion.yrel = dy;
  return 1;
}

static int wrap_SDL_PollEvent(SDL_Event *ev) {
  if (ev && nxgy_on && nxgy_pop_motion(ev)) return 1;
  return SDL_PollEvent(ev);
}

static void *wrap_SDL_GL_GetProcAddress(const char *name) {
  if (!strcmp(name, "glGetString")) {
    printf("patching glGetString\n");
    real_glGetString = SDL_GL_GetProcAddress(name);
    return wrap_glGetString;
  } else if (!strcmp(name, "glShaderSource")) {
    printf("patching glShaderSource\n");
    real_glShaderSource = SDL_GL_GetProcAddress(name);
    return wrap_glShaderSource;
  } else if (!strcmp(name, "glTexImage2D")) {
    printf("patching glTexImage2D\n");
    real_glTexImage2D = SDL_GL_GetProcAddress(name);
    if (!real_glTexImage2D) return NULL;
    return wrap_glTexImage2D;
  } else if (!strcmp(name, "glGenBuffers")) {
    real_glGenBuffers = nxg_getproc(name);
    return real_glGenBuffers;
  } else if (!strcmp(name, "glBindBuffer")) {
    real_glBindBuffer = nxg_getproc(name);
    return real_glBindBuffer;
  } else if (!strcmp(name, "glBufferData")) {
    real_glBufferData = nxg_getproc(name);
    return real_glBufferData;
  } else if (!strcmp(name, "glBufferSubData")) {
    real_glBufferSubData = nxg_getproc(name);
    return real_glBufferSubData;
  } else if (!strcmp(name, "glDrawArrays")) {
    real_glDrawArrays = nxg_getproc(name);
    if (!real_glDrawArrays) return NULL;
    return wrap_glDrawArrays;
  } else if (!strcmp(name, "glDrawElements")) {
    real_glDrawElements = nxg_getproc(name);
    if (!real_glDrawElements) return NULL;
    return wrap_glDrawElements;
  } else if (!strcmp(name, "glBindTexture")) {
    real_glBindTexture = SDL_GL_GetProcAddress(name);
    if (!real_glBindTexture) return NULL;
    return wrap_glBindTexture;
  } else if (!strcmp(name, "glDeleteTextures")) {
    real_glDeleteTextures = nxg_getproc(name);
    if (!real_glDeleteTextures) return NULL;
    return wrap_glDeleteTextures;
  } else if (!strcmp(name, "glActiveTexture")) {
    real_glActiveTexture = nxg_getproc(name);
    return real_glActiveTexture;
  } else if (!strcmp(name, "glPixelStorei")) {
    real_glPixelStorei = nxg_getproc(name);
    return real_glPixelStorei;
  } else if (!strcmp(name, "glGetIntegerv")) {
    real_glGetIntegerv = SDL_GL_GetProcAddress(name);
    return real_glGetIntegerv;
  } else if (!strcmp(name, "glTexSubImage2D")) {
    printf("patching glTexSubImage2D\n");
    real_glTexSubImage2D = SDL_GL_GetProcAddress(name);
    if (!real_glTexSubImage2D) return NULL;
    return wrap_glTexSubImage2D;
  }

  void *p = SDL_GL_GetProcAddress(name);
  if (p) return p;

  static const char *exts[] = { "ARB", "EXT "};
  char tmp[128];
  for (size_t i = 0; i < sizeof(exts) / sizeof(*exts); ++i) {
    snprintf(tmp, sizeof(tmp), "%s%s", name, exts[i]);
    p = SDL_GL_GetProcAddress(tmp);
    if (p) return p;
  }

  printf("SDL_GL_GetProcAddress(): `%s` not found\n", name);

  return NULL;
}

static SDL_bool wrap_SDL_GL_ExtensionSupported(const char *name) {
  if (strstr(name, "shader_draw_parameters"))
    return SDL_FALSE; // not properly supported despite being in ext list
  if (strstr(name, "texture_storage"))
    return SDL_TRUE;  // listed as ARB_ instead of EXT_
  if (strstr(name, "bindless_texture"))
    return SDL_FALSE;  // listed as ARB_ instead of IMG_
  if (strstr(name, "texture_compression_s3tc"))
    return SDL_TRUE;  // for some reason was reported as unsupported
  if (strstr(name, "texture_filter_anisotropic"))
    return SDL_TRUE;  // for some reason was reported as unsupported
  return SDL_GL_ExtensionSupported(name);
}

// the game likes executing this on startup, but on the switch it also pops up the OSK
static void wrap_SDL_StartTextInput(void) { }

// the game uses the SDL_Joystick API, but SDL_GameController is way better
static int wrap_SDL_Init(u32 flags) {
  const int ret = SDL_Init(flags | SDL_INIT_GAMECONTROLLER);
  SDL_GameControllerEventState(SDL_IGNORE);
  SDL_JoystickEventState(SDL_IGNORE);
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
  if (SDL_GameControllerAddMappingsFromFile("gamecontrollerdb.txt") < 0)
    SDL_GameControllerAddMappingsFromFile("../gamecontrollerdb.txt"); // might be a dir up from SystemARM
  return ret;
}

static const u8 *wrap_SDL_GetKeyboardState(int *numkeys) {
  if (numkeys)
    *numkeys = SDL_NUM_SCANCODES;
  return sdl_keys;
}

static SDL_Joystick *wrap_SDL_JoystickOpen(int index) {
  sdl_ctrl = SDL_GameControllerOpen(index);
  if (sdl_ctrl)
    return SDL_GameControllerGetJoystick(sdl_ctrl);
  return NULL;
}

static void wrap_SDL_JoystickClose(SDL_Joystick *joy) {
  if (sdl_ctrl) {
    SDL_GameControllerClose(sdl_ctrl);
    sdl_ctrl = NULL;
  }
}

static inline void do_osk(void) {
  SwkbdConfig kbd;
  char out[1024];
  Result rc;
  out[0] = 0;
  out[sizeof(out) - 1] = 0;

  rc = swkbdCreate(&kbd, 0);
  if (R_FAILED(rc)) return;

  swkbdConfigMakePresetDefault(&kbd);
  rc = swkbdShow(&kbd, out, sizeof(out));
  swkbdClose(&kbd);
  if (R_FAILED(rc)) return;

  static SDL_Event ev;
  ev.text.type = ev.type = SDL_TEXTINPUT;
  ev.text.timestamp = SDL_GetTicks();
  for (char *p = out; *p; ++p) {
    ev.text.text[0] = *p;
    SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
  }
}

// we'll map controller buttons to keys and get axis values from the GameController system

static inline void check_btn(const int val, const int map) {
  static SDL_Event ev;
  const int new_btn = val;
  const int old_btn = sdl_keys[map];
  // need to inject keypress events for the UI to work
  if (new_btn && !old_btn) {
    ev.key.type = ev.type = SDL_KEYDOWN;
    ev.key.timestamp = SDL_GetTicks();
    ev.key.keysym.scancode = map;
    ev.key.keysym.sym =SDL_SCANCODE_TO_KEYCODE(map);
    ev.key.state = SDL_PRESSED;
    SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
  } else if (!new_btn && old_btn) {
    ev.key.type = ev.type = SDL_KEYUP;
    ev.key.timestamp = SDL_GetTicks();
    ev.key.keysym.scancode = map;
    ev.key.keysym.sym =SDL_SCANCODE_TO_KEYCODE(map);
    ev.key.state = SDL_RELEASED;
    SDL_PeepEvents(&ev, 1, SDL_ADDEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT);
  }
  sdl_keys[map] = new_btn;
}

// do all the input processing here since the game calls it right before PollEvent
static void wrap_SDL_JoystickUpdate(void) {
  static const int keymap[] = {
    // SDL names the face buttons by POSITION using the Xbox convention, so its
    // "A" is the bottom button - which on a Nintendo pad is B.  Swapped here so
    // the button labelled A on the hardware confirms and B goes back, which is
    // what the console's own menus do.
    SDL_SCANCODE_BACKSPACE, // SDL_CONTROLLER_BUTTON_A  = Nintendo B -> back
    SDL_SCANCODE_RETURN,    // SDL_CONTROLLER_BUTTON_B  = Nintendo A -> confirm
    SDL_SCANCODE_SPACE,     // SDL_CONTROLLER_BUTTON_X
    SDL_SCANCODE_DELETE,    // SDL_CONTROLLER_BUTTON_Y
    SDL_SCANCODE_F6,        // SDL_CONTROLLER_BUTTON_BACK -> QuickSave
    0,                      // SDL_CONTROLLER_BUTTON_GUIDE
    SDL_SCANCODE_ESCAPE,    // SDL_CONTROLLER_BUTTON_START
    SDL_SCANCODE_KP_0,      // SDL_CONTROLLER_BUTTON_LSTICK
    SDL_SCANCODE_KP_1,      // SDL_CONTROLLER_BUTTON_RSTICK
    SDL_SCANCODE_LSHIFT,    // SDL_CONTROLLER_BUTTON_LSHOULDER
    SDL_SCANCODE_RSHIFT,    // SDL_CONTROLLER_BUTTON_RSHOULDER
    SDL_SCANCODE_UP,        // SDL_CONTROLLER_BUTTON_DPAD_UP
    SDL_SCANCODE_DOWN,      // SDL_CONTROLLER_BUTTON_DPAD_DOWN
    SDL_SCANCODE_LEFT,      // SDL_CONTROLLER_BUTTON_DPAD_LEFT
    SDL_SCANCODE_RIGHT,     // SDL_CONTROLLER_BUTTON_DPAD_RIGHT
  };

  static const int trigmap[] = {
    SDL_SCANCODE_LCTRL,     // trigger left
    SDL_SCANCODE_RCTRL,     // trigger right
  };

  static int osk_button = 0;

  SDL_GameControllerUpdate();

  nxgy_check_toggle();


  for (size_t i = 0; i < sizeof(keymap) / sizeof(*keymap); ++i)
    check_btn(SDL_GameControllerGetButton(sdl_ctrl, i), keymap[i]);

  for (size_t i = 0; i < sizeof(trigmap) / sizeof(*trigmap); ++i) {
    const int val = (SDL_GameControllerGetAxis(sdl_ctrl, SDL_CONTROLLER_AXIS_TRIGGERLEFT + i) > TRIGGER_THRESHOLD);
    check_btn(val, trigmap[i]);
  }

  // MINUS used to open the on-screen keyboard. It now sends F6, which the
  // game binds to QuickSave - far more useful with a controller, and the
  // keyboard was never needed during play.
  (void)osk_button;
}

static int wrap_SDL_JoystickNumButtons(SDL_Joystick *joy) {
  return 0; // our buttons are mapped to keyboard instead
}

static s16 wrap_SDL_JoystickGetAxis(SDL_Joystick *joy, int axis) {
  int val = SDL_GameControllerGetAxis(sdl_ctrl, axis);
  if (abs(val) < AXIS_DEADZONE) val = 0;

  // Soften the movement stick only. Axes 0 and 1 are aStrafe and aBaseY; the
  // look axes are left alone, since shaping those would fight the gyro.
  //
  // The dead zone above cuts to zero at its edge, which makes the stick jump
  // straight to that value the moment it is crossed. Rescaling the remaining
  // travel back to a full range restores a start from nothing, and the curve
  // then keeps small pushes genuinely small.
  if (axis <= 1 && val != 0 && nxgy_curve > 1.0f) {
    float f = (float)val / 32767.0f;
    float mag = fabsf(f);
    float t = (mag - nxgy_dead) / (1.0f - nxgy_dead);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    t = powf(t, nxgy_curve);
    val = (int)((f < 0.0f ? -t : t) * 32767.0f);
  }

  return (axis & 1) ? -val : val; // invert vertical axis
}

// SDL2 >= 2.0.16 provides this for real, so the old local stub is gone

// __libc_start_main replacement
static void start_main(int (*main_fn)(int, char **, char **), int argc, char **argv, ...) {
  printf("calling main() @ %p\n", main_fn);
  main_fn(fake_argc, fake_argv, NULL);
  exit(0);
}

/* tables */

static const solder_export_t aux_exports[] = {
  SOLDER_EXPORT_SYMBOL(__cxa_atexit),
  SOLDER_EXPORT_SYMBOL(_Unwind_Resume),

  SOLDER_EXPORT_SYMBOL(setjmp),
  SOLDER_EXPORT_SYMBOL(longjmp),

  SOLDER_EXPORT_SYMBOL(inet_pton),
  SOLDER_EXPORT_SYMBOL(getifaddrs),

  SOLDER_EXPORT_SYMBOL(setbuf),
  SOLDER_EXPORT_SYMBOL(setvbuf),

  SOLDER_EXPORT_SYMBOL(wcscat),
  SOLDER_EXPORT_SYMBOL(wcscasecmp),
  SOLDER_EXPORT_SYMBOL(wcschr),
  SOLDER_EXPORT_SYMBOL(wcsdup),
  SOLDER_EXPORT_SYMBOL(wcsncat),
  SOLDER_EXPORT_SYMBOL(wcsncasecmp),
  SOLDER_EXPORT_SYMBOL(wcsncmp),
  SOLDER_EXPORT_SYMBOL(wcsncpy),
  SOLDER_EXPORT_SYMBOL(wcsnlen),
  SOLDER_EXPORT_SYMBOL(wcsstr),
  SOLDER_EXPORT_SYMBOL(wcstol),
  SOLDER_EXPORT_SYMBOL(wcstombs),
  SOLDER_EXPORT_SYMBOL(wprintf),
  SOLDER_EXPORT_SYMBOL(vswprintf),

  SOLDER_EXPORT_SYMBOL(strsep),

  SOLDER_EXPORT_SYMBOL(sqrtf),
  SOLDER_EXPORT_SYMBOL(sin),
  SOLDER_EXPORT_SYMBOL(cos),
  SOLDER_EXPORT_SYMBOL(tan),
  SOLDER_EXPORT_SYMBOL(atan2),
  SOLDER_EXPORT_SYMBOL(fmod),

  SOLDER_EXPORT_SYMBOL(access),
  SOLDER_EXPORT_SYMBOL(unlink),

  SOLDER_EXPORT_SYMBOL(signal),
  SOLDER_EXPORT_SYMBOL(sigaction),

  SOLDER_EXPORT_SYMBOL(sleep),
  SOLDER_EXPORT_SYMBOL(usleep),
  SOLDER_EXPORT_SYMBOL(nanosleep),

  SOLDER_EXPORT_SYMBOL(pthread_kill),

  SOLDER_EXPORT_SYMBOL(iconv),
  SOLDER_EXPORT_SYMBOL(iconv_open),
  SOLDER_EXPORT_SYMBOL(iconv_close),

  SOLDER_EXPORT_SYMBOL(putenv),

  SOLDER_EXPORT_SYMBOL(__h_errno_location),

  SOLDER_EXPORT_SYMBOL(sincos),
  SOLDER_EXPORT_SYMBOL(sincosf),

  SOLDER_EXPORT_SYMBOL(SDL_SetWindowAlwaysOnTop),

  SOLDER_EXPORT_SYMBOL(xmp_set_player),
  SOLDER_EXPORT_SYMBOL(xmp_get_player),
  SOLDER_EXPORT_SYMBOL(xmp_start_player),
  SOLDER_EXPORT_SYMBOL(xmp_end_player),
  SOLDER_EXPORT_SYMBOL(xmp_create_context),
  SOLDER_EXPORT_SYMBOL(xmp_get_module_info),
  SOLDER_EXPORT_SYMBOL(xmp_get_frame_info),
  SOLDER_EXPORT_SYMBOL(xmp_load_module_from_memory),
  SOLDER_EXPORT_SYMBOL(xmp_release_module),
  SOLDER_EXPORT_SYMBOL(xmp_restart_module),
  SOLDER_EXPORT_SYMBOL(xmp_seek_time),
  SOLDER_EXPORT_SYMBOL(xmp_free_context),
  SOLDER_EXPORT_SYMBOL(xmp_set_position),
  SOLDER_EXPORT_SYMBOL(xmp_play_buffer),

  SOLDER_EXPORT_SYMBOL(alcMakeContextCurrent),
  SOLDER_EXPORT_SYMBOL(alcGetContextsDevice),
  SOLDER_EXPORT_SYMBOL(alcOpenDevice),
  SOLDER_EXPORT_SYMBOL(alcGetCurrentContext),
  SOLDER_EXPORT_SYMBOL(alcDestroyContext),
  SOLDER_EXPORT_SYMBOL(alcGetIntegerv),
  SOLDER_EXPORT_SYMBOL(alcGetError),
  SOLDER_EXPORT_SYMBOL(alcGetString),
  SOLDER_EXPORT_SYMBOL(alcCreateContext),
  SOLDER_EXPORT_SYMBOL(alcCloseDevice),
  SOLDER_EXPORT_SYMBOL(alcIsExtensionPresent),

  SOLDER_EXPORT_SYMBOL(alureBufferDataFromMemory),
  SOLDER_EXPORT_SYMBOL(alureShutdownDevice),
  SOLDER_EXPORT_SYMBOL(alureGetErrorString),

  SOLDER_EXPORT_SYMBOL(alDeleteBuffers),
  SOLDER_EXPORT_SYMBOL(alGenSources),
  SOLDER_EXPORT_SYMBOL(alGetError),
  SOLDER_EXPORT_SYMBOL(alDistanceModel),
  SOLDER_EXPORT_SYMBOL(alSourceQueueBuffers),
  SOLDER_EXPORT_SYMBOL(alFilteri),
  SOLDER_EXPORT_SYMBOL(alSourcei),
  SOLDER_EXPORT_SYMBOL(alIsBuffer),
  SOLDER_EXPORT_SYMBOL(alDopplerFactor),
  SOLDER_EXPORT_SYMBOL(alGetSourcei),
  SOLDER_EXPORT_SYMBOL(alFilterf),
  SOLDER_EXPORT_SYMBOL(alDeleteSources),
  SOLDER_EXPORT_SYMBOL(alSource3f),
  SOLDER_EXPORT_SYMBOL(alSourceUnqueueBuffers),
  SOLDER_EXPORT_SYMBOL(alSourcePlay),
  SOLDER_EXPORT_SYMBOL(alIsSource),
  SOLDER_EXPORT_SYMBOL(alListenerfv),
  SOLDER_EXPORT_SYMBOL(alSpeedOfSound),
  SOLDER_EXPORT_SYMBOL(alSource3i),
  SOLDER_EXPORT_SYMBOL(alGetString),
  SOLDER_EXPORT_SYMBOL(alBufferData),
  SOLDER_EXPORT_SYMBOL(alGenBuffers),
  SOLDER_EXPORT_SYMBOL(alSourcef),
  SOLDER_EXPORT_SYMBOL(alGetProcAddress),
  SOLDER_EXPORT_SYMBOL(alSourcefv),
  SOLDER_EXPORT_SYMBOL(alGetBufferi),
  SOLDER_EXPORT_SYMBOL(alSourceStop),
  SOLDER_EXPORT_SYMBOL(alListenerf),
  SOLDER_EXPORT_SYMBOL(alGetSourcef),

  SOLDER_EXPORT("stdin", &export_stdin),
  SOLDER_EXPORT("stdout", &export_stdout),
  SOLDER_EXPORT("stderr", &export_stderr),

  SOLDER_EXPORT("bcmp", memcmp),
  SOLDER_EXPORT("_setjmp", setjmp),
  SOLDER_EXPORT("_longjmp", longjmp),
  SOLDER_EXPORT("__errno_location", __errno),
  SOLDER_EXPORT("__pthread_key_create", pthread_key_create),
  SOLDER_EXPORT("sigemptyset", sigemptyset_fn),
  SOLDER_EXPORT("tolower", tolower_fn),
  SOLDER_EXPORT("toupper", toupper_fn),
  SOLDER_EXPORT("__isoc99_sscanf", sscanf),

  SOLDER_EXPORT("dlopen", wrap_dlopen),
  SOLDER_EXPORT("dlclose", solder_dlclose),
  SOLDER_EXPORT("dlsym", solder_dlsym),
  SOLDER_EXPORT("dlerror", solder_dlerror),

  SOLDER_EXPORT("setpwent", dummy_fn),
  SOLDER_EXPORT("endpwent", dummy_fn),

  SOLDER_EXPORT("__libc_start_main", start_main),
};

static const solder_export_t override_exports[] = {
  SOLDER_EXPORT("__cxa_throw", wrap_cxa_throw),
  SOLDER_EXPORT("accept", wrap_accept),
  SOLDER_EXPORT("connect", wrap_connect),
  SOLDER_EXPORT("bind", wrap_bind),
  SOLDER_EXPORT("sendto", wrap_sendto),
  SOLDER_EXPORT("recvfrom", wrap_recvfrom),
  SOLDER_EXPORT("getsockname", wrap_getsockname),
  SOLDER_EXPORT("getsockopt", wrap_getsockopt),
  SOLDER_EXPORT("setsockopt", wrap_setsockopt),
  SOLDER_EXPORT("fcntl", wrap_fcntl),
  SOLDER_EXPORT("read", wrap_read),
  SOLDER_EXPORT("fopen", wrap_fopen),
  SOLDER_EXPORT("fread", wrap_fread),
  SOLDER_EXPORT("fseek", wrap_fseek),
  SOLDER_EXPORT("lseek", wrap_lseek),
  SOLDER_EXPORT("readdir", wrap_readdir),
  SOLDER_EXPORT("clock_gettime", wrap_clock_gettime),
  SOLDER_EXPORT("SDL_Init", wrap_SDL_Init),
  SOLDER_EXPORT("SDL_GL_CreateContext", wrap_SDL_GL_CreateContext),
  SOLDER_EXPORT("SDL_GL_SetAttribute", wrap_SDL_GL_SetAttribute),
  SOLDER_EXPORT("SDL_GL_SwapWindow", wrap_SDL_GL_SwapWindow),
  SOLDER_EXPORT("SDL_PollEvent", wrap_SDL_PollEvent),
  SOLDER_EXPORT("SDL_GL_GetProcAddress", wrap_SDL_GL_GetProcAddress),
  SOLDER_EXPORT("SDL_GL_ExtensionSupported", wrap_SDL_GL_ExtensionSupported),
  SOLDER_EXPORT("SDL_CreateWindow", wrap_SDL_CreateWindow),
  SOLDER_EXPORT("SDL_StartTextInput", wrap_SDL_StartTextInput),
  SOLDER_EXPORT("SDL_GetKeyboardState", wrap_SDL_GetKeyboardState),
  SOLDER_EXPORT("SDL_JoystickOpen", wrap_SDL_JoystickOpen),
  SOLDER_EXPORT("SDL_JoystickClose", wrap_SDL_JoystickClose),
  SOLDER_EXPORT("SDL_JoystickUpdate", wrap_SDL_JoystickUpdate),
  SOLDER_EXPORT("SDL_JoystickGetAxis", wrap_SDL_JoystickGetAxis),
  SOLDER_EXPORT("SDL_JoystickNumButtons", wrap_SDL_JoystickNumButtons),
};

const solder_export_t *__solder_aux_exports = aux_exports;
const size_t __solder_num_aux_exports = sizeof(aux_exports) / sizeof(*aux_exports);

const solder_export_t *__solder_override_exports = override_exports;
const size_t __solder_num_override_exports = sizeof(override_exports) / sizeof(*override_exports);
