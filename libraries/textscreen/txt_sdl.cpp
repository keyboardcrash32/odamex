//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Text mode emulation in SDL
//

#pragma warning(disable : 5045) // SPECTRE mitigation warning

#include <SDL.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstdlib>

#include "doomkeys.h"

#include "txt_main.h"
#include "txt_sdl.h"

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

typedef struct
{
    char *name;
    const uint8_t *data;
    int w;
    int h;
} txt_font_t;

// Fonts:

#include "fonts/small.h"
#include "fonts/normal.h"
#include "fonts/large.h"

// Time between character blinks in ms

#define BLINK_PERIOD 250

#ifdef SDL12
static SDL_Surface *screenbuffer;
#define SDL_Keysym SDL_keysym
#else
SDL_Window *TXT_SDLWindow;
static SDL_Surface *screenbuffer;
static SDL_Renderer *renderer;
#endif
static unsigned char* screendata;
static int key_mapping = 1;

// Dimensions of the screen image in screen coordinates (not pixels); this
// is the value that was passed to SDL_CreateWindow().
static int screen_image_w, screen_image_h;

static TxtSDLEventCallbackFunc event_callback;
static void *event_callback_data;

static int modifier_state[TXT_NUM_MODIFIERS];

// Font we are using:
static const txt_font_t *font;

// Dummy "font" that means to try highdpi rendering, or fallback to
// normal_font otherwise.
static const txt_font_t highdpi_font = { (char*)"normal-highdpi", NULL, 8, 16 };

// Just for I_Endoom
//static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

//#define TANGO

#ifndef TANGO

static SDL_Color ega_colors[] = 
{
    {0x00, 0x00, 0x00, 0xff},          // 0: Black
    {0x00, 0x00, 0xa8, 0xff},          // 1: Blue
    {0x00, 0xa8, 0x00, 0xff},          // 2: Green
    {0x00, 0xa8, 0xa8, 0xff},          // 3: Cyan
    {0xa8, 0x00, 0x00, 0xff},          // 4: Red
    {0xa8, 0x00, 0xa8, 0xff},          // 5: Magenta
    {0xa8, 0x54, 0x00, 0xff},          // 6: Brown
    {0xa8, 0xa8, 0xa8, 0xff},          // 7: Grey
    {0x54, 0x54, 0x54, 0xff},          // 8: Dark grey
    {0x54, 0x54, 0xfe, 0xff},          // 9: Bright blue
    {0x54, 0xfe, 0x54, 0xff},          // 10: Bright green
    {0x54, 0xfe, 0xfe, 0xff},          // 11: Bright cyan
    {0xfe, 0x54, 0x54, 0xff},          // 12: Bright red
    {0xfe, 0x54, 0xfe, 0xff},          // 13: Bright magenta
    {0xfe, 0xfe, 0x54, 0xff},          // 14: Yellow
    {0xfe, 0xfe, 0xfe, 0xff},          // 15: Bright white
};

#else

// Colors that fit the Tango desktop guidelines: see
// http://tango.freedesktop.org/ also
// http://uwstopia.nl/blog/2006/07/tango-terminal

static SDL_Color ega_colors[] = 
{
    {0x2e, 0x34, 0x36, 0xff},          // 0: Black
    {0x34, 0x65, 0xa4, 0xff},          // 1: Blue
    {0x4e, 0x9a, 0x06, 0xff},          // 2: Green
    {0x06, 0x98, 0x9a, 0xff},          // 3: Cyan
    {0xcc, 0x00, 0x00, 0xff},          // 4: Red
    {0x75, 0x50, 0x7b, 0xff},          // 5: Magenta
    {0xc4, 0xa0, 0x00, 0xff},          // 6: Brown
    {0xd3, 0xd7, 0xcf, 0xff},          // 7: Grey
    {0x55, 0x57, 0x53, 0xff},          // 8: Dark grey
    {0x72, 0x9f, 0xcf, 0xff},          // 9: Bright blue
    {0x8a, 0xe2, 0x34, 0xff},          // 10: Bright green
    {0x34, 0xe2, 0xe2, 0xff},          // 11: Bright cyan
    {0xef, 0x29, 0x29, 0xff},          // 12: Bright red
    {0x34, 0xe2, 0xe2, 0xff},          // 13: Bright magenta
    {0xfc, 0xe9, 0x4f, 0xff},          // 14: Yellow
    {0xee, 0xee, 0xec, 0xff},          // 15: Bright white
};

