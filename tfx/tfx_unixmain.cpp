#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/time.h>
#include "textfx.h"

#include <SDL2/SDL.h>

extern void demomain();

short TFX_FrameBuffer[TFX_ConsoleHeight * TFX_ConsoleWidth];
int TFX_Paramc;
char **TFX_Params;

static bool          gWindowMode = false;
static SDL_Window   *gWindow     = NULL;
static SDL_Renderer *gRenderer   = NULL;
static SDL_Texture  *gTexture    = NULL;
static Uint32        gSDLPalette[16];

// Each character glyph is 8 pixels wide × 12 pixels tall.
#define CHAR_W 8
#define CHAR_H 12

// Integer scale factor applied to the logical (640×600) canvas.
// Overridden at runtime by -s N.
static int WIN_SCALE = 1;

static void build_sdl_palette()
{
    // TFX_Palette32 packs colors as 0xBBGGRR:
    //   bits  0- 7 = red channel
    //   bits  8-15 = green channel
    //   bits 16-23 = blue channel
    // SDL_PIXELFORMAT_ARGB8888 Uint32 layout: 0xAARRGGBB
    for (int i = 0; i < 16; i++)
    {
        int    c = TFX_Palette32[i];
        Uint8  r = (Uint8)((c >>  0) & 0xff);
        Uint8  g = (Uint8)((c >>  8) & 0xff);
        Uint8  b = (Uint8)((c >> 16) & 0xff);
        gSDLPalette[i] = 0xff000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | b;
    }
}

static bool init_window()
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "SDL video init failed: %s\n", SDL_GetError());
        return false;
    }

    const int log_w = TFX_ConsoleWidth  * CHAR_W;          // 640
    const int log_h = TFX_ConsoleHeight * CHAR_H;          // 600
    const int win_w = log_w * WIN_SCALE;
    const int win_h = log_h * WIN_SCALE;

    gWindow = SDL_CreateWindow(
        "Litterae Finis",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h, 0);
    if (!gWindow)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // On IRIX the GL driver limits textures to 512×512, which is smaller than
    // our 640×600 canvas.  Force the SDL software renderer so display goes
    // through SDL's CPU blitter and has no GL texture-size constraint.
#if defined(__sgi)
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_SOFTWARE);
#else
    gRenderer = SDL_CreateRenderer(
        gWindow, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer)
        gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
#endif
    if (!gRenderer)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return false;
    }

    // Map the logical canvas onto the (potentially scaled) window.
    SDL_RenderSetLogicalSize(gRenderer, log_w, log_h);

    gTexture = SDL_CreateTexture(
        gRenderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        log_w, log_h);
    if (!gTexture)
    {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return false;
    }

    build_sdl_palette();
    return true;
}

static void destroy_window()
{
    if (gTexture)  { SDL_DestroyTexture(gTexture);   gTexture  = NULL; }
    if (gRenderer) { SDL_DestroyRenderer(gRenderer); gRenderer = NULL; }
    if (gWindow)   { SDL_DestroyWindow(gWindow);     gWindow   = NULL; }
}

// CGA color index -> ANSI foreground escape code number
static const int cga_to_ansi_fg[16] = {
    30, 34, 32, 36, 31, 35, 33, 37,   // 0-7:  dark colors
    90, 94, 92, 96, 91, 95, 93, 97    // 8-15: bright colors
};

// CGA color index (0-7) -> ANSI background escape code number
static const int cga_to_ansi_bg[8] = {
    40, 44, 42, 46, 41, 45, 43, 47
};

static struct termios old_termios;
static bool terminal_setup = false;

static void restore_terminal()
{
    if (terminal_setup)
        tcsetattr(STDIN_FILENO, TCSANOW, &old_termios);
    fputs("\033[?25h\033[0m\n", stdout);
    fflush(stdout);
}

static void setup_terminal()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &old_termios);
    t = old_termios;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    terminal_setup = true;
    atexit(restore_terminal);

    // Hide cursor and clear screen
    fputs("\033[?25l\033[2J\033[H", stdout);
    fflush(stdout);
}

int _kbhit()
{
    if (gWindowMode)
    {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)    return 1;
            if (ev.type == SDL_KEYDOWN) return 1;
        }
        return 0;
    }

    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

unsigned int linux_tick_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned int)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void TFX_SetTitle(char *aTitle)
{
    if (gWindowMode)
    {
        SDL_SetWindowTitle(gWindow, aTitle);
        return;
    }
    printf("\033]2;%s\007", aTitle);
    fflush(stdout);
}

