// [CHOSEN, not recovered] The reference run over every case of ff_cases.cpp through an
// IDirect3DDevice9: shared by the Windows generator (ff_reference_win.cpp, the real runtime) and
// the macOS device check (macos_metal_device --ff-device, Metal/FixedFunctionDraw.inc
// ProcessVertices), so both drive the device with exactly the same calls. Writes the case, matrix,
// vertex/clip/nocopy/offset, variants and end lines (format: ff_reference_win.cpp).
// Include after <cstdio>, <cstring>, the D3D9 declarations and ff_cases.h.
// FF_RUN_USAGE: the buffers' usage. The generator's software-vertex-processing device needs
// D3DUSAGE_SOFTWAREPROCESSING; the macOS device is a hardware-vertex-processing device, where
// buffers carry no such usage.
#ifndef FF_RUN_USAGE
#define FF_RUN_USAGE D3DUSAGE_SOFTWAREPROCESSING
#endif
namespace ffrun {
struct OutVertex { float x, y, z, rhw; DWORD diffuse, specular; float t0[4], t1[4]; };
const DWORD kOutFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX2 |
                      D3DFVF_TEXCOORDSIZE4(0) | D3DFVF_TEXCOORDSIZE4(1);

void putMatrix(FILE* f, const char* name, const Mat& m) {
    std::fprintf(f, "matrix %s", name);
    for (int i = 0; i < 16; ++i) std::fprintf(f, " %a", m.m[i]);
    std::fprintf(f, "\n");
}
D3DMATRIX dx(const Mat& m) { D3DMATRIX r; std::memcpy(&r, m.m, sizeof r); return r; }
D3DCOLORVALUE col(const float* c) { D3DCOLORVALUE r = { c[0], c[1], c[2], c[3] }; return r; }

DWORD inputFvf(const Case& c) {
    const int betas = c.weights + (c.indexedBlend ? 1 : 0);
    static const DWORD xyzb[6] = { D3DFVF_XYZ, D3DFVF_XYZB1, D3DFVF_XYZB2, D3DFVF_XYZB3, D3DFVF_XYZB4, D3DFVF_XYZB5 };
    DWORD fvf = xyzb[betas];
    if (c.indexedBlend) fvf |= D3DFVF_LASTBETA_UBYTE4;
    if (c.normal) fvf |= D3DFVF_NORMAL;
    if (c.diffuse) fvf |= D3DFVF_DIFFUSE;
    if (c.specular) fvf |= D3DFVF_SPECULAR;
    fvf |= static_cast<DWORD>(c.texSets) << D3DFVF_TEXCOUNT_SHIFT;
    return fvf;
}
// Packs one input vertex in FVF order; returns the byte size.
size_t pack(const Case& c, const Vertex& v, unsigned char* out) {
    unsigned char* p = out;
    auto putF = [&](float f) { std::memcpy(p, &f, 4); p += 4; };
    auto putU = [&](unsigned u) { std::memcpy(p, &u, 4); p += 4; };
    putF(v.x); putF(v.y); putF(v.z);
    const float w[3] = { v.w0, v.w1, v.w2 };
    for (int i = 0; i < c.weights; ++i) putF(w[i]);
    if (c.indexedBlend) putU(v.indices);
    if (c.normal) { putF(v.nx); putF(v.ny); putF(v.nz); }
    if (c.diffuse) putU(v.diffuse);
    if (c.specular) putU(v.specular);
    if (c.texSets > 0) { putF(v.u0); putF(v.v0); }
    if (c.texSets > 1) { putF(v.u1); putF(v.v1); }
    return static_cast<size_t>(p - out);
}
} // namespace