#endif

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Examine system DPI settings to determine whether to use the large font.

static int Win32_UseLargeFont(void)
{
    HDC hdc = GetDC(NULL);
    int dpix;

    if (!hdc)
    {
        return 0;
    }

    dpix = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);

    // 144 is the DPI when using "150%" scaling. If the user has this set
    // then consider this an appropriate threshold for using the large font.

    return dpix >= 144;
}

#endif

static const txt_font_t *FontForName(char *name)
{
    int i;
    const txt_font_t *fonts[] =
    {
        &small_font,
        &normal_font,
        &large_font,
        &highdpi_font,
        NULL,
    };

    for (i = 0; fonts[i]->name != NULL; ++i)
    {
        if (!strcmp(fonts[i]->name, name))
        {
            return fonts[i];
        }
    }
    return NULL;
}

//
// Select the font to use, based on screen resolution
//
// If the highest screen resolution available is less than
// 640x480, use the small font.
//

static void ChooseFont(void)
{
#ifdef SDL12
	const SDL_VideoInfo *desktop_info;
#else
    SDL_DisplayMode desktop_info;
#endif
    char *env;
    size_t len = 0; // amount of envvar characters
    int w, h;

    // Allow normal selection to be overridden from an environment variable:
#if defined(_MSC_VER)
    _dupenv_s(&env, &len, "TEXTSCREEN_FONT");
#else
    env = std::getenv("TEXTSCREEN_FONT");
#endif

    if (env != NULL)
    {
        font = FontForName(env);

        if (font != NULL)
        {
            return;
        }
    }

    // Get desktop resolution.
    // If in doubt and we can't get a list, always prefer to
    // fall back to the normal font:
#ifdef SDL12
	desktop_info = SDL_GetVideoInfo();
	if(NULL == desktop_info)
#else
    if (SDL_GetCurrentDisplayMode(0, &desktop_info))
#endif
    {
        font = &highdpi_font;
        return;
    }

#ifdef SDL12
	w = desktop_info->current_w;
	h = desktop_info->current_h;
#else
	w = desktop_info.w;
	h = desktop_info.h;
#endif

    // On tiny low-res screens (eg. palmtops) use the small font.
    // If the screen resolution is at least 1920x1080, this is
    // a modern high-resolution display, and we can use the
    // large font.

    if (w < 640 || h < 480)
    {
        font = &small_font;
    }
#ifdef _WIN32
    // On Windows we can use the system DPI settings to make a
    // more educated guess about whether to use the large font.

    else if (Win32_UseLargeFont())
    {
        font = &large_font;
    }
#endif
    // TODO: Detect high DPI on Linux by inquiring about Gtk+ scale
    // settings. This looks like it should just be a case of shelling
    // out to invoke the 'gsettings' command, eg.
    //   gsettings get org.gnome.desktop.interface text-scaling-factor
    // and using large_font if the result is >= 2.
    else
    {
        // highdpi_font usually means normal_font (the normal resolution
        // version), but actually means "set the HIGHDPI flag and try
        // to use large_font if we initialize successfully".
        font = &highdpi_font;
    }
}

//
// Initialize text mode screen
//
// Returns 1 if successful, 0 if an error occurred
//

