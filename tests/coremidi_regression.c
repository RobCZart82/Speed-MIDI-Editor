/* Actual CoreMIDI backend lifecycle with injected host calls and allocation accounting. */
#include <CoreMIDI/MIDIServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
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
static int created_ports;
static ItemCount test_device_count(void) { return 0; }
static ItemCount test_endpoint_count(void) { return 1; }
static MIDIEndpointRef test_endpoint(ItemCount index) { (void)index; return 1; }
static OSStatus test_client_create(CFStringRef name, MIDINotifyProc proc, void* info, MIDIClientRef* result) {
    (void)name; (void)proc; (void)info; ++created_ports; *result=1; return noErr;
}
static OSStatus test_input_create(MIDIClientRef c, CFStringRef n, MIDIReadProc r, void* i, MIDIPortRef* p) {
    (void)c; (void)n; (void)r; (void)i; ++created_ports; *p=2; return noErr;
}
static OSStatus test_output_create(MIDIClientRef c, CFStringRef n, MIDIPortRef* p) {
    (void)c; (void)n; ++created_ports; *p=3; return noErr;
}
static OSStatus test_dispose(MIDIObjectRef object) { (void)object; return noErr; }
static OSStatus test_entity(MIDIEndpointRef endpoint, MIDIEntityRef* entity) {
    (void)endpoint; *entity=0; return noErr;
}
static OSStatus test_name(MIDIObjectRef object, CFStringRef property, CFStringRef* value) {
    (void)object; (void)property; *value=CFRetain(CFSTR("Virtual endpoint")); return noErr;
}
static OSStatus test_data(MIDIObjectRef object, CFStringRef property, CFDataRef* value) {
    (void)object; (void)property; *value=NULL; return noErr;
}
#define MIDIGetNumberOfDevices test_device_count
#define MIDIGetNumberOfSources test_endpoint_count
#define MIDIGetNumberOfDestinations test_endpoint_count
#define MIDIGetSource test_endpoint
#define MIDIGetDestination test_endpoint
#define MIDIClientCreate test_client_create
#define MIDIInputPortCreate test_input_create
#define MIDIOutputPortCreate test_output_create
#define MIDIClientDispose test_dispose
#define MIDIPortDispose test_dispose
#define MIDIEndpointGetEntity test_entity
#define MIDIObjectGetStringProperty test_name
#define MIDIObjectGetDataProperty test_data
#define pm_alloc test_alloc
#define pm_free test_free
#define MIDIPortConnectSource test_connect
#define MIDIPortDisconnectSource test_disconnect
#define timestamp_cm_to_pm test_timestamp_cm_to_pm
#define timestamp_pm_to_cm test_timestamp_pm_to_cm
#define cm_get_full_endpoint_name test_endpoint_name
#define EndpointName test_EndpointName
#define pm_macosxcm_init test_backend_init
#define pm_macosxcm_term test_backend_term
#define pm_macosx_in_dictionary test_input_dictionary
#define pm_macosx_out_dictionary test_output_dictionary
#include "../third_party/portmidi/pm_mac/pmmacosxcm.c"
static PmTimestamp test_time(void* info) { (void)info; return 0; }
static void parser_timeout(int signal_number) { (void)signal_number; _exit(2); }
static void check_packets(void) {
    PmInternal midi;
    midi_macosxcm_node backend;
    PmEvent event;
    MIDIPacket packet;
    const unsigned char bytes[]={0x90,60,100,0xf8,61,101,0xf7,0x90,62,0xf8,102};
    const PmMessage expected[]={Pm_Message(0x90,60,100),Pm_Message(0xf8,0,0),
        Pm_Message(0x90,61,101),Pm_Message(0xf8,0,0),Pm_Message(0x90,62,102)};
    memset(&midi,0,sizeof(midi)); memset(&backend,0,sizeof(backend));
    memset(&packet,0,sizeof(packet)); memset(&event,0,sizeof(event));
    midi.queue=Pm_QueueCreate(32,sizeof(PmEvent)); midi.channel_mask=0xffff;
    CHECK(midi.queue!=NULL);
    packet.length=sizeof(bytes); memcpy(packet.data,bytes,sizeof(bytes));
    signal(SIGALRM,parser_timeout); alarm(2);
    process_packet(&packet,&event,&midi,&backend);
    alarm(0);
    for(unsigned int i=0;i<sizeof(expected)/sizeof(expected[0]);++i) {
        CHECK(Pm_Dequeue(midi.queue,&event)==1); CHECK(event.message==expected[i]);
    }
    CHECK(Pm_QueueEmpty(midi.queue));
    /* A partial message survives packet boundaries; System Common cancels running status. */
    packet.length=2; packet.data[0]=0x90; packet.data[1]=63;
    process_packet(&packet,&event,&midi,&backend); CHECK(Pm_QueueEmpty(midi.queue));
    packet.length=5; memcpy(packet.data,(unsigned char[]){103,0xf1,1,64,104},5);
    process_packet(&packet,&event,&midi,&backend);
    CHECK(Pm_Dequeue(midi.queue,&event)==1 && event.message==Pm_Message(0x90,63,103));
    CHECK(Pm_Dequeue(midi.queue,&event)==1 && event.message==Pm_Message(0xf1,1,0));
    CHECK(Pm_QueueEmpty(midi.queue));
    CHECK(Pm_QueueDestroy(midi.queue)==pmNoError);
}
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
    check_packets();
    CHECK(test_backend_init()==pmNoError && created_ports==3 && pm_descriptor_index==2);
    CHECK(descriptors[0].pub.input && descriptors[1].pub.output);
    CHECK(strcmp(descriptors[0].pub.name,"Virtual endpoint")==0);
    test_backend_term();
    free((void*)descriptors[0].pub.name); free((void*)descriptors[1].pub.name);
    free(descriptors); descriptors=NULL; pm_descriptor_index=0; pm_descriptor_max=0;
    puts("CoreMIDI lifecycle, packet progress, running status and virtual endpoints passed");
    return 0;
}
