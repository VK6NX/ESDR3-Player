/*
 * Copyright (c) 2007 - 2026 Joseph Gaeddert
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/time.h>
#  include <sys/resource.h>
#endif

#include "liquid.h"

// timer data structure
struct liquid_timer_s
{
    // timer type
    int type;

#ifdef _WIN32
    // Windows has neither gettimeofday() nor getrusage(); use Win32 API and
    // store both references as 100-ns ticks
    unsigned long long tic_clock;   // wall clock (LIQUID_TIMER_CLOCK)
    unsigned long long tic_cpu;     // user+kernel CPU time (LIQUID_TIMER_RUSAGE)
#else
    // time object was created (LIQUID_TIMER_CLOCK)
    struct timeval tic_timeval;

    // time object was created (LIQUID_TIMER_RUSAGE)
    struct rusage tic_rusage;
#endif
};

#ifdef _WIN32
static unsigned long long liquid_timer_filetime_to_u64(const FILETIME * _ft)
{
    return ((unsigned long long)_ft->dwHighDateTime << 32) | _ft->dwLowDateTime;
}

// wall-clock time in 100-ns units
static unsigned long long liquid_timer_win_clock(void)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return liquid_timer_filetime_to_u64(&ft);
}

// user+kernel CPU time of the current process in 100-ns units
static unsigned long long liquid_timer_win_cpu(void)
{
    FILETIME creation, exit, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user))
        return 0;
    return liquid_timer_filetime_to_u64(&kernel) + liquid_timer_filetime_to_u64(&user);
}
#endif

// create liquid_timer object
liquid_timer liquid_timer_create(int _type)
{
    liquid_timer q = (liquid_timer) malloc(sizeof(struct liquid_timer_s));

    //
    q->type = _type;
    if (q->type == LIQUID_TIMER_CLOCK) {
    } else if (q->type == LIQUID_TIMER_RUSAGE) {
    } else {
        free(q);
        return liquid_error_config("liquid_create(), invalid timer type");
    }

    // reset timer
    if (liquid_timer_tic(q))
    {
        free(q);
        return liquid_error_config("liquid_create(), could not reset timer");
    }
    return q;
}

// create liquid_timer object
int liquid_timer_destroy(liquid_timer _q)
{
    free(_q);
    return LIQUID_OK;
}

// create liquid_timer object
int liquid_timer_tic(liquid_timer _q)
{
    if (_q->type == LIQUID_TIMER_CLOCK) {
#ifdef _WIN32
        _q->tic_clock = liquid_timer_win_clock();
#else
        if (gettimeofday(&_q->tic_timeval, NULL))
            return liquid_error(LIQUID_EINT,"liquid_timer_tic(), gettimeofday() returned invalid flag");
#endif
    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
#ifdef _WIN32
        _q->tic_cpu = liquid_timer_win_cpu();
#else
        getrusage(RUSAGE_SELF, &_q->tic_rusage);
#endif
    } else {
        return liquid_error(LIQUID_EINT,"liquid_timer_tic(), invalid timer type");
    }
    return LIQUID_OK;
}

// get elapsed time since 'tic' in seconds
float liquid_timer_toc(liquid_timer _q)
{
#ifdef _WIN32
    if (_q->type == LIQUID_TIMER_CLOCK) {
        return (float)(liquid_timer_win_clock() - _q->tic_clock) * 1e-7f;
    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
        return (float)(liquid_timer_win_cpu() - _q->tic_cpu) * 1e-7f;
    }
#else
    if (_q->type == LIQUID_TIMER_CLOCK) {
        struct timeval toc;
        if (gettimeofday(&toc, NULL))
        {
            liquid_error(LIQUID_EINT,"liquid_timer_toc(), gettimeofday() returned invalid flag");
            return -1;
        }

        // compute execution time (in seconds)
        float s  = (float)(toc.tv_sec  - _q->tic_timeval.tv_sec);
        float us = (float)(toc.tv_usec - _q->tic_timeval.tv_usec);
        return s + us*1e-6f;

    } else if (_q->type == LIQUID_TIMER_RUSAGE) {
        struct rusage toc;
        getrusage(RUSAGE_SELF, &toc);
        // compute run time
        float time_s  = toc.ru_utime.tv_sec - _q->tic_rusage.ru_utime.tv_sec
                      + toc.ru_stime.tv_sec - _q->tic_rusage.ru_stime.tv_sec;
        float time_us = toc.ru_utime.tv_usec - _q->tic_rusage.ru_utime.tv_usec
                      + toc.ru_stime.tv_usec - _q->tic_rusage.ru_stime.tv_usec;
        return time_s + 1e-6f*time_us;
    }
#endif
    return liquid_error(LIQUID_EINT,"liquid_timer_toc(), invalid timer type");
}

// compact: create and start timer
liquid_timer liquid_tic(void)
{
    return liquid_timer_create(LIQUID_TIMER_CLOCK);
}

// compact: destroy timer and retrieve runtime in seconds
float liquid_toc(liquid_timer _q)
{
    float toc = liquid_timer_toc(_q);
    liquid_timer_destroy(_q);
    return toc;
}

