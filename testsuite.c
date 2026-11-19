/*
 * testsuite.c -- libc.fix, 2038 mitigation
 *
 * Boundary verification harness for 32-bit unsigned epoch extension,
 * specifically validating the non-leap century year 2100 transition
 * and round-trip consistency between gmtime() and mktime().
 *
 * Copyright (c) 2026 Roberto A. Foglietta <roberto.foglietta@gmail.com>, GPLv2
 *
 ******************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

typedef struct {
    const char *label;
    uint32_t epoch_u32;
    int expected_year;  /* Full calendar year, e.g., 2100 */
    int expected_mon;   /* 1-12 */
    int expected_mday;  /* 1-31 */
    int expected_hour;  /* 0-23 */
    int expected_min;   /* 0-59 */
    int expected_sec;   /* 0-59 */
} boundary_case_t;

/*
 * Exact UTC epoch timestamps (unsigned 32-bit) for critical Y2K38,
 * 2100 century non-leap boundary, and Y2106 ceiling transitions.
 */
static const boundary_case_t test_cases[] = {
    { "Y2K38 signed max (last positive int32)",   0x7FFFFFFFU, 2038,  1, 19,  3, 14,  7 },
    { "Y2K38 unsigned rollover (+1s)",            0x80000000U, 2038,  1, 19,  3, 14,  8 },
    { "2099-12-31 23:59:59 (Pre-century end)",    4102444799U, 2099, 12, 31, 23, 59, 59 },
    { "2100-01-01 00:00:00 (Century start)",      4102444800U, 2100,  1,  1,  0,  0,  0 },
    { "2100-02-28 23:59:59 (Non-leap Feb end)",   4107542399U, 2100,  2, 28, 23, 59, 59 },
    { "2100-03-01 00:00:00 (Skipped Feb 29)",     4107542400U, 2100,  3,  1,  0,  0,  0 },
    { "2100-12-31 23:59:59 (2100 year end)",      4133980799U, 2100, 12, 31, 23, 59, 59 },
    { "2101-01-01 00:00:00 (Post-2100 start)",    4133980800U, 2101,  1,  1,  0,  0,  0 },
    { "2101-03-01 00:00:00 (2101 March check)",   4139078400U, 2101,  3,  1,  0,  0,  0 },
    { "2104-02-29 12:00:00 (Next valid leap)",    4233729600U, 2104,  2, 29, 12,  0,  0 },
    { "2106-02-07 06:28:15 (Max uint32 ceiling)", 0xFFFFFFFFU, 2106,  2,  7,  6, 28, 15 }
};

int test_2100_boundary(void)
{
    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    int failures = 0;

    printf("\n=== Running Y2K38 / 2100 Century / Y2106 Boundary Testsuite ===\n\n");

    for (size_t i = 0; i < num_cases; i++) {
        const boundary_case_t *tc = &test_cases[i];
        time_t t_in = (time_t) tc->epoch_u32;
        struct tm *gmt = gmtime(&t_in);

        if (!gmt) {
            printf("[FAIL] %-39s | gmtime(%u) returned NULL\n",
                tc->label, tc->epoch_u32);
            failures++;
            continue;
        }

        int got_year = gmt->tm_year + 1900;
        int got_mon  = gmt->tm_mon + 1;
        int got_mday = gmt->tm_mday;
        int got_hour = gmt->tm_hour;
        int got_min  = gmt->tm_min;
        int got_sec  = gmt->tm_sec;

        int date_ok = (got_year == tc->expected_year &&
                       got_mon  == tc->expected_mon  &&
                       got_mday == tc->expected_mday &&
                       got_hour == tc->expected_hour &&
                       got_min  == tc->expected_min  &&
                       got_sec  == tc->expected_sec);

        /* Copy struct tm before calling inverse function (gmtime uses static buffer) */
        struct tm tm_copy = *gmt;
        tm_copy.tm_isdst = 0;

        /* Under TZ=UTC, mktime() acts as inverse of gmtime() */
        time_t t_roundtrip = mktime(&tm_copy);
        uint32_t rt_u32 = (uint32_t) t_roundtrip;
        int roundtrip_ok = (rt_u32 == tc->epoch_u32);

        if (date_ok && roundtrip_ok) {
            printf("[PASS] %-40s | %04d-%02d-%02d %02d:%02d:%02d UTC (epoch=%u)\n",
              tc->label, got_year, got_mon, got_mday, got_hour, got_min, got_sec,
              tc->epoch_u32);
        } else {
            printf("[FAIL] %-40s\n", tc->label);
            printf("       Expected: %04d-%02d-%02d %02d:%02d:%02d (epoch=%u)\n",
                tc->expected_year, tc->expected_mon, tc->expected_mday,
                tc->expected_hour, tc->expected_min, tc->expected_sec, tc->epoch_u32);
            printf("       Got:      %04d-%02d-%02d %02d:%02d:%02d (roundtrip_epoch=%u)\n",
                got_year, got_mon, got_mday, got_hour, got_min, got_sec, rt_u32);
            failures++;
        }
    }

    printf("\nSummary: %zu/%zu passed and %d failed, sizeof(time_t) = %zu bytes\n\n",
        num_cases - (size_t)failures, num_cases, failures, sizeof(time_t));

    return failures;
}

int main(void)
{
    /* Force UTC so mktime() and gmtime() act as exact mathematical inverses */
    if (setenv("TZ", "UTC", 1) != 0) {
        perror("setenv(TZ)");
        return EXIT_FAILURE;
    }
    tzset();

    int failures = test_2100_boundary();
    return (failures == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
