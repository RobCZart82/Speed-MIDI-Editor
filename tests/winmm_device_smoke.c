/* Native device test: no notes are sounded; stream scheduling and callbacks are real. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "portmidi.h"
#include "porttime.h"

static int check(PmError error, const char *operation) {
    if (error == pmNoError) return 1;
    fprintf(stderr, "%s: %s\n", operation, Pm_GetErrorText(error));
    return 0;
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
    if (!tested) { puts("SKIP: Microsoft software MIDI outputs are unavailable"); return 77; }
    printf("Passed %d Microsoft outputs (both latency modes, 10 reopen cycles each)\n", tested);
    return 0;
}
