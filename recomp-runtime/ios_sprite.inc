// A submitted UI quad has two independent aspect ratios: its sampled texels
// and its final extent on the display. Preserve deliberate stretching and
// tiling. Undo the 4:3 squeeze only when it explains the mismatch.
struct SpriteRegion { float u0=INFINITY,v0=INFINITY,u1=-INFINITY,v1=-INFINITY; };
static float iosSpriteWidth(const RecompD3dPresenterDrawCommand &draw, unsigned offset,
    float left,float right,float top,float bottom,float guestWidth,float guestHeight,
    float physicalAspect,float scaleX,float scaleY)
{
    if (!draw.has_texture || draw.texture_is_backbuffer || draw.target.offscreen ||
        draw.vertex_count<4 || draw.vertex_count>6 || offset==RECOMP_D3D_FVF_ABSENT ||
        offset+8>draw.vertex_stride || !(left<right && top<bottom)) return 1.f;
    SpriteRegion region;
    for (unsigned i=0;i<draw.vertex_count;++i) {
        const auto *v=static_cast<const uint8_t *>(draw.vertex_bytes)+i*draw.vertex_stride;
        float pos[2], uv[2];
        std::memcpy(pos,v,sizeof pos);
        std::memcpy(uv,v+offset,sizeof uv);
        if (!std::isfinite(uv[0]) || !std::isfinite(uv[1]) ||
            (std::abs(pos[0]-left)>.1f && std::abs(pos[0]-right)>.1f) ||
            (std::abs(pos[1]-top)>.1f && std::abs(pos[1]-bottom)>.1f)) return 1.f;
        region.u0=std::min(region.u0,uv[0]); region.u1=std::max(region.u1,uv[0]);
        region.v0=std::min(region.v0,uv[1]); region.v1=std::max(region.v1,uv[1]);
    }
    float us=draw.texture.linear?1.f:float(draw.texture.width);
    float vs=draw.texture.linear?1.f:float(draw.texture.height);
    float w=(region.u1-region.u0)*us, h=(region.v1-region.v0)*vs;
    if (w<2 || h<2 || w>draw.texture.width+1 || h>draw.texture.height+1) return 1.f;
    float shown=((right-left)/guestWidth)/((bottom-top)/guestHeight)*physicalAspect*scaleX/scaleY;
    float ratio=shown*h/w;
    // The settings screen consists of eight glyphs on a vertical baseline.
    // Acquire their source pixels while the retail game has its atlas loaded.
    float x=.5f+((left+right)*.5f/guestWidth-.5f)*scaleX*physicalAspect/(16.f/9.f);
    float y=(top+bottom)*.5f/guestHeight;
    int row=int(std::lround((y-.189f)/.071f));
    if (recomp_ios_frontend_frame() && row>=0 && row<8 && std::abs(x-.6575f)<.012f &&
        std::abs(y-(.189f+.071f*row))<.012f && w<=96 && h<=96 && w/h>.45f && w/h<2.f &&
        draw.texture_bytes && draw.palette_bytes && draw.palette_byte_count>=1024 &&
        draw.texture.format_byte==RECOMP_D3D_TEXTURE_FORMAT_P8 && !draw.texture.linear &&
        region.u0>=0 && region.v0>=0 && region.u1<=1 && region.v1<=1) {
        recomp_ios_observe_icon(row,draw,int(std::lround(region.u0*us)),int(std::lround(region.v0*vs)),
            int(std::lround(w)),int(std::lround(h)));
    }
    // Some atlas pages combine the icon, the backing and the action label
    // into a single strip. Locate the icon inside its source rectangle.
    if (row>=0 && row<8 && w>64 && h>12 && w/h>3 && w/h<8 &&
        std::abs(y-(.189f+.071f*row))<.012f && recomp_ios_frontend_frame() &&
        draw.texture_bytes && draw.palette_bytes && draw.palette_byte_count>=1024 &&
        draw.texture.format_byte==RECOMP_D3D_TEXTURE_FORMAT_P8 && !draw.texture.linear) {
        const float screenWidth=(right-left)/guestWidth*scaleX*physicalAspect/(16.f/9.f);
        const float anchor=(.6575f-x)/screenWidth+.5f;
        if (anchor>.035f && anchor<.22f) {
            const float halfWidth=.013f/screenWidth;
            const float halfHeight=.024f*guestHeight/(bottom-top);
            const int sx=int(std::lround(region.u0*us+(anchor-halfWidth)*w));
            const int sy=int(std::lround(region.v0*vs+(.5f-halfHeight)*h));
            recomp_ios_observe_icon(row,draw,sx,sy,int(std::lround(2*halfWidth*w)),
                int(std::lround(2*halfHeight*h)));
        }
    }
    return ratio>=.71f && ratio<=.79f ? 1.f/ratio : 1.f;
}
