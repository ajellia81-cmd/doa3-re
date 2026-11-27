// Each row must come from the same frame. Check the distinctive four face
// button colours before adopting the two shoulder and two trigger images.
namespace {
constexpr int iconSide=32;
using IconPixels=std::array<uint32_t,iconSide*iconSide>;
struct GuideIcons { std::array<IconPixels,8> pixels{}; std::array<float,8> aspect{}; };
GuideIcons guideIcons;
bool haveIcons;
void loadIcons() {
    static bool tried;
    if (tried) return;
    tried=true;
    const auto path=settingsFile();
    if (path.empty()) return;
    std::ifstream in(path+".button-icons-v4",std::ios::binary);
    GuideIcons saved;
    if (!in.read(reinterpret_cast<char*>(&saved),sizeof saved)) return;
    for(float a:saved.aspect) if (!(a>=.4f && a<=2.1f)) return;
    guideIcons=saved;
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
    return !haveIcons;
}
// Validate four aligned face buttons before adopting any image from a frame.
void recomp_ios_read_button_strip(const uint8_t *bgra, unsigned width, unsigned height,
    unsigned top, unsigned frameHeight)
{
    if (haveIcons || !bgra || width<16 || height<16) return;
    const unsigned radius = std::max(1u,frameHeight/65);
    const unsigned halfHeight = std::max(1u,frameHeight/46);
    const auto pixel = [=](unsigned x,unsigned y) { return bgra+4*(y*width+x); };
    const auto rowY = [=](unsigned row) {
        return int(std::lround((.190+.071*row)*frameHeight))-int(top);
    };
    if (rowY(0)<int(halfHeight) || rowY(7)+int(halfHeight)>=int(height)) return;

    unsigned bestScore=0, centerX=0;
    for (unsigned x=radius; x+radius<width; x+=2) {
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
        if (valid && score>bestScore) { bestScore=score; centerX=x; }
    }
    const unsigned halfWidth = halfHeight;
    if (!bestScore || centerX<halfWidth || centerX+halfWidth>=width) return;
    for (unsigned row=0; row<8; ++row) {
        guideIcons.aspect[row]=1.0f;
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
    haveIcons=true;
    const auto path=settingsFile();
    if (path.empty()) return;
    const auto temp=path+".icons.tmp";
    std::ofstream out(temp,std::ios::binary);
    out.write(reinterpret_cast<const char*>(&guideIcons),sizeof guideIcons);
    out.close();
    if (out) std::rename(temp.c_str(),(path+".button-icons-v4").c_str());
}
