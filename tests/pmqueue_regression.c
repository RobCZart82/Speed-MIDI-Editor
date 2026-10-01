#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int allocation_calls, fail_at, live_allocations;
void* pm_alloc(size_t size) {
    if(++allocation_calls==fail_at)return NULL;
    void* result=malloc(size); if(result)++live_allocations; return result;
}
void pm_free(void* pointer) { if(pointer) { --live_allocations; free(pointer); } }
#include "../third_party/portmidi/pm_common/pmutil.c"
int main(void) {
    for(fail_at=1;fail_at<=3;++fail_at) {
        allocation_calls=0;
        CHECK(Pm_QueueCreate(1024,sizeof(PmEvent))==NULL);
        CHECK(live_allocations==0);
    }
    fail_at=0;
    PmQueue* queue=Pm_QueueCreate(2,sizeof(PmEvent));
    PmEvent in={Pm_Message(0x90,60,100),0},out;
    CHECK(queue!=NULL && Pm_Enqueue(queue,&in)==pmNoError);
    CHECK(Pm_Dequeue(queue,&out)==1 && out.message==in.message && out.timestamp==0);
    CHECK(Pm_QueueDestroy(queue)==pmNoError && live_allocations==0);
    puts("PortMidi queue allocation failures and message roundtrip passed");
    return 0;
}
