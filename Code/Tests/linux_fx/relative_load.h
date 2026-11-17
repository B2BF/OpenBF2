// [TEMP] [CHOSEN, not recovered] Synthetic matrix-array loads expose CSE's
// constant-storage comparison without depending on Battlefield assets.
#pragma once

constexpr const char relativeLoadSource[] = R"fx(
float4x3 Matrices[4];
struct Input {
    float4 position : POSITION0;
    float2 indices : BLENDINDICES0;
    float3 row0 : TEXCOORD0;
    float3 row1 : TEXCOORD1;
    float3 row2 : TEXCOORD2;
};
struct Output {
    float4 position : POSITION0;
    float3 a : TEXCOORD0;
    float3 b : TEXCOORD1;
    float3 c : TEXCOORD2;
    float3 d : TEXCOORD3;
    float3 e : TEXCOORD4;
    float3 f : TEXCOORD5;
};
Output relativeVertex(Input input) {
    Output result;
    float3x3 basis = float3x3(input.row0, input.row1, input.row2);
    int2 indices = (int2)input.indices;
    float3x3 first = transpose(mul(basis, Matrices[indices.x]));
    float3x3 second = transpose(mul(basis, Matrices[indices.y]));
    result.position = input.position;
    result.a = first[0]; result.b = first[1]; result.c = first[2];
    result.d = second[0]; result.e = second[1]; result.f = second[2];
    return result;
}
float4 relativePixel(Output input) : COLOR0 {
    return float4((input.a + input.b + input.c + (input.d + input.e + input.f) * 0.5) * 0.05, 1);
}
technique Main {
    pass Draw {
        AlphaBlendEnable = FALSE;
        VertexShader = compile vs_1_1 relativeVertex();
        PixelShader = compile ps_2_0 relativePixel();
    }
}
)fx";