#ifdef SDL12
int TXT_Init(void)
{
    int flags;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        return 0;
    }

    flags = SDL_SWSURFACE | SDL_HWPALETTE | SDL_DOUBLEBUF;

    ChooseFont();

	// There is no high dpi support in SDL12
    if (font == &highdpi_font)
    {
        font = &normal_font;
    }

    screen_image_w = TXT_SCREEN_W * font->w;
    screen_image_h = TXT_SCREEN_H * font->h;
 
    screenbuffer = SDL_SetVideoMode(screen_image_w, screen_image_h, 8, flags);

    if (screenbuffer == NULL)
        return 0;

    SDL_SetColors(screenbuffer, ega_colors, 0, 16);
    SDL_EnableUNICODE(1);

    screendata = (unsigned char*)malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);

    // Ignore all mouse motion events

//    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    // Repeat key presses so we can hold down arrows to scroll down the
    // menu, for example. This is what setup.exe does.

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    return 1;
}
#else
int TXT_Init(void)
{
    Uint32 flags = 0;

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        return 0;
    }

    ChooseFont();

    screen_image_w = TXT_SCREEN_W * font->w;
    screen_image_h = TXT_SCREEN_H * font->h;

    // If highdpi_font is selected, try to initialize high dpi rendering.
    if (font == &highdpi_font)
    {
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }

    TXT_SDLWindow =
        SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                         screen_image_w, screen_image_h, flags);

    if (TXT_SDLWindow == NULL)
        return 0;

    renderer = SDL_CreateRenderer(TXT_SDLWindow, -1, 0);

    // Special handling for OS X retina display. If we successfully set the
    // highdpi flag, check the output size for the screen renderer. If we get
    // the 2x doubled size we expect from a retina display, use the large font
    // for drawing the screen.
    if ((SDL_GetWindowFlags(TXT_SDLWindow) & SDL_WINDOW_ALLOW_HIGHDPI) != 0)
    {
        int render_w, render_h;

        if (SDL_GetRendererOutputSize(renderer, &render_w, &render_h) == 0
         && render_w >= TXT_SCREEN_W * large_font.w
         && render_h >= TXT_SCREEN_H * large_font.h)
        {
            font = &large_font;
            // Note that we deliberately do not update screen_image_{w,h}
            // since these are the dimensions of textscreen image in screen
            // coordinates, not pixels.
        }
    }

    // Failed to initialize for high dpi (retina display) rendering? If so
    // then use the normal resolution font instead.
    if (font == &highdpi_font)
    {
        font = &normal_font;
    }

    // Instead, we draw everything into an intermediate 8-bit surface
    // the same dimensions as the screen. SDL then takes care of all the
    // 8->32 bit (or whatever depth) color conversions for us.
    screenbuffer = SDL_CreateRGBSurface(0,
                                        TXT_SCREEN_W * font->w,
                                        TXT_SCREEN_H * font->h,
                                        8, 0, 0, 0, 0);

    SDL_LockSurface(screenbuffer);
    SDL_SetPaletteColors(screenbuffer->format->palette, ega_colors, 0, 16);
    SDL_UnlockSurface(screenbuffer);

    screendata = (unsigned char*)malloc(TXT_SCREEN_W * TXT_SCREEN_H * 2);
    memset(screendata, 0, TXT_SCREEN_W * TXT_SCREEN_H * 2);

    // Ignore all mouse motion events
//    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

    // Repeat key presses so we can hold down arrows to scroll down the
    // menu, for example. This is what setup.exe does.

    return 1;
}
#endif

