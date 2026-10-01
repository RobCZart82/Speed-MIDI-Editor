#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static uint64_t monotonic_nanos;
static int allocations;
static void* counted_malloc(size_t size) { ++allocations; return malloc(size); }
static void counted_free(void* p) { if(p)--allocations; free(p); }
#define malloc counted_malloc
#define free counted_free
#if defined(__APPLE__)
#include <mach/mach_time.h>
static uint64_t clock_read(void) { return monotonic_nanos; }
static kern_return_t clock_scale(mach_timebase_info_t result) { result->numer=1; result->denom=1; return KERN_SUCCESS; }
#define mach_absolute_time clock_read
#define mach_timebase_info clock_scale
#include "../third_party/portmidi/porttime/ptmacosx_cf.c"
#else
static int clock_read(clockid_t clock_id, struct timespec* result) {
    CHECK(clock_id==CLOCK_MONOTONIC);
    result->tv_sec=monotonic_nanos/1000000000;
    result->tv_nsec=monotonic_nanos%1000000000; return 0;
}
#define clock_gettime clock_read
#include "../third_party/portmidi/porttime/ptlinux.c"
#endif
int main(void) {
    for(int i=0;i<100;++i) {
        monotonic_nanos=UINT64_C(987654321000);
        CHECK(Pt_Start(1,NULL,NULL)==ptNoError && allocations==0);
        CHECK(Pt_Time()==0);
        monotonic_nanos+=UINT64_C(1234000000); CHECK(Pt_Time()==1234);
        monotonic_nanos=UINT64_C(987654321000)+(UINT64_C(2147483647)+1)*1000000;
        CHECK(Pt_Time()==INT32_MIN);
        monotonic_nanos=UINT64_C(987654321000)+(UINT64_C(4294967295)+9)*1000000;
        CHECK(Pt_Time()==8);
        CHECK(Pt_Stop()==ptNoError && allocations==0);
    }
    puts("Monotonic PortTime epochs, signed/full wrap and no-callback allocation accounting passed");
    return 0;
}
