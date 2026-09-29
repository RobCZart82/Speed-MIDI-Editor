/* Exercise the actual backend with deterministic WinMM failures, no MIDI device required. */
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (CALLBACK *OutputCallback)(HMIDIOUT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);
typedef void (CALLBACK *InputCallback)(HMIDIIN, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int live_allocations, fail_after = -1;
static int prepare_error, submit_error, unprepare_error, open_error, property_error;
static int unprepares, submits, stream_closes, simple_closes;
static MIDIHDR *expected_header;
static int input_unprepares;

static void *test_alloc(size_t n) {
    void *p;
    if (fail_after == 0) return NULL;
    if (fail_after > 0) --fail_after;
    p = malloc(n);
    if (p) ++live_allocations;
    return p;
}
static void test_free(void *p) {
    if (p) { --live_allocations; CHECK(live_allocations >= 0); free(p); }
}
static MMRESULT WINAPI test_prepare(HMIDIOUT out, LPMIDIHDR hdr, UINT size) {
    CHECK(hdr == expected_header);
    if (!prepare_error) hdr->dwFlags |= MHDR_PREPARED;
    return prepare_error;
}
static MMRESULT WINAPI test_unprepare(HMIDIOUT out, LPMIDIHDR hdr, UINT size) {
    CHECK(hdr == expected_header);
    ++unprepares;
    if (!unprepare_error) hdr->dwFlags &= ~MHDR_PREPARED;
    return unprepare_error;
}
static MMRESULT WINAPI test_submit(HMIDIOUT out, LPMIDIHDR hdr, UINT size) {
    CHECK(hdr == expected_header);
    ++submits;
    return submit_error;
}
static MMRESULT WINAPI test_stream_submit(HMIDISTRM out, LPMIDIHDR hdr, UINT size) {
    return test_submit((HMIDIOUT)out, hdr, size);
}
static MMRESULT WINAPI test_stream_open(LPHMIDISTRM stream, LPUINT device, DWORD count,
                                        DWORD_PTR callback, DWORD_PTR instance, DWORD flags) {
    if (open_error) return open_error;
    *stream = (HMIDISTRM)(UINT_PTR)1;
    ((OutputCallback)callback)((HMIDIOUT)*stream, MOM_OPEN, instance, 0, 0);
    return MMSYSERR_NOERROR;
}
static MMRESULT WINAPI test_simple_open(LPHMIDIOUT out, UINT device, DWORD_PTR callback,
                                       DWORD_PTR instance, DWORD flags) {
    return test_stream_open((LPHMIDISTRM)out, &device, 1, callback, instance, flags);
}
static MMRESULT WINAPI test_property(HMIDISTRM out, LPBYTE data, DWORD property) { return property_error; }
static MMRESULT WINAPI test_restart(HMIDISTRM out) { return 0; }
static MMRESULT WINAPI test_stream_close(HMIDISTRM out) { ++stream_closes; return 0; }
static MMRESULT WINAPI test_simple_close(HMIDIOUT out) { ++simple_closes; return 0; }
static MMRESULT WINAPI test_input_prepare(HMIDIIN in, LPMIDIHDR hdr, UINT size) {
    hdr->dwFlags |= MHDR_PREPARED;
    return 0;
}
static MMRESULT WINAPI test_input_add(HMIDIIN in, LPMIDIHDR hdr, UINT size) { return MMSYSERR_ERROR; }
static MMRESULT WINAPI test_input_unprepare(HMIDIIN in, LPMIDIHDR hdr, UINT size) {
    ++input_unprepares;
    hdr->dwFlags &= ~MHDR_PREPARED;
    return 0;
}

#define pm_alloc test_alloc
#define pm_free test_free
#define midiOutPrepareHeader test_prepare
#define midiOutUnprepareHeader test_unprepare
#define midiOutLongMsg test_submit
#define midiStreamOut test_stream_submit
#define midiStreamOpen test_stream_open
#define midiOutOpen test_simple_open
#define midiStreamProperty test_property
#define midiStreamRestart test_restart
#define midiStreamClose test_stream_close
#define midiOutClose test_simple_close
#define midiInPrepareHeader test_input_prepare
#define midiInAddBuffer test_input_add
#define midiInUnprepareHeader test_input_unprepare
#include "../third_party/portmidi/pm_win/pmwinmm.c"

#ifdef _MSC_VER
#pragma warning(error: 4113 4133)
#endif

static void callbacks(void) {
    PmInternal midi = {0};
    midiwinmm_node backend = {0};
    MIDIHDR header = {0};
    OutputCallback output = winmm_streamout_callback;
    InputCallback input = winmm_in_callback;
    CHECK(sizeof(void *) == 8);
    CHECK((UINT_PTR)&midi > 0xffffffffULL);
    CHECK((UINT_PTR)&header > 0xffffffffULL);
    midi.descriptor = &backend;
    backend.buffer_signal = CreateEvent(NULL, FALSE, FALSE, NULL);
    CHECK(backend.buffer_signal != NULL);
    InitializeCriticalSection(&backend.lock);
    expected_header = &header;
    header.dwFlags = MHDR_PREPARED;
    output(NULL, MOM_DONE, (DWORD_PTR)&midi, (DWORD_PTR)&header, 0);
    CHECK(!(header.dwFlags & MHDR_PREPARED));
    CHECK(WaitForSingleObject(backend.buffer_signal, 0) == WAIT_OBJECT_0);
    /* Invalid MIDI data avoids a queue while still exercising the input instance. */
    input(NULL, MIM_DATA, (DWORD_PTR)&midi, 0, 1);
    DeleteCriticalSection(&backend.lock);
    CloseHandle(backend.buffer_signal);
}

static void flush_failures(void) {
    int latency, failure;
    for (latency = 0; latency <= 5; latency += 5) {
        for (failure = 0; failure < 4; ++failure) {
            PmInternal midi = {0};
            midiwinmm_node backend = {0};
            MIDIHDR header = {0};
            midi.descriptor = &backend;
            midi.latency = latency;
            midi.fill_base = (unsigned char *)&header;
            backend.hdr = &header;
            header.dwBytesRecorded = 3;
            expected_header = &header;
            prepare_error = failure == 1 ? MMSYSERR_ERROR : 0;
            submit_error = failure >= 2 ? MMSYSERR_ERROR : 0;
            unprepare_error = failure == 3 ? MIDIERR_STILLPLAYING : 0;
            submits = unprepares = 0;
            CHECK(winmm_write_flush(&midi, 0) == (failure ? pmHostError : pmNoError));
            CHECK(backend.hdr == NULL && midi.fill_base == NULL);
            CHECK(submits == (failure == 1 ? 0 : 1));
            CHECK(unprepares == (failure >= 2 ? 1 : 0));
            if (failure == 2) CHECK(!(header.dwFlags & MHDR_PREPARED));
            if (failure == 3) CHECK(header.dwFlags & MHDR_PREPARED);
        }
    }
    prepare_error = submit_error = unprepare_error = 0;
}

static void failed_open_cleanup(void) {
    int i;
    DWORD before, after;
    PmInternal midi = {0};
    descriptor_node descriptor = {0};
    descriptors = &descriptor;
    midi.latency = 5;
    midi.buffer_len = 16;
    /* Warm up WinMM's lazy error-message resources before counting handles. */
    midiOutGetErrorText(MMSYSERR_ERROR, pm_hosterror_text, PM_HOST_ERROR_MSG_LEN);
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &before));
    open_error = MMSYSERR_ALLOCATED;
    CHECK(winmm_out_open(&midi, NULL) == pmHostError);
    CHECK(!midi.descriptor && live_allocations == 0);
    open_error = 0;
    property_error = MMSYSERR_ERROR;
    CHECK(winmm_out_open(&midi, NULL) == pmHostError);
    CHECK(!midi.descriptor && live_allocations == 0);
    CHECK(stream_closes == 1 && simple_closes == 0);
    property_error = 0;
    /* Descriptor, pointer array, and every partial buffer allocation failure. */
    for (i = 0; i < 18; ++i) {
        pm_hosterror = 0;
        fail_after = i;
        CHECK(winmm_out_open(&midi, NULL) == pmInsufficientMemory);
        CHECK(!midi.descriptor && live_allocations == 0);
    }
    fail_after = -1;
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &after));
    if (before != after) fprintf(stderr, "handle count: %lu -> %lu\n", before, after);
    CHECK(before == after);
    descriptors = NULL;
}

int main(void) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    callbacks();
    flush_failures();
    failed_open_cleanup();
    CHECK(allocate_input_buffer(NULL, 64) != pmNoError);
    CHECK(input_unprepares == 1 && live_allocations == 0);
    puts("WinMM x64 callbacks, flush failures and failed-open cleanup passed");
    return 0;
}
