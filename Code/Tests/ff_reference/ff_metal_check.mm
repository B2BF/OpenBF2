// [CHOSEN, not recovered] Runs the generated fixed-function vertex MSL (FFVertex.h vertexMsl) on the
// Metal GPU for every case in ff_cases.cpp and compares each output with the CPU model (process),
// which ff_check compares with the real d3d9 runtime. The vertex body is called from a compute
// kernel, so the outputs are read back exactly. Usage: ff_metal_check [--all] [--dump]
#include "ff_setup.h"
#import <Metal/Metal.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
bool close(float a, float b) {
    return std::fabs(a - b) <= 2e-5f * std::fmax(1.0f, std::fmax(std::fabs(a), std::fabs(b)));
}
} // namespace

int main(int argc, char** argv) {
    bool all = false, dump = false;
    for (int i = 1; i < argc; ++i) { if (!std::strcmp(argv[i], "--all")) all = true; if (!std::strcmp(argv[i], "--dump")) dump = true; }
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device) { std::fprintf(stderr, "ff_metal_check: no Metal device\n"); return 2; }
        id<MTLCommandQueue> queue = [device newCommandQueue];
        int count = 0, vcount = 0;
        const ffref::Case* cases = ffref::cases(count);
        const ffref::Vertex* verts = ffref::vertices(vcount);
        int failed = 0, checked = 0, skipped = 0;
        for (int ci = 0; ci < count; ++ci) {
            const ffref::Case& c = cases[ci];
            bf2ff::Key k;
            static bf2ff::Constants K;
            const ffref::Mat mats[8] = {c.world[0], c.world[1], c.world[2], c.world[3], c.view, c.projection, c.texture[0], c.texture[1]};
            ffref::setup(c, mats, k, K);

            std::vector<bf2ff::Input> inputs;
            std::string msl = "#include <metal_stdlib>\nusing namespace metal;\n" + bf2ff::vertexMsl(k, inputs);
            const unsigned outSlots = 4 + k.outputs;
            msl += "kernel void bf2_ff_test(device const bf2_ff_in* vin [[buffer(1)]], constant float4* c [[buffer(0)]],\n"
                   "    device float4* outp [[buffer(2)]], uint id [[thread_position_in_grid]]) {\n"
                   "\tconst bf2_ff_in i = vin[id];\n\tconst bf2_ff_out o = bf2_ff_transform(i, c, float4(0.0, 0.0, 1.0, 1.0));\n"
                   "\tdevice float4* d = outp + id * " + std::to_string(outSlots) + ";\n"
                   "\td[0] = o.position; d[1] = o.color0; d[2] = o.color1; d[3] = o.fog;\n";
            for (unsigned s = 0; s < k.outputs; ++s) msl += "\td[" + std::to_string(4 + s) + "] = o.texcoord" + std::to_string(s) + ";\n";
            msl += "}\n";
            NSError* error = nil;
            id<MTLLibrary> lib = [device newLibraryWithSource:[NSString stringWithUTF8String:msl.c_str()] options:nil error:&error];
            if (!lib) {
                std::printf("FAIL %s: MSL does not compile: %s\n", c.name, error.localizedDescription.UTF8String);
                if (dump) std::printf("%s\n", msl.c_str());
                ++failed; continue;
            }
            id<MTLComputePipelineState> pso = [device newComputePipelineStateWithFunction:[lib newFunctionWithName:@"bf2_ff_test"] error:&error];
            if (!pso) { std::printf("FAIL %s: pipeline: %s\n", c.name, error.localizedDescription.UTF8String); ++failed; continue; }
            // Inputs as the vertex fetch delivers them: every attribute a float4 in register order.
            std::vector<float> in(size_t(vcount) * inputs.size() * 4, 0.0f);
            std::vector<bf2ff::VertexIn> cpu(vcount);
            for (int vi = 0; vi < vcount; ++vi) {
                const ffref::Vertex& v = verts[vi];
                bf2ff::VertexIn& x = cpu[vi];
                ffref::vertexIn(c, v, x);
                for (size_t a = 0; a < inputs.size(); ++a) {
                    float* d = &in[(vi * inputs.size() + a) * 4];
                    float f[4] = {0, 0, 0, 1};
                    switch (inputs[a].usage) {
                    case bf2ff::Position: std::memcpy(f, x.position, 16); break;
                    case bf2ff::BlendWeight: for (int j = 0; j < k.weights; ++j) f[j] = x.weights[j]; break;
                    case bf2ff::BlendIndices: for (int j = 0; j < 4; ++j) f[j] = x.indices[j]; break;   // UBYTE4
                    case bf2ff::Normal: std::memcpy(f, x.normal, 12); break;
                    case bf2ff::Color: std::memcpy(f, inputs[a].index ? x.specular : x.diffuse, 16); break;
                    case bf2ff::TexCoord: std::memcpy(f, x.tex[inputs[a].index], 16); break;   // FLOAT2 -> (u, v, 0, 1)
                    default: break;
                    }
                    std::memcpy(d, f, 16);
                }
            }
            std::vector<std::uint8_t> constants;
            bf2ff::pack(k, K, constants);
            id<MTLBuffer> bIn = [device newBufferWithBytes:in.data() length:in.size() * 4 options:MTLResourceStorageModeShared];
            id<MTLBuffer> bC = [device newBufferWithBytes:constants.data() length:constants.size() options:MTLResourceStorageModeShared];
            id<MTLBuffer> bOut = [device newBufferWithLength:size_t(vcount) * outSlots * 16 options:MTLResourceStorageModeShared];
            id<MTLCommandBuffer> cb = [queue commandBuffer];
            id<MTLComputeCommandEncoder> e = [cb computeCommandEncoder];
            [e setComputePipelineState:pso];
            [e setBuffer:bC offset:0 atIndex:0]; [e setBuffer:bIn offset:0 atIndex:1]; [e setBuffer:bOut offset:0 atIndex:2];
            [e dispatchThreads:MTLSizeMake(vcount, 1, 1) threadsPerThreadgroup:MTLSizeMake(vcount, 1, 1)];
            [e endEncoding]; [cb commit]; [cb waitUntilCompleted];
            const float* g = static_cast<const float*>(bOut.contents);
            bool ok = true;
            for (int vi = 0; vi < vcount; ++vi) {
                bf2ff::VertexOut o;
                bf2ff::process(k, K, cpu[vi], o);
                const float* d = g + vi * outSlots * 4;
                const float fog[4] = {o.fog, o.fog, o.fog, o.fog};
                std::vector<const float*> want = {o.clip, o.diffuse, o.specular, fog};
                for (unsigned s = 0; s < k.outputs; ++s) want.push_back(o.tex[s]);
                static const char* names[] = {"position", "color0", "color1", "fog", "texcoord0", "texcoord1"};
                for (unsigned slot = 0; slot < outSlots; ++slot)
                    for (int j = 0; j < 4; ++j)
                        if (!close(d[slot * 4 + j], want[slot][j])) {
                            if (ok) std::printf("FAIL %s\n", c.name);
                            ok = false;
                            std::printf("  vertex %d %s: gpu %g %g %g %g cpu %g %g %g %g\n", vi, names[slot],
                                        d[slot * 4], d[slot * 4 + 1], d[slot * 4 + 2], d[slot * 4 + 3],
                                        want[slot][0], want[slot][1], want[slot][2], want[slot][3]);
                            break;
                        }
            }
            ++checked;
            if (ok) std::printf("ok   %s\n", c.name); else ++failed;
        }
        std::printf("ff_metal_check: %d checked, %d failed\n", checked, failed);
        return failed ? 1 : 0;
    }
}
