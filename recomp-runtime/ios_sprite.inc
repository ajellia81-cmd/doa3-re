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
    // Source texels in Xbox UI atlases can already be anamorphic. These
    // menu elements have known locations and are squeezed by the desktop
    // 4:3 rule even when their texture ratio alone cannot identify them.
    if (recomp_ios_frontend_frame() && scaleX < .95f) {
        const float x = .5f+((left+right)*.5f/guestWidth-.5f)*scaleX*physicalAspect/(16.f/9.f);
        const float y = (top+bottom)*.5f/guestHeight;
        const float width = (right-left)/guestWidth*scaleX*physicalAspect/(16.f/9.f);
        const float height = (bottom-top)/guestHeight;
        const bool controller = std::abs(x-.5f)<.025f && width>.14f && width<.22f &&
            ((y>.32f && y<.42f && height>.26f && height<.36f) ||
             (y>.65f && y<.73f && height>.16f && height<.24f));
        const bool thumbnail = recomp_ios_select_grid() && y>.64f && y<.90f &&
            width>.037f && width<.085f && height>.07f && height<.125f;
        if (controller || thumbnail) return 4.f/3.f;
    }
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
    return ratio>=.64f && ratio<=.81f ? 1.f/ratio : 1.f;
}
