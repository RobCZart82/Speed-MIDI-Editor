/* Actual CoreMIDI backend lifecycle with injected host calls and allocation accounting. */
#include <CoreMIDI/MIDIServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int live_allocations, connect_error, disconnect_error, allocation_failure;
static void* test_alloc(size_t size) {
    if(allocation_failure)return NULL;
    void* p=malloc(size); if(p)++live_allocations; return p;
}
static void test_free(void* p) {
    if(p) { --live_allocations; CHECK(live_allocations>=0); free(p); }
}
static OSStatus test_connect(MIDIPortRef port, MIDIEndpointRef endpoint, void* ref) {
    (void)port; (void)endpoint; (void)ref; return connect_error;
}
static OSStatus test_disconnect(MIDIPortRef port, MIDIEndpointRef endpoint) {
    (void)port; (void)endpoint; return disconnect_error;
}
#define pm_alloc test_alloc
#define pm_free test_free
#define MIDIPortConnectSource test_connect
#define MIDIPortDisconnectSource test_disconnect
#define pm_macosxcm_init test_backend_init
#define pm_macosxcm_term test_backend_term
#define pm_macosx_in_dictionary test_input_dictionary
#define pm_macosx_out_dictionary test_output_dictionary
#include "../third_party/portmidi/pm_mac/pmmacosxcm.c"
static PmTimestamp test_time(void* info) { (void)info; return 0; }
int main(void) {
    descriptor_node device;
    memset(&device,0,sizeof(device));
    device.descriptor=(void*)1;
    descriptors=&device;
    for(int iteration=0;iteration<100;++iteration) {
        PmInternal midi;
        memset(&midi,0,sizeof(midi)); midi.time_proc=test_time;
        CHECK(midi_in_open(&midi,NULL)==pmNoError && live_allocations==1);
        CHECK(midi_in_close(&midi)==pmNoError && live_allocations==0 && midi.descriptor==NULL);
        CHECK(midi_out_open(&midi,NULL)==pmNoError && live_allocations==1);
        CHECK(midi_out_close(&midi)==pmNoError && live_allocations==0 && midi.descriptor==NULL);
    }
    PmInternal midi;
    memset(&midi,0,sizeof(midi)); midi.time_proc=test_time;
    connect_error=-1;
    CHECK(midi_in_open(&midi,NULL)==pmHostError && live_allocations==0 && midi.descriptor==NULL);
    connect_error=0; disconnect_error=-1;
    CHECK(midi_in_open(&midi,NULL)==pmNoError);
    CHECK(midi_in_close(&midi)==pmHostError && live_allocations==0 && midi.descriptor==NULL);
    allocation_failure=1;
    CHECK(midi_in_open(&midi,NULL)==pmInsufficientMemory && live_allocations==0);
    CHECK(midi_out_open(&midi,NULL)==pmInsufficientMemory && live_allocations==0);
    descriptors=NULL;
    puts("CoreMIDI repeated close and failed open/close allocation accounting passed");
    return 0;
}
