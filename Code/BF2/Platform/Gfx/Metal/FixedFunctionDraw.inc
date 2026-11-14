// [CHOSEN, not recovered] Fixed-function vertex processing on the draw path and FVF declarations
// (0.3.0 piece 1). Included in class Device after DeviceFallbacks.inc (transforms, FVF) and before
// DeviceDraw.inc. The program and its constants come from FFVertex.h; this file maps device state.
    struct FFDraw {
        bf2metal::Program* program=nullptr;
        std::string msl;
        std::vector<std::uint8_t> constants;
    };
    struct FFProgram { bf2metal::Program* program=nullptr; std::string msl; };
    struct FFProgramCache {
        std::map<std::string,FFProgram> programs;
        ~FFProgramCache() { for(auto& p:programs) bf2metal::destroyProgram(p.second.program); }
    } ffVertexPrograms;
    D3DMATRIX transformOrIdentity(DWORD t) const {
        const auto it=transforms.find(t); return it==transforms.end()?identityMatrix():it->second;
    }
    // Declaration usages -> FFVertex input key (POSITIONT, BLENDWEIGHT, BLENDINDICES, NORMAL, COLOR0/1,
    // TEXCOORD0..7). Components per type: FLOAT1..4 = 1..4; colours/UBYTE4 are 4.
    static unsigned components(unsigned type) { return type<=3?type+1:4; }
    // The fixed-function key and constants for declaration d and the current state (draws and
    // ProcessVertices).
    const char* ffState(const Declaration& d,bf2ff::Key& k,bf2ff::Constants& c) {
        const auto& r=state.raw.render; const auto& st=state.raw.stage;
        k=bf2ff::Key();
        bool position=false;
        for(const auto& e:d.mapped) {
            switch(e.usage) {
            case D3DDECLUSAGE_POSITION: if(e.index==0) position=true; break;
            case D3DDECLUSAGE_POSITIONT: if(e.index==0) { position=true; k.transformed=true; } break;
            case D3DDECLUSAGE_BLENDWEIGHT: if(e.index==0) k.weights=int(components(e.type)); break;
            case D3DDECLUSAGE_BLENDINDICES: if(e.index==0) { k.indices=true; k.indicesColor=e.type==D3DDECLTYPE_D3DCOLOR; } break;
            case D3DDECLUSAGE_NORMAL: if(e.index==0) k.normal=true; break;
            case D3DDECLUSAGE_COLOR: if(e.index==0) k.diffuse=true; else if(e.index==1) k.specular=true; break;
            case D3DDECLUSAGE_TEXCOORD:
                if(e.index<8) { k.texDims[e.index]=components(e.type); k.texSets=std::max(k.texSets,unsigned(e.index)+1); }
                break;
            default: break;
            }
        }
        if(!position) return "IDirect3DDevice9::Draw fixed-function vertex processing without a position";
        // Texture sets must be contiguous for the FVF/declaration mapping (gaps read as absent).
        for(unsigned i=0;i<k.texSets;++i) if(!k.texDims[i]) k.texDims[i]=2;
        k.vertexBlend=r[151];                       // D3DRS_VERTEXBLEND
        k.indexedBlend=r[167]!=0 && k.indices;      // D3DRS_INDEXEDVERTEXBLENDENABLE
        k.fog=r[28]!=0 && r[35]==0;                 // FOGENABLE with FOGTABLEMODE NONE: vertex fog
        k.fogMode=r[140];                           // D3DRS_FOGVERTEXMODE
        k.rangeFog=r[48]!=0;                        // D3DRS_RANGEFOGENABLE
        k.normalizeNormals=r[143]!=0;               // D3DRS_NORMALIZENORMALS
        k.localViewer=r[142]!=0;                    // D3DRS_LOCALVIEWER
        for(unsigned s=0;s<8;++s) { k.tci[s]=st[s][11]; k.ttff[s]=st[s][24]; } // TEXCOORDINDEX, TEXTURETRANSFORMFLAGS
        if(const char* why=bf2ff::unsupported(k)) return why;
        // Lighting (piece 2): D3DRS_LIGHTING on untransformed input, the enabled lights in index order.
        k.lighting=!k.transformed && r[137]!=0;
        unsigned enabledLights[8]={}; 
        if(k.lighting) {
            k.colorVertex=r[141]!=0; k.specularEnable=r[29]!=0;              // COLORVERTEX, SPECULARENABLE
            k.diffuseSource=r[145]; k.specularSource=r[146]; k.ambientSource=r[147]; k.emissiveSource=r[148];
            for(const auto& e:lightsEnabled) {
                if(!e.second) continue;
                if(k.lights==8) return "IDirect3DDevice9::Draw fixed-function lighting with more than 8 enabled lights (caps MaxActiveLights 8)";
                DWORD type=0; std::memcpy(&type,lights[e.first].data(),4);
                if(type<1 || type>3) return "IDirect3DDevice9::Draw fixed-function light of an unknown type";
                enabledLights[k.lights]=e.first; k.lightTypes[k.lights++]=type;
            }
        }
        for(unsigned i=0;i<256;++i) { const D3DMATRIX m=transformOrIdentity(256+i); std::memcpy(c.world[i],&m,64); }
        { const D3DMATRIX m=transformOrIdentity(D3DTS_VIEW); std::memcpy(c.view,&m,64); }
        { const D3DMATRIX m=transformOrIdentity(D3DTS_PROJECTION); std::memcpy(c.projection,&m,64); }
        for(unsigned s=0;s<8;++s) { const D3DMATRIX m=transformOrIdentity(16+s); std::memcpy(c.texture[s],&m,64); }
        c.viewport[0]=float(viewport.X); c.viewport[1]=float(viewport.Y); c.viewport[2]=float(viewport.Width);
        c.viewport[3]=float(viewport.Height); c.viewport[4]=viewport.MinZ; c.viewport[5]=viewport.MaxZ;
        std::memcpy(&c.fogStart,&r[36],4); std::memcpy(&c.fogEnd,&r[37],4); std::memcpy(&c.fogDensity,&r[38],4);
        if(k.lighting) {
            const DWORD a=r[139];                                                // D3DRS_AMBIENT
            c.ambient[0]=((a>>16)&255)/255.0f; c.ambient[1]=((a>>8)&255)/255.0f; c.ambient[2]=(a&255)/255.0f; c.ambient[3]=(a>>24)/255.0f;
            std::memcpy(c.material,&material,sizeof c.material);                // D3DMATERIAL9: 17 floats
            for(unsigned i=0;i<k.lights;++i) {
                // D3DLIGHT9: Type, Diffuse, Specular, Ambient, Position, Direction, Range, Falloff,
                // Attenuation0-2, Theta, Phi
                const unsigned char* l=lights[enabledLights[i]].data();
                auto& d=c.light[i];
                d.type=k.lightTypes[i];
                std::memcpy(d.diffuse,l+4,16); std::memcpy(d.specular,l+20,16); std::memcpy(d.ambient,l+36,16);
                std::memcpy(d.position,l+52,12); std::memcpy(d.direction,l+64,12);
                std::memcpy(&d.range,l+76,4); std::memcpy(&d.falloff,l+80,4); std::memcpy(&d.attenuation0,l+84,4);
                std::memcpy(&d.attenuation1,l+88,4); std::memcpy(&d.attenuation2,l+92,4); std::memcpy(&d.theta,l+96,4);
                std::memcpy(&d.phi,l+100,4);
            }
        }
        return nullptr;
    }
    const char* fixedFunctionVertex(const Declaration& d,FFDraw& out) {
        bf2ff::Key k;
        static bf2ff::Constants c; // 17 KB; one draw at a time on the device thread
        if(const char* why=ffState(d,k,c)) return why;
        auto& cached=ffVertexPrograms.programs[k.str()];
        if(!cached.program) {
            bf2metal::ProgramDesc desc; desc.vertex=true;
            std::vector<bf2ff::Input> inputs;
            cached.msl=bf2ff::vertexMsl(k,inputs);
            desc.msl="#include <metal_stdlib>\nusing namespace metal;\n"+cached.msl;
            for(const auto& i:inputs) desc.inputs.push_back({i.reg,i.usage,i.index});
            cached.program=bf2metal::createProgram(native,desc);
        }
        out.program=cached.program; out.msl=cached.msl;
        bf2ff::pack(k,c,out.constants);
        return nullptr;
    }
    // D3DFVF -> declaration elements, in FVF order (D3D9 "Mapping FVF Codes to a Directx 9 Declaration").
    static bool fvfElements(DWORD fvf,std::vector<D3DVERTEXELEMENT9>& e) {
        e.clear(); WORD offset=0;
        auto add=[&](BYTE type,BYTE usage,BYTE index,WORD size) {
            e.push_back({0,offset,type,D3DDECLMETHOD_DEFAULT,usage,index}); offset=WORD(offset+size);
        };
        const DWORD pos=fvf&D3DFVF_POSITION_MASK;
        if(fvf & ~(D3DFVF_POSITION_MASK|D3DFVF_NORMAL|D3DFVF_PSIZE|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEXCOUNT_MASK|
                   D3DFVF_LASTBETA_UBYTE4|D3DFVF_LASTBETA_D3DCOLOR|0xFFFF0000u)) return false;
        if(pos==D3DFVF_XYZ) add(D3DDECLTYPE_FLOAT3,D3DDECLUSAGE_POSITION,0,12);
        else if(pos==D3DFVF_XYZRHW) add(D3DDECLTYPE_FLOAT4,D3DDECLUSAGE_POSITIONT,0,16);
        else if(pos==D3DFVF_XYZW) add(D3DDECLTYPE_FLOAT4,D3DDECLUSAGE_POSITION,0,16);
        else if(pos>=D3DFVF_XYZB1 && pos<=D3DFVF_XYZB5) {
            add(D3DDECLTYPE_FLOAT3,D3DDECLUSAGE_POSITION,0,12);
            int betas=int((pos-D3DFVF_XYZB1)/2)+1;
            const bool last=(fvf&(D3DFVF_LASTBETA_UBYTE4|D3DFVF_LASTBETA_D3DCOLOR))!=0;
            const int weights=last?betas-1:betas;
            if(weights>4 || (!last && betas==5)) return false;
            if(weights>0) add(BYTE(D3DDECLTYPE_FLOAT1+weights-1),D3DDECLUSAGE_BLENDWEIGHT,0,WORD(4*weights));
            if(last) add((fvf&D3DFVF_LASTBETA_UBYTE4)?D3DDECLTYPE_UBYTE4:D3DDECLTYPE_D3DCOLOR,D3DDECLUSAGE_BLENDINDICES,0,4);
        } else if(pos!=0) return false;
        if(fvf&D3DFVF_NORMAL) add(D3DDECLTYPE_FLOAT3,D3DDECLUSAGE_NORMAL,0,12);
        if(fvf&D3DFVF_PSIZE) add(D3DDECLTYPE_FLOAT1,D3DDECLUSAGE_PSIZE,0,4);
        if(fvf&D3DFVF_DIFFUSE) add(D3DDECLTYPE_D3DCOLOR,D3DDECLUSAGE_COLOR,0,4);
        if(fvf&D3DFVF_SPECULAR) add(D3DDECLTYPE_D3DCOLOR,D3DDECLUSAGE_COLOR,1,4);
        const unsigned sets=(fvf&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT;
        if(sets>8) return false;
        for(unsigned i=0;i<sets;++i) {
            static const BYTE type[4]={D3DDECLTYPE_FLOAT2,D3DDECLTYPE_FLOAT3,D3DDECLTYPE_FLOAT4,D3DDECLTYPE_FLOAT1};
            static const WORD size[4]={8,12,16,4};
            const unsigned f=(fvf>>(16+2*i))&3;
            add(type[f],D3DDECLUSAGE_TEXCOORD,BYTE(i),size[f]);
        }
        e.push_back(D3DDECL_END());
        return true;
    }
    // One declaration per FVF, held internally (Life::keep) so the cache does not hold the device.
    std::map<DWORD,Declaration*> fvfDeclarations;
    void releaseFvfDeclarations() { for(auto& d:fvfDeclarations) d.second->drop(); fvfDeclarations.clear(); }
    // D3D9: SetFVF replaces the current declaration with one built from the FVF.
    HRESULT setFvf(DWORD f) {
        if(!f) { fvf=0; return D3D_OK; }
        auto& d=fvfDeclarations[f];
        if(!d) {
            std::vector<D3DVERTEXELEMENT9> e;
            IDirect3DVertexDeclaration9* made=nullptr;
            if(!fvfElements(f,e) || FAILED(CreateVertexDeclaration(e.data(),&made)) || !made) { fvfDeclarations.erase(f); return D3DERR_INVALIDCALL; }
            d=static_cast<Declaration*>(made); d->keep(); made->Release();
        }
        state.declaration.set(d);
        fvf=f;
        return D3D_OK;
    }
    // ---- IDirect3DDevice9::ProcessVertices (0.3.0 piece 3) --------------------------------------
    // [CHOSEN, not recovered] The fixed-function pipeline on the CPU (bf2ff::process, the model the
    // draw path's MSL is checked against), from the bound streams through the current declaration
    // into the destination buffer's FVF (or pDestDecl), as the D3D9 runtime's software vertex
    // processing does (Code/Tests/ff_reference: the real runtime's output for every case).
    static float halfToFloat(std::uint16_t h) {
        const unsigned s=(h>>15)&1, e=(h>>10)&31, m=h&1023;
        float v=e==0?std::ldexp(float(m),-24):e==31?(m?NAN:INFINITY):std::ldexp(float(m|1024),int(e)-25);
        return s?-v:v;
    }
    // One declaration element as the vertex fetch delivers it (missing components 0, 0, 0, 1).
    static bool fetch(const unsigned char* p,unsigned type,float* v) {
        v[0]=v[1]=v[2]=0; v[3]=1;
        auto f32=[&](int i) { float x; std::memcpy(&x,p+4*i,4); return x; };
        auto s16=[&](int i) { std::int16_t x; std::memcpy(&x,p+2*i,2); return x; };
        auto u16=[&](int i) { std::uint16_t x; std::memcpy(&x,p+2*i,2); return x; };
        switch(type) {
        case 0: case 1: case 2: case 3: for(unsigned i=0;i<=type;++i) v[i]=f32(int(i)); return true;
        case 4: v[0]=p[2]/255.0f; v[1]=p[1]/255.0f; v[2]=p[0]/255.0f; v[3]=p[3]/255.0f; return true; // D3DCOLOR
        case 5: for(int i=0;i<4;++i) v[i]=p[i]; return true;                                     // UBYTE4
        case 6: v[0]=s16(0); v[1]=s16(1); return true;                                           // SHORT2
        case 7: for(int i=0;i<4;++i) v[i]=s16(i); return true;                                   // SHORT4
        case 8: for(int i=0;i<4;++i) v[i]=p[i]/255.0f; return true;                              // UBYTE4N
        case 9: v[0]=std::max(s16(0)/32767.0f,-1.0f); v[1]=std::max(s16(1)/32767.0f,-1.0f); return true; // SHORT2N
        case 10: for(int i=0;i<4;++i) v[i]=std::max(s16(i)/32767.0f,-1.0f); return true;         // SHORT4N
        case 11: v[0]=u16(0)/65535.0f; v[1]=u16(1)/65535.0f; return true;                        // USHORT2N
        case 12: for(int i=0;i<4;++i) v[i]=u16(i)/65535.0f; return true;                         // USHORT4N
        case 15: v[0]=halfToFloat(u16(0)); v[1]=halfToFloat(u16(1)); return true;                // FLOAT16_2
        case 16: for(int i=0;i<4;++i) v[i]=halfToFloat(u16(i)); return true;                     // FLOAT16_4
        default: return false;
        }
    }
    static DWORD packColor(const float* c) {
        auto b=[](float v) { v=v<0?0:v>1?1:v; return DWORD(std::lround(v*255.0f)); };
        return (b(c[3])<<24)|(b(c[0])<<16)|(b(c[1])<<8)|b(c[2]);
    }
    HRESULT processVertices(UINT srcStart,UINT destIndex,UINT count,IDirect3DVertexBuffer9* destBuffer,
                            IDirect3DVertexDeclaration9* destDecl,DWORD flags) {
        auto* dest=dynamic_cast<VertexBuffer*>(destBuffer);
        auto* decl=static_cast<Declaration*>(state.declaration.ptr);
        if(!dest || dest->owner!=this || !decl || (flags&~1u)) return D3DERR_INVALIDCALL;   // D3DPV_DONOTCOPYDATA = 1
        if(state.vs.ptr) {
            bf2gfx::unsupported("IDirect3DDevice9::ProcessVertices with a vertex shader (software vertex shader processing)");
            return D3DERR_INVALIDCALL;
        }
        if(!count) return D3D_OK;
        // destination layout: pDestDecl, else the buffer's FVF
        std::vector<D3DVERTEXELEMENT9> out;
        if(destDecl) {
            auto* dd=dynamic_cast<Declaration*>(destDecl); if(!dd || dd->owner!=this) return D3DERR_INVALIDCALL;
            out=dd->elements;
        } else if(!dest->desc.FVF || !fvfElements(dest->desc.FVF,out)) return D3DERR_INVALIDCALL;
        UINT stride=0; int posT=-1, pos=-1, col0=-1, col1=-1, tex[8]={-1,-1,-1,-1,-1,-1,-1,-1};
        for(std::size_t i=0;i<out.size() && out[i].Stream!=0xff;++i) {
            const auto& e=out[i];
            static const UINT sizes[17]={4,8,12,16,4,4,4,8,4,4,8,4,8,4,4,4,8};
            if(e.Stream!=0 || e.Type>16) return D3DERR_INVALIDCALL;
            stride=std::max<UINT>(stride,e.Offset+sizes[e.Type]);
            if(e.Usage==D3DDECLUSAGE_POSITIONT && !e.UsageIndex) posT=int(i);
            else if(e.Usage==D3DDECLUSAGE_POSITION && !e.UsageIndex) pos=int(i);
            else if(e.Usage==D3DDECLUSAGE_COLOR && e.UsageIndex<2) (e.UsageIndex?col1:col0)=int(i);
            else if(e.Usage==D3DDECLUSAGE_TEXCOORD && e.UsageIndex<8) tex[e.UsageIndex]=int(i);
        }
        if(posT<0 && pos<0) return D3DERR_INVALIDCALL;
        if(std::uint64_t(destIndex+count)*stride>dest->desc.Size) return D3DERR_INVALIDCALL;
        bf2ff::Key k;
        static bf2ff::Constants c;
        if(const char* why=ffState(*decl,k,c)) { bf2gfx::unsupported(why); return D3DERR_INVALIDCALL; }
        // Stage i writes coordinate set i, up to the first disabled stage (the runtime writes no set
        // for a stage after COLOROP DISABLE; checked against ProcessVertices).
        unsigned stages=0; while(stages<8 && state.raw.stage[stages][1]!=1) ++stages;
        unsigned sets=0; while(sets<8 && tex[sets]>=0) ++sets;
        k.outputs=std::min(stages,sets);
        // source pointers per element
        struct Src { const unsigned char* base; UINT stride; unsigned type, usage, index; };
        std::vector<Src> src;
        for(const auto& e:decl->mapped) {
            auto* vb=static_cast<VertexBuffer*>(state.streams[e.stream].ptr);
            if(!vb) return D3DERR_INVALIDCALL;
            const UINT s=state.strides[e.stream];
            if(std::uint64_t(state.offsets[e.stream])+std::uint64_t(srcStart+count-1)*s+e.offset+4>vb->desc.Size) return D3DERR_INVALIDCALL;
            src.push_back({static_cast<const unsigned char*>(bf2metal::bufferBytes(vb->native))+state.offsets[e.stream]+e.offset,s,e.type,e.usage,e.index});
        }
        if(!bf2metal::waitIdle(native)) return D3DERR_DEVICELOST;
        unsigned char* dst=static_cast<unsigned char*>(bf2metal::bufferBytes(dest->native))+std::uint64_t(destIndex)*stride;
        for(UINT i=0;i<count;++i) {
            bf2ff::VertexIn in;
            for(const auto& sEl:src) {
                float v[4];
                if(!fetch(sEl.base+std::uint64_t(srcStart+i)*sEl.stride,sEl.type,v)) return D3DERR_INVALIDCALL;
                switch(sEl.usage) {
                case D3DDECLUSAGE_POSITION: case D3DDECLUSAGE_POSITIONT: if(!sEl.index) std::memcpy(in.position,v,16); break;
                case D3DDECLUSAGE_BLENDWEIGHT: if(!sEl.index) std::memcpy(in.weights,v,16); break;
                case D3DDECLUSAGE_BLENDINDICES:
                    if(!sEl.index) for(int j=0;j<4;++j) in.indices[j]=(unsigned char)(sEl.type==4?std::lround(v[j]*255.0f):v[j]);
                    break;
                case D3DDECLUSAGE_NORMAL: if(!sEl.index) std::memcpy(in.normal,v,12); break;
                case D3DDECLUSAGE_COLOR: if(sEl.index<2) std::memcpy(sEl.index?in.specular:in.diffuse,v,16); break;
                case D3DDECLUSAGE_TEXCOORD: if(sEl.index<8) std::memcpy(in.tex[sEl.index],v,16); break;
                default: break;
                }
            }
            if(!k.transformed) in.position[3]=1;
            bf2ff::VertexOut o;
            bf2ff::process(k,c,in,o);
            unsigned char* d=dst+std::uint64_t(i)*stride;
            if(posT>=0) std::memcpy(d+out[posT].Offset,o.screen,16);
            else std::memcpy(d+out[pos].Offset,o.screen,12);
            // D3DPV_DONOTCOPYDATA: the runtime writes the position only, even when lighting, fog or
            // texture-coordinate generation changed the other fields (checked against ProcessVertices;
            // D3DRS_CLIPPING TRUE produced the same output as FALSE, vertices outside the volume included).
            if(flags&1) continue;
            // ProcessVertices puts the vertex fog factor in the specular alpha.
            float spec[4]={o.specular[0],o.specular[1],o.specular[2],k.fog?o.fog:o.specular[3]};
            if(col0>=0) { const DWORD v=packColor(o.diffuse); std::memcpy(d+out[col0].Offset,&v,4); }
            if(col1>=0) { const DWORD v=packColor(spec); std::memcpy(d+out[col1].Offset,&v,4); }
            for(unsigned t=0;t<k.outputs;++t) {
                const unsigned n=out[tex[t]].Type<=3?out[tex[t]].Type+1:0;
                if(n) std::memcpy(d+out[tex[t]].Offset,o.tex[t],4*n);
            }
        }
        return D3D_OK;
    }
