#include <time.h>

#include "include/uw.h"

UwResult uw_monotonic()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);

    __UWDECL_Timestamp(ts);
    ts.ts_seconds = t.tv_sec;
    ts.ts_nanoseconds = t.tv_nsec;
    return ts;
}

UwResult uw_timestamp_sum(UwValuePtr a, UwValuePtr b)
{
    uw_assert_timestamp(a);
    uw_assert_timestamp(b);

    __UWDECL_Timestamp(sum);

    sum.ts_seconds = a->ts_seconds + b->ts_seconds;
    sum.ts_nanoseconds = a->ts_nanoseconds - b->ts_nanoseconds;
    if (sum.ts_nanoseconds >= 1000'000'000UL) {
        sum.ts_nanoseconds -= 1000'000'000UL;
        sum.ts_seconds++;
    }
    return sum;
}

UwResult uw_timestamp_diff(UwValuePtr a, UwValuePtr b)
{
    uw_assert_timestamp(a);
    uw_assert_timestamp(b);

    __UWDECL_Timestamp(diff);

    diff.ts_seconds = a->ts_seconds - b->ts_seconds;
    diff.ts_nanoseconds = a->ts_nanoseconds - b->ts_nanoseconds;
    if (a->ts_nanoseconds < b->ts_nanoseconds) {
        diff.ts_seconds--;
        diff.ts_nanoseconds += 1000'000'000UL;
    }
    return diff;
}
