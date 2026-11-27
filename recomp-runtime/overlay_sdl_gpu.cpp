#include "overlay_sdl_gpu.h"
#include "overlay_imgui_panel.h"

#include "imgui.h"
#include "backends/imgui_impl_sdlgpu3.h"

#include <SDL3/SDL_timer.h>

#include <cstdio>

#ifdef RECOMP_IOS_LIFECYCLE
#include "input_model.h"
#include <SDL3/SDL_gamepad.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <fstream>
#include <string>
#include <cstring>
#include <SDL3/SDL_filesystem.h>
extern "C" SDL_Gamepad *recomp_ios_primary_pad(void);
#include "ios_guide.inc"
extern "C" void recomp_ios_touch_set_pad(const RecompInputGamepad *pad);
#endif

namespace {

#ifdef RECOMP_IOS_LIFECYCLE
struct TouchButton {
    const char *label;
    float x, y;
    uint16_t digital;
    int analog;
};

constexpr TouchButton buttons[] = {
    {"UP", .15f, .64f, 0x0001, -1}, {"DOWN", .15f, .90f, 0x0002, -1},
    {"LEFT", .09f, .77f, 0x0004, -1}, {"RIGHT", .21f, .77f, 0x0008, -1},
    {"START", .55f, .91f, 0x0010, -1}, {"BACK", .45f, .91f, 0x0020, -1},
    {"A", .85f, .90f, 0, 0}, {"B", .91f, .77f, 0, 1},
    {"X", .79f, .77f, 0, 2}, {"Y", .85f, .64f, 0, 3},
    {"LB", .10f, .24f, 0, 4}, {"RB", .90f, .24f, 0, 5},
    {"LT", .10f, .10f, 0, 6}, {"RT", .90f, .10f, 0, 7},
};
struct Finger { SDL_TouchID device; SDL_FingerID id; float x, y; };
std::vector<Finger> fingers;
RecompInputGamepad touch_pad{};
bool touch_visible;
float touch_aspect = 2868.0f / 1320.0f;
constexpr float touch_radius = .064f;

void updateTouchPad()
{
    touch_pad = {};
    for (const auto &finger : fingers) {
        for (const auto &button : buttons) {
            const float dx = (finger.x - button.x) * touch_aspect;
            const float dy = finger.y - button.y;
            if (dx * dx + dy * dy > touch_radius * touch_radius) continue;
            touch_pad.buttons |= button.digital;
            if (button.analog >= 0) touch_pad.analog_buttons[button.analog] = 255;
        }
    }
    recomp_ios_touch_set_pad(&touch_pad);
}

void drawTouchPad(ImVec2 size)
{
    touch_aspect = size.x / size.y;
    ImDrawList *draw = ImGui::GetForegroundDrawList();
    draw->AddText(ImGui::GetFont(), size.y*.032f, {size.x*.46f,size.y*.04f}, IM_COL32_WHITE, "Guide");
    for (const auto &button : buttons) {
        const ImVec2 center(button.x * size.x, button.y * size.y);
        const float radius = touch_radius * size.y;
        const bool held = button.analog >= 0 ? touch_pad.analog_buttons[button.analog] != 0
                                             : (touch_pad.buttons & button.digital) != 0;
        draw->AddCircleFilled(center, radius, held ? IM_COL32(90, 130, 180, 200) : IM_COL32(12, 18, 28, 120));
        draw->AddCircle(center, radius, IM_COL32(235, 240, 250, 160));
        const float font_size = size.y * .025f;
        const ImVec2 label = ImGui::GetFont()->CalcTextSizeA(font_size, 1000.f, 0.f, button.label);
        draw->AddText(ImGui::GetFont(), font_size, ImVec2(center.x - label.x * .5f,
            center.y - label.y * .5f), IM_COL32_WHITE, button.label);
    }
}
#endif

bool ready;
bool failed;
SDL_GPUDevice *owner;
Uint64 last_ticks;
Uint32 styled_height;

bool start(SDL_GPUDevice *device, SDL_Window *window, Uint32 height)
{
    if (ready) return true;
    if (failed) return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    recomp_overlay_imgui_style(static_cast<float>(height));
    styled_height = height;

    ImGui_ImplSDLGPU3_InitInfo info{};
    info.Device = device;
    info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
    info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    if (!ImGui_ImplSDLGPU3_Init(&info)) {
        std::fprintf(stderr, "[overlay] ImGui SDL GPU backend failed to start\n");
        ImGui::DestroyContext();
        failed = true;
        return false;
    }
    owner = device;
    ready = true;
    last_ticks = SDL_GetTicks();
    return true;
}

// The window is resizable: size the panel for the new height. The style and
// font start over so the scale is not applied twice; the backend rebuilds
// the font texture on the next frame.
void restyle(Uint32 height)
{
    if (height == styled_height) return;
    styled_height = height;
    ImGui::GetStyle() = ImGuiStyle();
    ImGui::GetIO().Fonts->Clear();
    recomp_overlay_imgui_style(static_cast<float>(height));
    ImGui_ImplSDLGPU3_DestroyFontsTexture();
}

}  // namespace

