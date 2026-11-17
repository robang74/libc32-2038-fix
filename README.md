# libc.fix 2038

**Source code available under MIT license**

> [!WARNING]
>
> **LIMITITATIONS APPLY**
> 
> * **The Pre-1970 Blind Spot**: it is a fundamentally flawed approach for systems that rely on timestamps before January 1, 1970 (UNIX epoch). Because standard 32-bit `time_t` is signed 32-bit integers, historical dates are represented as negative numbers while the unigned wraps after 2038.
>
> * **ABI Boundary Mismatch**: if a precompiled binary passes `time_t` pointers into kernel system calls directly (via raw syscalls rather than wrapped libc functions), the kernel will still interpret them as signed 32-bit integers, causing immediate overflow failures in 2038.
>
> * **Static Closed-Source**: the `LD_PRELOAD` and `/etc/ld.so.preload` apply to all dynamically linked executables. Statically linked binaries bypass the dynamic linker and will not load preloaded libraries. If the binary is closed-source and statically linked, `libc.fix` cannot hook into it.

## Lightweight Y2K38 Mitigation 

This lightweight Y2K38 mitigation for 32-bit embedded Linux (2106 extension) is a minimal, zero-dependency C shared library (`libc.fix.so`, ~few KB) designed to extend the operational lifespan of legacy 32-bit Linux embedded systems and BusyBox environments from **January 19, 2038** (`0x7FFFFFFF`) up to **February 7, 2106** (`0xFFFFFFFF`).

Built specifically for deployed hardware running legacy kernels and closed-source userland binaries where migrating to 64-bit `time_t` (`_TIME_BITS=64`) or recompiling the firmware is not feasible.

<br>

## How It Works

Instead of altering the 4-byte `time_t` ABI—which would corrupt stack layouts and registers in pre-compiled 32-bit binaries—`libc.fix.so` intercepts standard `<time.h>` calls via `LD_PRELOAD` (or `/etc/ld.so.preload`) and reinterprets the 32-bit epoch counter as an **unsigned integer** (`0` to `4,294,967,295` seconds).

### Key Technical Features

* **ABI-Safe 32-bit Unsigned Reinterpretation:** Keeps `sizeof(time_t) == 4` intact at the binary boundary while performing all internal calendar and epoch math using `unsigned int`, unlocking an extra ~68 years (2,147,483,648 seconds) with zero binary recompilation.

* **Two-Step Kernel Clock Stepping (`clock_adjtime`):** Legacy 32-bit kernels reject single clock jumps larger than `±2^31 - 1` seconds (`EINVAL`). `lucy_set_rtcc_tp()` uses `ADJ_SETOFFSET | ADJ_NANO` split across up to two bounded steps (`±2,147,385,600,000,000,000 ns`, ~68.09 years per step) to smoothly step the kernel timekeeper from 1970 directly into post-2038 dates.

* **Nanosecond Phase Preservation:** Tracks `struct timespec` in RAM and applies adjustments directly in nanoseconds (`ADJ_NANO`) using pure 64-bit integer arithmetic, avoiding 0.5-second rounding drift.

* **Minimal Footprint:** Pure C with integer arithmetic and no external dependencies beyond standard `libc` syscalls (`clock_gettime`, `clock_adjtime`). Ideal for flash- and RAM-constrained embedded devices.

* **BusyBox & Legacy Daemon Compatible:** Works seamlessly with dynamically linked BusyBox applets (`date`, `crond`, `syslogd`, `ntpd`, `httpd`) and proprietary userland services.

<br>

## Intercepted Functions

* `time()`, `stime()`, `gettimeofday()`, `settimeofday()`
* `gmtime()`, `gmtime_r()`, `localtime()`, `localtime_r()`
* `mktime()`, `timegm()`, `timelocal()`
* `asctime()`, `asctime_r()`, `ctime()`, `ctime_r()`
* `strftime()` *(includes custom `%n` total epoch nanoseconds and `%N` fractional nanoseconds)*

<br>

## Building

### Cross-Compilation (ARM / MIPS / x86 / PowerPC 32-bit)

```bash
# Generic ARM 32-bit (Buildroot / OpenWrt / armhf)
make CROSS_COMPILE=arm-linux-gnueabihf-

# Qualcomm MDM9607 / OpenEmbedded / Yocto
make CROSS_COMPILE=arm-oe-linux-gnueabi- \
CFLAGS="-O2 -march=armv7-a -mfloat-abi=softfp -mfpu=neon -fPIC -D_GNU_SOURCE"

# MIPS 32-bit
make CROSS_COMPILE=mipsel-linux-gnu-
```

### Native Compilation (32-bit host/container)

```bash
make
```

<br>

## Deployment & Usage

### 1. Test on a single command (BusyBox `date`)

```bash
LD_PRELOAD=/path/to/libc.fix.so date -s "2045-06-15 12:00:00"
LD_PRELOAD=/path/to/libc.fix.so date
```

### 2. System-wide activation

Copy `libc.fix.so` to `/lib/` (or `/data/lib/` on read-only root filesystems) and register it in `/etc/ld.so.preload`:

```bash
cp libc.fix.so /lib/libc.fix.so
chmod 0755 /lib/libc.fix.so
echo "/lib/libc.fix.so" > /etc/ld.so.preload
```

