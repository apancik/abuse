/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *  Copyright (c) 2024 Andrej Pancik
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <cstring>

#include "SDL.h"

#include "common.h"
#include "video.h"
#include "image.h"
#include "setup.h"
#include "errorui.h"

// Core SDL components
SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Surface *surface = nullptr; // 8-bit paletted surface for game rendering
SDL_Surface *screen = nullptr;  // 32-bit RGB surface for final display
SDL_Texture *texture = nullptr; // GPU texture for hardware-accelerated rendering
image *main_screen = nullptr;   // Game's primary drawing surface

// Factors for converting window coordinates to game coordinates
int mouse_xpad = 0;   // Horizontal padding for letterboxing
int mouse_ypad = 0;   // Vertical padding for letterboxing
int mouse_xscale = 1; // 16.16 fixed point horizontal scale factor
int mouse_yscale = 1; // 16.16 fixed point vertical scale factor

// Virtual screen dimensions (game coordinates)
int xres = 0;
int yres = 0;

extern palette *lastl;
extern Settings settings;

//
// Calculate mouse scaling factors and window aspect ratio
// This needs to be exposed for event handling
//
void handle_window_resize()
{
    // Get current viewport and scaling information
    SDL_Rect viewport;
    float scale_x, scale_y;

    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetScale(renderer, &scale_x, &scale_y);

    // Maintain aspect ratio by adjusting window width
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    float target_aspect = static_cast<float>(xres) / yres;
    if (window_width != static_cast<int>(window_height * target_aspect))
    {
        window_width = static_cast<int>(window_height * target_aspect);
        SDL_SetWindowSize(window, window_width, window_height);
    }

    // Convert logical coordinates to actual screen pixels
    int width = static_cast<int>(viewport.w * scale_x);
    int height = static_cast<int>(viewport.h * scale_y);

    // Set up coordinate conversion for mouse input
    // Uses 16.16 fixed point to maintain precision
    mouse_xscale = (width << 16) / xres;
    mouse_yscale = (height << 16) / yres;

    mouse_xpad = static_cast<int>(viewport.x * scale_x);
    mouse_ypad = static_cast<int>(viewport.y * scale_y);
}

//
// Initialize video subsystem
//
void set_mode(int argc, char **argv)
{
    try
    {
        // Auto-detect screen dimensions if not specified in settings
        if (settings.screen_width == 0 || settings.screen_height == 0)
        {
            SDL_DisplayMode current_display;
            if (SDL_GetCurrentDisplayMode(0, &current_display) != 0)
            {
                throw std::runtime_error(SDL_GetError());
            }

            // Use full screen dimensions or virtual resolution adjusted for display's aspect ratio
            if (settings.fullscreen)
            {
                settings.screen_width = current_display.w;
                settings.screen_height = current_display.h;
            }
            else if (settings.screen_width != 0)
            {
                // Use user-defined screen dimensions
                settings.screen_width = settings.screen_width;
                settings.screen_height = (int)(settings.screen_width / ((float)current_display.w / current_display.h));
            }
            else
            {
                settings.screen_width = settings.virtual_width;
                settings.screen_height = (int)(settings.virtual_width / ((float)current_display.w / current_display.h));
            }
        }

        // Calculate virtual resolution preserving aspect ratio
        xres = settings.virtual_width;
        yres = settings.virtual_height ? settings.virtual_height : (int)(xres / ((float)settings.screen_width / settings.screen_height));

        // Set up window flags based on display settings
        uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
        if (settings.fullscreen == 1)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        else if (settings.fullscreen == 2)
            flags |= SDL_WINDOW_FULLSCREEN;
        if (settings.borderless)
            flags |= SDL_WINDOW_BORDERLESS;

        // Initialize rendering pipeline:
        // Window -> Renderer -> Texture -> 32-bit screen surface -> 8-bit game surface
        window = SDL_CreateWindow("Abuse",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  settings.screen_width,
                                  settings.screen_height,
                                  flags);
        if (!window)
        {
            throw std::runtime_error(SDL_GetError());
        }

        // Try hardware acceleration first, fall back to software rendering
        uint32_t render_flags = SDL_RENDERER_ACCELERATED;
        if (settings.vsync)
            render_flags |= SDL_RENDERER_PRESENTVSYNC;

        renderer = SDL_CreateRenderer(window, -1, render_flags);
        if (!renderer)
        {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            if (!renderer)
            {
                throw std::runtime_error(SDL_GetError());
            }
        }

        SDL_RenderSetLogicalSize(renderer, xres, yres);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,
                    settings.linear_filter ? "1" : "0");

        main_screen = new image(ivec2(xres, yres), nullptr, 2);
        if (!main_screen)
        {
            throw std::runtime_error("Unable to create screen image");
        }
        main_screen->clear();

        surface = SDL_CreateRGBSurface(0, xres, yres, 8, 0, 0, 0, 0);
        if (!surface)
        {
            throw std::runtime_error(SDL_GetError());
        }

        screen = SDL_CreateRGBSurface(0, xres, yres, 32, 0, 0, 0, 0);
        if (!screen)
        {
            throw std::runtime_error(SDL_GetError());
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    xres, yres);
        if (!texture)
        {
            throw std::runtime_error(SDL_GetError());
        }

        handle_window_resize();
        SDL_ShowCursor(0);
    }
    catch (const std::exception &e)
    {
        show_startup_error("Video initialization failed: %s", e.what());
        exit(1);
    }
}

