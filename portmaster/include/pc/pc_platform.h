/* Party Board PortMaster SDL2/GLES platform state.
 *
 * The GX implementation is adapted from OpenCrossing's PC layer. This
 * platform header intentionally contains only the state needed by Party
 * Board; desktop settings, overlays, and Animal Crossing asset helpers are
 * not part of the handheld target.
 */
#ifndef PARTYBOARD_PC_PLATFORM_H
#define PARTYBOARD_PC_PLATFORM_H

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <GLES3/gl32.h>
#include <dolphin/types.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef _WIN32
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define PC_GC_WIDTH 640
#define PC_GC_HEIGHT 480
#define PC_SCREEN_WIDTH PC_GC_WIDTH
#define PC_SCREEN_HEIGHT PC_GC_HEIGHT
#define PC_WINDOW_TITLE "Party Board"
#define PC_PIf 3.14159265358979323846f
#define PC_PI 3.14159265358979323846
#define PC_DEG_TO_RAD (PC_PI / 180.0)
#define PC_DEG_TO_RADf (PC_PIf / 180.0f)

#define PC_MAIN_MEMORY_SIZE (24 * 1024 * 1024)
#define PC_LOW_HEAP_BASE 0x20000000u
#define PC_LOW_HEAP_SIZE (128 * 1024 * 1024)
#define PC_ARAM_SIZE (16 * 1024 * 1024)
#define PC_FIFO_SIZE (256 * 1024)
#define GC_BUS_CLOCK 162000000u
#define GC_CORE_CLOCK 486000000u
#define GC_TIMER_CLOCK (GC_BUS_CLOCK / 4)

extern SDL_Window *g_pc_window;
extern SDL_GLContext g_pc_gl_context;
extern int g_pc_running;
extern int g_pc_verbose;
extern int g_pc_no_framelimit;
extern int g_pc_time_override;
extern int g_pc_min_override;
extern int g_pc_sec_override;
extern int g_pc_window_w;
extern int g_pc_window_h;
extern int g_pc_render_w;
extern int g_pc_render_h;
extern int g_pc_scale_mode;
extern int g_pc_widescreen_stretch;
extern int g_pc_frameskip_active;
extern float g_pc_zoom;
extern int g_pc_model_viewer_no_cull;

extern u32 pc_frame_counter;
extern int pc_gx_draw_call_count;

extern u8 *pc_arena_base;
extern u8 *pc_arena_end;

void pc_gx_blit_to_screen(void);
void pc_gx_begin_frame(void);
void pc_platform_poll_events(void);
void pc_platform_set_run_limit(double seconds);

#endif
