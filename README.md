# libc.fix — Lightweight Y2K38 Mitigation for 32-bit Embedded Linux (2106 Extension)

A minimal, zero-dependency C shared library (`libc.fix.so`, ~few KB) designed to extend the operational lifespan of legacy 32-bit Linux embedded systems and BusyBox environments from **January 19, 2038** (`0x7FFFFFFF`) up to **February 7, 2106** (`0xFFFFFFFF`).

Built specifically for deployed hardware running legacy kernels and closed-source userland binaries where migrating to 64-bit `time_t` (`_TIME_BITS=64`) or recompiling the firmware is not feasible.

---

## How It Works

Instead of altering the 4-byte `time_t` ABI—which would corrupt stack layouts and registers in pre-compiled 32-bit binaries—`libc.fix.so` intercepts standard `<time.h>` calls via `LD_PRELOAD` (or `/etc/ld.so.preload`) and reinterprets the 32-bit epoch counter as an **unsigned integer** (`0` to `4,294,967,295` seconds).

### Key Technical Features

* **ABI-Safe 32-bit Unsigned Reinterpretation:** Keeps `sizeof(time_t) == 4` intact at the binary boundary while performing all internal calendar and epoch math using `unsigned int`, unlocking an extra ~68 years (2,147,483,648 seconds) with zero binary recompilation.
* **Two-Step Kernel Clock Stepping (`clock_adjtime`):** Legacy 32-bit kernels reject single clock jumps larger than `±2^31 - 1` seconds (`EINVAL`). `lucy_set_rtcc_tp()` uses `ADJ_SETOFFSET | ADJ_NANO` split across up to two bounded steps (`±2,147,385,600,000,000,000 ns`, ~68.09 years per step) to smoothly step the kernel timekeeper from 1970 directly into post-2038 dates.
* **Nanosecond Phase Preservation:** Tracks `struct timespec` in RAM and applies adjustments directly in nanoseconds (`ADJ_NANO`) using pure 64-bit integer arithmetic, avoiding 0.5-second rounding drift.
* **Minimal Footprint:** Pure C with integer arithmetic and no external dependencies beyond standard `libc` syscalls (`clock_gettime`, `clock_adjtime`). Ideal for flash- and RAM-constrained embedded devices.
* **BusyBox & Legacy Daemon Compatible:** Works seamlessly with dynamically linked BusyBox applets (`date`, `crond`, `syslogd`, `ntpd`, `httpd`) and proprietary userland services.

---

## Intercepted Functions

* `time()`, `stime()`, `gettimeofday()`, `settimeofday()`
* `gmtime()`, `gmtime_r()`, `localtime()`, `localtime_r()`
* `mktime()`, `timegm()`, `timelocal()`
* `asctime()`, `asctime_r()`, `ctime()`, `ctime_r()`
* `strftime()` *(includes custom `%n` total epoch nanoseconds and `%N` fractional nanoseconds)*

---

## Building

### Cross-Compilation (ARM / MIPS / x86 / PowerPC 32-bit)

```bash
# Generic ARM 32-bit (Buildroot / OpenWrt / armhf)
make CROSS_COMPILE=arm-linux-gnueabihf-

# Qualcomm MDM9607 / OpenEmbedded / Yocto
make CROSS_COMPILE=arm-oe-linux-gnueabi- CFLAGS="-O2 -march=armv7-a -mfloat-abi=softfp -mfpu=neon -fPIC -D_GNU_SOURCE"

# MIPS 32-bit
make CROSS_COMPILE=mipsel-linux-gnu-
```

### Native Compilation (32-bit host/container)

```bash
make
```

---

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

> **Note:** `LD_PRELOAD` and `/etc/ld.so.preload` apply to all dynamically linked executables (including dynamically linked BusyBox builds). Statically linked binaries bypass the dynamic linker and will not load preloaded libraries.

---

## Resumen en Español

**`libc.fix.so`** es una librería compartida ultracompacta (pocos KB) en C puro que extiende la vida útil de sistemas **Linux embebidos de 32 bits** y entornos **BusyBox** desde el **19 de enero de 2038** hasta el **7 de febrero de 2106**, sin necesidad de recompilar el kernel ni los binarios cerrados del fabricante.

* **Compatibilidad binaria (ABI) intacta:** Mantiene `time_t` en 4 bytes para no alterar la pila ni los registros de los ejecutables existentes, pero reinterpreta internamente los bits como `unsigned int` (de `0` a `4.294.967.295` segundos).
* **Salto doble en el kernel (`clock_adjtime`):** Divide los saltos de reloj mayores a 68 años en dos pasos consecutivos mediante `ADJ_SETOFFSET | ADJ_NANO`, evitando el error `EINVAL` de los kernels antiguos de 32 bits al fijar fechas posteriores a 2038 desde 1970.
* **Precisión al nanosegundo:** Conserva la fase de nanosegundos en RAM sin redondeos de medio segundo.
