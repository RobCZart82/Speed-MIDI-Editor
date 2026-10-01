/* ptmacosx.c -- portable timer implementation for mac os x */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <CoreFoundation/CoreFoundation.h>

#import <mach/mach.h>
#import <mach/mach_error.h>
#import <mach/mach_time.h>
#import <mach/clock.h>

#include "porttime.h"

#define THREAD_IMPORTANCE 30
#define LONG_TIME 1000000000.0

static int time_started_flag = FALSE;
static int pt_thread_created = FALSE;
static CFAbsoluteTime startTime = 0.0;
static uint64_t startHostTime;
static mach_timebase_info_data_t timebase;
static CFRunLoopRef timerRunLoop;

typedef struct {
    int resolution;
    PtCallback *callback;
    void *userData;
} PtThreadParams;


void Pt_CFTimerCallback(CFRunLoopTimerRef timer, void *info)
{
    PtThreadParams *params = (PtThreadParams*)info;
    (*params->callback)(Pt_Time(), params->userData);
}

static void* Pt_Thread(void *p)
{
    CFTimeInterval timerInterval;
    CFRunLoopTimerContext timerContext;
    CFRunLoopTimerRef timer;
    PtThreadParams *params = (PtThreadParams*)p;
    //CFTimeInterval timeout;

    /* raise the thread's priority */
    kern_return_t error;
    thread_extended_policy_data_t extendedPolicy;
    thread_precedence_policy_data_t precedencePolicy;

    extendedPolicy.timeshare = 0;
    error = thread_policy_set(mach_thread_self(), THREAD_EXTENDED_POLICY,
                              (thread_policy_t)&extendedPolicy,
                              THREAD_EXTENDED_POLICY_COUNT);
    if (error != KERN_SUCCESS) {
        mach_error("Couldn't set thread timeshare policy", error);
    }

    precedencePolicy.importance = THREAD_IMPORTANCE;
    error = thread_policy_set(mach_thread_self(), THREAD_PRECEDENCE_POLICY,
                              (thread_policy_t)&precedencePolicy,
                              THREAD_PRECEDENCE_POLICY_COUNT);
    if (error != KERN_SUCCESS) {
        mach_error("Couldn't set thread precedence policy", error);
    }

    /* set up the timer context */
    timerContext.version = 0;
    timerContext.info = params;
    timerContext.retain = NULL;
    timerContext.release = NULL;
    timerContext.copyDescription = NULL;

    /* create a new timer */
    timerInterval = (double)params->resolution / 1000.0;
    timer = CFRunLoopTimerCreate(NULL, startTime+timerInterval, timerInterval,
                                 0, 0, Pt_CFTimerCallback, &timerContext);

    timerRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddTimer(timerRunLoop, timer, CFSTR("PtTimeMode"));

    /* run until we're told to stop by Pt_Stop() */
    CFRunLoopRunInMode(CFSTR("PtTimeMode"), LONG_TIME, false);

    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, CFSTR("PtTimeMode"));
    CFRelease(timer);
    free(params);

    return NULL;
}

PtError Pt_Start(int resolution, PtCallback *callback, void *userData)
{
    PtThreadParams *params;
    pthread_t pthread_id;

    //Holger 2011-04-07 printf("Pt_Start() called\n");

    // /* make sure we're not already playing */
    if (time_started_flag) return ptAlreadyStarted;
    if (mach_timebase_info(&timebase) != KERN_SUCCESS) return ptHostError;
    startHostTime = mach_absolute_time();
    startTime = CFAbsoluteTimeGetCurrent();

    if (callback) {
        params = (PtThreadParams*)malloc(sizeof(PtThreadParams));
        if (!params) return ptInsufficientMemory;
        params->resolution = resolution;
        params->callback = callback;
        params->userData = userData;

        if (pthread_create(&pthread_id, NULL, Pt_Thread, params) != 0) {
            free(params);
            return ptHostError;
        }
        pt_thread_created = TRUE;
    }

    time_started_flag = TRUE;
    return ptNoError;
}


PtError Pt_Stop()
{
    //Holger 2011-04-07 printf("Pt_Stop called\n");

    if (pt_thread_created)
    {
        CFRunLoopStop(timerRunLoop);
        pt_thread_created = FALSE;
    }

    time_started_flag = FALSE;
    return ptNoError;
}


int Pt_Started()
{
    return time_started_flag;
}


PtTimestamp Pt_Time()
{
    uint64_t ticks, nanos;
    uint32_t raw;
    if (!timebase.denom) return 0;
    ticks = mach_absolute_time() - startHostTime;
    nanos = (ticks / timebase.denom) * timebase.numer +
            (ticks % timebase.denom) * timebase.numer / timebase.denom;
    raw = (uint32_t)(nanos / 1000000);
    return raw <= INT32_MAX ? (PtTimestamp)raw :
            (PtTimestamp)((int64_t)raw - INT64_C(4294967296));
}


void Pt_Sleep(int32_t duration)
{
    usleep(duration * 1000);
}
