// Included by the SDL overlay after ImGui has been declared.
namespace {
struct GuideSettings {
    bool smooth = true;
    bool fps = false;
    int binding[8] = {0,1,2,3,4,5,6,7};
};
GuideSettings settings;
bool loaded, guideOpen, bindingPage, waitForButton, highRefresh;
int selection;
unsigned lastKeys, suppress;
const char *const targets[] = {"A","B","X","Y","Black","White","L trigger","R trigger"};
const char *const sources[] = {"South","East","West","North","RB","LB","LT","RT"};

std::string settingsFile()
{
    char *dir = SDL_GetPrefPath("doa3re","DOA3");
    std::string path = dir ? std::string(dir)+"guide.cfg" : std::string();
    SDL_free(dir);
    return path;
}
void loadGuide()
{
    if (loaded) return;
    loaded = true;
    std::ifstream in(settingsFile());
    GuideSettings candidate;
    int version, flag;
    if (!(in >> version >> flag) || (version != 1 && version != 2) || (flag != 0 && flag != 1)) return;
    unsigned used = 0;
    for (int &key : candidate.binding) {
        if (!(in >> key) || key < 0 || key >= 8 || (used & (1u << key))) return;
        used |= 1u << key;
    }
    candidate.smooth = flag != 0;
    if (version==2 && !(in >> candidate.fps)) return;
    settings = candidate;
}
void saveGuide()
{
    const auto path = settingsFile();
    if (path.empty()) return;
    const auto tmp = path+".tmp";
    std::ofstream out(tmp);
    out << 2 << ' ' << int(settings.smooth);
    for (int key : settings.binding) out << ' ' << key;
    out << ' ' << settings.fps << '\n';
    out.close();
    if (out) std::rename(tmp.c_str(),path.c_str());
}
int guideRows() { return bindingPage ? 10 : 4; }
void openGuide(bool open)
{
    guideOpen = open;
    if (!open) suppress = lastKeys;
    bindingPage = waitForButton = false;
    selection = 0;
    recomp_ios_touch_reset();
}
void activateGuide()
{
    if (!bindingPage) {
        if (selection == 0 && highRefresh) { settings.smooth = !settings.smooth; saveGuide(); }
        if (selection == 1) { settings.fps=!settings.fps; saveGuide(); }
        if (selection == 2) { bindingPage = true; selection = 0; }
        else if (selection == 3) openGuide(false);
    } else if (selection < 8) {
        waitForButton = true;
    } else if (selection == 8) {
        for (int i=0;i<8;++i) settings.binding[i]=i;
        saveGuide();
    } else { bindingPage = false; selection = 2; }
}
void pollGuide(SDL_Gamepad *pad)
{
    loadGuide();
    unsigned keys=0;
    const SDL_GamepadButton physical[] = {SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST,SDL_GAMEPAD_BUTTON_NORTH,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER};
    if (pad) {
        for (int i=0;i<6;++i) if (SDL_GetGamepadButton(pad,physical[i])) keys |= 1u<<i;
        if (SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>16000) keys |= 1u<<7;
        if (SDL_GetGamepadAxis(pad,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>16000) keys |= 1u<<6;
        if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_GUIDE)) keys |= 1u<<8;
        if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_UP)) keys |= 1u<<9;
        if (SDL_GetGamepadButton(pad,SDL_GAMEPAD_BUTTON_DPAD_DOWN)) keys |= 1u<<10;
    }
    const unsigned press = keys & ~lastKeys;
    lastKeys = keys;
    suppress &= keys;
    if (press & (1u<<8)) { openGuide(!guideOpen); return; }
    if (!guideOpen) return;
    if (waitForButton) {
        for (int i=0;i<8;++i) if (press & (1u<<i)) {
            auto other=std::find(settings.binding,settings.binding+8,i);
            std::swap(*other,settings.binding[selection]);
            waitForButton=false;
            saveGuide();
            break;
        }
        return;
    }
    if (press & (1u<<9)) selection=(selection+guideRows()-1)%guideRows();
    if (press & (1u<<10)) selection=(selection+1)%guideRows();
    if (press & 1u) activateGuide();
    else if (press & 2u) {
        if (bindingPage) { bindingPage=false; selection=2; }
        else openGuide(false);
    }
}
float rowHeight() { return .71f / guideRows(); }
void guideTouch(float x,float y)
{
    if (waitForButton) { waitForButton=false; return; }
    if (x<.22f || x>.78f || y<.18f || y>=.89f) return;
    selection=int((y-.18f)/rowHeight());
    if (selection < guideRows()) activateGuide();
}
}
#include "ios_icons.inc"
namespace {
void drawGuide(ImVec2 size)
{
    auto *draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled({0,0},size,IM_COL32(4,9,16,235));
    float font=size.y*.045f;
    draw->AddText(ImGui::GetFont(),font,{size.x*.23f,size.y*.09f},IM_COL32_WHITE,
        bindingPage ? "Button mapping" : "Dead or Alive 3");
    for (int i=0;i<guideRows();++i) {
        float y=size.y*(.18f+rowHeight()*i);
        if (selection==i) draw->AddRectFilled({size.x*.22f,y},{size.x*.78f,y+size.y*rowHeight()},IM_COL32(33,69,108,255));
        std::string title;
        if (bindingPage && i<8)
            title=std::string(targets[i])+"     "+((waitForButton && selection==i)?"Press a controller button":sources[settings.binding[i]]);
        else if (bindingPage) title=i==8?"Restore defaults":"Back";
        else if (i==0) title=std::string("Smooth Motion 120 fps     ")+(highRefresh?(settings.smooth?"On":"Off"):"Unavailable");
        else if (i==1) title=std::string("Show FPS     ")+(settings.fps?"On":"Off");
        else title=i==2?"Button mapping":"Resume game";
        float labelX=size.x*.24f;
        if (bindingPage && i<8) {
            loadIcons();
            if (haveIcons) {
                const int row=i==4?5:i==5?4:i;
                drawIcon(draw,row,{labelX,y+size.y*.006f},size.y*.052f);
                labelX+=size.x*.06f;
            }
        }
        draw->AddText(ImGui::GetFont(),font,{labelX,y+size.y*.008f},IM_COL32_WHITE,title.c_str());
    }
}
}
extern "C" void recomp_ios_map_buttons(RecompInputGamepad *pad)
{
    loadGuide();
    if (guideOpen || suppress) { *pad = {}; return; }
    uint8_t raw[8];
    std::memcpy(raw,pad->analog_buttons,sizeof raw);
    for (int i=0;i<8;++i) pad->analog_buttons[i]=raw[settings.binding[i]];
}
bool recomp_ios_guide_open() { return guideOpen; }
bool recomp_ios_smooth(bool supported) { loadGuide(); highRefresh=supported; return supported && settings.smooth; }
