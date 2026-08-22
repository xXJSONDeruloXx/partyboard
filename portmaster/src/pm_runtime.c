/* PortMaster entry point for the native Party Board runtime. */
#define _GNU_SOURCE

#include "pc_platform.h"
#include "pc_gx_internal.h"

#include <dolphin/dvd.h>
#include <dolphin/os.h>

#include <errno.h>
#include <dlfcn.h>
#include <signal.h>
#include <ucontext.h>

SDL_Window *g_pc_window;
SDL_GLContext g_pc_gl_context;
int g_pc_running = 1;
int g_pc_verbose;
int g_pc_no_framelimit;
int g_pc_time_override = -1;
int g_pc_min_override = -1;
int g_pc_sec_override = -1;
int g_pc_window_w = PC_SCREEN_WIDTH;
int g_pc_window_h = PC_SCREEN_HEIGHT;
int g_pc_render_w = PC_SCREEN_WIDTH;
int g_pc_render_h = PC_SCREEN_HEIGHT;
int g_pc_scale_mode;
int g_pc_widescreen_stretch;
int g_pc_frameskip_active;
float g_pc_zoom = 1.0f;
int g_pc_model_viewer_no_cull;

static double run_limit_seconds;
static Uint64 run_start_ticks;

extern int game_main(void);
extern void InitializeDol(void);
extern void pm_dvd_set_root(const char *root);
extern BOOL PADInit(void);
extern void PADCleanup(void);

static void pm_crash_signal(int signal_number, siginfo_t *info, void *context)
{
    uintptr_t pc = 0;
    uintptr_t frame = 0;
    uintptr_t link = 0;

#if defined(__aarch64__)
    if (context != NULL) {
        ucontext_t *uc = (ucontext_t *)context;
        pc = (uintptr_t)uc->uc_mcontext.pc;
        frame = (uintptr_t)uc->uc_mcontext.regs[29];
        link = (uintptr_t)uc->uc_mcontext.regs[30];
        dprintf(STDERR_FILENO,
                "[PM/CRASH] x0=%p x1=%p x2=%p x3=%p x4=%p x5=%p x6=%p x7=%p\n",
                (void *)uc->uc_mcontext.regs[0],
                (void *)uc->uc_mcontext.regs[1],
                (void *)uc->uc_mcontext.regs[2],
                (void *)uc->uc_mcontext.regs[3],
                (void *)uc->uc_mcontext.regs[4],
                (void *)uc->uc_mcontext.regs[5],
                (void *)uc->uc_mcontext.regs[6],
                (void *)uc->uc_mcontext.regs[7]);
    }
#elif defined(__x86_64__) && defined(REG_RIP)
    if (context != NULL) {
        ucontext_t *uc = (ucontext_t *)context;
        pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
        frame = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
        link = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    }
#endif
    dprintf(STDERR_FILENO,
            "[PM/CRASH] signal=%d address=%p pc=%p frame=%u fp=%p lr=%p\n",
            signal_number, info != NULL ? info->si_addr : NULL,
            (void *)pc, pc_frame_counter, (void *)frame, (void *)link);
#if defined(__aarch64__)
    {
        uintptr_t *walk = (uintptr_t *)frame;
        int index;
        for (index = 0; index < 16 && walk != NULL; index++) {
            uintptr_t next = walk[0];
            uintptr_t return_address = walk[1];
            Dl_info frame_info;
            dprintf(STDERR_FILENO,
                    "[PM/CRASH] bt[%d] fp=%p lr=%p\n",
                    index, (void *)walk, (void *)return_address);
            if (dladdr((void *)return_address, &frame_info) != 0) {
                dprintf(STDERR_FILENO,
                        "[PM/CRASH] bt[%d] module=%s offset=%p symbol=%s\n",
                        index,
                        frame_info.dli_fname != NULL ? frame_info.dli_fname : "?",
                        (void *)(return_address - (uintptr_t)frame_info.dli_fbase),
                        frame_info.dli_sname != NULL ? frame_info.dli_sname : "?");
            }
            if (next <= (uintptr_t)walk || next - (uintptr_t)walk > 0x100000) {
                break;
            }
            walk = (uintptr_t *)next;
        }
    }
#endif
    {
        Dl_info module_info;
        if (dladdr((void *)pc, &module_info) != 0) {
            uintptr_t offset = pc - (uintptr_t)module_info.dli_fbase;
            dprintf(STDERR_FILENO,
                    "[PM/CRASH] module=%s base=%p symbol=%s offset=%p\n",
                    module_info.dli_fname != NULL ? module_info.dli_fname : "?",
                    module_info.dli_fbase,
                    module_info.dli_sname != NULL ? module_info.dli_sname : "?",
                    (void *)offset);
        }
    }
    _exit(128 + signal_number);
}

