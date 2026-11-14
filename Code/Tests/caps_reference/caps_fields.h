// [CHOSEN, not recovered] D3DCAPS9 fields as an X-macro, so the Windows reference dump
// (caps_win.cpp, the real HAL device) and the macOS device (macos_metal_device --caps) print the
// same "name value" lines. Include <cstdio> first. U: DWORD/UINT/INT printed as hex, F: float printed as %g.
#define BF2_CAPS_FIELDS(U, F) \
    U(DeviceType) U(Caps) U(Caps2) U(Caps3) U(PresentationIntervals) U(CursorCaps) U(DevCaps) \
    U(PrimitiveMiscCaps) U(RasterCaps) U(ZCmpCaps) U(SrcBlendCaps) U(DestBlendCaps) U(AlphaCmpCaps) \
    U(ShadeCaps) U(TextureCaps) U(TextureFilterCaps) U(CubeTextureFilterCaps) U(VolumeTextureFilterCaps) \
    U(TextureAddressCaps) U(VolumeTextureAddressCaps) U(LineCaps) U(MaxTextureWidth) U(MaxTextureHeight) \
    U(MaxVolumeExtent) U(MaxTextureRepeat) U(MaxTextureAspectRatio) U(MaxAnisotropy) \
    F(MaxVertexW) F(GuardBandLeft) F(GuardBandTop) F(GuardBandRight) F(GuardBandBottom) F(ExtentsAdjust) \
    U(StencilCaps) U(FVFCaps) U(TextureOpCaps) U(MaxTextureBlendStages) U(MaxSimultaneousTextures) \
    U(VertexProcessingCaps) U(MaxActiveLights) U(MaxUserClipPlanes) U(MaxVertexBlendMatrices) \
    U(MaxVertexBlendMatrixIndex) F(MaxPointSize) U(MaxPrimitiveCount) U(MaxVertexIndex) U(MaxStreams) \
    U(MaxStreamStride) U(VertexShaderVersion) U(MaxVertexShaderConst) U(PixelShaderVersion) \
    F(PixelShader1xMaxValue) U(DevCaps2) F(MaxNpatchTessellationLevel) U(NumberOfAdaptersInGroup) \
    U(DeclTypes) U(NumSimultaneousRTs) U(StretchRectFilterCaps) \
    U(VS20Caps.Caps) U(VS20Caps.DynamicFlowControlDepth) U(VS20Caps.NumTemps) U(VS20Caps.StaticFlowControlDepth) \
    U(PS20Caps.Caps) U(PS20Caps.DynamicFlowControlDepth) U(PS20Caps.NumTemps) U(PS20Caps.StaticFlowControlDepth) \
    U(PS20Caps.NumInstructionSlots) U(VertexTextureFilterCaps) U(MaxVShaderInstructionsExecuted) \
    U(MaxPShaderInstructionsExecuted) U(MaxVertexShader30InstructionSlots) U(MaxPixelShader30InstructionSlots)

// Prints every field of `caps` (a D3DCAPS9) to `f`.
// Ends with "fields N" (the number printed); caps_compare.py checks it against the lines it read.
template <class Caps> void bf2PrintCaps(std::FILE* f, const Caps& caps) {
    int n_ = 0;
#define BF2_CAPS_U_(n) std::fprintf(f, "%s 0x%08lx\n", #n, (unsigned long)(caps.n)); ++n_;
#define BF2_CAPS_F_(n) std::fprintf(f, "%s %g\n", #n, (double)(caps.n)); ++n_;
    BF2_CAPS_FIELDS(BF2_CAPS_U_, BF2_CAPS_F_)
    std::fprintf(f, "fields %d\n", n_);
#undef BF2_CAPS_U_
#undef BF2_CAPS_F_
}