void TFX_Present()
{
    if (gWindowMode)
    {
        void *pixels;
        int   pitch;
        if (SDL_LockTexture(gTexture, NULL, &pixels, &pitch) < 0)
            return;

        Uint32 *dst    = (Uint32 *)pixels;
        int     stride = pitch / 4;   // pixels per row in the texture

        for (int row = 0; row < TFX_ConsoleHeight; row++)
        {
            for (int col = 0; col < TFX_ConsoleWidth; col++)
            {
                short         cell  = TFX_FrameBuffer[row * TFX_ConsoleWidth + col];
                unsigned char ch    = (unsigned char)(cell & 0xff);
                unsigned char attr  = (unsigned char)((cell >> 8) & 0xff);
                Uint32        fg    = gSDLPalette[attr & 0x0f];
                Uint32        bg    = gSDLPalette[(attr >> 4) & 0x07];

                for (int py = 0; py < CHAR_H; py++)
                {
                    unsigned char bits = TFX_AsciiFontdata[ch * CHAR_H + py];
                    int base = (row * CHAR_H + py) * stride + col * CHAR_W;
                    for (int px = 0; px < CHAR_W; px++)
                        dst[base + px] = (bits >> (7 - px)) & 1 ? fg : bg;
                }
            }
        }

        SDL_UnlockTexture(gTexture);
        SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);
        SDL_RenderPresent(gRenderer);
        return;
    }

    // --- Terminal path (unchanged) ---
    // Max chars per cell: ESC [ XX ; XX m  = 9 chars + 1 char = 10
    // Plus ESC[H (3) + newlines (TFX_ConsoleHeight) + ESC[0m (4)
    static char buf[TFX_ConsoleHeight * TFX_ConsoleWidth * 10 + 64];
    int pos = 0;

    // Home cursor
    buf[pos++] = '\033';
    buf[pos++] = '[';
    buf[pos++] = 'H';

    int last_fg = -1, last_bg = -1;

    for (int row = 0; row < TFX_ConsoleHeight; row++)
    {
        for (int col = 0; col < TFX_ConsoleWidth; col++)
        {
            short cell = TFX_FrameBuffer[row * TFX_ConsoleWidth + col];
            unsigned char ch   = cell & 0xff;
            unsigned char attr = (cell >> 8) & 0xff;
            int fg = attr & 0x0f;
            int bg = (attr >> 4) & 0x07;

            int ansi_fg = cga_to_ansi_fg[fg];
            int ansi_bg = cga_to_ansi_bg[bg];

            if (ansi_fg != last_fg || ansi_bg != last_bg)
            {
                buf[pos++] = '\033';
                buf[pos++] = '[';
                // fg
                if (ansi_fg >= 90) {
                    buf[pos++] = '9';
                    buf[pos++] = '0' + (char)(ansi_fg - 90);
                } else {
                    buf[pos++] = '3';
                    buf[pos++] = '0' + (char)(ansi_fg - 30);
                }
                buf[pos++] = ';';
                // bg
                buf[pos++] = '4';
                buf[pos++] = '0' + (char)(ansi_bg - 40);
                buf[pos++] = 'm';
                last_fg = ansi_fg;
                last_bg = ansi_bg;
            }

            // Replace non-printable ASCII with space to avoid layout issues
            if (ch < 32 || ch == 127)
                ch = ' ';
            buf[pos++] = (char)ch;
        }
        buf[pos++] = '\n';
    }

    // Reset colors at end of frame
    buf[pos++] = '\033';
    buf[pos++] = '[';
    buf[pos++] = '0';
    buf[pos++] = 'm';

    write(STDOUT_FILENO, buf, pos);
}

static void print_help(const char *argv0)
{
    printf("\nLitterae Finis - by Trauma\n\n");
    printf("1st place textmode demo, TMDC 2012\n\n");
    printf("Sound: !Cube - Starchild\n\n");
    printf("Usage: %s -c | -w [-s N]\n\n", argv0);
    printf("Options:\n");
    printf("  -c      Run in console/terminal mode (ANSI output)\n");
    printf("  -w      Run in window mode (SDL)\n");
    printf("  -s N    Window scale factor (windowed mode only, default 1)\n\n");
}

int main(int paramc, char **params)
{
    bool consoleMode = false;

    for (int i = 1; i < paramc; i++)
    {
        if (strcmp(params[i], "-w") == 0) {
            gWindowMode = true;
        } else if (strcmp(params[i], "-c") == 0) {
            consoleMode = true;
        } else if (strcmp(params[i], "-s") == 0) {
            if (i + 1 < paramc) {
                int s = atoi(params[++i]);
                if (s >= 1) WIN_SCALE = s;
            }
        } else if (strcmp(params[i], "-h") == 0) {
            print_help(params[0]);
            return 0;
        }
    }

    if (!gWindowMode && !consoleMode)
    {
        print_help(params[0]);
        return 0;
    }

    if (gWindowMode && consoleMode)
    {
        fprintf(stderr, "Error: -w and -c are mutually exclusive.\n");
        print_help(params[0]);
        return 1;
    }

    TFX_Paramc = paramc;
    TFX_Params = params;

    if (gWindowMode)
    {
        // SDL_Init(0) is required before SDL_InitSubSystem; audio will be
        // initialised separately by FSOUND_Init via SDL_InitSubSystem.
        SDL_Init(0);
        if (!init_window())
        {
            SDL_Quit();
            return 1;
        }
    }
    else
    {
        setup_terminal();
    }

    demomain();

    if (gWindowMode)
    {
        destroy_window();
        SDL_Quit();
    }

    return 0;
}
