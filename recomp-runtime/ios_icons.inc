// Each row must come from the same frame. Check the distinctive four face
// button colours before adopting the two shoulder and two trigger images.
namespace {
constexpr int iconSide=32;
using IconPixels=std::array<uint32_t,iconSide*iconSide>;
struct GuideIcons { std::array<IconPixels,8> pixels{}; std::array<float,8> aspect{}; };
GuideIcons guideIcons;
bool haveIcons;
bool wantsWideCapture = true;
// The menu capture has already been composited over a charcoal panel.
void maskIconBackground(IconPixels &icon) {
    std::array<int, 4> corners{};
    for (int i = 0; i < 4; ++i) {
        const int x = (i & 1) ? iconSide - 1 : 0;
        const int y = (i & 2) ? iconSide - 1 : 0;
        corners[i] = int(icon[y * iconSide + x] & 255);
    }
    std::sort(corners.begin(), corners.end());
    const int background = (corners[1] + corners[2]) / 2;
    std::array<int, iconSide * iconSide> queue{};
    std::array<bool, iconSide * iconSide> visited{};
    int head = 0, tail = 0;
    auto visit = [&](int index) {
        if (visited[index]) return;
        visited[index] = true;
        const auto color = icon[index];
        int distance = 0;
        for (int shift : {0, 8, 16})
            distance = std::max(distance, std::abs(int((color >> shift) & 255) - background));
        if (distance > 20) return;
        const int alpha = std::clamp((distance - 7) * 255 / 13, 0, 255);
        icon[index] = (color & 0xffffffu) | (uint32_t(alpha) << 24);
        if (distance <= 14) queue[tail++] = index;
    };
    for (int i = 0; i < iconSide; ++i) {
        visit(i); visit((iconSide - 1) * iconSide + i);
        visit(i * iconSide); visit(i * iconSide + iconSide - 1);
    }
    while (head < tail) {
        const int index = queue[head++];
        if (index % iconSide) visit(index - 1);
        if (index % iconSide < iconSide - 1) visit(index + 1);
        if (index >= iconSide) visit(index - iconSide);
        if (index + iconSide < iconSide * iconSide) visit(index + iconSide);
    }
}
void loadIcons() {
    static bool tried;
    if (tried) return;
    tried=true;
    const auto path=settingsFile();
    if (path.empty()) return;
    std::ifstream in(path+".button-icons-v6",std::ios::binary);
    const bool wideCache = in.is_open();
    if (!wideCache) in.open(path+".button-icons-v5",std::ios::binary);
    GuideIcons saved;
    if (!in.read(reinterpret_cast<char*>(&saved),sizeof saved)) return;
    for(float a:saved.aspect) if (!(a>=.4f && a<=2.1f)) return;
    guideIcons=saved;
    wantsWideCapture = !wideCache;
    for (auto &icon : guideIcons.pixels) maskIconBackground(icon);
    haveIcons=true;
}
void drawIcon(ImDrawList *draw,int index,ImVec2 origin,float height) {
    const float width=height*guideIcons.aspect[index];
    const auto &pixels=guideIcons.pixels[index];
    for(int y=0;y<iconSide;++y) for(int x=0;x<iconSide;) {
        const auto colour=pixels[y*iconSide+x];
        int end=x+1;
        while(end<iconSide && pixels[y*iconSide+end]==colour) ++end;
        if(colour>>24) draw->AddRectFilled({origin.x+x*width/iconSide,origin.y+y*height/iconSide},
            {origin.x+end*width/iconSide,origin.y+(y+1)*height/iconSide},colour);
        x=end;
    }
}
}
bool recomp_ios_wants_icons()
{
    loadIcons();
    return wantsWideCapture;
}
// Validate four aligned face buttons before adopting any image from a frame.
void recomp_ios_read_button_strip(const uint8_t *bgra, unsigned width, unsigned height,
    unsigned stride, unsigned top, unsigned frameHeight)
{
    if (!wantsWideCapture || !bgra || width<16 || height<16) return;
    const unsigned radius = std::max(1u,frameHeight/65);
    const unsigned halfHeight = std::max(1u,frameHeight/46);
    const auto pixel = [=](unsigned x,unsigned y) { return bgra+4*(y*stride+x); };
    int yOffset=0, bestOffset=0;
    const auto rowY = [&](unsigned row) {
        return int(std::lround((.190+.071*row)*frameHeight))-int(top)+yOffset;
    };

    unsigned bestScore=0, centerX=0;
    const int range=int(frameHeight/50);
    for (yOffset=-range; yOffset<=range; yOffset+=std::max(1,range/3)) {
    if (rowY(0)<int(halfHeight) || rowY(7)+int(halfHeight)>=int(height)) continue;
    for (unsigned x=radius; x+radius<width; x+=4) {
        unsigned score=0;
        bool valid=true;
        for (unsigned row=0; row<4; ++row) {
            unsigned matches=0;
            for (int y=rowY(row)-int(radius); y<=rowY(row)+int(radius); y+=3) {
                for (unsigned sx=x-radius; sx<=x+radius; sx+=3) {
                    auto p=pixel(sx,y);
                    int b=p[0],g=p[1],r=p[2];
                    const bool colors[] = {g>b+37 && g>r+3, r>g+43 && r>b+43,
                        b>r+40 && b>g+13, r>b+50 && g>b+33 && r<g+65};
                    matches+=colors[row];
                }
            }
            if (matches<(radius*radius)/16) { valid=false; break; }
            score+=matches;
        }
        if (valid && score>bestScore) { bestScore=score; centerX=x; bestOffset=yOffset; }
    }
    }
    yOffset=bestOffset;
    const unsigned halfWidth = (halfHeight * 3 + 1) / 2;
    if (!bestScore || centerX<halfWidth || centerX+halfWidth>=width) return;
    for (unsigned row=0; row<8; ++row) {
        guideIcons.aspect[row]=float(halfWidth)/float(halfHeight);
        auto &icon = guideIcons.pixels[row];
        for (int y=0; y<iconSide; ++y) {
            for (int x=0; x<iconSide; ++x) {
                const unsigned srcX=centerX-halfWidth+x*2*halfWidth/iconSide;
                const unsigned srcY=rowY(row)-halfHeight+y*2*halfHeight/iconSide;
                auto p=pixel(srcX,srcY);
                icon[y*iconSide+x]=IM_COL32(p[2],p[1],p[0],255);
            }
        }
    }
    wantsWideCapture = false;
    GuideIcons raw = guideIcons;
    for (auto &icon : guideIcons.pixels) maskIconBackground(icon);
    haveIcons=true;
    std::fprintf(stderr,"[guide] loaded Xbox button art from Button Config\n");
    const auto path=settingsFile();
    if (path.empty()) return;
    const auto temp=path+".icons.tmp";
    std::ofstream out(temp,std::ios::binary);
    out.write(reinterpret_cast<const char*>(&raw),sizeof raw);
    out.close();
    if (out) std::rename(temp.c_str(),(path+".button-icons-v6").c_str());
}