//
// Update video settings (fullscreen, scaling)
//
void toggle_fullscreen()
{
    // Cycle through fullscreen modes: windowed -> fullscreen desktop -> fullscreen
    settings.fullscreen = (settings.fullscreen + 1) % 3;

    uint32_t flags = 0;
    if (settings.fullscreen == 1)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    else if (settings.fullscreen == 2)
        flags |= SDL_WINDOW_FULLSCREEN;
    if (settings.borderless)
        flags |= SDL_WINDOW_BORDERLESS;

    SDL_SetWindowFullscreen(window, flags);
    handle_window_resize();
}

//
// Cleanup video subsystem
//
void close_graphics()
{
    delete lastl;
    lastl = nullptr;

    if (surface)
    {
        SDL_FreeSurface(surface);
        surface = nullptr;
    }

    if (screen)
    {
        SDL_FreeSurface(screen);
        screen = nullptr;
    }

    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }

    delete main_screen;
    main_screen = nullptr;

    SDL_DestroyRenderer(renderer);
    renderer = nullptr;

    SDL_DestroyWindow(window);
    window = nullptr;
}

//
// Draw a portion of an image to the screen
//
void put_part_image(image *im, int x, int y, int x1, int y1, int x2, int y2)
{
    // Skip if completely off screen
    if (y > yres || x > xres)
        return;

    // Validate coordinates
    CHECK(x1 >= 0 && x2 >= x1 && y1 >= 0 && y2 >= y1);

    // Clip drawing region to screen boundaries
    if (x < 0)
    {
        x1 += -x;
        x = 0;
    }

    int xe = (x + (x2 - x1) >= xres) ? xres - x + x1 - 1 : x2;

    if (y < 0)
    {
        y1 += -y;
        y = 0;
    }

    int ye = (y + (y2 - y1) >= yres) ? yres - y + y1 - 1 : y2;

    // Nothing to draw after clipping
    if (x1 >= xe || y1 >= ye)
        return;

    // Lock surface for direct pixel access if needed
    if (SDL_MUSTLOCK(surface))
    {
        SDL_LockSurface(surface);
    }

    // Copy image data line by line to surface
    uint8_t *dpixel = static_cast<uint8_t *>(surface->pixels);
    dpixel += y * surface->pitch + x;

    for (int i = 0; i < ye - y1; i++)
    {
        std::memcpy(dpixel, im->scan_line(y1 + i) + x1, xe - x1);
        dpixel += surface->pitch;
    }

    if (SDL_MUSTLOCK(surface))
    {
        SDL_UnlockSurface(surface);
    }
}

//
// Load and apply a palette
//
void palette::load()
{
    if (lastl)
        delete lastl;
    lastl = copy();

    // Force to only 256 colours
    if (ncolors > 256)
        ncolors = 256;

    // Set up SDL color palette
    std::array<SDL_Color, 256> colors{};
    for (int i = 0; i < ncolors; i++)
    {
        colors[i] = {
            static_cast<uint8_t>(red(i)),
            static_cast<uint8_t>(green(i)),
            static_cast<uint8_t>(blue(i)),
            255};
    }

    // Update palette and redraw
    SDL_SetPaletteColors(surface->format->palette, colors.data(), 0, ncolors);
    update_window_done();
}

void palette::load_nice()
{
    load();
}

//
// Update the window with current screen contents
//
void update_window_done()
{
    // Convert paletted surface to 32-bit RGB for display
    SDL_BlitSurface(surface, nullptr, screen, nullptr);

    // Update GPU texture and render to display
    SDL_UpdateTexture(texture, nullptr, screen->pixels, screen->pitch);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}