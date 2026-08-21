#include "pc_platform.h"
#include "pc_gx_internal.h"
#include <dolphin/gx.h>

#include <stdio.h>
#include <stdlib.h>

SDL_Window *g_pc_window;
SDL_GLContext g_pc_gl_context;
int g_pc_running = 1;
int g_pc_verbose = 1;
int g_pc_no_framelimit = 0;
int g_pc_window_w = PC_SCREEN_WIDTH;
int g_pc_window_h = PC_SCREEN_HEIGHT;
int g_pc_render_w = PC_SCREEN_WIDTH;
int g_pc_render_h = PC_SCREEN_HEIGHT;
int g_pc_scale_mode = 0;
int g_pc_widescreen_stretch = 0;
int g_pc_frameskip_active = 0;
float g_pc_zoom = 1.0f;
int g_pc_model_viewer_no_cull = 0;
u32 pc_frame_counter = 0;

static int requested_seconds(void)
{
    const char *value = getenv("PM_GX_PROBE_SECONDS");
    char *end = NULL;
    long seconds;

    if (value == NULL || *value == '\0') {
        return 5;
    }
    seconds = strtol(value, &end, 10);
    if (end == value || *end != '\0' || seconds < 1 || seconds > 3600) {
        return 5;
    }
    return (int)seconds;
}

static int handle_events(void)
{
    SDL_Event event;
    static int start_held;
    static int select_held;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return 0;
        }
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
                return 0;
            }
        }
        if (event.type == SDL_CONTROLLERBUTTONDOWN ||
            event.type == SDL_CONTROLLERBUTTONUP) {
            const int held = event.type == SDL_CONTROLLERBUTTONDOWN;
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                start_held = held;
            } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
                select_held = held;
            }
            if (start_held && select_held) {
                puts("[PM/GX] exit chord: Start+Select");
                return 0;
            }
        }
    }
    return 1;
}

static int create_context(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[PM/GX] SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    g_pc_window = SDL_CreateWindow(
        PC_WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        PC_SCREEN_WIDTH,
        PC_SCREEN_HEIGHT,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    if (g_pc_window == NULL) {
        fprintf(stderr, "[PM/GX] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }

    g_pc_gl_context = SDL_GL_CreateContext(g_pc_window);
    if (g_pc_gl_context == NULL) {
        fprintf(stderr, "[PM/GX] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_GL_GetDrawableSize(g_pc_window, &g_pc_window_w, &g_pc_window_h);
    g_pc_render_w = g_pc_window_w;
    g_pc_render_h = g_pc_window_h;
    printf("[PM/GX] video=%s renderer=%s version=%s drawable=%dx%d\n",
           SDL_GetCurrentVideoDriver(),
           (const char *)glGetString(GL_RENDERER),
           (const char *)glGetString(GL_VERSION),
           g_pc_window_w, g_pc_window_h);
    return 1;
}

static void draw_test_triangle(void)
{
    GXSetViewport(0, 0, PC_SCREEN_WIDTH, PC_SCREEN_HEIGHT, 0.0f, 1.0f);
    GXSetScissor(0, 0, PC_SCREEN_WIDTH, PC_SCREEN_HEIGHT);
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetNumChans(0);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

    GXBegin(GX_TRIANGLES, 0, 3);
    GXPosition3f32(-0.7f, -0.6f, 0.0f);
    GXColor4u8(255, 32, 32, 255);
    GXPosition3f32(0.7f, -0.6f, 0.0f);
    GXColor4u8(32, 255, 32, 255);
    GXPosition3f32(0.0f, 0.7f, 0.0f);
    GXColor4u8(32, 32, 255, 255);
    GXEnd();
}

int main(void)
{
    const int duration = requested_seconds();
    double start;
    double last_report;
    unsigned long frames = 0;

    if (!create_context()) {
        SDL_Quit();
        return 1;
    }
    if (SDL_GL_SetSwapInterval(1) != 0) {
        fprintf(stderr, "[PM/GX] vsync unavailable: %s\n", SDL_GetError());
        SDL_GL_SetSwapInterval(0);
    }

    pc_gx_init();
    start = (double)SDL_GetPerformanceCounter() /
            (double)SDL_GetPerformanceFrequency();
    last_report = start;
    while (g_pc_running) {
        const double now = (double)SDL_GetPerformanceCounter() /
                           (double)SDL_GetPerformanceFrequency();
        if (!handle_events() || now - start >= duration) {
            break;
        }

        pc_gx_begin_frame();
        draw_test_triangle();
        pc_gx_blit_to_screen();
        SDL_GL_SwapWindow(g_pc_window);
        ++frames;
        ++pc_frame_counter;

        if (now - last_report >= 1.0) {
            printf("[PM/GX] frames=%lu fps=%.2f draws=%d\n",
                   frames, (double)frames / (now - last_report),
                   pc_gx_draw_call_count);
            fflush(stdout);
            frames = 0;
            last_report = now;
        }
    }

    pc_gx_shutdown();
    SDL_GL_DeleteContext(g_pc_gl_context);
    SDL_DestroyWindow(g_pc_window);
    SDL_Quit();
    puts("[PM/GX] clean exit");
    return 0;
}
