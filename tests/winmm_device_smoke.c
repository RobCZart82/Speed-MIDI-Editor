/* Native device test: no notes are sounded; stream scheduling and callbacks are real. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>
#include "portmidi.h"
#include "porttime.h"

static int check(PmError error, const char *operation) {
    if (error == pmNoError) return 1;
    fprintf(stderr, "%s: %s\n", operation, Pm_GetErrorText(error));
    if (error == pmHostError) {
        char message[256];
        Pm_GetHostErrorText(message, sizeof(message));
        fprintf(stderr, "%s\n", message);
    }
    return 0;
}
static int native_device_available(const char *name) {
    UINT device = MIDI_MAPPER;
    HMIDIOUT handle = NULL;
    MMRESULT error;
    if (!strstr(name, "Microsoft MIDI Mapper")) {
        for (device = 0; device < midiOutGetNumDevs(); ++device) {
            MIDIOUTCAPSA caps;
            if (midiOutGetDevCapsA(device, &caps, sizeof(caps)) == MMSYSERR_NOERROR &&
                strcmp(caps.szPname, name) == 0) break;
        }
        if (device == midiOutGetNumDevs()) return 0;
    }
    /* A hosted runner can enumerate the mapper without a usable audio/MIDI driver.
       Probe WinMM directly so a PortMidi regression can never become a skip. */
    error = midiOutOpen(&handle, device, 0, 0, CALLBACK_NULL);
    if (error != MMSYSERR_NOERROR) {
        char message[256];
        midiOutGetErrorTextA(error, message, sizeof(message));
        printf("Unavailable in native WinMM: %s (error %u: %s)\n", name, error, message);
        return 0;
    }
    if (midiOutClose(handle) != MMSYSERR_NOERROR) return -1;
    return 1;
}
int main(void) {
    int id, latency, cycle, tested = 0;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (!check(Pm_Initialize(), "initialize")) return 1;
    if (Pt_Start(1, NULL, NULL) != ptNoError) return 1;
    for (id = 0; id < Pm_CountDevices(); ++id) {
        const PmDeviceInfo *info = Pm_GetDeviceInfo(id);
        if (!info->output) continue;
        /* Do not send to unrelated external hardware on a developer's machine. */
        if (!strstr(info->name, "Microsoft MIDI Mapper") &&
            !strstr(info->name, "Microsoft GS Wavetable Synth")) continue;
        {
            int available = native_device_available(info->name);
            if (available < 0) return 1;
            if (!available) continue;
        }
        printf("Testing %s\n", info->name);
        fflush(stdout);
        ++tested;
        for (latency = 0; latency <= 5; latency += 5) {
            for (cycle = 0; cycle < 10; ++cycle) {
                PortMidiStream *stream = NULL;
                if (!check(Pm_OpenOutput(&stream, id, NULL, 1024, NULL, NULL, latency), "open")) return 1;
                if (!check(Pm_WriteShort(stream, Pt_Time(), Pm_Message(0xb0, 123, 0)), "write")) return 1;
                Pt_Sleep(30);
                if (!check(Pm_Close(stream), "close")) return 1;
            }
        }
    }
    Pm_Terminate();
    Pt_Stop();
    if (!tested) { puts("SKIP: no Microsoft output is usable through native WinMM"); return 77; }
    printf("Passed %d Microsoft outputs (both latency modes, 10 reopen cycles each)\n", tested);
    return 0;
}
