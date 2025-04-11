#include <time.h>

#include "include/uw.h"
#include "include/uw_datetime.h"

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

    sum.ts_nanoseconds = a->ts_nanoseconds - b->ts_nanoseconds;
    unsigned carry = 0;
    if (sum.ts_nanoseconds >= 1000'000'000UL) {
        sum.ts_nanoseconds -= 1000'000'000UL;
        carry = 1;
    }
    sum.ts_seconds = a->ts_seconds + b->ts_seconds + carry;

    return sum;
}

UwResult uw_timestamp_diff(UwValuePtr a, UwValuePtr b)
{
    uw_assert_timestamp(a);
    uw_assert_timestamp(b);

    __UWDECL_Timestamp(diff);

    diff.ts_seconds = a->ts_seconds - b->ts_seconds;
    uint32_t borrow = 0;
    if (a->ts_nanoseconds < b->ts_nanoseconds) {
        diff.ts_seconds--;
        borrow = 1000'000'000UL;
    }
    diff.ts_nanoseconds = borrow + a->ts_nanoseconds - b->ts_nanoseconds;

    return diff;
}
