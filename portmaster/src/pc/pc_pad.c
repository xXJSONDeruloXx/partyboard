/* Party Board PortMaster controller bridge.
 *
 * PortMaster's gptokeyb commonly presents the handheld as SDL keyboard input,
 * so the keyboard bindings intentionally mirror the physical GameCube layout:
 * A=east, B=south, X=north, Y=west.  SDL_GameController itself exposes the
 * usual Xbox-positioned semantic buttons (A=south, B=west, X=north, Y=east),
 * which are converted by position below.
 */
#include "pc_platform.h"
#include <dolphin/pad.h>

#define STICK_MAGNITUDE 80
#define AXIS_DEADZONE 5000
#define TRIGGER_THRESHOLD 8192
#define EXIT_CHORD_HOLD_MS 250

static SDL_GameController *g_controller;
static Uint32 g_exit_chord_started;
static int g_input_trace = -1;
static u16 g_last_trace_buttons;
static s8 g_last_trace_stick_x;
static s8 g_last_trace_stick_y;

static int key_down(const Uint8 *keys, SDL_Scancode primary,
                    SDL_Scancode secondary)
{
    return keys[primary] || (secondary != SDL_SCANCODE_UNKNOWN && keys[secondary]);
}

static int controller_button(SDL_GameControllerButton button)
{
    return g_controller && SDL_GameControllerGetButton(g_controller, button);
}

static void update_exit_chord(u16 buttons, const Uint8 *keys)
{
    int start = (buttons & PAD_BUTTON_START) != 0;
    int select = key_down(keys, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_BACKSPACE);

    if (g_controller) {
        select = select || controller_button(SDL_CONTROLLER_BUTTON_BACK);
    }

    if (start && select) {
        Uint32 now = SDL_GetTicks();
        if (g_exit_chord_started == 0) {
            g_exit_chord_started = now;
        } else if (now - g_exit_chord_started >= EXIT_CHORD_HOLD_MS) {
            g_pc_running = 0;
        }
    } else {
        g_exit_chord_started = 0;
    }
}

BOOL PADInit(void)
{
    int count = SDL_NumJoysticks();

    for (int i = 0; i < count; i++) {
        if (!SDL_IsGameController(i)) {
            continue;
        }
        g_controller = SDL_GameControllerOpen(i);
        if (g_controller) {
            printf("[PM/PAD] controller=%s\n",
                   SDL_GameControllerName(g_controller));
            break;
        }
    }

    if (!g_controller) {
        printf("[PM/PAD] no SDL game controller; keyboard/gptokeyb input enabled\n");
    }
    return TRUE;
}