static void pm_install_crash_trace(void)
{
    struct sigaction action;

    if (getenv("PARTYBOARD_CRASH_TRACE") == NULL) {
        return;
    }
    memset(&action, 0, sizeof(action));
    sigemptyset(&action.sa_mask);
    action.sa_sigaction = pm_crash_signal;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGFPE, &action, NULL);
}

static double now_seconds(void)
{
    return (double)SDL_GetPerformanceCounter() /
           (double)SDL_GetPerformanceFrequency();
}

void pc_platform_set_run_limit(double seconds)
{
    run_limit_seconds = seconds;
}

void pc_platform_poll_events(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_pc_running = 0;
        }
        if (event.type == SDL_KEYDOWN && !event.key.repeat &&
            event.key.keysym.scancode == SDL_SCANCODE_F12) {
            g_pc_running = 0;
        }
    }

    if (run_limit_seconds > 0.0 &&
        now_seconds() - (double)run_start_ticks /
            (double)SDL_GetPerformanceFrequency() >= run_limit_seconds) {
        g_pc_running = 0;
    }
}

static const char *argument_root(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            g_pc_verbose = 1;
        } else if (strcmp(argv[i], "--no-frame-limit") == 0) {
            g_pc_no_framelimit = 1;
        } else if (strcmp(argv[i], "--windowed") == 0) {
            setenv("PARTYBOARD_WINDOWED", "1", 1);
        } else if (strncmp(argv[i], "--seconds=", 10) == 0) {
            run_limit_seconds = strtod(argv[i] + 10, NULL);
        } else if (argv[i][0] != '-') {
            return argv[i];
        }
    }
    return getenv("PARTYBOARD_GAME_ROOT");
}

static int create_window(void)
{
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

    if (getenv("PARTYBOARD_WINDOWED") == NULL) {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    g_pc_window = SDL_CreateWindow(PC_WINDOW_TITLE,
                                   SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED,
                                   PC_GC_WIDTH, PC_GC_HEIGHT, flags);
    if (g_pc_window == NULL) {
        fprintf(stderr, "[PM] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }
    g_pc_gl_context = SDL_GL_CreateContext(g_pc_window);
    if (g_pc_gl_context == NULL) {
        fprintf(stderr, "[PM] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_pc_window);
        g_pc_window = NULL;
        return 0;
    }
    SDL_GL_GetDrawableSize(g_pc_window, &g_pc_window_w, &g_pc_window_h);
    g_pc_render_w = g_pc_window_w;
    g_pc_render_h = g_pc_window_h;
    printf("[PM] video=%s renderer=%s version=%s drawable=%dx%d\n",
           SDL_GetCurrentVideoDriver() != NULL ? SDL_GetCurrentVideoDriver() : "none",
           (const char *)glGetString(GL_RENDERER),
           (const char *)glGetString(GL_VERSION),
           g_pc_window_w, g_pc_window_h);
    return 1;
}

int main(int argc, char **argv)
{
    const char *root = argument_root(argc, argv);
    const char *verbose = getenv("PARTYBOARD_VERBOSE");
    int result;

    if (verbose != NULL && strcmp(verbose, "0") != 0) {
        g_pc_verbose = 1;
    }
    if (getenv("PARTYBOARD_NO_VSYNC") != NULL) {
        g_pc_no_framelimit = 1;
    }
    if (root != NULL) {
        pm_dvd_set_root(root);
    }

    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    pm_install_crash_trace();
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "[PM] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!create_window()) {
        SDL_Quit();
        return 2;
    }
    if (g_pc_no_framelimit || SDL_GL_SetSwapInterval(1) != 0) {
        SDL_GL_SetSwapInterval(0);
    }

    run_start_ticks = SDL_GetPerformanceCounter();
    pc_gx_init();
    pc_gx_begin_frame();
    OSInit();
    DVDInit();
    InitializeDol();
    PADInit();

    printf("[PM] starting Party Board root=%s\n", root != NULL ? root : ".");
    result = game_main();
    printf("[PM] game returned %d\n", result);

    PADCleanup();
    pc_gx_shutdown();
    SDL_GL_DeleteContext(g_pc_gl_context);
    SDL_DestroyWindow(g_pc_window);
    SDL_Quit();
    puts("[PM] clean exit");
    return result;
}
