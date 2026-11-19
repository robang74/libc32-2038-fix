/* time.c/.h -- libc.fix, 2038 mitigation
 *
 * Copyright (c) 2026 [LuCy] <luchoas26@gmail.com>, MIT License
 * Copyright (c) 2026 Roberto A. Foglietta <roberto.foglietta@gmail.com>
 *
 ******************************************************************************/

#ifndef __TIME_H_
#define __TIME_H_

#ifdef __GLIBC__
#include <bits/time.h>
#else
#include <time.h> // Ensure standard musl time declarations are pulled
#endif

//#include <time.h>
//******************************************************************************
//DEFINICION

#define NSEC_PER_SEC	1000000000ULL
#define NSEC_PER_MSEC	1000000ULL
#define NSEC_PER_USEC	1000UL
#define USEC_PER_SEC	1000000ULL
#define USEC_PER_MSEC	1000ULL
#define MSEC_PER_SEC	1000UL

#define LIM_NSTIME64_NEG	-2147385600000000000LL
#define LIM_NSTIME64_POS	 2147385600000000000LL

//******************************************************************************
//DEFINICION TIPOS Y ESTRUCTURAS

#ifdef __GLIBC__
typedef __time_t time_t;
#else
typedef long __syscall_slong_t;
#endif

struct tm {
    int tm_sec;                 /* Seconds. [0-60] (1 leap second) */
    int tm_min;                 /* Minutes. [0-59] */
    int tm_hour;                /* Hours.   [0-23] */
    int tm_mday;                /* Day.     [1-31] */
    int tm_mon;                 /* Month.   [0-11] */
    int tm_year;                /* Year - 1900.  */
    int tm_wday;                /* Day of week. [0-6] */
    int tm_yday;                /* Days in year.[0-365] */
    int tm_isdst;               /* DST.     [-1/0/1] */

#ifdef	__USE_MISC
    long int tm_gmtoff;         /* Seconds east of UTC.  */
    const char *tm_zone;        /* Timezone abbreviation.  */
#else
    long int __tm_gmtoff;       /* Seconds east of UTC.  */
    const char *__tm_zone;      /* Timezone abbreviation.  */
#endif
};

typedef union {
    struct {
        unsigned long long ull;
        unsigned long long ulh;
    };
    struct {
        long long sll;
        long long slh;
    };
    int si[4];
    unsigned int ui[4];
    short ss[8];
    unsigned short us[8];
    char c[16];
    unsigned char uc[16];
} __tvar128;

//******************************************************************************
//CONSTANTES

extern const unsigned int kLStime[];
extern const char *__afmt[];
extern const char *__Afmt[];
extern const char *__bfmt[];
extern const char *__Bfmt[];
extern const char *__hfmt[];
extern const char *__Hfmt[];

//******************************************************************************
//DEFINICION VARIABLES GLOBALES

extern long int __timezone; // Seconds west of UTC

//******************************************************************************
//DEFINICION DE FUNCIONES EXTERNAS

#ifdef __GLIBC__
extern int clock_gettime(__clockid_t __clock_id, struct timespec *__tp) __THROW;
extern int clock_adjtime(__clockid_t __clock_id, struct timex *__utx) __THROW;
#endif

//******************************************************************************
//DEFINICION DE FUNCIONES

struct tm *gmtime(const time_t * __timer);
struct tm *gmtime_r(const time_t * __restrict __timer, struct tm *__restrict __tp);
struct tm *localtime(const time_t * __timer);
struct tm *localtime_r(const time_t * __restrict __timer, struct tm *__restrict __tp);
char *asctime(const struct tm *__tp);
char *ctime(const time_t * __timer);
char *asctime_r(const struct tm *__restrict __tp, char *__restrict __buf);
char *ctime_r(const time_t * __restrict __timer, char *__restrict __buf);
//char *strptime (const char *__restrict __s, const char *__restrict __fmt, struct tm *__tp);
size_t strftime(char *__restrict __s, size_t __maxsize, const char *__restrict __format,
                const struct tm *__restrict __tp);
time_t mktime(struct tm *__tp);
time_t timegm(struct tm *__tp);
time_t timelocal(struct tm *__tp);
int stime(const time_t * __when);
time_t time(time_t * __timer);
#ifdef OLD_TOOL_CHAIN
#ifndef ADJ_SETOFFSET
#define ADJ_SETOFFSET 0x0100
#endif
#ifndef ADJ_NANO
#define ADJ_NANO 0x2000
#endif
int gettimeofday(struct timeval *__restrict __tv, struct timezone *__restrict __tz);
#else
int gettimeofday(struct timeval *__restrict __tv, void *__restrict __tz);
#endif
int settimeofday(const struct timeval *__tv, const struct timezone *__tz);
unsigned long long timespec2ns(struct timespec *__tp);

//******************************************************************************
#endif /* __TIME_H_ */
