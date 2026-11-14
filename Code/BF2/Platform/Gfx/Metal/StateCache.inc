// [CHOSEN, not recovered] D3D9 defaults, distinct from the engine's reset seeds.
struct StateCache {
    bf2metal::RawState raw, defaults;
    std::map<unsigned,DWORD> extraRender;
    std::map<std::pair<unsigned,unsigned>,DWORD> extraStage, extraSampler;
    Binding<IDirect3DBaseTexture9> textures[21];
    Binding<IDirect3DVertexBuffer9> streams[16];
    UINT offsets[16]={}, strides[16]={}, frequencies[16]={};
    Binding<IDirect3DIndexBuffer9> indices;
    Binding<IDirect3DVertexDeclaration9> declaration;
    Binding<IDirect3DVertexShader9> vs;
    Binding<IDirect3DPixelShader9> ps;
    float vf[256][4]={}, pf[224][4]={};
    int vi[16][4]={}, pi[16][4]={};
    BOOL vb[16]={}, pb[16]={};
    float clip[6][4]={};
    RECT scissor={};
    static int samplerIndex(unsigned n) {
        return n<16?int(n):n>=256 && n<=260?int(n-240):-1;
    }
    static bool renderID(unsigned n) {
        switch(n) {
        case 7:case 8:case 9:case 14:case 15:case 16:case 19:case 20:case 22:case 23:case 24:case 25:case 26:case 27:case 28:case 29:
        case 34:case 35:case 36:case 37:case 38:case 48:case 52:case 53:case 54:case 55:case 56:case 57:case 58:case 59:case 60:
        case 136:case 137:case 139:case 140:case 141:case 142:case 143:case 145:case 146:case 147:case 148:case 151:case 152:
        case 154:case 155:case 156:case 157:case 158:case 159:case 160:case 161:case 162:case 163:case 165:case 166:case 167:
        case 168:case 170:case 171:case 172:case 173:case 174:case 175:case 176:case 178:case 179:case 180:case 181:case 182:
        case 183:case 184:case 185:case 186:case 187:case 188:case 189:case 190:case 191:case 192:case 193:case 194:case 195:
        case 206:case 207:case 208:case 209: return true;
        default: return (n>=128 && n<=135) || (n>=198 && n<=205);
        }
    }
    static bool stageID(unsigned n) { return (n>=1 && n<=11) || (n>=22 && n<=24) || (n>=26 && n<=28) || n==32; }
    void reset(bool depth,unsigned w,unsigned h) {
        raw={}; extraRender.clear(); extraStage.clear(); extraSampler.clear();
        auto* r=raw.render;
        for(const auto& p : std::initializer_list<std::pair<unsigned,DWORD>>{
            {7,depth?1u:0u},{8,3},{9,2},{14,1},{16,1},{19,2},{20,1},{22,3},{23,4},{25,8},
            {37,0x3f800000},{38,0x3f800000},{53,1},{54,1},{55,1},{56,8},{58,0xffffffff},
            {59,0xffffffff},{60,0xffffffff},{136,1},{137,1},{141,1},{142,1},{145,1},{146,2},
            {154,0x3f800000},{155,0x3f800000},{158,0x3f800000},{161,1},{162,0xffffffff},
            {166,0x42800000},{168,15},{171,1},{172,3},{173,1},{178,0x3f800000},{179,0x3f800000},
            {182,0x3f800000},{186,1},{187,1},{188,1},{189,8},{190,15},{191,15},{192,15},
            {193,0xffffffff},{207,2},{208,1},{209,1}}) r[p.first]=p.second;
        for(auto& a:raw.sampler) { a[1]=a[2]=a[3]=a[5]=a[6]=a[10]=1; }
        for(unsigned i=0;i<8;++i) {
            auto* a=raw.stage[i]; a[1]=i?1:4; a[2]=2; a[3]=1; a[4]=i?1:2;
            a[5]=2; a[6]=1; a[11]=i; a[26]=a[27]=a[28]=1;
        }
        defaults=raw;
        for(auto& t:textures) t.set(nullptr);
        for(unsigned i=0;i<16;++i) { streams[i].set(nullptr); offsets[i]=strides[i]=0; frequencies[i]=1; }
        indices.set(nullptr); declaration.set(nullptr); vs.set(nullptr); ps.set(nullptr);
        std::memset(vf,0,sizeof(vf)); std::memset(pf,0,sizeof(pf));
        std::memset(vi,0,sizeof(vi)); std::memset(pi,0,sizeof(pi));
        std::memset(vb,0,sizeof(vb)); std::memset(pb,0,sizeof(pb)); std::memset(clip,0,sizeof(clip));
        scissor={0,0,LONG(w),LONG(h)};
    }
    const char* map(bf2metal::StateMapping& m) const {
        if(!extraRender.empty()) return "IDirect3DDevice9::Draw state unmapped render-state ID";
        if(!extraStage.empty()) return "IDirect3DDevice9::Draw state unmapped texture-stage ID";
        if(!extraSampler.empty()) return "IDirect3DDevice9::Draw state unmapped sampler-state ID";
        for(unsigned i=0;i<256;++i) {
            switch(i) {
            case 7:case 8:case 9:case 14:case 15: // 9 SHADEMODE: flat is a fragment variant (FragmentVariant::flat)
           case 19:case 20:case 22:case 23:case 24:case 25:case 27:
            case 34:case 36:case 37: // Fog parameters are inert while fog is disabled.
            // 60 TEXTUREFACTOR: read only by a fixed-function stage argument D3DTA_TFACTOR
            // (shaders, ps_1_x included, cannot read it). No default stage argument is TFACTOR,
            // so a stage that uses it is non-default and stops below at the named
            // "texture-stage operation/transform" frontier; the value is never silently dropped.
            case 52:case 53:case 54:case 55:case 56:case 57:case 58:case 59:case 60:case 152:case 161:
            case 168:case 171:case 174:case 175:case 185:case 186:case 187:case 188:case 189:
            case 190:case 191:case 192:case 193:case 194:case 195:case 206:case 207:case 208:case 209: break;
            default:
                if(raw.render[i]!=defaults.render[i]) {
                    // [CHOSEN, not recovered] name the state in the stop (D3DRENDERSTATETYPE).
                    std::fprintf(stderr,"[metal] unmapped render state %u = 0x%08x (D3D9 default 0x%08x)\n",
                                 i,unsigned(raw.render[i]),unsigned(defaults.render[i]));
                    return "IDirect3DDevice9::Draw state unmapped render-state value";
                }
            }
        }
        for(unsigned i=0;i<8;++i) for(unsigned j=0;j<33;++j)
            if(raw.stage[i][j]!=defaults.stage[i][j]) {
                std::fprintf(stderr,"[metal] texture stage %u state %u = 0x%08x (D3D9 default 0x%08x)\n",
                             i,j,unsigned(raw.stage[i][j]),unsigned(defaults.stage[i][j]));
                return "IDirect3DDevice9::Draw state texture-stage operation/transform";
            }
        for(unsigned f:frequencies) if(f!=1) return "IDirect3DDevice9::Draw state stream frequency";
        return bf2metal::mapState(raw,m);
    }
};
