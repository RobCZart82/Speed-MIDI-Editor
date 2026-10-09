/* Actual CoreMIDI backend lifecycle with injected host calls and allocation accounting. */
#include <CoreMIDI/MIDIServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "portmidi.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
static int live_allocations, connect_error, disconnect_error, allocation_failure;
static int live_names, name_allocation_failure, conversion_failure;
static int name_budget=-1, registration_budget=-1;
static CFStringRef endpoint_label;
static CFMutableStringRef owned_strings[32];
static int live_strings;
static CFMutableStringRef test_string_create(CFAllocatorRef allocator, CFIndex capacity) {
    CFMutableStringRef value=CFStringCreateMutable(allocator,capacity);
    CHECK(value && live_strings<32); owned_strings[live_strings++]=value; return value;
}
static void test_cf_release(CFTypeRef value) {
    for(int i=0;i<live_strings;++i)if(owned_strings[i]==value) {
        owned_strings[i]=owned_strings[--live_strings]; break;
    }
    CFRelease(value);
}
static Boolean test_string_convert(CFStringRef string, char* buffer, CFIndex size, CFStringEncoding encoding) {
    if(conversion_failure)return 0;
    return CFStringGetCString(string,buffer,size,encoding);
}
static void* test_name_alloc(size_t size) {
    if(name_allocation_failure || name_budget==0)return NULL;
    if(name_budget>0)--name_budget;
    void* value=malloc(size); if(value)++live_names; return value;
}
static void test_name_free(void* value) {
    if(value) { CHECK(live_names>0); --live_names; free(value); }
}
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
    (void)object; (void)property; *value=CFRetain(endpoint_label ? endpoint_label : CFSTR("Virtual endpoint")); return noErr;
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
#define CFStringCreateMutable test_string_create
#define CFRelease test_cf_release
#define CFStringGetCString test_string_convert
#define pm_add_device test_register_device
#define malloc test_name_alloc
#define free test_name_free
#include "../third_party/portmidi/pm_mac/pmmacosxcm.c"
#undef malloc
#undef free
#undef pm_add_device
PmError pm_add_device(char* interf, char* name, int input, void* descriptor, pm_fns_type dictionary);
PmError test_register_device(char* interf, char* name, int input, void* descriptor, pm_fns_type dictionary) {
    if(registration_budget==0)return pmInsufficientMemory;
    if(registration_budget>0)--registration_budget;
    return pm_add_device(interf,name,input,descriptor,dictionary);
}

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
    for(const char** text=(const char*[]){"ASCII", "", "\xc5\x91\xc5\xb1", "\xe6\x97\xa5\xe6\x9c\xac", "\xf0\x9f\x8e\xb9", NULL}; *text; ++text) {
        endpoint_label=CFStringCreateWithCString(NULL,*text,kCFStringEncodingUTF8); CHECK(endpoint_label);
        char* name=test_endpoint_name(1); CHECK(name && strcmp(name,*text)==0);
        test_name_free(name); CFRelease(endpoint_label); endpoint_label=NULL;
        CHECK(live_names==0 && live_strings==0);
    }
    conversion_failure=1;
    char* fallback=test_endpoint_name(1); CHECK(fallback && strcmp(fallback,"Unnamed MIDI endpoint")==0);
    test_name_free(fallback); conversion_failure=0;
    name_allocation_failure=1; CHECK(test_endpoint_name(1)==NULL); name_allocation_failure=0;
    CHECK(live_names==0 && live_strings==0);
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
    CHECK(descriptors[0].pub.name==NULL && descriptors[1].pub.name==NULL);
    CHECK(live_names==0 && live_strings==0);
    test_backend_term(); CHECK(live_names==0 && live_strings==0);
    free(descriptors); descriptors=NULL; pm_descriptor_index=0; pm_descriptor_max=0;
    pm_default_input_device_id=-1; pm_default_output_device_id=-1;
    for(int registration=0;registration<2;++registration)for(int after=0;after<2;++after) {
        name_budget=registration ? -1:after;
        registration_budget=registration ? after:-1;
        CHECK(test_backend_init()==pmHostError);
        CHECK(pm_descriptor_index==0 && pm_default_input_device_id==-1 && pm_default_output_device_id==-1);
        CHECK(client==NULL_REF && portIn==NULL_REF && portOut==NULL_REF);
        CHECK(live_names==0 && live_strings==0);
        if(pm_descriptor_max)CHECK(descriptors[0].pub.name==NULL);
        test_backend_term(); CHECK(live_names==0 && live_strings==0);
        free(descriptors); descriptors=NULL; pm_descriptor_max=0;
    }
    name_budget=registration_budget=-1;
    CHECK(test_backend_init()==pmNoError && pm_descriptor_index==2);
    test_backend_term(); CHECK(live_names==0 && live_strings==0);
    free(descriptors); descriptors=NULL; pm_descriptor_index=pm_descriptor_max=0;
    puts("CoreMIDI lifecycle, packet progress, running status and virtual endpoints passed");
    return 0;
}
