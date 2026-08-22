/* Small non-platform compatibility surface retained by the decompilation. */
#include "pc_platform.h"

#include <dolphin.h>
#include <game/msm.h>

void HuDvdErrDispInit(GXRenderModeObj *mode, void *xfb1, void *xfb2)
{
    (void)mode; (void)xfb1; (void)xfb2;
}

s32 HuSoftResetButtonCheck(void) { return 0; }
u32 OSGetSoundMode(void) { return OS_SOUND_MODE_STEREO; }
void OSSetSoundMode(u32 mode) { (void)mode; }

s16 HuTHPSprCreateVol(char *path, s16 loop, s16 prio, float volume)
{
    (void)path; (void)loop; (void)prio; (void)volume;
    return 0;
}
s16 HuTHPSprCreate(char *path, s16 loop, s16 prio)
{
    (void)path; (void)loop; (void)prio;
    return 0;
}
s16 HuTHP3DCreateVol(char *path, s16 loop, float volume)
{
    (void)path; (void)loop; (void)volume;
    return 0;
}
s16 HuTHP3DCreate(char *path, s16 loop)
{
    (void)path; (void)loop;
    return 0;
}
void HuTHPStop(void) {}
void HuTHPClose(void) {}
void HuTHPRestart(void) {}
BOOL HuTHPEndCheck(void) { return TRUE; }
s32 HuTHPFrameGet(void) { return 0; }
s32 HuTHPTotalFrameGet(void) { return 0; }
void HuTHPSetVolume(s32 left, s32 right) { (void)left; (void)right; }

void *OSAllocFixed(void *range_start, void *range_end)
{
    (void)range_start;
    (void)range_end;
    return NULL;
}
s32 OSCheckHeap(OSHeapHandle heap) { (void)heap; return 4 * 1024 * 1024; }
void OSDumpHeap(OSHeapHandle heap) { (void)heap; }

static OSTime stopwatch_now(const OSStopwatch *stopwatch)
{
    return stopwatch->running ? OSGetTime() - stopwatch->last : stopwatch->total;
}

void OSInitStopwatch(OSStopwatch *stopwatch, char *name)
{
    memset(stopwatch, 0, sizeof(*stopwatch));
    stopwatch->name = name;
}
void OSStartStopwatch(OSStopwatch *stopwatch)
{
    if (!stopwatch->running) {
        stopwatch->last = OSGetTime();
        stopwatch->running = TRUE;
    }
}
void OSStopStopwatch(OSStopwatch *stopwatch)
{
    if (stopwatch->running) {
        OSTime elapsed = OSGetTime() - stopwatch->last;
        stopwatch->total += elapsed;
        stopwatch->last = elapsed;
        stopwatch->running = FALSE;
        stopwatch->hits++;
    }
}
OSTime OSCheckStopwatch(OSStopwatch *stopwatch) { return stopwatch_now(stopwatch); }
void OSResetStopwatch(OSStopwatch *stopwatch)
{
    char *name = stopwatch->name;
    OSInitStopwatch(stopwatch, name);
}
void OSDumpStopwatch(OSStopwatch *stopwatch) { (void)stopwatch; }

void GXSetGPMetric(GXPerf0 perf0, GXPerf1 perf1) { (void)perf0; (void)perf1; }
void GXReadGPMetric(u32 *a, u32 *b) { if (a) *a = 0; if (b) *b = 0; }
void GXClearGPMetric(void) {}
void GXReadMemMetric(u32 *a, u32 *b, u32 *c, u32 *d, u32 *e,
                     u32 *f, u32 *g, u32 *h, u32 *i, u32 *j)
{
    u32 *values[] = {a, b, c, d, e, f, g, h, i, j};
    int index;
    for (index = 0; index < 10; index++) if (values[index]) *values[index] = 0;
}
void GXClearMemMetric(void) {}
void GXClearVCacheMetric(void) {}
void GXReadPixMetric(u32 *a, u32 *b, u32 *c, u32 *d, u32 *e, u32 *f)
{
    u32 *values[] = {a, b, c, d, e, f};
    int index;
    for (index = 0; index < 6; index++) if (values[index]) *values[index] = 0;
}
void GXClearPixMetric(void) {}
void GXSetVCacheMetric(GXVCachePerf attr) { (void)attr; }
void GXReadVCacheMetric(u32 *a, u32 *b, u32 *c)
{
    if (a) *a = 0; if (b) *b = 0; if (c) *c = 0;
}

void GXInitSpecularDir(GXLightObj *light, f32 x, f32 y, f32 z)
{
    (void)light; (void)x; (void)y; (void)z;
}
void GXSetTevIndTile(GXTevStageID stage, GXIndTexStageID ind_stage,
                     u16 tile_s, u16 tile_t, u16 spacing_s, u16 spacing_t,
                     GXIndTexFormat format, GXIndTexMtxID matrix,
                     GXIndTexBiasSel bias, GXIndTexAlphaSel alpha)
{
    (void)stage; (void)ind_stage; (void)tile_s; (void)tile_t;
    (void)spacing_s; (void)spacing_t; (void)format; (void)matrix;
    (void)bias; (void)alpha;
}

s32 CARDGetSerialNo(s32 chan, u64 *serial)
{
    if (serial) *serial = (u64)(chan + 1);
    return CARD_RESULT_READY;
}

void msmSysRegularProc(void) {}
/* The handheld audio bridge is intentionally silent for now, but overlays
 * still query the persisted output mode while entering file select/options. */
s32 msmSysGetOutputMode(void) { return SND_OUTPUTMODE_STEREO; }
void msmMusFdoutEnd(void) {}
int msmMusPlay(int id, MSM_MUSPARAM *param) { (void)id; (void)param; return 0; }
s32 msmMusGetStatus(int id) { (void)id; return MSM_MUS_DONE; }
s32 msmStreamGetStatus(int id) { (void)id; return MSM_STREAM_DONE; }
s32 msmSeSetParam(int id, MSM_SEPARAM *param) { (void)id; (void)param; return 0; }
s32 msmMusSetParam(s32 id, MSM_MUSPARAM *param) { (void)id; (void)param; return 0; }