struct FFRunResult { int failures, written, vertexLines; };
static FFRunResult ffRun(FILE* f, IDirect3DDevice9* dev) {
    using namespace ffrun;
    HRESULT hr;
    int vcount = 0, ccount = 0;
    const Vertex* verts = vertices(vcount);
    const Case* list = cases(ccount);
    IDirect3DVertexBuffer9* dst = 0;
    hr = dev->CreateVertexBuffer(vcount * sizeof(OutVertex), FF_RUN_USAGE,
                                 kOutFvf, D3DPOOL_SYSTEMMEM, &dst, 0);
    if (FAILED(hr)) { std::fprintf(f, "error dest VB 0x%08lx\n", hr); return FFRunResult{1, 0, 0}; }
    // A missing or short case must not pass silently: every failure writes an "error" line and
    // the exit code is non-zero; the file ends with "end <cases> <vertex lines>".
    int failures = 0, written = 0, vertexLines = 0, extraBlocks = 0;
    if (ccount != kCaseCount || vcount != kVertexCount) {
        std::fprintf(f, "error table has %d cases and %d vertices, expected %d and %d\n", ccount, vcount, kCaseCount, kVertexCount);
        std::fprintf(stderr, "ff_reference: table has %d cases and %d vertices, expected %d and %d\n", ccount, vcount, kCaseCount, kVertexCount);
        ++failures;
    }
    for (int ci = 0; ci < ccount; ++ci) {
        const Case& c = list[ci];
        std::fprintf(f, "case %d %s\n", ci, c.name);
        static const char* names[8] = { "world0", "world1", "world2", "world3", "view", "projection", "texture0", "texture1" };
        const Mat* mats[8] = { &c.world[0], &c.world[1], &c.world[2], &c.world[3], &c.view, &c.projection, &c.texture[0], &c.texture[1] };
        for (int i = 0; i < 8; ++i) putMatrix(f, names[i], *mats[i]);

        const DWORD fvf = inputFvf(c);
        unsigned char packed[6 * 128];
        size_t stride = 0;
        for (int i = 0; i < vcount; ++i) stride = pack(c, verts[i], packed + i * 128);
        IDirect3DVertexBuffer9* src = 0;
        dev->CreateVertexBuffer(static_cast<UINT>(stride * vcount), FF_RUN_USAGE, fvf, D3DPOOL_SYSTEMMEM, &src, 0);
        void* p = 0;
        src->Lock(0, 0, &p, 0);
        for (int i = 0; i < vcount; ++i) std::memcpy(static_cast<unsigned char*>(p) + i * stride, packed + i * 128, stride);
        src->Unlock();

        dev->SetFVF(fvf);
        dev->SetStreamSource(0, src, 0, static_cast<UINT>(stride));
        for (int i = 0; i < 4; ++i) { D3DMATRIX m = dx(c.world[i]); dev->SetTransform(D3DTS_WORLDMATRIX(i), &m); }
        { D3DMATRIX m = dx(c.view); dev->SetTransform(D3DTS_VIEW, &m); }
        { D3DMATRIX m = dx(c.projection); dev->SetTransform(D3DTS_PROJECTION, &m); }
        { D3DMATRIX m = dx(c.texture[0]); dev->SetTransform(D3DTS_TEXTURE0, &m); }
        { D3DMATRIX m = dx(c.texture[1]); dev->SetTransform(D3DTS_TEXTURE1, &m); }
        D3DVIEWPORT9 vp = { (DWORD)c.viewport[0], (DWORD)c.viewport[1], (DWORD)c.viewport[2], (DWORD)c.viewport[3], c.viewport[4], c.viewport[5] };
        dev->SetViewport(&vp);
        dev->SetRenderState(D3DRS_CLIPPING, FALSE);
        dev->SetRenderState(D3DRS_VERTEXBLEND, c.vertexBlend);
        dev->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, c.indexedBlend);
        dev->SetRenderState(D3DRS_LIGHTING, c.lighting);
        dev->SetRenderState(D3DRS_COLORVERTEX, c.colorVertex);
        dev->SetRenderState(D3DRS_LOCALVIEWER, c.localViewer);
        dev->SetRenderState(D3DRS_NORMALIZENORMALS, c.normalizeNormals);
        dev->SetRenderState(D3DRS_SPECULARENABLE, c.specularEnable);
        dev->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, c.diffuseSource);
        dev->SetRenderState(D3DRS_SPECULARMATERIALSOURCE, c.specularSource);
        dev->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, c.ambientSource);
        dev->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, c.emissiveSource);
        dev->SetRenderState(D3DRS_AMBIENT, c.ambient);
        dev->SetRenderState(D3DRS_FOGENABLE, c.fogEnable);
        dev->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
        dev->SetRenderState(D3DRS_FOGVERTEXMODE, c.fogVertexMode);
        dev->SetRenderState(D3DRS_RANGEFOGENABLE, c.rangeFog);
        dev->SetRenderState(D3DRS_FOGSTART, *reinterpret_cast<const DWORD*>(&c.fogStart));
        dev->SetRenderState(D3DRS_FOGEND, *reinterpret_cast<const DWORD*>(&c.fogEnd));
        dev->SetRenderState(D3DRS_FOGDENSITY, *reinterpret_cast<const DWORD*>(&c.fogDensity));
        // Stages 0 and 1 enabled (stage 1 defaults to COLOROP DISABLE, and the runtime then writes no
        // second coordinate set); stage 2 ends the cascade.
        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
        dev->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
        for (DWORD s = 0; s < 2; ++s) {
            dev->SetTextureStageState(s, D3DTSS_TEXCOORDINDEX, c.tci[s]);
            dev->SetTextureStageState(s, D3DTSS_TEXTURETRANSFORMFLAGS, c.ttff[s]);
        }
        D3DMATERIAL9 m;
        m.Diffuse = col(c.material); m.Ambient = col(c.material + 4); m.Specular = col(c.material + 8);
        m.Emissive = col(c.material + 12); m.Power = c.material[16];
        dev->SetMaterial(&m);
        for (DWORD li = 0; li < 2; ++li) {
            const Light& L = c.lights[li];
            if (!L.type) { dev->LightEnable(li, FALSE); continue; }
            D3DLIGHT9 l = {};
            l.Type = static_cast<D3DLIGHTTYPE>(L.type);
            l.Diffuse = col(L.diffuse); l.Specular = col(L.specular); l.Ambient = col(L.ambient);
            l.Position.x = L.position[0]; l.Position.y = L.position[1]; l.Position.z = L.position[2];
            l.Direction.x = L.direction[0]; l.Direction.y = L.direction[1]; l.Direction.z = L.direction[2];
            l.Range = L.range; l.Falloff = L.falloff; l.Attenuation0 = L.attenuation0; l.Attenuation1 = L.attenuation1;
            l.Attenuation2 = L.attenuation2; l.Theta = L.theta; l.Phi = L.phi;
            dev->SetLight(li, &l);
            dev->LightEnable(li, TRUE);
        }
        // Variants (0.3.0 piece 3): "vertex" = CLIPPING FALSE, all vertices; "clip" = CLIPPING TRUE;
        // "nocopy" = D3DPV_DONOTCOPYDATA; "offset" = source vertices 2..4 to destination 1..3.
        // Components the runtime does not write keep a NaN sentinel ("nan" in the file; colours
        // 0xdeadbeef), so nothing carries over from the previous variant or case.
        struct Variant { const char* tag; BOOL clipping; DWORD flags; UINT src, dst, count; };
        const Variant variants[4] = { { "vertex", FALSE, 0, 0, 0, (UINT)vcount }, { "clip", TRUE, 0, 0, 0, (UINT)vcount },
                                      { "nocopy", FALSE, D3DPV_DONOTCOPYDATA, 0, 0, (UINT)vcount }, { "offset", FALSE, 0, 2, 1, 3 } };
        bool caseOk = true;
        for (int vi = 0; vi < 4 && caseOk; ++vi) {
            const Variant& va = variants[vi];
            OutVertex* o = 0;
            if (SUCCEEDED(dst->Lock(0, 0, reinterpret_cast<void**>(&o), 0)) && o) {
                const unsigned nanBits = 0x7FC0DEADu;
                float nan; std::memcpy(&nan, &nanBits, 4);
                for (int i = 0; i < vcount; ++i) {
                    o[i].x = o[i].y = o[i].z = o[i].rhw = nan;
                    o[i].diffuse = o[i].specular = 0xDEADBEEFu;
                    for (int j = 0; j < 4; ++j) o[i].t0[j] = o[i].t1[j] = nan;
                }
                dst->Unlock();
            }
            dev->SetRenderState(D3DRS_CLIPPING, va.clipping);
            hr = dev->ProcessVertices(va.src, va.dst, va.count, dst, 0, va.flags);
            if (FAILED(hr)) {
                std::fprintf(f, "error case %d %s ProcessVertices 0x%08lx\n", ci, va.tag, hr);
                std::fprintf(stderr, "ff_reference: case %d (%s) %s ProcessVertices 0x%08lx\n", ci, c.name, va.tag, hr);
                ++failures; caseOk = false; break;
            }
            o = 0;
            if (FAILED(dst->Lock(0, 0, reinterpret_cast<void**>(&o), D3DLOCK_READONLY)) || !o) {
                std::fprintf(f, "error case %d %s Lock\n", ci, va.tag);
                ++failures; caseOk = false; break;
            }
            if (vi == 0) { ++written; vertexLines += vcount; } else ++extraBlocks;
            for (int i = 0; i < vcount; ++i) {
                const OutVertex& v = o[i];
                std::fprintf(f, "%s %d %a %a %a %a %08lx %08lx %a %a %a %a %a %a %a %a\n", va.tag, i, v.x, v.y, v.z, v.rhw,
                             v.diffuse, v.specular, v.t0[0], v.t0[1], v.t0[2], v.t0[3], v.t1[0], v.t1[1], v.t1[2], v.t1[3]);
            }
            dst->Unlock();
        }
        dev->SetRenderState(D3DRS_CLIPPING, FALSE);
        src->Release();
    }
    dst->Release();
    if (written != kCaseCount || vertexLines != kCaseCount * kVertexCount) {
        std::fprintf(f, "error wrote %d cases and %d vertex lines, expected %d and %d\n", written, vertexLines,
                     kCaseCount, kCaseCount * kVertexCount);
        ++failures;
    }
    if (extraBlocks != kCaseCount * 3) {
        std::fprintf(f, "error wrote %d variant blocks, expected %d\n", extraBlocks, kCaseCount * 3);
        ++failures;
    }
    std::fprintf(f, "variants %d\n", extraBlocks);
    std::fprintf(f, "end %d %d\n", written, vertexLines);
    return FFRunResult{failures, written, vertexLines};
}
