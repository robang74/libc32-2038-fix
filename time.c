/* time.c/.h -- libc.fix, 2038 mitigation
 *
 * Copyright (c) 2026 [LuCy] <luchoas26@gmail.com>, MIT License
 * Copyright (c) 2026 Roberto A. Foglietta <roberto.foglietta@gmail.com>
 *
 ******************************************************************************/

//Include's
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#ifdef __GLIBC__
#include <bits/types.h>
#endif
#include <sys/syscall.h>
#include <sys/timex.h>
#include <sys/ioctl.h>
#include <sys/timeb.h>

#define frmarg_half_size (sizeof(frmarg) >> 1)

//RAF: to have one snapshot for each calculation,
//     doesn't protect against async re-entering
#pragma message \
("__timezone atomic read is system/hw dependent, in the most general case")
#define noval_tmezne 86400L //RAF: potentially 0L could be more suitable here
#define reset_tmezne() { tmezne = noval_tmezne; }
#define initl_tmezne() ({ if(tmezne == noval_tmezne) tmezne = __timezone; tmezne; })
static __thread long tmezne = noval_tmezne;

#include "time.h"
//******************************************************************************
//DEFINICION CONSTANTES DE USO INTERNO

const unsigned kLStime[] =
    { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
const char *__afmt[] =
    { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", };
const char *__Afmt[] =
    { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", };
const char *__bfmt[] =
    { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", };
const char *__Bfmt[] =
    { "January", "February", "March", "April", "May", "June", "July", "August", "September",
        "October", "November", "December", };
const char *__hfmt[] = { "am", "pm", };
const char *__Hfmt[] = { "AM", "PM", };

//******************************************************************************
//DEFINICION TIPOS Y CLASES DE USO INTERNO

//******************************************************************************
//DEFINICION VARIABLES GLOBALES

//******************************************************************************
//DEFINICION VARIABLES GLOBALES DE USO INTERNO

struct timespec actual_time = { 0, 0 };

//******************************************************************************
//DEFINICION DE FUNCIONES EXTERNAS

//******************************************************************************
//DEFINICION DE FUNCIONES DE USO INTERNO

char *ltrim(char *s);
char *rtrim(char *s);
char *trim(char *s);
int lucy_set_rtcc_tp(struct timespec *__tp);

char *ltrim(char *s)
{
    while ((*s <= ' ') & (*s != 0))
        s++;
    return s;
}

char *rtrim(char *s)
{
    int len = strlen(s);
    if (len > 0) {
        char *back = s + len;
        while (len) {
            if (*--back > ' ')
                break;
            len--;
        }
        *(back + 1) = 0;
    }
    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s));
}

int lucy_set_rtcc_tp(struct timespec *__tp)
{
    struct timespec tp;
    struct timex utx;
    unsigned long long ns_set;
    long long ns_dif;
    int retry;

    ns_set = timespec2ns(__tp);
    for (retry = 0; retry < 2; retry++) {
        memset(&utx, 0, sizeof(struct timex));
        utx.modes = ADJ_SETOFFSET | ADJ_NANO;
        clock_gettime(CLOCK_REALTIME, &tp);
        ns_dif = ((long long)ns_set - (long long)timespec2ns(&tp));
        if (ns_dif < LIM_NSTIME64_NEG)
            ns_dif = LIM_NSTIME64_NEG;
        else if (ns_dif > LIM_NSTIME64_POS)
            ns_dif = LIM_NSTIME64_POS;
        else
            retry = 0xFFFF;
        tp.tv_sec = (time_t)
          ((long long)ns_dif / (long long)NSEC_PER_SEC);
        tp.tv_nsec = (__syscall_slong_t)
          ((long long)ns_dif % (long long)NSEC_PER_SEC);
        while (tp.tv_nsec < 0) {
            tp.tv_nsec += NSEC_PER_SEC;
            tp.tv_sec--;
        }
        utx.time.tv_sec = tp.tv_sec;
        utx.time.tv_usec = tp.tv_nsec;
        if (clock_adjtime(CLOCK_REALTIME, &utx) == -1)
            return -1;
    }
    memcpy(&actual_time, __tp, sizeof(struct timespec));
    return 0;
}

#if defined(__USE_MISC) || defined(__USE_BSD) || defined(_GNU_SOURCE)
// Populate tm_zone as +/-HH:MM (matching %:z) for callers like GNU date
#define USE_TZ_HHMM 1
#else
#define USE_TZ_HHMM 0
#endif

static void format_tz_hhmm(struct tm *__ptm) {
#if USE_TZ_HHMM
    //RAF: lib.fix.so gmtime_r() and localtime_r() are thread-safe
    //     because of the __thread usage in format_tz_hhmm(), watch out!
    static __thread char tz_offset_str[16];
    initl_tmezne();
    int off_h = -(int)tmezne / 3600;
    int off_m = (int)tmezne % 3600;
    if (off_m < 0) off_m = -off_m;
    off_m /= 60;
    sprintf(tz_offset_str, "%+03d:%02d", off_h, off_m);
    __ptm->tm_zone = tz_offset_str;
    __ptm->tm_gmtoff = -tmezne;
#endif
    reset_tmezne();
}

//******************************************************************************
//                      FUNCIONES DEL MODULO
//******************************************************************************
struct tm *gmtime(const time_t * __timer)
{
    //RAF: lib.fix.so gmtime_r() and localtime_r() are thread-safe
    //     because of the __thread usage in gmtime(), watch out!
    static __thread struct tm __ret;
    struct tm __tm;
    unsigned ui0, ui1;

    ui0 = (unsigned)*__timer;
    if (actual_time.tv_sec != (time_t) ui0) {
        actual_time.tv_sec = ui0;
        actual_time.tv_nsec = 0;
    }
    memset(&__tm, 0, sizeof(struct tm));

    __tm.tm_wday = (((ui0 / 86400) + 4) % 7);
    ui1 = ui0 / 60;
    __tm.tm_sec = (ui0 - (60 * ui1));
    ui0 = ui1 / 60;
    __tm.tm_min = (ui1 - (60 * ui0));
    ui1 = ui0 / 24;
    __tm.tm_hour = (ui0 - (24 * ui1));
    ui0 = 4 * (ui1 + 731);
    /* RAF: 2100 leap year exception fix */
    if (ui1 >= 47847)
        ui0 += 4;
    __tm.tm_year = (ui0 / 1461);
    __tm.tm_yday = ((ui0 - 1461 * (unsigned)(__tm.tm_year)) / 4);
    __tm.tm_mon = 13;
    do {
        __tm.tm_mon--;
        ui1 = 0;
        /* RAF: 2100 leap year exception fix */
        if (__tm.tm_mon > 2 && __tm.tm_year != 2100 - 1900 - 68)
            ui1 = !(__tm.tm_year & 3);
        ui0 = kLStime[__tm.tm_mon] + ui1;
    } while ((unsigned)__tm.tm_yday < ui0);
    __tm.tm_mday = __tm.tm_yday - ui0 + 1;
    if (__tm.tm_mday < 1)
        __tm.tm_mday = 1;
    if (__tm.tm_mon > 0)
        __tm.tm_mon--;
    __tm.tm_year += 68;

    format_tz_hhmm(&__tm);

    memcpy(&__ret, &__tm, sizeof(struct tm));
    return &__ret;
}

struct tm *gmtime_r(const time_t * __restrict __timer,
    struct tm *__restrict __tp)
{
    if (__tp != NULL) {
        struct tm *__ret = gmtime(__timer);
        memcpy(__tp, __ret, sizeof(struct tm));
    }
    return __tp;
}

struct tm *localtime(const time_t * __timer)
{
    time_t tm = *__timer + initl_tmezne();
    return gmtime(&tm);
}

struct tm *localtime_r(const time_t * __restrict __timer,
    struct tm *__restrict __tp)
{
    if (__tp != NULL) {
        struct tm *__ret = localtime(__timer);
        memcpy(__tp, __ret, sizeof(struct tm));
    }
    return __tp;
}

//"Day Mon dd hh:mm:ss yyyy\n"
char *asctime(const struct tm *__tp)
{
    //RAF: lib.fix.so asctime_r() and ctime_r() are thread-safe
    //     because of the __thread usage in asctime(), watch out!
    static __thread char str_time[32];
    unsigned tm_mon = (unsigned)__tp->tm_mon;
    unsigned tm_wday = (unsigned)__tp->tm_wday;
    if (tm_mon > 11)
        tm_mon = 0;
    if (tm_wday > 6)
        tm_wday = 0;
    sprintf(str_time, "%s %s %02d %02d:%02d:%02d %04d\n",
      __afmt[tm_wday], __bfmt[tm_mon], __tp->tm_mday, __tp->tm_hour,
      __tp->tm_min, __tp->tm_sec, __tp->tm_year + 1900);
    return str_time;
}

char *ctime(const time_t * __timer)
{
    return asctime(localtime(__timer));
}

char *asctime_r(const struct tm *__restrict __tp, char *__restrict __buf)
{
    if (__buf != NULL) {
        char *str_time = asctime(__tp);
        strcpy(__buf, str_time);
    }
    return __buf;
}

/* Equivalent to `asctime_r (localtime_r (timer, *TMP*), buf)'.  */
char *ctime_r(const time_t * __restrict __timer, char *__restrict __buf)
{
    return asctime_r(localtime(__timer), __buf);
}

time_t mktime(struct tm *__tp)
{
    unsigned tm_year = (unsigned)__tp->tm_year + 1900;
    unsigned tm_mon = (unsigned)__tp->tm_mon + 1;
    if (0 >= (int)(tm_mon -= 2)) {
        tm_mon += 12;
        tm_year -= 1;
    }
    unsigned time_sec = ((((unsigned)
      (tm_year / 4 - tm_year / 100 + tm_year / 400 + 367 * tm_mon / 12
             + (unsigned)__tp->tm_mday) + tm_year * 365 - 719499) * 24
             + (unsigned)__tp->tm_hour /* now have hours */
      ) * 60 + (unsigned)__tp->tm_min  /* now have minutes */
      ) * 60 + (unsigned)__tp->tm_sec; /* finally seconds */
    actual_time.tv_sec = time_sec;
    actual_time.tv_nsec = 0;
    return time_sec;
}

time_t timegm(struct tm *__tp)
{
    return mktime(__tp);
}

time_t timelocal(struct tm *__tp)
{
    return mktime(__tp) + initl_tmezne();
}

int strcpy_len(char *__dest, const char *__src)
{
    int len_ret = 0;
    while (*__src != 0) {
        *__dest = *__src;
        __dest++;
        __src++;
        len_ret++;
    }
    *__dest = 0;
    return len_ret;
}

#define strftime_macro_1 \
  if ((lret+tlen) >= __maxsize) { reset_tmezne(); return lret; } \
  strcpy(__s,tmp); tmp[0]=0; __s+=tlen; *__s=0; lret+=tlen; tlen=0
size_t strftime(char *__restrict __s, size_t __maxsize,
    const char *__restrict __format, const struct tm *__restrict __tp)
{
    unsigned lret = 0;
    unsigned tlen = 0;
    char token[256];
    char tmp[64], arg[16];
    char frmarg[32];
    int arglen;
    __tvar128 var128A;

    int i, j = 0, k = 0;
    char fmt_ch;
    char *pch1;

    *__s = 0;
    memset(token, 0x00, sizeof(token));
    initl_tmezne();
    if (tmezne < -43200)
        tmezne = -43200;
    else if (tmezne > 50400)
        tmezne = 50400;

    struct tm tm;
    struct timespec tp;

    memcpy(&tm, __tp, sizeof(tm));
    memcpy(&tp, &actual_time, sizeof(tp));

    // parsing the formatted string
    //fprintf(stderr, "%s\n", __format);
    for (i = 0; __format[i] != '\0'; i++) {
        if ((unsigned)k < (sizeof(token) - 4)) {
            token[k++] = __format[i];
        } else {
            k = (sizeof(token) - 4);
            token[k] = 0;
        }

        if (__format[i + 1] == '%' || __format[i + 1] == '\0') {
            token[k] = '\0';

            k = 0;
            if (token[0] != '%') {
                // it is not a format specifier
                if (token[0]) {
                    tlen = strcpy_len(tmp, token);
                    strftime_macro_1;
                }
            } else {
                fmt_ch = 0;
                j = 1;
                while ((unsigned)j < (sizeof(token) - 4)) {
                    fmt_ch = token[j++];
                    if (fmt_ch >= 'A')
                        break;
                }

                arglen = 0;
                memset(frmarg, 0, sizeof(frmarg));
                if (j >= 3) {
                    if ((unsigned)j < frmarg_half_size) {
                        size_t copy_len = (size_t)(j - 2);
                        // RAF: silencing the  __builtin___strncpy_chk -Wstringop-truncation
                        if (copy_len >= sizeof(tmp))
                            copy_len = sizeof(tmp) - 1;
                        memcpy(tmp, &token[1], copy_len);
                        // RAF: having set the string terminator, don't need string zeroing
                        tmp[copy_len] = '\0';
                        pch1 = trim(tmp);
                        pch1 = trim(tmp);
                        while ((*pch1 == '-') | (*pch1 == '+'))
                            pch1++;
                        arglen = atoi(pch1);
                        *pch1 = 0;
                        if (arglen < 1)
                            arglen = 1;
                        else if (arglen > 32)
                            arglen = 32;
                        // RAF: copy safely into 'arg' (size 16) w/o risking buffer overflow
                        strncpy(&frmarg[frmarg_half_size], tmp, frmarg_half_size - 1);
                        frmarg[sizeof(frmarg) - 1] = '\0';
                    }
                }

                if (token[0] != '%')
                    fmt_ch = 0;
                var128A.si[0] = arglen;
                switch (fmt_ch) {
                case 'C': //Año, dividido por 100 (por ejemplo, el siglo).
                case 'd': //Day of the month (01-31) 19
                case 'e': //Se sustituye por el día del mes como número decimal (1-31).
                case 'H': //Hour in 24h format (00-23) 14
                case 'I': //Hour in 12h format (01-12) 05
                case 'm': //Month as a decimal number (01-12) 08
                case 'M': //Minute (00-59) 55
                case 'S': //Second (00-59) 02
                case 'U': //Week number with the first Sunday
                         // as the first day of week one (00-53) 33
                case 'W': //Week number with the first Monday
                         // as the first day of week one (00-53) 34
                case 'y': //Year, last two digits (00-99) 01
                    if (arglen < 2)
                        arglen = 2;
                    break;
                case 'j': //Day of the year (001-366) 231
                    if (arglen < 3)
                        arglen = 3;
                    break;
                case 'Y': //Year  2012
                    if (arglen < 4)
                        arglen = 4;
                    break;
                case 'n': //Nanosegundos
                    if (arglen < 1)
                        arglen = 19;
                    var128A.si[0] = arglen;
                    if (arglen < 19)
                        arglen = 19;
                    break;
                case 'N': //Nanosegundos
                    if (arglen < 1)
                        arglen = 9;
                    var128A.si[0] = arglen;
                    if (arglen < 9)
                        arglen = 9;
                    break;
                case 's': //Segundos
                    if (arglen < 1)
                        arglen = 1;
                    break;
                }
                memcpy(arg, &frmarg[frmarg_half_size], frmarg_half_size);
/*
 * WARNING -- Potential Format String RiskLocation
 *
 * The Issue: arg is derived from user-controlled format specifier modifiers
 * extracted from __format. If arg contains unexpected format specifiers (such
 * as %n or additional % signs), passing it directly into sprintf's format
 * argument can cause undefined behavior or format string injection vulnerabilities.
 * Ensure arg is strictly filtered to only contain numeric digits or expected width
 * specifiers before passing it to sprintf (aka user inputs sanification).
 */
                sprintf(frmarg, "\%%%s0%du", arg, arglen);

                switch (fmt_ch) {
                case 'a': //Abbreviated weekday name Sun
                    tlen = strcpy_len(tmp, __afmt[tm.tm_wday]);
                    break;
                case 'A': //Full weekday name Sunday
                    tlen = strcpy_len(tmp, __Afmt[tm.tm_wday]);
                    break;
                case 'b': //Abbreviated month name Mar
                    tlen = strcpy_len(tmp, __bfmt[tm.tm_mon]);
                    break;
                case 'B': //Full month name March
                    tlen = strcpy_len(tmp, __Bfmt[tm.tm_mon]);
                    break;
                case 'c': //Date and time representation Sun Aug 19 02:56:02 2012
                    tlen = sprintf(tmp, "%s %s %02d %02d:%02d:%02d %04d",
                        __afmt[tm.tm_wday], __bfmt[tm.tm_mon], tm.tm_mday,
                        tm.tm_hour, tm.tm_min, tm.tm_sec % 60, tm.tm_year + 1900);
                    break;
                case 'C': //Año, dividido por 100 (por ejemplo, el siglo).
                    tlen = sprintf(tmp, frmarg, (unsigned)(tm.tm_year + 1900) / 100);
                    break;
                case 'd': //Day of the month (01-31) 19
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_mday);
                    break;
                case 'D': //Equivalente a %m/%d/%y "25/09/08"
                    tlen =
                      sprintf(tmp, "%02d/%02d/%02d", tm.tm_mon + 1, tm.tm_mday,
                        (tm.tm_year + 1900) % 100);
                    break;
                case 'e': //Se sustituye por el día del mes como número decimal (1-31).
                    tlen = sprintf(tmp, "%d", tm.tm_mday);
                    break;
                case 'H': //Hour in 24h format (00-23) 14
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_hour);
                    break;
                case 'I': //Hour in 12h format (01-12) 05
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_hour % 12);
                    break;
                case 'j': //Day of the year (001-366) 231
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_yday + 1);
                    break;
                case 'm': //Month as a decimal number (01-12) 08
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_mon + 1);
                    break;
                case 'M': //Minute (00-59) 55
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_min);
                    break;
                case 'n': //Nanosegundos
                    memcpy(arg, &frmarg[frmarg_half_size], frmarg_half_size);
/*
 * WARNING -- Potential Format String RiskLocation
 */
                    //  Maximo valor : 0-4294967295999999999
                    sprintf(frmarg, "\%%%s0%dllu", arg, arglen);
                    sprintf(tmp, frmarg, timespec2ns(&tp));
                    tmp[var128A.si[0]] = 0;
                    tlen = strlen(tmp);
                    break;
                case 'N': //Nanosegundos
                    //  Maximo valor : 0-2146828337
                    //               o 0-2146828337999999
                    //               o 0-86399999999
                    //               o 0-86399
                    sprintf(tmp, frmarg, (unsigned)tp.tv_nsec);
                    tmp[var128A.si[0]] = 0;
                    tlen = strlen(tmp);
                    break;
                case 'p': //am or pm designation PM
                    tlen = strcpy_len(tmp, __hfmt[(tm.tm_hour / 12) & 1]);
                    break;
                case 'P': //AM or PM designation PM
                    tlen = strcpy_len(tmp, __Hfmt[(tm.tm_hour / 12) & 1]);
                    break;
                case 'R': //Equivalente a %H:%M.
                    tlen = sprintf(tmp, "%02d:%02d", tm.tm_hour, tm.tm_min);
                    break;
                case 'r': //Equivalente a %I:%M:%S %p .
                    tlen =
                      sprintf(tmp, "%02d:%02d:%02d %s", tm.tm_hour % 12, tm.tm_min,
                        tm.tm_sec % 60, __hfmt[(tm.tm_hour / 12) & 1]);
                    break;
                case 'S': //Second (00-59) 02
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_sec % 60);
                    break;
                case 's': //Segundos
                    tlen = sprintf(tmp, frmarg, (unsigned)actual_time.tv_sec);
                    break;
                case 'U': //Week number with the first Sunday
                         // as the first day of week one (00-53) 33
                    tlen =
                      sprintf(tmp, frmarg,
                        (unsigned)((tm.tm_yday + 7 - tm.tm_wday) / 7) % 100);
                    break;
                case 'w': //Weekday as a decimal number with Sunday as 0 (0-6)
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_wday);
                    break;
                case 'W': //Week number with the first Monday
                         // as the first day of week one (00-53) 34
                    tlen =
                      sprintf(tmp, frmarg,
                        (unsigned)((tm.tm_yday + 7 -
                           (tm.tm_wday ? (tm.tm_wday - 1) : 6)) / 7) % 100);
                    break;
                case 'x': //Date representation 08/19/12
                    tlen = sprintf(tmp, "%02d/%02d/%02d", tm.tm_mday,
                        tm.tm_mon + 1, (tm.tm_year + 1900) % 100);
                    break;
                case 'X': //Time representation 02:50:06
                    tlen = sprintf(tmp, "%02d:%02d:%02d",
                      tm.tm_hour, tm.tm_min, tm.tm_sec % 60);
                    break;
                case 'y': //Year, last two digits (00-99) 01
                    tlen = sprintf(tmp, frmarg, (unsigned)(tm.tm_year + 1900) % 100);
                    break;
                case 'Y': //Year  2012
                    tlen = sprintf(tmp, frmarg, (unsigned)tm.tm_year + 1900);
                    break;
                case 'Z': //Timezone name or abbreviation CDT
                          //not yet, but formatted like %:z (+HH:MM)
                case 'z':
                    var128A.si[0] = (int)tmezne / 3600;
                    var128A.si[2] = (int)tmezne - (var128A.si[0] * 3600);
                    if (var128A.si[2] < 0)
                        var128A.si[2] *= -1;
                    var128A.si[1] = var128A.si[2] / 60;
                    var128A.si[2] = var128A.si[2] - (var128A.si[1] * 60);
                    if (token[2] == 'H') {
                        j++;
                    } else if (token[2] == 'M') {
                        j++;
                        var128A.si[1] |= 0xF0000;
                    } else if (token[2] == 'S') {
                        j++;
                        var128A.si[1] |= 0xF0000;
                        var128A.si[2] |= 0xF0000;
                    } else {
                        var128A.si[1] |= 0xF0000;
                    }
                    if (var128A.si[2]  & 0xF0000)
                        pch1 = "%+03d:%02d:%02d";
                    else
                    if (var128A.si[1]  & 0xF0000)
                        pch1 = "%+03d:%02d";
                    else
                        pch1 = "%+03d";
                    var128A.si[1] &= 0xFF;
                    var128A.si[2] &= 0xFF;
                    tlen = sprintf(tmp, pch1, var128A.si[0],
                               var128A.si[1], var128A.si[2]);
                    break;
                default:
                    tlen = strcpy_len(tmp, token);
                    break;
                }
                if (token[j])
                    tlen += strcpy_len(&tmp[tlen], &token[j]);
                strftime_macro_1;
                fmt_ch = 0;
            }
        }
        strftime_macro_1;
    }
    strftime_macro_1;
    reset_tmezne();
    return lret;
}

