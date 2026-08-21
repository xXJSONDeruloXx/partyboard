#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PM_PROBE_NAME
#define PM_PROBE_NAME "PortMaster GLES bring-up"
#endif

typedef struct ProbeState {
    SDL_Window *window;
    SDL_GLContext context;
    SDL_GameController *controller;
    int start_held;
    int back_held;
    int running;
} ProbeState;

static double now_seconds(void)
{
    return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}

static int requested_seconds(void)
{
    const char *value = getenv("PM_PROBE_SECONDS");
    char *end = NULL;
    long seconds;

    if (value == NULL || *value == '\0') {
        return 0;
    }
    seconds = strtol(value, &end, 10);
    if (end == value || *end != '\0' || seconds < 1 || seconds > 3600) {
        return 0;
    }
    return (int)seconds;
}

static SDL_Window *create_window(void)
{
    const int profiles[][2] = {{3, 2}, {3, 0}, {2, 0}};
    size_t i;

    for (i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, profiles[i][0]);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, profiles[i][1]);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        SDL_Window *window = SDL_CreateWindow(
            PM_PROBE_NAME,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            640,
            480,
            SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
        if (window != NULL) {
            printf("[PM] GLES context request: %d.%d\n", profiles[i][0], profiles[i][1]);
            return window;
        }
        fprintf(stderr, "[PM] GLES %d.%d window failed: %s\n",
                profiles[i][0], profiles[i][1], SDL_GetError());
    }
    return NULL;
}

static void open_first_controller(ProbeState *state)
{
    int i;

    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) {
            continue;
        }
        state->controller = SDL_GameControllerOpen(i);
        if (state->controller != NULL) {
            printf("[PM] controller: %s\n", SDL_GameControllerName(state->controller));
            return;
        }
        fprintf(stderr, "[PM] controller %d failed: %s\n", i, SDL_GetError());
    }
    puts("[PM] controller: none detected");
}

static void handle_event(ProbeState *state, const SDL_Event *event)
{
    if (event->type == SDL_QUIT) {
        state->running = 0;
        return;
    }

    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_q) {
            state->running = 0;
        }
        return;
    }

    if (event->type == SDL_CONTROLLERBUTTONDOWN || event->type == SDL_CONTROLLERBUTTONUP) {
        const int held = event->type == SDL_CONTROLLERBUTTONDOWN;
        if (event->cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            state->start_held = held;
        } else if (event->cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
            state->back_held = held;
        }
        if (state->start_held && state->back_held) {
            puts("[PM] exit chord: Start+Select");
            state->running = 0;
        }
    }
}

int main(void)
{
    ProbeState state = {0};
    SDL_Event event;
    const int duration = requested_seconds();
    double start_time;
    double last_report;
    double last_frame;
    double frame_time_sum = 0.0;
    unsigned long frames = 0;
    int width = 0;
    int height = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "[PM] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("[PM] %s\n", PM_PROBE_NAME);
    printf("[PM] SDL version: %u.%u.%u\n", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
    printf("[PM] video driver: %s\n", SDL_GetCurrentVideoDriver() != NULL ? SDL_GetCurrentVideoDriver() : "none");

    state.window = create_window();
    if (state.window == NULL) {
        fprintf(stderr, "[PM] no GLES window: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    state.context = SDL_GL_CreateContext(state.window);
    if (state.context == NULL) {
        fprintf(stderr, "[PM] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(state.window);
        SDL_Quit();
        return 3;
    }

    SDL_GL_GetDrawableSize(state.window, &width, &height);
    printf("[PM] drawable: %dx%d\n", width, height);
    printf("[PM] GL vendor: %s\n", (const char *)glGetString(GL_VENDOR));
    printf("[PM] GL renderer: %s\n", (const char *)glGetString(GL_RENDERER));
    printf("[PM] GL version: %s\n", (const char *)glGetString(GL_VERSION));
    printf("[PM] GLSL version: %s\n", (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION));

    if (SDL_GL_SetSwapInterval(1) != 0) {
        fprintf(stderr, "[PM] vsync unavailable: %s\n", SDL_GetError());
        SDL_GL_SetSwapInterval(0);
    }

    open_first_controller(&state);
    state.running = 1;
    start_time = now_seconds();
    last_report = now_seconds();
    last_frame = last_report;

    while (state.running) {
        const double frame_start = now_seconds();
        const double elapsed = frame_start - last_report;

        while (SDL_PollEvent(&event)) {
            handle_event(&state, &event);
        }

        glViewport(0, 0, width, height);
        glClearColor(0.08f, 0.03f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(state.window);

        ++frames;
        frame_time_sum += frame_start - last_frame;
        last_frame = frame_start;

        if (elapsed >= 1.0) {
            const double average_ms = frames > 0 ? (frame_time_sum * 1000.0) / (double)frames : 0.0;
            printf("[PM] frames=%lu fps=%.2f avg_ms=%.3f\n",
                   frames, (double)frames / elapsed, average_ms);
            fflush(stdout);
            frames = 0;
            frame_time_sum = 0.0;
            last_report = frame_start;
        }

        if (duration > 0 && frame_start - start_time >= (double)duration) {
            state.running = 0;
        }
    }

    if (state.controller != NULL) {
        SDL_GameControllerClose(state.controller);
    }
    SDL_GL_DeleteContext(state.context);
    SDL_DestroyWindow(state.window);
    SDL_Quit();
    puts("[PM] clean exit");
    return 0;
}
