// [CHOSEN, not recovered] G4 shader creation and programmable draws. Included in class Device.
    template<class Object,class I> HRESULT createShader(const DWORD* function,I** out,bool vertex,const char* method) {
        mainThread(method);
        if(!out || !function) return D3DERR_INVALIDCALL; *out=nullptr;
        const std::size_t bytes=bf2metal::shaderLength(function);
        if(!bytes) return D3DERR_INVALIDCALL;
        auto record=bf2metal::ShaderOffer::take(function,bytes);
        // Bytecode that no effect offered (the D3DXAssembleShader AA probes) is translated at run
        // time; a client without native effect compilation has no translator and stops here.
        if(!record) record=bf2metal::translateRuntimeShader(function,bytes,vertex);
        if(!record) frontier(vertex?"IDirect3DDevice9::CreateVertexShader (bytecode outside the effect cache)":
                                    "IDirect3DDevice9::CreatePixelShader (bytecode outside the effect cache)");
        if(record->msl.empty() || record->stage!=(vertex?16u:15u)) return D3DERR_INVALIDCALL;
        for(const auto& s:record->samplers) if(s.constant)
            frontier("IDirect3DDevice9::CreatePixelShader (ps_1_x TEXBEM sampler constants)");
        bf2metal::ProgramDesc desc; desc.vertex=vertex;
        std::string error;
        if(!patchMsl(*record,vertex,desc.msl,error)) {
            const std::string name=std::string(method)+" (MSL interface: "+error+")"; frontier(name.c_str());
        }
        if(vertex) for(const auto& a:record->attributes) {
            if(a.name.size()<2 || a.name[0]!='v' || a.name.find_first_not_of("0123456789",1)!=std::string::npos)
                frontier("IDirect3DDevice9::CreateVertexShader (vertex input register name)");
            desc.inputs.push_back({unsigned(std::stoul(a.name.substr(1))),a.usage,a.index});
        }
        for(const auto& s:record->samplers) desc.samplers.push_back({s.index,s.kind});
        auto* object=new Object(this,native,vertex,record);
        object->program=bf2metal::createProgram(native,desc);
        createVariants(*object);
        *out=object; object->AddRef(); return D3D_OK;
    }
    // [CHOSEN, not recovered] The vertex-fog variant (fog 1) of a new pixel shader below ps_3_0 is
    // created with it, so its library is compiled ahead of the first draw like the shader's own
    // program (ShaderPrebuild.inc). BF2's effects draw with D3DRS_FOGENABLE and a vertex shader that
    // writes oFog nearly everywhere, so this variant, not the shader's own program, is what most
    // draws use (all 63 variants of a combat session on Dalian Plant). A patch that does not apply
    // is left to the draw that needs the variant.
    void createVariants(VertexShaderObject&) {}
    void createVariants(PixelShaderObject& ps) {
        if(ps.record->major>=3 || !bf2metal::backgroundBuilds(native)) return;
        FragmentVariant fog; fog.fog=1;
        const std::string key=fog.key();
        bf2metal::ProgramDesc desc; std::string error;
        if(!ps.variants.count(key) && variantDesc(ps,fog,desc,error)) ps.variants[key]=bf2metal::createProgram(native,desc);
    }
    // The program description of a pixel shader variant: its MSL patched for the variant, its
    // sampler kinds with the variant's retyped slots. False with the reason when the patch does not
    // apply.
    bool variantDesc(PixelShaderObject& ps,const FragmentVariant& v,bf2metal::ProgramDesc& desc,std::string& error) {
        desc.vertex=false; desc.variant=true;
        for(const auto& s:ps.record->samplers) {
            unsigned kind=s.kind;
            for(const auto& retyped:v.samplers) if(retyped.first==s.index) kind=retyped.second;
            desc.samplers.push_back({s.index,kind});
        }
        return patchMsl(ps.record->msl,false,desc.msl,error,&v);
    }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD* f,IDirect3DVertexShader9** out) override {
        return createShader<VertexShaderObject>(f,out,true,"IDirect3DDevice9::CreateVertexShader");
    }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD* f,IDirect3DPixelShader9** out) override {
        return createShader<PixelShaderObject>(f,out,false,"IDirect3DDevice9::CreatePixelShader");
    }
    // MojoShader's buffer(0) layout: float4 array, int4 array, then one byte per bool, each in
    // reflection order, padded to 16 bytes. Registers come from the device's constant files.
    template<unsigned NF> static bool packUniforms(const bf2fx::Shader& s,const float (&f)[NF][4],const int (&n)[16][4],
                                                 const BOOL (&b)[16],std::vector<std::uint8_t>& out) {
        out.clear(); std::size_t alignment=1;
        for(unsigned kind=0;kind<3;++kind) for(const auto& u:s.uniforms) if(u.kind==kind && !u.constant) {
            const std::size_t width=kind==2?1:16; alignment=std::max(alignment,width);
            while(out.size()%width) out.push_back(0);
            for(unsigned r=0;r<u.count;++r) {
                const unsigned reg=u.index+r;
                if((kind==0 && reg>=NF) || (kind!=0 && reg>=16)) return false;
                if(kind==2) { out.push_back(b[reg]?1:0); continue; }
                const auto* p=kind==0?reinterpret_cast<const std::uint8_t*>(f[reg]):reinterpret_cast<const std::uint8_t*>(n[reg]);
                out.insert(out.end(),p,p+16);
            }
        }
        while(out.size()%alignment) out.push_back(0);
        return true;
    }
    static bf2metal::Texture* nativeTexture(IDirect3DBaseTexture9* t) {
        std::shared_ptr<Image> image;
        if(auto* x=dynamic_cast<Texture2D*>(t)) image=x->image;
        else if(auto* x=dynamic_cast<CubeTexture*>(t)) image=x->image;
        else if(auto* x=dynamic_cast<VolumeTexture*>(t)) image=x->image;
        return image?image->native:nullptr;
    }
    // D3D9 ignores fixed-function vertex/pixel states while shaders run. These states are reset to
    // their defaults for the mapping only; StateCache::map keeps refusing every other change.
    const char* programmableState(bf2metal::StateMapping& m,const bf2fx::Shader* ps,bool points) {
        auto& r=state.raw.render; const auto& d=state.defaults.render;
        bf2metal::RawState saved=state.raw;
        for(unsigned i:{26u,29u,48u,136u,137u,139u,140u,141u,142u,143u,145u,146u,147u,148u,151u,
                        162u,163u,165u,167u,170u,172u,173u,178u,179u,180u,181u,182u,183u,184u}) r[i]=d[i];
        // Fog (FOGENABLE/COLOR/START/END/DENSITY/TABLEMODE) is a fragment variant (fogFor); with a
        // vertex shader D3DRS_FOGVERTEXMODE (140, reset above) and RANGEFOGENABLE (48) are inert.
        for(unsigned i:{28u,34u,35u,36u,37u,38u}) r[i]=d[i];
        if(!points) for(unsigned i:{154u,155u,156u,157u,158u,159u,160u,166u}) r[i]=d[i];
        if(ps) r[60]=d[60];
        const char* why=nullptr;
        for(unsigned i=0;i<8 && !why;++i)
            if(ps && ps->major==1 && ps->minor<4 && (state.raw.stage[i][24]&256))
                why="IDirect3DDevice9::Draw state D3DTTFF_PROJECTED with ps_1_1..1_3";
        std::memcpy(state.raw.stage,state.defaults.stage,sizeof(state.raw.stage));
        if(!why) why=state.map(m);
        state.raw=saved;
        return why;
    }
    // [CHOSEN, not recovered] Fixed-function pixel stages behind a vertex shader, for the ops,
    // arguments and modifiers BF2's effects use. Unsupported stage features stop by name.
    // An operation that selects the texture of a stage without one passes CURRENT through (see the
    // stage loop). [UNRESOLVED] BLENDTEXTUREALPHA without a texture reads the alpha as 1 here.
    struct ProgramCache {
        std::map<std::string,bf2metal::Program*> programs;
        ~ProgramCache() { for(auto& p:programs) bf2metal::destroyProgram(p.second); }
    } ffpPrograms;
    // D3D9 fog with a vertex shader: FOGTABLEMODE NONE uses the
    // vertex shader's oFog (FOGVERTEXMODE is ignored); EXP/EXP2/LINEAR table fog uses the pixel's
    // depth. ps_3_0 has no fixed-function fog. Fills the fog constants for buffer(3).
    unsigned fogFor(const std::string& vsMsl,const bf2fx::Shader* ps,float constants[8]) const {
        const auto& r=state.raw.render;
        if(!r[28] || (ps && ps->major>=3)) return 0;
        const DWORD c=r[34];
        constants[0]=((c>>16)&255)/255.0f; constants[1]=((c>>8)&255)/255.0f; constants[2]=(c&255)/255.0f; constants[3]=(c>>24)/255.0f;
        float start,end,density; std::memcpy(&start,&r[36],4); std::memcpy(&end,&r[37],4); std::memcpy(&density,&r[38],4);
        constants[4]=float(r[35]); constants[5]=end; constants[6]=end!=start?1.0f/(end-start):0.0f; constants[7]=density;
        if(r[35]) { // D3DFOG_NONE = 0
            if(r[35]>3) frontier("IDirect3DDevice9::Draw state D3DRS_FOGTABLEMODE value");
            return 3;
        }
        return vsMsl.find("[[user(fog)]]")!=std::string::npos?1:2;
    }
    // The fragment program for this draw: the shader's own, or a variant for fog and for ps_1_x
    // samplers bound to cube/volume textures (FragmentVariant, Shaders.inc).
    bf2metal::Program* fragmentProgram(PixelShaderObject& ps,unsigned fog,const bf2metal::StateMapping& m) {
        FragmentVariant v; v.fog=fog; v.flat=state.raw.render[9]==1; // D3DRS_SHADEMODE = D3DSHADE_FLAT
        for(const auto& s:ps.record->samplers) {
            if(s.index<16 && m.sampler[s.index].lodBias!=0) v.bias.push_back({s.index,m.sampler[s.index].lodBias});
            if(ps.record->major==1 && s.kind==0 && s.index<16) if(auto* t=state.textures[s.index].ptr) {
                const unsigned bound=dynamic_cast<CubeTexture*>(t)?1:dynamic_cast<VolumeTexture*>(t)?2:0;
                if(bound) v.samplers.push_back({s.index,bound});
            }
        }
        if(!v.fog && !v.flat && v.samplers.empty() && v.bias.empty()) return ps.program;
        auto& cached=ps.variants[v.key()];
        if(!cached) {
            bf2metal::ProgramDesc desc; std::string error;
            if(!variantDesc(ps,v,desc,error)) {
                const std::string name="IDirect3DDevice9::Draw pixel shader variant ("+error+")"; frontier(name.c_str());
            }
            cached=bf2metal::createProgram(native,desc);
        }
        return cached;
    }
    // ffVertex: the vertex stage is fixed-function too (FixedFunctionDraw.inc), which already applied
    // each stage's D3DTSS_TEXCOORDINDEX generation and D3DTSS_TEXTURETRANSFORMFLAGS and wrote stage
    // i's coordinates to texcoord i; the pixel stage only divides by the PROJECTED component.
    bf2metal::Program* fixedFunctionPixel(const std::string& vsMsl,float factor[4],unsigned fog,const bf2metal::StateMapping& m,bool ffVertex=false) {
        const auto& st=state.raw.stage; const auto& r=state.raw.render;
        if(r[29]) frontier("IDirect3DDevice9::Draw fixed-function pixel pipeline D3DRS_SPECULARENABLE");
        auto output=[&](const std::string& user) { return vsMsl.find("[[user("+user+")]]")!=std::string::npos; };
        std::string inputs, args, body;
        auto declare=[&](const std::string& user) {
            if(!output(user)) return false;
            if(inputs.find("[[user("+user+")]]")==std::string::npos) inputs+="\tfloat4 v_"+user+" [[user("+user+")]];\n";
            return true;
        };
        body+="\tfloat4 diffuse = "+std::string(declare("color0")?"in.v_color0":"float4(1.0)")+";\n";
        body+="\tfloat4 specular = "+std::string(declare("color1")?"in.v_color1":"float4(0.0)")+";\n";
        body+="\tfloat4 current = diffuse, tex;\n";
        std::vector<bf2metal::ProgramSampler> samplers;
        auto arg=[&](DWORD a,bool alpha)->std::string {
            if(a&~0x3fu) frontier("IDirect3DDevice9::Draw fixed-function pixel argument modifier");
            std::string v;
            switch(a&15) {
            case 0: v="diffuse"; break; case 1: v="current"; break; case 2: v="tex"; break;
            case 3: v="tfactor"; break; case 4: v="specular"; break;
            default: frontier("IDirect3DDevice9::Draw fixed-function pixel argument (temp/constant)");
            }
            if(a&0x20) v="float4("+v+".a)";
            if(a&0x10) v="(1.0 - "+v+")";
            return alpha?v+".a":v+".rgb";
        };
        auto op=[&](DWORD o,const std::string& a1,const std::string& a2,const std::string& cur,bool alpha)->std::string {
            const std::string t=alpha?"":".rgb";
            switch(o) {
            case 2: return a1; case 3: return a2;
            case 4: return a1+" * "+a2; case 5: return "2.0 * "+a1+" * "+a2; case 6: return "4.0 * "+a1+" * "+a2;
            case 7: return a1+" + "+a2; case 8: return a1+" + "+a2+" - 0.5"; case 9: return "2.0 * ("+a1+" + "+a2+" - 0.5)";
            case 10: return a1+" - "+a2;
            case 12: return "mix("+a2+", "+a1+", diffuse.a)"; case 13: return "mix("+a2+", "+a1+", tex.a)";
            case 14: return "mix("+a2+", "+a1+", tfactor.a)"; case 16: return "mix("+a2+", "+a1+", "+cur+".a)";
            default: frontier("IDirect3DDevice9::Draw fixed-function pixel texture operation");
            }
        };
        for(unsigned i=0;i<8 && st[i][1]!=1;++i) {
            const DWORD* s=st[i];
            if(!ffVertex && s[24]) frontier("IDirect3DDevice9::Draw fixed-function pixel texture transform");
            if(!ffVertex && (s[11]>>16)) frontier("IDirect3DDevice9::Draw fixed-function pixel texture-coordinate generation");
            if(s[28]!=1) frontier("IDirect3DDevice9::Draw fixed-function pixel D3DTSS_RESULTARG");
            bool usesTexture=false;
            for(unsigned k:{2u,3u,5u,6u}) usesTexture=usesTexture || (s[k]&15)==2;
            if(s[1]==13 || s[4]==13) usesTexture=true;
            body+="\ttex = float4(1.0);\n";
            if(usesTexture && state.textures[i].ptr) {
                auto* t=state.textures[i].ptr;
                const unsigned kind=dynamic_cast<CubeTexture*>(t)?1:dynamic_cast<VolumeTexture*>(t)?2:0;
                const std::string user="texcoord"+std::to_string(ffVertex?i:(s[11]&0xffff)), n=std::to_string(i);
                std::string coord=declare(user)?"in.v_"+user:"float4(0.0)";
                if(ffVertex && (s[24]&256)) {
                    // D3DTTFF_PROJECTED: divide by the last of the COUNT components (w without a count)
                    const unsigned count=s[24]&255, last=count>=2 && count<=4?count-1:3;
                    if(kind) frontier("IDirect3DDevice9::Draw fixed-function pixel projected cube/volume coordinates");
                    coord="float4("+coord+".xy / "+coord+std::string(1,'.')+"xyzw"[last]+", 0.0, 1.0)";
                }
                args+=std::string(",\n\t")+(kind==1?"texturecube":kind==2?"texture3d":"texture2d")+"<float> t"+n+" [[texture("+n+")]], sampler s"+n+" [[sampler("+n+")]]";
                // D3DSAMP_MIPMAPLODBIAS as an MSL bias (Shaders.inc lodBiasSamples).
                const std::string bias=m.sampler[i].lodBias!=0?", bias("+biasLiteral(m.sampler[i].lodBias)+")":"";
                body+="\ttex = t"+n+".sample(s"+n+", "+coord+(kind?".xyz":".xy")+bias+");\n";
                samplers.push_back({i,kind});
            }
            // D3D9 with no texture on the stage: an operation that reads D3DTA_TEXTURE as an
            // argument it selects (anything but SELECTARG2 for ARG1, anything but SELECTARG1 for
            // ARG2) passes CURRENT through instead, separately for colour and alpha (Wine's
            // is_invalid_op, which models the Windows runtime). SwiffMenu's Line technique draws
            // with TexMap unbound and the default stage 0 (MODULATE / SELECTARG1 of TEXTURE), so
            // its strokes take the line colour's alpha, as on the retail client: reading the
            // unbound texture as opaque white drew the faded menu tab and button borders opaque.
            auto missing=[&](DWORD o,DWORD a1,DWORD a2) {
                return !state.textures[i].ptr && (((a1&15)==2 && o!=3) || ((a2&15)==2 && o!=2));
            };
            const bool colorPass=missing(s[1],s[2],s[3]);
            const bool alphaPass=s[4]==1 || missing(s[4],s[5],s[6]); // a disabled alpha op keeps the current alpha
            body+="\tcurrent = saturate(float4("+
                (colorPass?std::string("current.rgb"):op(s[1],arg(s[2],false),arg(s[3],false),"current",false))+", "+
                (alphaPass?std::string("current.a"):op(s[4],arg(s[5],true),arg(s[6],true),"current",true))+"));\n";
        }
        const DWORD tf=r[60];
        factor[0]=((tf>>16)&255)/255.0f; factor[1]=((tf>>8)&255)/255.0f; factor[2]=(tf&255)/255.0f; factor[3]=(tf>>24)/255.0f;
        std::string msl="#include <metal_stdlib>\nusing namespace metal;\nstruct bf2_ffp_in {\n"+
            (inputs.empty()?std::string("\tfloat4 bf2_unused [[position]];\n"):inputs)+"};\n"
            "struct bf2_ffp_out { float4 color [[color(0)]]; };\n"
            "fragment bf2_ffp_out bf2_ffp(bf2_ffp_in in [[stage_in]],\n"
            "\tconstant float4 &tfactor [[buffer(2)]]"+args+"\n) {\n"+body+
            "\tbf2_ffp_out output; output.color = current;\n"
            "\treturn output;\n}\n";
        // The alpha test (and its bf2_alpha argument) is patched exactly as for translated shaders.
        FragmentVariant variant; variant.fog=fog; variant.flat=r[9]==1; // D3DRS_SHADEMODE = D3DSHADE_FLAT
        std::string patched, error;
        if(!patchMsl(msl,false,patched,error,&variant)) frontier("IDirect3DDevice9::Draw fixed-function pixel MSL");
        auto& cached=ffpPrograms.programs[variant.key()+"|"+msl];
        if(!cached) {
            bf2metal::ProgramDesc desc; desc.vertex=false; desc.samplers=samplers;
            desc.msl=patched; cached=bf2metal::createProgram(native,desc);
        }
        return cached;
    }
    struct DrawSource {
        D3DPRIMITIVETYPE type; UINT primitives;
        UINT first=0; INT baseVertex=0; UINT minIndex=0, vertices=0, startIndex=0;
        bool indexed=false;
        const void* upVertices=nullptr; UINT upStride=0;
        const void* upIndices=nullptr; D3DFORMAT upFormat=D3DFMT_INDEX16;
    };
    static UINT primitiveVertices(D3DPRIMITIVETYPE t,UINT n) {
        switch(t) {
        case D3DPT_POINTLIST: return n; case D3DPT_LINELIST: return n*2; case D3DPT_LINESTRIP: return n+1;
        case D3DPT_TRIANGLELIST: return n*3; case D3DPT_TRIANGLESTRIP: case D3DPT_TRIANGLEFAN: return n+2;
        default: return 0;
        }
    }
    // [CHOSEN, not recovered] Debug aid, off by default: BF2_METAL_SKIP_REFUSED_DRAWS=1 drops a
    // draw whose state would stop and names each reason once, instead of the named stop.
    static bool skipRefused(const char* why) {
        static const bool on=[]{const char* e=std::getenv("BF2_METAL_SKIP_REFUSED_DRAWS"); return e && *e=='1';}();
        if(!on) frontier(why);
        static std::map<std::string,unsigned long> seen;
        if(++seen[why]==1) std::fprintf(stderr,"[metal] BF2_METAL_SKIP_REFUSED_DRAWS: dropping draws: %s\n",why);
        return true;
    }
    HRESULT drawSharedImpl(const DrawSource& src,const char* method) {
        drawOutcome="drawn";
        mainThread(method);
        const UINT count=primitiveVertices(src.type,src.primitives);
        if(!scene || !count) { drawOutcome="outside BeginScene or no vertices"; return D3DERR_INVALIDCALL; }
        auto* vs=dynamic_cast<VertexShaderObject*>(state.vs.ptr);
        auto* ps=dynamic_cast<PixelShaderObject*>(state.ps.ptr);
        auto* decl=dynamic_cast<Declaration*>(state.declaration.ptr);
        bf2metal::DrawCall call;
        if(const char* why=programmableState(call.state,ps?ps->record.get():nullptr,src.type==D3DPT_POINTLIST)) { drawOutcome="refused state (skipped)"; skipRefused(why); return D3D_OK; }
        if(!decl) { drawOutcome="no vertex declaration or FVF"; return D3DERR_INVALIDCALL; } // D3D9: a draw needs one
        // [CHOSEN, not recovered] No vertex shader: fixed-function vertex processing (FFVertex.h).
        FFDraw ff;
        if(!vs) { if(const char* why=fixedFunctionVertex(*decl,ff)) { drawOutcome="refused state (skipped)"; skipRefused(why); return D3D_OK; } }
        const auto& r=state.raw.render;
        if(call.state.clipMask) { drawOutcome="refused state (skipped)"; skipRefused("IDirect3DDevice9::Draw state D3DRS_CLIPPLANEENABLE (user clip planes)"); return D3D_OK; }
        if(call.state.srgbWrite) { drawOutcome="refused state (skipped)"; skipRefused("IDirect3DDevice9::Draw state D3DRS_SRGBWRITEENABLE"); return D3D_OK; }
        for(unsigned i=0;i<21;++i) {
            const auto& s=call.state.sampler[i];
            // Pixel samplers take the bias in the fragment program (fragmentProgram, fixedFunctionPixel).
            if(i>=16 && s.lodBias!=0) { drawOutcome="refused state (skipped)"; skipRefused("IDirect3DDevice9::Draw state D3DSAMP_MIPMAPLODBIAS on a vertex/displacement sampler"); return D3D_OK; }
            if(s.srgb) { drawOutcome="refused state (skipped)"; skipRefused("IDirect3DDevice9::Draw state D3DSAMP_SRGBTEXTURE"); return D3D_OK; }
        }
        const std::string& vsMsl=vs?vs->record->msl:ff.msl;
        call.vertex=vs?vs->program:ff.program; call.layout=decl->native;
        const unsigned fog=fogFor(vsMsl,ps?ps->record.get():nullptr,call.fragmentFog);
        if(ps) call.fragment=fragmentProgram(*ps,call.state.colorMask?fog:0,call.state);
        else if(call.state.colorMask) call.fragment=fixedFunctionPixel(vsMsl,call.fragmentFactor,fog,call.state,!vs);
        if(activeQuery) call.occlusion=activeQuery->occlusion;
        auto* color=static_cast<Surface*>(colorTarget.ptr); auto* depth=static_cast<Surface*>(depthTarget.ptr);
        if(color->plane->locked || (depth && depth->plane->locked)) { drawOutcome="locked target"; return D3DERR_INVALIDCALL; }
        call.color=color->plane->image->native; call.colorLevel=color->plane->level; call.colorSlice=color->plane->slice;
        if(depth) {
            if(depth->desc.Width<color->desc.Width || depth->desc.Height<color->desc.Height) { drawOutcome="depth smaller than colour target"; return D3DERR_INVALIDCALL; }
            call.depth=depth->plane->image->native; call.depthLevel=depth->plane->level; call.depthSlice=depth->plane->slice;
            call.depthBiasUnits=depth->desc.Format==D3DFMT_D16?65535.0f:16777215.0f;
        }
        // D3D9 accepts viewports beyond the target (pixels outside are clipped); Metal requires
        // one inside the attachment. The Metal viewport is the clamped rectangle and the vertex
        // fixup remaps clip space so the D3D viewport transform, plus D3D9's half-pixel centre
        // offset, is preserved: ndc' = ndc * scale + offset (fixup = {offset.xy, scale.xy}).
        const double tw=color->desc.Width, th=color->desc.Height;
        const double vx=viewport.X, vy=viewport.Y, vw=viewport.Width, vh=viewport.Height;
        const double cx0=std::max(0.0,vx), cy0=std::max(0.0,vy), cx1=std::min(tw,vx+vw), cy1=std::min(th,vy+vh);
        if(cx1<=cx0 || cy1<=cy0) { drawOutcome="viewport off target"; return D3D_OK; } // nothing of the viewport lies on the target
        const double cw=cx1-cx0, ch=cy1-cy0;
        call.viewport[0]=cx0; call.viewport[1]=cy0; call.viewport[2]=cw; call.viewport[3]=ch;
        call.viewport[4]=viewport.MinZ; call.viewport[5]=viewport.MaxZ; // Metal accepts MinZ > MaxZ too
        call.vertexFixup[0]=float((2*(vx-cx0)+vw+1)/cw-1); call.vertexFixup[1]=float(1-(2*(vy-cy0)+vh+1)/ch);
        call.vertexFixup[2]=float(vw/cw); call.vertexFixup[3]=float(vh/ch);
        const RECT full={0,0,LONG(color->desc.Width),LONG(color->desc.Height)};
        const RECT& sc=call.state.scissor?state.scissor:full;
        const LONG sx0=std::max(sc.left,LONG(0)), sy0=std::max(sc.top,LONG(0));
        const LONG sx1=std::min(sc.right,full.right), sy1=std::min(sc.bottom,full.bottom);
        if(sx1<=sx0 || sy1<=sy0) { drawOutcome="scissor excludes target"; return D3D_OK; } // the scissor rectangle excludes the whole target
        call.scissor[0]=unsigned(sx0); call.scissor[1]=unsigned(sy0);
        call.scissor[2]=unsigned(sx1-sx0); call.scissor[3]=unsigned(sy1-sy0);
        call.fragmentAlpha[0]=call.state.alphaTest?float(r[25]):0.0f; call.fragmentAlpha[1]=float(r[24]&255);
        if(!vs) call.vertexConstants=std::move(ff.constants);
        if((vs && !packUniforms(*vs->record,state.vf,state.vi,state.vb,call.vertexConstants)) ||
            (ps && !packUniforms(*ps->record,state.pf,state.pi,state.pb,call.fragmentConstants)))
            frontier("IDirect3DDevice9::Draw shader constant register beyond the device constant file");
        for(unsigned i=0;i<21;++i) if(auto* t=state.textures[i].ptr) {
            call.textures[i]=nativeTexture(t);
            if(!call.textures[i]) { drawOutcome="SYSTEMMEM texture bound"; return D3DERR_INVALIDCALL; } // SYSTEMMEM textures cannot be sampled
        }
        for(unsigned i=0;i<16;++i) {
            call.streams[i].stride=state.strides[i];
            if(auto* b=dynamic_cast<VertexBuffer*>(state.streams[i].ptr)) { call.streams[i].buffer=b->native; call.streams[i].offset=state.offsets[i]; }
        }
        if(src.upVertices) {
            call.streams[0]={nullptr,src.upVertices,(src.indexed?src.minIndex+src.vertices:src.first+count)*src.upStride,0,src.upStride};
        }
        call.primitive=src.type; call.count=count; call.first=src.first; call.baseVertex=src.baseVertex;
        std::vector<std::uint32_t> fan;
        if(src.indexed) {
            if(src.upIndices) {
                call.indexSize=src.upFormat==D3DFMT_INDEX32?4:2; call.indexBytes=src.upIndices; call.indexLength=count*call.indexSize;
            } else {
                auto* ib=dynamic_cast<IndexBuffer*>(state.indices.ptr);
                if(!ib) { drawOutcome="no index buffer"; return D3DERR_INVALIDCALL; }
                call.indexSize=ib->desc.Format==D3DFMT_INDEX32?4:2; call.indexBuffer=ib->native;
                call.indexOffset=src.startIndex*call.indexSize;
                if(call.indexOffset+std::size_t(count)*call.indexSize>ib->desc.Size) { drawOutcome="indices beyond the index buffer"; return D3DERR_INVALIDCALL; }
            }
        }
        if(src.type==D3DPT_TRIANGLEFAN) {
            // Metal has no fans: expand to an indexed triangle list with the same vertices.
            auto index=[&](UINT i)->std::uint32_t {
                if(!src.indexed) return src.first+i;
                const auto* base=call.indexBytes?static_cast<const std::uint8_t*>(call.indexBytes):
                    static_cast<const std::uint8_t*>(bf2metal::bufferBytes(call.indexBuffer))+call.indexOffset;
                if(call.indexSize==4) { std::uint32_t v; std::memcpy(&v,base+i*4,4); return v; }
                std::uint16_t v; std::memcpy(&v,base+i*2,2); return v;
            };
            for(UINT p=0;p<src.primitives;++p) { fan.push_back(index(0)); fan.push_back(index(p+1)); fan.push_back(index(p+2)); }
            call.primitive=D3DPT_TRIANGLELIST; call.count=UINT(fan.size()); call.indexSize=4;
            call.indexBuffer=nullptr; call.indexBytes=fan.data(); call.indexLength=UINT(fan.size()*4); call.indexOffset=0;
            if(!src.indexed) call.baseVertex=0;
        }
        std::string error;
        if(!bf2metal::draw(native,call,error)) {
            drawOutcome="bridge draw failed (skipped)"; const std::string name=std::string(method)+" ("+error+")"; skipRefused(name.c_str());
        }
        return D3D_OK;
    }
    // [CHOSEN, not recovered] BF2_METAL_DRAW_TRACE (EffectShaders.h): outcome of every draw.
    const char* drawOutcome="drawn";
    std::map<std::string,unsigned long> drawTrace;
    HRESULT drawShared(const DrawSource& src,const char* method) {
        const HRESULT hr=drawSharedImpl(src,method);
        if(bf2metal::drawTraceEnabled()) {
            std::string key=std::string(bf2metal::drawTraceLabel())+" | "+drawOutcome;
            if(FAILED(hr)) key+=" (INVALIDCALL)";
            ++drawTrace[key];
        }
        return hr;
    }
    void flushDrawTrace() {
        static unsigned long presents=0;
        if(!bf2metal::drawTraceEnabled() || ++presents%600) return;
        std::fprintf(stderr,"[metal] draw trace after %lu presents (%zu keys):\n",presents,drawTrace.size());
        for(const auto& k:drawTrace) std::fprintf(stderr,"[metal]   %8lu  %s\n",k.second,k.first.c_str());
        drawTrace.clear();
    }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE t,UINT start,UINT n) override {
        DrawSource s{t,n}; s.first=start; return drawShared(s,"IDirect3DDevice9::DrawPrimitive");
    }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE t,INT base,UINT minIndex,UINT vertices,UINT start,UINT n) override {
        DrawSource s{t,n}; s.indexed=true; s.baseVertex=base; s.minIndex=minIndex; s.vertices=vertices; s.startIndex=start;
        return drawShared(s,"IDirect3DDevice9::DrawIndexedPrimitive");
    }
    // D3D9 clears stream 0 (and the index buffer for the indexed form) after a UP draw.
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE t,UINT n,const void* data,UINT stride) override {
        if(!data || !stride) return D3DERR_INVALIDCALL;
        DrawSource s{t,n}; s.upVertices=data; s.upStride=stride;
        const HRESULT hr=drawShared(s,"IDirect3DDevice9::DrawPrimitiveUP");
        if(SUCCEEDED(hr)) { state.streams[0].set(nullptr); state.offsets[0]=state.strides[0]=0; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE t,UINT minIndex,UINT vertices,UINT n,const void* indices,
                                                     D3DFORMAT format,const void* data,UINT stride) override {
        if(!data || !stride || !indices || (format!=D3DFMT_INDEX16 && format!=D3DFMT_INDEX32)) return D3DERR_INVALIDCALL;
        DrawSource s{t,n}; s.indexed=true; s.minIndex=minIndex; s.vertices=vertices; s.upVertices=data; s.upStride=stride;
        s.upIndices=indices; s.upFormat=format;
        const HRESULT hr=drawShared(s,"IDirect3DDevice9::DrawIndexedPrimitiveUP");
        if(SUCCEEDED(hr)) { state.streams[0].set(nullptr); state.offsets[0]=state.strides[0]=0; state.indices.set(nullptr); }
        return hr;
    }