int stime(const time_t * __when)
{
    struct timespec ts;
    ts.tv_sec = *__when;
    ts.tv_nsec = 0;
    return lucy_set_rtcc_tp(&ts);
}

time_t time(time_t * __timer)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (__timer != NULL)
        *__timer = tv.tv_sec;
    return tv.tv_sec;
}

#ifdef OLD_TOOL_CHAIN
int gettimeofday(struct timeval *__restrict __tv, struct timezone *__restrict __tz)
#else
int gettimeofday(struct timeval *__restrict __tv, void *__restrict __tz)
#endif
{
    struct timespec ts;
    (void)__tz;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        return -1;
    memcpy(&actual_time, &ts, sizeof(struct timespec));
#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
    if (__tv != NULL) {
        __tv->tv_sec = ts.tv_sec;
        __tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
    }
#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
    return 0;
}

int settimeofday(const struct timeval *__tv, const struct timezone *__tz)
{
    struct timespec ts;
    (void)__tz;
    ts.tv_sec = __tv->tv_sec;
    ts.tv_nsec = __tv->tv_usec * NSEC_PER_USEC;
    memcpy(&actual_time, &ts, sizeof(struct timespec));
    return lucy_set_rtcc_tp(&ts);
}

unsigned long long timespec2ns(struct timespec *__tp)
{
    return (unsigned long long)
         (((unsigned long long)((unsigned)__tp->tv_sec)
         * (unsigned long long)NSEC_PER_SEC)
         + (unsigned long long)__tp->tv_nsec);
}

//******************************************************************************