u32 PADRead(PADStatus *status)
{
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    u16 buttons = 0;
    s8 stick_x = 0;
    s8 stick_y = 0;
    s8 cstick_x = 0;
    s8 cstick_y = 0;
    u8 trigger_l = 0;
    u8 trigger_r = 0;

    if (g_input_trace < 0) {
        g_input_trace = getenv("PARTYBOARD_INPUT_TRACE") != NULL;
    }

    memset(status, 0, sizeof(PADStatus) * PAD_MAX_CONTROLLERS);
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
        status[i].err = PAD_ERR_NO_CONTROLLER;
    }

    if (g_controller) {
        SDL_GameControllerUpdate();

        /* SDL's semantic buttons are positional, not Nintendo-labelled. */
        if (controller_button(SDL_CONTROLLER_BUTTON_A)) buttons |= PAD_BUTTON_B;
        if (controller_button(SDL_CONTROLLER_BUTTON_B)) buttons |= PAD_BUTTON_Y;
        if (controller_button(SDL_CONTROLLER_BUTTON_X)) buttons |= PAD_BUTTON_X;
        if (controller_button(SDL_CONTROLLER_BUTTON_Y)) buttons |= PAD_BUTTON_A;
        if (controller_button(SDL_CONTROLLER_BUTTON_START)) buttons |= PAD_BUTTON_START;
        if (controller_button(SDL_CONTROLLER_BUTTON_BACK)) buttons |= PAD_BUTTON_BACK;

        if (controller_button(SDL_CONTROLLER_BUTTON_DPAD_UP)) buttons |= PAD_BUTTON_UP;
        if (controller_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) buttons |= PAD_BUTTON_DOWN;
        if (controller_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) buttons |= PAD_BUTTON_LEFT;
        if (controller_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) buttons |= PAD_BUTTON_RIGHT;

        s16 lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        s16 ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        s16 rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        s16 ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        s16 lt = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        s16 rt = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

        if (abs(lx) > AXIS_DEADZONE) stick_x = (s8)(lx / 256);
        if (abs(ly) > AXIS_DEADZONE) stick_y = (s8)(-ly / 256);
        if (abs(rx) > AXIS_DEADZONE) cstick_x = (s8)(rx / 256);
        if (abs(ry) > AXIS_DEADZONE) cstick_y = (s8)(-ry / 256);
        trigger_l = (u8)((lt < 0 ? 0 : lt) / 256);
        trigger_r = (u8)((rt < 0 ? 0 : rt) / 256);
        if (trigger_l > (TRIGGER_THRESHOLD / 256)) buttons |= PAD_TRIGGER_L;
        if (trigger_r > (TRIGGER_THRESHOLD / 256)) buttons |= PAD_TRIGGER_R;
    }

    /* Keyboard layout used by gptokeyb and desktop smoke tests. */
    if (key_down(keys, SDL_SCANCODE_SPACE, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_BUTTON_A;
    if (key_down(keys, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_BUTTON_B;
    if (key_down(keys, SDL_SCANCODE_X, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_BUTTON_X;
    if (key_down(keys, SDL_SCANCODE_Y, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_BUTTON_Y;
    if (key_down(keys, SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER)) buttons |= PAD_BUTTON_START;
    if (key_down(keys, SDL_SCANCODE_Q, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_TRIGGER_L;
    if (key_down(keys, SDL_SCANCODE_E, SDL_SCANCODE_UNKNOWN)) buttons |= PAD_TRIGGER_R;

    if (key_down(keys, SDL_SCANCODE_I, SDL_SCANCODE_UP)) buttons |= PAD_BUTTON_UP;
    if (key_down(keys, SDL_SCANCODE_K, SDL_SCANCODE_DOWN)) buttons |= PAD_BUTTON_DOWN;
    if (key_down(keys, SDL_SCANCODE_J, SDL_SCANCODE_LEFT)) buttons |= PAD_BUTTON_LEFT;
    if (key_down(keys, SDL_SCANCODE_L, SDL_SCANCODE_RIGHT)) buttons |= PAD_BUTTON_RIGHT;

    if (keys[SDL_SCANCODE_W]) stick_y = STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_S]) stick_y = -STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_A]) stick_x = -STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_D]) stick_x = STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_UP]) cstick_y = STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_DOWN]) cstick_y = -STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_LEFT]) cstick_x = -STICK_MAGNITUDE;
    if (keys[SDL_SCANCODE_RIGHT]) cstick_x = STICK_MAGNITUDE;

    /* A stickless handheld still needs to move through board menus and play. */
    if (buttons & PAD_BUTTON_UP) stick_y = STICK_MAGNITUDE;
    if (buttons & PAD_BUTTON_DOWN) stick_y = -STICK_MAGNITUDE;
    if (buttons & PAD_BUTTON_LEFT) stick_x = -STICK_MAGNITUDE;
    if (buttons & PAD_BUTTON_RIGHT) stick_x = STICK_MAGNITUDE;

    status[0].button = buttons;
    status[0].stickX = stick_x;
    status[0].stickY = stick_y;
    status[0].substickX = cstick_x;
    status[0].substickY = cstick_y;
    status[0].triggerLeft = trigger_l;
    status[0].triggerRight = trigger_r;
    status[0].err = PAD_ERR_NONE;

    if (g_input_trace && (buttons != g_last_trace_buttons ||
                          stick_x != g_last_trace_stick_x ||
                          stick_y != g_last_trace_stick_y)) {
        printf("[PM/PAD] input buttons=0x%04x stick=(%d,%d) controller=%s\n",
               buttons, stick_x, stick_y,
               g_controller ? SDL_GameControllerName(g_controller) : "none");
        g_last_trace_buttons = buttons;
        g_last_trace_stick_x = stick_x;
        g_last_trace_stick_y = stick_y;
    }

    update_exit_chord(buttons, keys);
    return PAD_CHAN0_BIT;
}

void PADControlMotor(u32 chan, u32 command)
{
    if (g_controller && chan == 0) {
        Uint16 strength = command == PAD_MOTOR_RUMBLE ? 0xFFFF : 0;
        SDL_GameControllerRumble(g_controller, strength, strength, 200);
    }
}

void PADControlAllMotors(const u32 *commands)
{
    PADControlMotor(0, commands[0]);
}

BOOL PADReset(u32 mask) { (void)mask; return TRUE; }
BOOL PADRecalibrate(u32 mask) { (void)mask; return TRUE; }
BOOL PADSync(void) { return TRUE; }
void PADSetSpec(u32 spec) { (void)spec; }
void PADSetAnalogMode(u32 mode) { (void)mode; }

void PADClamp(PADStatus *status)
{
    for (int i = 0; i < PAD_MAX_CONTROLLERS; i++) {
        if (status[i].stickX < -128) status[i].stickX = -128;
        if (status[i].stickX > 127) status[i].stickX = 127;
        if (status[i].stickY < -128) status[i].stickY = -128;
        if (status[i].stickY > 127) status[i].stickY = 127;
    }
}

void PADCleanup(void)
{
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}
