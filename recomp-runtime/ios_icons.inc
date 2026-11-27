// Each row must come from the same frame. Check the distinctive four face
// button colours before adopting the two shoulder and two trigger images.
namespace {
constexpr int iconSide=32;
using IconPixels=std::array<uint32_t,iconSide*iconSide>;
struct GuideIcons { std::array<IconPixels,8> pixels{}; std::array<float,8> aspect{}; };
GuideIcons candidateIcons,guideIcons;
unsigned observedRows;
bool haveIcons;
void loadIcons() {
    static bool tried;
    if (tried) return;
    tried=true;
    const auto path=settingsFile();
    if (path.empty()) return;
    std::ifstream in(path+".icons2",std::ios::binary);
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
void recomp_ios_observe_icon(int row,const RecompD3dPresenterDrawCommand &draw,int left,int top,int w,int h)
{
    loadIcons();
    if(haveIcons || w<=0 || h<=0 || left<0 || top<0 ||
        left+w>int(draw.texture.width) || top+h>int(draw.texture.height)) return;
    unsigned mx,my,matched=0;
    recomp_d3d_texture_swizzle_masks(draw.texture.width,draw.texture.height,&mx,&my);
    auto &pixels=candidateIcons.pixels[row];
    for(int y=0;y<iconSide;++y) for(int x=0;x<iconSide;++x) {
        unsigned off=recomp_d3d_texture_swizzle_offset(left+x*w/iconSide,top+y*h/iconSide,mx,my);
        if(off>=draw.texture_byte_count) return;
        unsigned idx=static_cast<const uint8_t*>(draw.texture_bytes)[off];
        const auto *p=static_cast<const uint8_t*>(draw.palette_bytes)+idx*4;
        pixels[y*iconSide+x]=IM_COL32(p[2],p[1],p[0],p[3]);
        if(p[3]>100 && ((row==0 && p[1]>p[0]+25 && p[1]>p[2]+8) ||
            (row==1 && p[2]>p[1]+30) || (row==2 && p[0]>p[2]+25) ||
            (row==3 && p[2]>p[0]+30 && p[1]>p[0]+25))) ++matched;
    }
    if(row<4 && matched<28) { observedRows &= ~(1u<<row); return; }
    candidateIcons.aspect[row]=float(w)/h;
    observedRows |= 1u<<row;
}
void recomp_ios_finish_icons() {
    if(!haveIcons && observedRows==255) {
        guideIcons=candidateIcons;
        haveIcons=true;
        const auto path=settingsFile();
        if(!path.empty()) {
            const auto tmp=path+".icons.tmp";
            std::ofstream out(tmp,std::ios::binary);
            out.write(reinterpret_cast<const char*>(&guideIcons),sizeof guideIcons);
            out.close();
            if(out) std::rename(tmp.c_str(),(path+".icons2").c_str());
        }
    }
    observedRows=0;
}