#ifdef RECOMP_IOS_LIFECYCLE
void recomp_ios_touch_reset(void)
{
    fingers.clear();
    updateTouchPad();
}

void recomp_ios_touch_refresh(void)
{
    pollGuide(recomp_ios_primary_pad());
    const bool visible = !SDL_HasGamepad();
    if (visible != touch_visible) recomp_ios_touch_reset();
    touch_visible = visible;
}

void recomp_ios_touch_event(const SDL_Event &event)
{
    const auto &touch = event.tfinger;
    if (guideOpen) {
        if (event.type == SDL_EVENT_FINGER_DOWN) guideTouch(touch.x,touch.y);
        return;
    }
    if (!touch_visible) return;
    if (event.type == SDL_EVENT_FINGER_DOWN && touch.x>.43f && touch.x<.57f && touch.y<.11f) {
        openGuide(true);
        return;
    }
    const auto finger = std::find_if(fingers.begin(), fingers.end(), [&](const Finger &entry) {
        return entry.device == touch.touchID && entry.id == touch.fingerID;
    });
    if (event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED) {
        if (finger != fingers.end()) fingers.erase(finger);
    } else if (finger != fingers.end()) {
        finger->x = touch.x;
        finger->y = touch.y;
    } else if (event.type == SDL_EVENT_FINGER_DOWN) {
        fingers.push_back({touch.touchID, touch.fingerID, touch.x, touch.y});
    }
    updateTouchPad();
}
#endif

void recomp_overlay_sdl_gpu_draw(SDL_GPUDevice *device, SDL_Window *window,
    SDL_GPUCommandBuffer *command_buffer, SDL_GPUTexture *target,
    Uint32 width, Uint32 height, const RecompOverlayText *text)
{
    bool show_touch = false;
#ifdef RECOMP_IOS_LIFECYCLE
    show_touch = touch_visible || guideOpen;
#endif
    if (device == nullptr || window == nullptr || command_buffer == nullptr || target == nullptr ||
        width == 0u || height == 0u || (!show_touch && recomp_overlay_empty(text))) {
        return;
    }
    if (!start(device, window, height)) return;
    restyle(height);

    ImGuiIO &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
    const Uint64 now = SDL_GetTicks();
    io.DeltaTime = now > last_ticks ? static_cast<float>(now - last_ticks) / 1000.0f : 1.0f / 60.0f;
    last_ticks = now;

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui::NewFrame();
    if (!recomp_overlay_empty(text)) {
        recomp_overlay_imgui_build(*text, io.DisplaySize.x, io.DisplaySize.y);
    }
#ifdef RECOMP_IOS_LIFECYCLE
    if (guideOpen) drawGuide(io.DisplaySize);
    else if (show_touch) drawTouchPad(io.DisplaySize);
#endif
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    if (draw_data == nullptr || draw_data->CmdListsCount == 0) return;

    Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);
    SDL_GPUColorTargetInfo color{};
    color.texture = target;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(command_buffer, &color, 1, nullptr);
    if (pass == nullptr) return;
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, pass);
    SDL_EndGPURenderPass(pass);
}

void recomp_overlay_sdl_gpu_shutdown(void)
{
    if (!ready) return;
    SDL_WaitForGPUIdle(owner);
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui::DestroyContext();
    ready = false;
}
