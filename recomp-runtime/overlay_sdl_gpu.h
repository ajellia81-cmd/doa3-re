#ifndef DOA3_RECOMP_OVERLAY_SDL_GPU_H
#define DOA3_RECOMP_OVERLAY_SDL_GPU_H

#include <SDL3/SDL_gpu.h>

#include "overlay_text.h"

#ifdef RECOMP_IOS_LIFECYCLE
struct RecompD3dPresenterDrawCommand;
bool recomp_ios_wants_icons();
void recomp_ios_observe_icon(int row, const RecompD3dPresenterDrawCommand &draw,
    int left, int top, int width, int height);
void recomp_ios_finish_icons();
#include <SDL3/SDL_events.h>
bool recomp_ios_smooth(bool supported);
bool recomp_ios_guide_open();
void recomp_ios_touch_refresh(void);
void recomp_ios_touch_reset(void);
void recomp_ios_touch_event(const SDL_Event &event);
#endif

/* Draws the runtime's overlay text (overlay_text.h) with Dear ImGui into a
   colour target of the window's swapchain format, after the game's frame.
   Created on first use; nothing is drawn while the overlay is empty. */
void recomp_overlay_sdl_gpu_draw(SDL_GPUDevice *device, SDL_Window *window,
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target,
    Uint32 width, Uint32 height, const RecompOverlayText *text);
void recomp_overlay_sdl_gpu_shutdown(void);

#endif