void TXT_Shutdown(void)
{
    free(screendata);
    screendata = NULL;
#ifdef SDL20
    SDL_FreeSurface(screenbuffer);
    screenbuffer = NULL;
#endif
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

unsigned char *TXT_GetScreenData(void)
{
    return screendata;
}

static inline void UpdateCharacter(int x, int y)
{
    unsigned char character;
    const uint8_t *p;
    unsigned char *s, *s1;
    unsigned int bit;
    int bg, fg;
    int x1, y1;

    p = &screendata[(y * TXT_SCREEN_W + x) * 2];
    character = p[0];

    fg = p[1] & 0xf;
    bg = (p[1] >> 4) & 0xf;

    if (bg & 0x8)
    {
        // blinking

        bg &= ~0x8;

        if (((SDL_GetTicks() / BLINK_PERIOD) % 2) == 0)
        {
            fg = bg;
        }
    }

    // How many bytes per line?
    p = &font->data[(character * font->w * font->h) / 8];
    bit = 0;

    s = ((unsigned char *) screenbuffer->pixels)
      + (y * font->h * screenbuffer->pitch)
      + (x * font->w);

    for (y1=0; y1<font->h; ++y1)
    {
        s1 = s;

        for (x1=0; x1<font->w; ++x1)
        {
            if (*p & (1 << bit))
            {
                *s1++ = fg;
            }
            else
            {
                *s1++ = bg;
            }

            ++bit;
            if (bit == 8)
            {
                ++p;
                bit = 0;
            }
        }
        s += screenbuffer->pitch;
    }
}

static int LimitToRange(int val, int min, int max)
{
    if (val < min)
    {
        return min;
    }
    else if (val > max)
    {
        return max;
    }
    else
    {
        return val;
    }
}

#ifdef SDL20
static void GetDestRect(SDL_Rect *rect)
{
    int w, h;

    SDL_GetRendererOutputSize(renderer, &w, &h);
    rect->x = (w - screenbuffer->w) / 2;
    rect->y = (h - screenbuffer->h) / 2;
    rect->w = screenbuffer->w;
    rect->h = screenbuffer->h;
}
#endif

void TXT_UpdateScreenArea(int x, int y, int w, int h)
{
#ifdef SDL20
    SDL_Texture *screentx;
    SDL_Rect rect;
#endif
    int x1, y1;
    int x_end;
    int y_end;

    SDL_LockSurface(screenbuffer);

    x_end = LimitToRange(x + w, 0, TXT_SCREEN_W);
    y_end = LimitToRange(y + h, 0, TXT_SCREEN_H);
    x = LimitToRange(x, 0, TXT_SCREEN_W);
    y = LimitToRange(y, 0, TXT_SCREEN_H);

    for (y1=y; y1<y_end; ++y1)
    {
        for (x1=x; x1<x_end; ++x1)
        {
            UpdateCharacter(x1, y1);
        }
    }

    SDL_UnlockSurface(screenbuffer);

#ifdef SDL12
    SDL_UpdateRect(screenbuffer,
                   x * font->w, y * font->h,
                   (x_end - x) * font->w, (y_end - y) * font->h);
#else
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    // TODO: This is currently creating a new texture every time we render
    // the screen; find a more efficient way to do it.
    screentx = SDL_CreateTextureFromSurface(renderer, screenbuffer);

    SDL_RenderClear(renderer);
    GetDestRect(&rect);
    SDL_RenderCopy(renderer, screentx, NULL, &rect);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(screentx);
#endif
}

void TXT_UpdateScreen(void)
{
    TXT_UpdateScreenArea(0, 0, TXT_SCREEN_W, TXT_SCREEN_H);
}

#ifdef SDL12
void TXT_GetMousePosition(int *x, int *y)
{
#if SDL_VERSION_ATLEAST(1, 3, 0)
    SDL_GetMouseState(0, x, y);
#else
    SDL_GetMouseState(x, y);
#endif

    *x /= font->w;
    *y /= font->h;
}
#else
void TXT_GetMousePosition(int *x, int *y)
{
    int window_w, window_h;
    int origin_x, origin_y;

    SDL_GetMouseState(x, y);

    // Translate mouse position from 'pixel' position into character position.
    // We are working here in screen coordinates and not pixels, since this is
    // what SDL_GetWindowSize() returns; we must calculate and subtract the
    // origin position since we center the image within the window.
    SDL_GetWindowSize(TXT_SDLWindow, &window_w, &window_h);
    origin_x = (window_w - screen_image_w) / 2;
    origin_y = (window_h - screen_image_h) / 2;
    *x = ((*x - origin_x) * TXT_SCREEN_W) / screen_image_w;
    *y = ((*y - origin_y) * TXT_SCREEN_H) / screen_image_h;

    if (*x < 0)
    {
        *x = 0;
    }
    else if (*x >= TXT_SCREEN_W)
    {
        *x = TXT_SCREEN_W - 1;
    }
    if (*y < 0)
    {
        *y = 0;
    }
    else if (*y >= TXT_SCREEN_H)
    {
        *y = TXT_SCREEN_H - 1;
    }
}
#endif

//
// Translates the SDL key
//

#ifdef SDL12
static int TranslateKey(SDL_Keysym *sym)
{
    switch(sym->sym)
    {
        case SDLK_LEFT:        return OKEY_LEFTARROW;
        case SDLK_RIGHT:       return OKEY_RIGHTARROW;
        case SDLK_DOWN:        return OKEY_DOWNARROW;
        case SDLK_UP:          return OKEY_UPARROW;
        case SDLK_ESCAPE:      return OKEY_ESCAPE;
        case SDLK_RETURN:      return OKEY_ENTER;
        case SDLK_TAB:         return OKEY_TAB;
        case SDLK_F1:          return OKEY_F1;
        case SDLK_F2:          return OKEY_F2;
        case SDLK_F3:          return OKEY_F3;
        case SDLK_F4:          return OKEY_F4;
        case SDLK_F5:          return OKEY_F5;
        case SDLK_F6:          return OKEY_F6;
        case SDLK_F7:          return OKEY_F7;
        case SDLK_F8:          return OKEY_F8;
        case SDLK_F9:          return OKEY_F9;
        case SDLK_F10:         return OKEY_F10;
        case SDLK_F11:         return OKEY_F11;
        case SDLK_F12:         return OKEY_F12;

        case SDLK_BACKSPACE:   return OKEY_BACKSPACE;
        case SDLK_DELETE:      return OKEY_DEL;

        case SDLK_PAUSE:       return OKEY_PAUSE;

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
                               return OKEY_RSHIFT;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
                               return OKEY_RCTRL;

        case SDLK_LALT:
        case SDLK_RALT:
#if !SDL_VERSION_ATLEAST(1, 3, 0)
        case SDLK_LMETA:
        case SDLK_RMETA:
#endif
                               return OKEY_RALT;

        case SDLK_CAPSLOCK:    return OKEY_CAPSLOCK;
        case SDLK_SCROLLOCK:   return OKEY_SCRLCK;

        case SDLK_HOME:        return OKEY_HOME;
        case SDLK_INSERT:      return OKEY_INS;
        case SDLK_END:         return OKEY_END;
        case SDLK_PAGEUP:      return OKEY_PGUP;
        case SDLK_PAGEDOWN:    return OKEY_PGDN;

#ifdef SDL_HAVE_APP_KEYS
        case SDLK_APP1:        return OKEY_F1;
        case SDLK_APP2:        return OKEY_F2;
        case SDLK_APP3:        return OKEY_F3;
        case SDLK_APP4:        return OKEY_F4;
        case SDLK_APP5:        return OKEY_F5;
        case SDLK_APP6:        return OKEY_F6;
#endif

        default:               break;
    }

    // Returned value is different, depending on whether key mapping is
    // enabled.  Key mapping is preferable most of the time, for typing
    // in text, etc.  However, when we want to read raw keyboard codes
    // for the setup keyboard configuration dialog, we want the raw
    // key code.

    if (key_mapping)
    {
        // Unicode characters beyond the ASCII range need to be
        // mapped up into textscreen's Unicode range.

        if (sym->unicode < 128)
        {
            return sym->unicode;
        }
        else
        {
            return sym->unicode - 128 + TXT_UNICODE_BASE;
        }
    }
    else
    {
        // Keypad mapping is only done when we want a raw value:
        // most of the time, the keypad should behave as it normally
        // does.

        switch (sym->sym)
        {
            case SDLK_KP0:         return OKEYP_0;
            case SDLK_KP1:         return OKEYP_1;
            case SDLK_KP2:         return OKEYP_2;
            case SDLK_KP3:         return OKEYP_3;
            case SDLK_KP4:         return OKEYP_4;
            case SDLK_KP5:         return OKEYP_5;
            case SDLK_KP6:         return OKEYP_6;
            case SDLK_KP7:         return OKEYP_7;
            case SDLK_KP8:         return OKEYP_8;
            case SDLK_KP9:         return OKEYP_9;

            case SDLK_KP_PERIOD:   return OKEYP_PERIOD;
            case SDLK_KP_MULTIPLY: return OKEYP_MULTIPLY;
            case SDLK_KP_PLUS:     return OKEYP_PLUS;
            case SDLK_KP_MINUS:    return OKEYP_MINUS;
            case SDLK_KP_DIVIDE:   return OKEYP_DIVIDE;
            case SDLK_KP_EQUALS:   return OKEYP_EQUALS;
            case SDLK_KP_ENTER:    return OKEYP_ENTER;

            default:
                return tolower(sym->sym);
        }
    }
}

#else
// XXX: duplicate from doomtype.h
#define arrlen(array) (sizeof(array) / sizeof(*array))

static int TranslateKey(SDL_Keysym *sym)
{
    int scancode = sym->scancode;

    switch (scancode)
    {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return OKEY_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return OKEY_RSHIFT;

        case SDL_SCANCODE_LALT:
            return OKEY_LALT;

        case SDL_SCANCODE_RALT:
            return OKEY_RALT;

        default:
            // Just for I_Endoom
            if (scancode >= 0 /*&& scancode < arrlen(scancode_translate_table)*/)
            {
                return 1;
                //return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }
}
#endif

// Convert an SDL button index to textscreen button index.
//
// Note special cases because 2 == mid in SDL, 3 == mid in textscreen/setup

static int SDLButtonToTXTButton(int button)
{
    switch (button)
    {
        case SDL_BUTTON_LEFT:
            return TXT_MOUSE_LEFT;
        case SDL_BUTTON_RIGHT:
            return TXT_MOUSE_RIGHT;
        case SDL_BUTTON_MIDDLE:
            return TXT_MOUSE_MIDDLE;
        default:
            return TXT_MOUSE_BASE + button - 1;
    }
}

// Convert an SDL wheel motion to a textscreen button index.
#ifdef SDL20
static int SDLWheelToTXTButton(SDL_MouseWheelEvent *wheel)
{
    if (wheel->y <= 0)
    {
        return TXT_MOUSE_SCROLLDOWN;
    }
    else
    {
        return TXT_MOUSE_SCROLLUP;
    }
}
#endif

static int MouseHasMoved(void)
{
    static int last_x = 0, last_y = 0;
    int x, y;

    TXT_GetMousePosition(&x, &y);

    if (x != last_x || y != last_y)
    {
        last_x = x; last_y = y;
        return 1;
    }
    else
    {
        return 0;
    }
}

// Examine a key press/release and update the modifier key state
// if necessary.

static void UpdateModifierState(SDL_Keysym *sym, int pressed)
{
    txt_modifier_t mod;

    switch (sym->sym)
    {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            mod = TXT_MOD_SHIFT;
            break;

        case SDLK_LCTRL:
        case SDLK_RCTRL:
            mod = TXT_MOD_CTRL;
            break;

        case SDLK_LALT:
        case SDLK_RALT:
            mod = TXT_MOD_ALT;
            break;

        default:
            return;
    }

    if (pressed)
    {
        ++modifier_state[mod];
    }
    else
    {
        --modifier_state[mod];
    }
}

signed int TXT_GetChar(void)
{
    SDL_Event ev;

    while (SDL_PollEvent(&ev))
    {
        // If there is an event callback, allow it to intercept this
        // event.

        if (event_callback != NULL)
        {
            if (event_callback(&ev, event_callback_data))
            {
                continue;
            }
        }

        // Process the event.

        switch (ev.type)
        {
            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button < TXT_MAX_MOUSE_BUTTONS)
                {
                    return SDLButtonToTXTButton(ev.button.button);
                }
                break;
#ifdef SDL20
            case SDL_MOUSEWHEEL:
                return SDLWheelToTXTButton(&ev.wheel);
#endif
            case SDL_KEYDOWN:
                UpdateModifierState(&ev.key.keysym, 1);

                return TranslateKey(&ev.key.keysym);

            case SDL_KEYUP:
                UpdateModifierState(&ev.key.keysym, 0);
                break;

            case SDL_QUIT:
                // Quit = escape
                return 27;

            case SDL_MOUSEMOTION:
                if (MouseHasMoved())
                {
                    return 0;
                }

            default:
                break;
        }
    }

    return -1;
}

int TXT_GetModifierState(txt_modifier_t mod)
{
    if (mod < TXT_NUM_MODIFIERS)
    {
        return modifier_state[mod] > 0;
    }

    return 0;
}

static const char *SpecialKeyName(int key)
{
    switch (key)
    {
        case ' ':             return "SPACE";
        case OKEY_RIGHTARROW:  return "RIGHT";
        case OKEY_LEFTARROW:   return "LEFT";
        case OKEY_UPARROW:     return "UP";
        case OKEY_DOWNARROW:   return "DOWN";
        case OKEY_ESCAPE:      return "ESC";
        case OKEY_ENTER:       return "ENTER";
        case OKEY_TAB:         return "TAB";
        case OKEY_F1:          return "F1";
        case OKEY_F2:          return "F2";
        case OKEY_F3:          return "F3";
        case OKEY_F4:          return "F4";
        case OKEY_F5:          return "F5";
        case OKEY_F6:          return "F6";
        case OKEY_F7:          return "F7";
        case OKEY_F8:          return "F8";
        case OKEY_F9:          return "F9";
        case OKEY_F10:         return "F10";
        case OKEY_F11:         return "F11";
        case OKEY_F12:         return "F12";
        case OKEY_BACKSPACE:   return "BKSP";
        case OKEY_PAUSE:       return "PAUSE";
        case OKEY_EQUALS:      return "EQUALS";
        case OKEY_MINUS:      return "MINUS";
        case OKEY_RSHIFT:      return "SHIFT";
        case OKEY_RCTRL:       return "CTRL";
        case OKEY_RALT:        return "ALT";
        case OKEY_CAPSLOCK:    return "CAPS";
        case OKEY_SCRLCK:      return "SCRLCK";
	    case OKEY_HOME:       return "HOME";
        case OKEY_END:         return "END";
        case OKEY_PGUP:        return "PGUP";
        case OKEY_PGDN:        return "PGDN";
        case OKEY_INS:         return "INS";
        case OKEY_DEL:         return "DEL";
        // Just for I_Endoom
        case OKEY_PRINT:       return "PRTSC";
                 /*
        case OKEYP_0:          return "PAD0";
        case OKEYP_1:          return "PAD1";
        case OKEYP_2:          return "PAD2";
        case OKEYP_3:          return "PAD3";
        case OKEYP_4:          return "PAD4";
        case OKEYP_5:          return "PAD5";
        case OKEYP_6:          return "PAD6";
        case OKEYP_7:          return "PAD7";
        case OKEYP_8:          return "PAD8";
        case OKEYP_9:          return "PAD9";
        case OKEYP_UPARROW:    return "PAD_U";
        case OKEYP_DOWNARROW:  return "PAD_D";
        case OKEYP_LEFTARROW:  return "PAD_L";
        case OKEYP_RIGHTARROW: return "PAD_R";
        case OKEYP_MULTIPLY:   return "PAD*";
        case OKEYP_PLUS:       return "PAD+";
        case OKEYP_MINUS:      return "PAD-";
        case OKEYP_DIVIDE:     return "PAD/";
                   */

        default:              return NULL;
    }
}

void TXT_GetKeyDescription(int key, char *buf, size_t buf_len)
{
    const char *keyname;

    keyname = SpecialKeyName(key);

    if (keyname != NULL)
    {
        TXT_StringCopy(buf, keyname, buf_len);
    }
    else if (isprint(key))
    {
        TXT_snprintf(buf, buf_len, "%c", toupper(key));
    }
    else
    {
        TXT_snprintf(buf, buf_len, "??%i", key);
    }
}

// Searches the desktop screen buffer to determine whether there are any
// blinking characters.

int TXT_ScreenHasBlinkingChars(void)
{
    int x, y;
    unsigned char *p;

    // Check all characters in screen buffer

    for (y=0; y<TXT_SCREEN_H; ++y)
    {
        for (x=0; x<TXT_SCREEN_W; ++x) 
        {
            p = &screendata[(y * TXT_SCREEN_W + x) * 2];

            if (p[1] & 0x80)
            {
                // This character is blinking

                return 1;
            }
        }
    }

    // None found

    return 0;
}

// Sleeps until an event is received, the screen needs to be redrawn, 
// or until timeout expires (if timeout != 0)

void TXT_Sleep(Uint32 timeout)
{
    unsigned int start_time;

    if (TXT_ScreenHasBlinkingChars())
    {
        Uint32 time_to_next_blink;

        time_to_next_blink = BLINK_PERIOD - (SDL_GetTicks() % BLINK_PERIOD);

        // There are blinking characters on the screen, so we 
        // must time out after a while
       
        if (timeout == 0 || timeout > time_to_next_blink)
        {
            // Add one so it is always positive

            timeout = time_to_next_blink + 1;
        }
    }

    if (timeout == 0)
    {
        // We can just wait forever until an event occurs

        SDL_WaitEvent(NULL);
    }
    else
    {
        // Sit in a busy loop until the timeout expires or we have to
        // redraw the blinking screen

        start_time = SDL_GetTicks();

        while (SDL_GetTicks() < start_time + timeout)
        {
            if (SDL_PollEvent(NULL) != 0)
            {
                // Received an event, so stop waiting

                break;
            }

            // Don't hog the CPU

            SDL_Delay(1);
        }
    }
}

void TXT_EnableKeyMapping(int enable)
{
    key_mapping = enable;
}

void TXT_SetWindowTitle(char *title)
{
#ifdef SDL12
	SDL_WM_SetCaption(title, NULL);
#else
    SDL_SetWindowTitle(TXT_SDLWindow, title);
#endif
}

void TXT_SDL_SetEventCallback(TxtSDLEventCallbackFunc callback, void *user_data)
{
    event_callback = callback;
    event_callback_data = user_data;
}

// Safe string functions.

void TXT_StringCopy(char *dest, const char *src, size_t dest_len)
{
    if (dest_len < 1)
    {
        return;
    }

    dest[dest_len - 1] = '\0';
    strncpy(dest, src, dest_len - 1);
}

void TXT_StringConcat(char *dest, const char *src, size_t dest_len)
{
    size_t offset;

    offset = strlen(dest);
    if (offset > dest_len)
    {
        offset = dest_len;
    }

    TXT_StringCopy(dest + offset, src, dest_len - offset);
}

// On Windows, vsnprintf() is _vsnprintf().
#ifdef _WIN32
#if _MSC_VER < 1400 /* not needed for Visual Studio 2008 */
#define vsnprintf _vsnprintf
#endif
#endif

// Safe, portable vsnprintf().
int TXT_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
{
    size_t result;

    if (buf_len < 1)
    {
        return 0;
    }

    // Windows (and other OSes?) has a vsnprintf() that doesn't always
    // append a trailing \0. So we must do it, and write into a buffer
    // that is one byte shorter; otherwise this function is unsafe.
    result = vsnprintf(buf, buf_len, s, args);

    // If truncated, change the final char in the buffer to a \0.
    // A negative result indicates a truncated buffer on Windows.
    if (result < 0 || result >= buf_len)
    {
        buf[buf_len - 1] = '\0';
        result = buf_len - 1;
    }

    return result;
}

// Safe, portable snprintf().
size_t TXT_snprintf(char *buf, size_t buf_len, const char *s, ...)
{
    va_list args;
    size_t result;
    va_start(args, s);
    result = TXT_vsnprintf(buf, buf_len, s, args);
    va_end(args);
    return result;
}

//#endif // SDL20
