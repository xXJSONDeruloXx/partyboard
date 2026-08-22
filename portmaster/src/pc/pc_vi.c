/* GameCube VI replacement: SDL2 swap, input pumping, and 60 Hz pacing. */
#include "pc_platform.h"
#include <dolphin/vi.h>

static u32 retrace_count;
static void *current_framebuffer;
static void *next_framebuffer;
static VIRetraceCallback pre_retrace_callback;
static VIRetraceCallback post_retrace_callback;
static Uint64 last_retrace_ticks;
static Uint64 performance_frequency;

u32 pc_frame_counter;

void VIInit(void)
{
    retrace_count = 0;
    current_framebuffer = NULL;
    next_framebuffer = NULL;
    pre_retrace_callback = NULL;
    post_retrace_callback = NULL;
    last_retrace_ticks = 0;
    performance_frequency = SDL_GetPerformanceFrequency();
}

void VIConfigure(const GXRenderModeObj *mode)
{
    (void)mode;
}

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height)
{
    (void)xOrg;
    (void)yOrg;
    (void)width;
    (void)height;
}

void VIFlush(void) {}

static void pace_retrace(void)
{
    const Uint64 period = performance_frequency / 60;
    Uint64 now;

    if (g_pc_no_framelimit || performance_frequency == 0) {
        return;
    }

    now = SDL_GetPerformanceCounter();
    if (last_retrace_ticks != 0 && now - last_retrace_ticks < period) {
        Uint64 remaining = period - (now - last_retrace_ticks);
        if (remaining > performance_frequency / 1000) {
            SDL_Delay((Uint32)(remaining * 1000 / performance_frequency));
        }
        do {
            now = SDL_GetPerformanceCounter();
        } while (now - last_retrace_ticks < period);
    }
    last_retrace_ticks = now;
}

void VIWaitForRetrace(void)
{
    if (!g_pc_running) {
        return;
    }

    pc_platform_poll_events();
    if (!g_pc_running) {
        return;
    }

    if (pre_retrace_callback != NULL) {
        pre_retrace_callback(retrace_count);
    }

    pc_gx_blit_to_screen();
    SDL_GL_SwapWindow(g_pc_window);
    pace_retrace();

    current_framebuffer = next_framebuffer;
    retrace_count++;
    pc_frame_counter++;

    if (post_retrace_callback != NULL) {
        post_retrace_callback(retrace_count);
    }
    pc_gx_begin_frame();
}

u32 VIGetTvFormat(void) { return VI_NTSC; }
u32 VIGetRetraceCount(void) { return retrace_count; }
u32 VIGetNextField(void) { return retrace_count & 1; }
u32 VIGetDTVStatus(void) { return 0; }

void *VIGetCurrentFrameBuffer(void) { return current_framebuffer; }
void *VIGetNextFrameBuffer(void) { return next_framebuffer; }
void VISetNextFrameBuffer(void *framebuffer) { next_framebuffer = framebuffer; }
void VISetBlack(BOOL black) { (void)black; }

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback old = pre_retrace_callback;
    pre_retrace_callback = callback;
    return old;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback old = post_retrace_callback;
    post_retrace_callback = callback;
    return old;
}
