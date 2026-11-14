# D3D9 and D3DX source inventory

[CHOSEN, not recovered] Read-only planning tool for the owner-requested macOS port. This inventories current source, not recovered binary behavior or executed paths.

Reproduce from the repository root: `python3 tools/macos/d3d9_inventory.py`. Python 3 standard library only; no SDK, compiler, game content or network required. Outputs are deterministic (no timestamps); `out/inventory/manifest.json` records input hashes. CSV arguments preserve the full whitespace-normalized source expression, including numeric literals, casts and enum names. Additional CSV columns preserve receiver, dispatch kind and scope.

## Scope and interpretation

Requested trees: `Code/BF2/RendDX9/`, `Code/BF2/Geom/`, `Code/BF2/GeomMisc/`, `Code/BF2/RendMisc/`, `Code/RendDX9Abi/`. Headers/inline bodies and inactive Windows/editor arms are included once per lexical site. TextureAtlasBuilder is included by the requested directory scope but is a separate DLL; its sites must not be mistaken for current RendDX9.dll imports. Image manifests: `Code/Build/renddx9.cmd`, `Code/Build/textureatlasbuilder.cmd`, and `BF2_RENDDX9_STATIC_SOURCES` in `CMakeLists.txt`. Other files under `Code/` naming D3D/D3DX types are scanned and reported separately; these include editor, tests and vendors.

The scanner strips comments/strings for matching, balances parentheses/indexing, recognizes typed declarations, explicit casts, unambiguous member names and function returns. Unambiguous cross-file declarations are lexical evidence, not compiler-verified overload resolution. No interface is guessed solely from a method name. Candidate COM-method names with unknown receivers are retained as `unresolved` (some may be engine methods). `IUnknown` calls preserve the declared type; they do not establish which D3D resource is held. D3DX slot adapters and immediate named/raw CommitChanges dispatches are included. Dynamic slot dispatch is retained unresolved; template fan-out is counted at its physical dispatch body, not once per instantiation. This is a lexical inventory, not proof that arbitrary aliasing/macros/raw vtables contain no additional GPU calls.

**Requested-tree totals: 833 calls; 520 D3D9/candidates, 313 D3DX; 12 unresolved.** Outside scope: 668 calls/candidates. Scanned 304 files.

| Image partition within requested trees | Sites |
| --- | --- |
| RendDX9 source/header/inline sites | 797 |
| TextureAtlasBuilder sibling DLL | 36 |

## Totals by interface

| Interface | Sites |
| --- | --- |
| IDirect3DSurface9 | 196 |
| ID3DXEffect | 142 |
| IDirect3DDevice9 | 135 |
| D3DX free | 85 |
| ID3DXMesh | 48 |
| IDirect3DTexture9 | 45 |
| IDirect3DBaseTexture9 | 29 |
| IUnknown | 27 |
| ID3DXBuffer | 21 |
| IDirect3D9 | 20 |
| IDirect3DIndexBuffer9 | 12 |
| IDirect3DVertexBuffer9 | 12 |
| unresolved | 12 |
| ID3DXEffectCompiler | 8 |
| IDirect3DQuery9 | 8 |
| ID3DXRenderToSurface | 6 |
| IDirect3DCubeTexture9 | 6 |
| IDirect3DVertexDeclaration9 | 6 |
| IDirect3DVolumeTexture9 | 4 |
| ID3DXInclude | 3 |
| IDirect3DResource9 | 3 |
| IDirect3DPixelShader9 | 2 |
| IDirect3DSwapChain9 | 2 |
| D3D9 free | 1 |

No direct calls were found on `IDirect3DStateBlock9`, `IDirect3DVertexShader9`, `IDirect3DVolume9`, `ID3DXFont` or `ID3DXSprite`. Shader creation/binding and generic IUnknown releases still belong to the seam; zero direct calls does not mean a resource type is absent.

## Totals by method name

| Method | Sites |
| --- | --- |
| Release | 214 |
| LockRect | 44 |
| UnlockRect | 44 |
| AddRef | 31 |
| D3DXLoadSurfaceFromSurface | 14 |
| GetBufferPointer | 12 |
| QueryInterface | 12 |
| SetTexture | 12 |
| D3DXFloat32To16Array | 10 |
| GetSurfaceLevel | 10 |
| CreateDepthStencilSurface | 8 |
| GetParameterDesc | 8 |
| GetNumFaces | 7 |
| SetTechnique | 7 |
| Clear | 6 |
| CommitChanges | 6 |
| D3DXCreateTexture | 6 |
| EndPass | 6 |
| EndScene | 6 |
| FindNextValidTechnique | 6 |
| GetDepthStencilSurface | 6 |
| GetDesc | 6 |
| GetParameterByName | 6 |
| SetArrayRange | 6 |
| SetFloatArray | 6 |
| SetRenderState | 6 |
| Unlock | 6 |
| Begin | 5 |
| BeginPass | 5 |
| BeginScene | 5 |
| CheckDeviceFormat | 5 |
| CreateRenderTarget | 5 |
| D3DXSaveSurfaceToFileA | 5 |
| End | 5 |
| GetAnnotationByName | 5 |
| GetIndexBuffer | 5 |
| GetNumVertices | 5 |
| GetTechniqueByName | 5 |
| GetVertexBuffer | 5 |
| SetDepthStencilSurface | 5 |
| SetValue | 5 |
| SetViewport | 5 |
| CloneMeshFVF | 4 |
| CreateDevice | 4 |
| D3DXCreateEffect | 4 |
| D3DXCreateEffectCompiler | 4 |
| GetBufferSize | 4 |
| GetTechniqueDesc | 4 |
| GetViewport | 4 |
| Lock | 4 |
| OnLostDevice | 4 |
| PreLoad | 4 |
| SetFloat | 4 |
| SetInt | 4 |
| SetScissorRect | 4 |
| SetVector | 4 |
| SetVertexShaderConstantF | 4 |
| UpdateSurface | 4 |
| CreateIndexBuffer | 3 |
| CreateOffscreenPlainSurface | 3 |
| CreateTexture | 3 |
| CreateVertexBuffer | 3 |
| CreateVertexDeclaration | 3 |
| D3DXComputeBoundingSphere | 3 |
| D3DXCreateSphere | 3 |
| D3DXFilterTexture | 3 |
| D3DXMatrixMultiply | 3 |
| D3DXSaveTextureToFileA | 3 |
| GetBackBuffer | 3 |
| GetDeclaration | 3 |
| GetInt | 3 |
| GetLevelCount | 3 |
| GetParameterElement | 3 |
| GetString | 3 |
| LockIndexBuffer | 3 |
| SetBool | 3 |
| StretchRect | 3 |
| UnlockIndexBuffer | 3 |
| Close | 2 |
| CreateQuery | 2 |
| CreateVolumeTexture | 2 |
| D3DXAssembleShader | 2 |
| D3DXCreateBuffer | 2 |
| D3DXGetImageInfoFromFileInMemory | 2 |
| D3DXLoadSurfaceFromMemory | 2 |
| D3DXMatrixInverse | 2 |
| D3DXMatrixOrthoLH | 2 |
| D3DXMatrixRotationZ | 2 |
| DrawIndexedPrimitive | 2 |
| DrawPrimitiveUP | 2 |
| EnumAdapterModes | 2 |
| GetAdapterModeCount | 2 |
| GetBool | 2 |
| GetCubeMapSurface | 2 |
| GetData | 2 |
| GetDeviceCaps | 2 |
| GetFunction | 2 |
| GetRenderTargetData | 2 |
| GetSwapChain | 2 |
| Issue | 2 |
| OnResetDevice | 2 |
| Present | 2 |
| SetIndices | 2 |
| SetMatrix | 2 |
| SetMatrixArray | 2 |
| SetMatrixPointerArray | 2 |
| SetRenderTarget | 2 |
| SetStateManager | 2 |
| SetStreamSource | 2 |
| SetTextureStageState | 2 |
| SetVectorArray | 2 |
| TestCooperativeLevel | 2 |
| CheckDeviceMultiSampleType | 1 |
| CheckDeviceType | 1 |
| CloneMesh | 1 |
| CompileEffect | 1 |
| CreateAdditionalSwapChain | 1 |
| CreateCubeTexture | 1 |
| CreatePixelShader | 1 |
| CreateVertexShader | 1 |
| D3DXComputeBoundingBox | 1 |
| D3DXCreateBox | 1 |
| D3DXCreateCylinder | 1 |
| D3DXCreateMesh | 1 |
| D3DXCreateMeshFVF | 1 |
| D3DXCreateRenderToSurface | 1 |
| D3DXCreateTextureFromFileExA | 1 |
| D3DXCreateTextureFromFileInMemory | 1 |
| D3DXFillTexture | 1 |
| D3DXGetImageInfoFromFileA | 1 |
| D3DXGetShaderVersion | 1 |
| D3DXMatrixLookAtLH | 1 |
| D3DXMatrixTranspose | 1 |
| Direct3DCreate9 | 1 |
| DrawPrimitive | 1 |
| GenerateAdjacency | 1 |
| GetAdapterCount | 1 |
| GetAdapterDisplayMode | 1 |
| GetAdapterIdentifier | 1 |
| GetIntArray | 1 |
| GetLevelDesc | 1 |
| GetParameter | 1 |
| GetPass | 1 |
| GetTechnique | 1 |
| GetType | 1 |
| IsParameterUsed | 1 |
| LockBox | 1 |
| LockVertexBuffer | 1 |
| Open | 1 |
| OptimizeInplace | 1 |
| Reset | 1 |
| SetBoolArray | 1 |
| SetClipPlane | 1 |
| SetIntArray | 1 |
| SetPixelShader | 1 |
| SetPixelShaderConstantB | 1 |
| SetPixelShaderConstantF | 1 |
| SetPixelShaderConstantI | 1 |
| SetPriority | 1 |
| SetSamplerState | 1 |
| SetVertexDeclaration | 1 |
| SetVertexShader | 1 |
| SetVertexShaderConstantB | 1 |
| SetVertexShaderConstantI | 1 |
| UnlockBox | 1 |
| UnlockVertexBuffer | 1 |
| ValidateTechnique | 1 |

## Totals by interface and method

| Interface::method | Sites |
| --- | --- |
| IDirect3DSurface9::Release | 104 |
| IDirect3DSurface9::LockRect | 40 |
| IDirect3DSurface9::UnlockRect | 39 |
| IUnknown::Release | 23 |
| IDirect3DTexture9::Release | 22 |
| D3DX free::D3DXLoadSurfaceFromSurface | 14 |
| ID3DXBuffer::GetBufferPointer | 12 |
| IDirect3DBaseTexture9::Release | 12 |
| D3DX free::D3DXFloat32To16Array | 10 |
| ID3DXEffect::SetTexture | 10 |
| ID3DXMesh::Release | 10 |
| IDirect3DTexture9::GetSurfaceLevel | 10 |
| IDirect3DBaseTexture9::QueryInterface | 9 |
| IDirect3DSurface9::AddRef | 9 |
| IDirect3DDevice9::CreateDepthStencilSurface | 8 |
| ID3DXEffect::SetTechnique | 7 |
| ID3DXMesh::GetNumFaces | 7 |
| IDirect3DIndexBuffer9::Release | 7 |
| IDirect3DVertexBuffer9::Release | 7 |
| unresolved::Release | 7 |
| D3DX free::D3DXCreateTexture | 6 |
| ID3DXEffect::CommitChanges | 6 |
| ID3DXEffect::EndPass | 6 |
| ID3DXEffect::FindNextValidTechnique | 6 |
| ID3DXEffect::GetParameterDesc | 6 |
| ID3DXEffect::SetArrayRange | 6 |
| ID3DXEffect::SetFloatArray | 6 |
| IDirect3DDevice9::AddRef | 6 |
| IDirect3DDevice9::Clear | 6 |
| IDirect3DDevice9::GetDepthStencilSurface | 6 |
| IDirect3DDevice9::SetRenderState | 6 |
| D3DX free::D3DXSaveSurfaceToFileA | 5 |
| ID3DXBuffer::Release | 5 |
| ID3DXEffect::Begin | 5 |
| ID3DXEffect::BeginPass | 5 |
| ID3DXEffect::End | 5 |
| ID3DXEffect::GetParameterByName | 5 |
| ID3DXEffect::GetTechniqueByName | 5 |
| ID3DXEffect::SetValue | 5 |
| ID3DXMesh::GetIndexBuffer | 5 |
| ID3DXMesh::GetNumVertices | 5 |
| ID3DXMesh::GetVertexBuffer | 5 |
| IDirect3D9::CheckDeviceFormat | 5 |
| IDirect3DBaseTexture9::AddRef | 5 |
| IDirect3DDevice9::CreateRenderTarget | 5 |
| IDirect3DDevice9::SetDepthStencilSurface | 5 |
| IDirect3DDevice9::SetViewport | 5 |
| D3DX free::D3DXCreateEffect | 4 |
| D3DX free::D3DXCreateEffectCompiler | 4 |
| ID3DXBuffer::GetBufferSize | 4 |
| ID3DXEffect::GetAnnotationByName | 4 |
| ID3DXEffect::GetTechniqueDesc | 4 |
| ID3DXEffect::OnLostDevice | 4 |
| ID3DXEffect::SetFloat | 4 |
| ID3DXEffect::SetInt | 4 |
| ID3DXEffect::SetVector | 4 |
| ID3DXMesh::CloneMeshFVF | 4 |
| IDirect3D9::CreateDevice | 4 |
| IDirect3DDevice9::BeginScene | 4 |
| IDirect3DDevice9::EndScene | 4 |
| IDirect3DDevice9::GetViewport | 4 |
| IDirect3DDevice9::SetScissorRect | 4 |
| IDirect3DDevice9::SetVertexShaderConstantF | 4 |
| IDirect3DDevice9::UpdateSurface | 4 |
| IDirect3DQuery9::Release | 4 |
| IDirect3DSurface9::GetDesc | 4 |
| IDirect3DTexture9::UnlockRect | 4 |
| IUnknown::AddRef | 4 |
| D3DX free::D3DXComputeBoundingSphere | 3 |
| D3DX free::D3DXCreateSphere | 3 |
| D3DX free::D3DXFilterTexture | 3 |
| D3DX free::D3DXMatrixMultiply | 3 |
| D3DX free::D3DXSaveTextureToFileA | 3 |
| ID3DXEffect::GetInt | 3 |
| ID3DXEffect::SetBool | 3 |
| ID3DXMesh::LockIndexBuffer | 3 |
| ID3DXMesh::UnlockIndexBuffer | 3 |
| ID3DXRenderToSurface::Release | 3 |
| IDirect3DDevice9::CreateIndexBuffer | 3 |
| IDirect3DDevice9::CreateOffscreenPlainSurface | 3 |
| IDirect3DDevice9::CreateTexture | 3 |
| IDirect3DDevice9::CreateVertexBuffer | 3 |
| IDirect3DDevice9::CreateVertexDeclaration | 3 |
| IDirect3DDevice9::Release | 3 |
| IDirect3DDevice9::StretchRect | 3 |
| IDirect3DResource9::PreLoad | 3 |
| IDirect3DTexture9::GetLevelCount | 3 |
| IDirect3DTexture9::LockRect | 3 |
| IDirect3DVertexDeclaration9::GetDeclaration | 3 |
| D3DX free::D3DXAssembleShader | 2 |
| D3DX free::D3DXCreateBuffer | 2 |
| D3DX free::D3DXGetImageInfoFromFileInMemory | 2 |
| D3DX free::D3DXLoadSurfaceFromMemory | 2 |
| D3DX free::D3DXMatrixInverse | 2 |
| D3DX free::D3DXMatrixOrthoLH | 2 |
| D3DX free::D3DXMatrixRotationZ | 2 |
| ID3DXEffect::GetBool | 2 |
| ID3DXEffect::GetParameterElement | 2 |
| ID3DXEffect::GetString | 2 |
| ID3DXEffect::OnResetDevice | 2 |
| ID3DXEffect::Release | 2 |
| ID3DXEffect::SetMatrix | 2 |
| ID3DXEffect::SetMatrixArray | 2 |
| ID3DXEffect::SetMatrixPointerArray | 2 |
| ID3DXEffect::SetStateManager | 2 |
| ID3DXEffect::SetVectorArray | 2 |
| ID3DXEffectCompiler::GetParameterDesc | 2 |
| ID3DXInclude::Close | 2 |
| ID3DXRenderToSurface::EndScene | 2 |
| IDirect3D9::EnumAdapterModes | 2 |
| IDirect3D9::GetAdapterModeCount | 2 |
| IDirect3DCubeTexture9::GetCubeMapSurface | 2 |
| IDirect3DDevice9::CreateQuery | 2 |
| IDirect3DDevice9::CreateVolumeTexture | 2 |
| IDirect3DDevice9::DrawIndexedPrimitive | 2 |
| IDirect3DDevice9::DrawPrimitiveUP | 2 |
| IDirect3DDevice9::GetBackBuffer | 2 |
| IDirect3DDevice9::GetRenderTargetData | 2 |
| IDirect3DDevice9::GetSwapChain | 2 |
| IDirect3DDevice9::SetIndices | 2 |
| IDirect3DDevice9::SetRenderTarget | 2 |
| IDirect3DDevice9::SetStreamSource | 2 |
| IDirect3DDevice9::SetTexture | 2 |
| IDirect3DDevice9::SetTextureStageState | 2 |
| IDirect3DDevice9::TestCooperativeLevel | 2 |
| IDirect3DIndexBuffer9::Unlock | 2 |
| IDirect3DPixelShader9::GetFunction | 2 |
| IDirect3DQuery9::GetData | 2 |
| IDirect3DQuery9::Issue | 2 |
| IDirect3DSwapChain9::Present | 2 |
| IDirect3DVertexBuffer9::Unlock | 2 |
| IDirect3DVertexDeclaration9::Release | 2 |
| unresolved::Lock | 2 |
| unresolved::Unlock | 2 |
| D3D9 free::Direct3DCreate9 | 1 |
| D3DX free::D3DXComputeBoundingBox | 1 |
| D3DX free::D3DXCreateBox | 1 |
| D3DX free::D3DXCreateCylinder | 1 |
| D3DX free::D3DXCreateMesh | 1 |
| D3DX free::D3DXCreateMeshFVF | 1 |
| D3DX free::D3DXCreateRenderToSurface | 1 |
| D3DX free::D3DXCreateTextureFromFileExA | 1 |
| D3DX free::D3DXCreateTextureFromFileInMemory | 1 |
| D3DX free::D3DXFillTexture | 1 |
| D3DX free::D3DXGetImageInfoFromFileA | 1 |
| D3DX free::D3DXGetShaderVersion | 1 |
| D3DX free::D3DXMatrixLookAtLH | 1 |
| D3DX free::D3DXMatrixTranspose | 1 |
| ID3DXEffect::AddRef | 1 |
| ID3DXEffect::GetIntArray | 1 |
| ID3DXEffect::GetParameter | 1 |
| ID3DXEffect::GetPass | 1 |
| ID3DXEffect::GetTechnique | 1 |
| ID3DXEffect::IsParameterUsed | 1 |
| ID3DXEffect::SetBoolArray | 1 |
| ID3DXEffect::SetIntArray | 1 |
| ID3DXEffect::ValidateTechnique | 1 |
| ID3DXEffectCompiler::CompileEffect | 1 |
| ID3DXEffectCompiler::GetAnnotationByName | 1 |
| ID3DXEffectCompiler::GetParameterByName | 1 |
| ID3DXEffectCompiler::GetParameterElement | 1 |
| ID3DXEffectCompiler::GetString | 1 |
| ID3DXEffectCompiler::Release | 1 |
| ID3DXInclude::Open | 1 |
| ID3DXMesh::AddRef | 1 |
| ID3DXMesh::CloneMesh | 1 |
| ID3DXMesh::GenerateAdjacency | 1 |
| ID3DXMesh::LockVertexBuffer | 1 |
| ID3DXMesh::OptimizeInplace | 1 |
| ID3DXMesh::UnlockVertexBuffer | 1 |
| ID3DXRenderToSurface::BeginScene | 1 |
| IDirect3D9::AddRef | 1 |
| IDirect3D9::CheckDeviceMultiSampleType | 1 |
| IDirect3D9::CheckDeviceType | 1 |
| IDirect3D9::GetAdapterCount | 1 |
| IDirect3D9::GetAdapterDisplayMode | 1 |
| IDirect3D9::GetAdapterIdentifier | 1 |
| IDirect3D9::GetDeviceCaps | 1 |
| IDirect3DBaseTexture9::GetType | 1 |
| IDirect3DBaseTexture9::PreLoad | 1 |
| IDirect3DBaseTexture9::SetPriority | 1 |
| IDirect3DCubeTexture9::LockRect | 1 |
| IDirect3DCubeTexture9::QueryInterface | 1 |
| IDirect3DCubeTexture9::Release | 1 |
| IDirect3DCubeTexture9::UnlockRect | 1 |
| IDirect3DDevice9::CreateAdditionalSwapChain | 1 |
| IDirect3DDevice9::CreateCubeTexture | 1 |
| IDirect3DDevice9::CreatePixelShader | 1 |
| IDirect3DDevice9::CreateVertexShader | 1 |
| IDirect3DDevice9::DrawPrimitive | 1 |
| IDirect3DDevice9::GetDeviceCaps | 1 |
| IDirect3DDevice9::Reset | 1 |
| IDirect3DDevice9::SetClipPlane | 1 |
| IDirect3DDevice9::SetPixelShader | 1 |
| IDirect3DDevice9::SetPixelShaderConstantB | 1 |
| IDirect3DDevice9::SetPixelShaderConstantF | 1 |
| IDirect3DDevice9::SetPixelShaderConstantI | 1 |
| IDirect3DDevice9::SetSamplerState | 1 |
| IDirect3DDevice9::SetVertexDeclaration | 1 |
| IDirect3DDevice9::SetVertexShader | 1 |
| IDirect3DDevice9::SetVertexShaderConstantB | 1 |
| IDirect3DDevice9::SetVertexShaderConstantI | 1 |
| IDirect3DIndexBuffer9::AddRef | 1 |
| IDirect3DIndexBuffer9::GetDesc | 1 |
| IDirect3DIndexBuffer9::Lock | 1 |
| IDirect3DTexture9::AddRef | 1 |
| IDirect3DTexture9::GetLevelDesc | 1 |
| IDirect3DTexture9::QueryInterface | 1 |
| IDirect3DVertexBuffer9::AddRef | 1 |
| IDirect3DVertexBuffer9::GetDesc | 1 |
| IDirect3DVertexBuffer9::Lock | 1 |
| IDirect3DVertexDeclaration9::AddRef | 1 |
| IDirect3DVolumeTexture9::LockBox | 1 |
| IDirect3DVolumeTexture9::QueryInterface | 1 |
| IDirect3DVolumeTexture9::Release | 1 |
| IDirect3DVolumeTexture9::UnlockBox | 1 |
| unresolved::GetBackBuffer | 1 |

## State, format and other seam constants

Counts below are code-token occurrences in the requested trees, including tables, declarations and non-COM wrapper calls. They are not runtime state-change counts. Each occurrence and source line is in `d3d9_constants.csv`; numeric/variable arguments remain in call CSVs. Do not drop numeric state IDs or vendor FOURCC values merely because they have no enum spelling. The supplementary `d3d9_wrapper_calls.csv` preserves arguments at the existing engine state/draw/lock forwarding helpers (excluded from COM totals), where many numeric state IDs, flags and primitive types originate.

### Render states

| Token | Occurrences |
| --- | --- |
| D3DRS_MULTISAMPLEANTIALIAS | 4 |
| D3DRS_STENCILENABLE | 3 |
| D3DRS_SCISSORTESTENABLE | 2 |
| D3DRS_STENCILFAIL | 2 |
| D3DRS_STENCILFUNC | 2 |
| D3DRS_STENCILPASS | 2 |
| D3DRS_STENCILREF | 2 |
| D3DRS_STENCILZFAIL | 2 |
| D3DRS_ZFUNC | 2 |
| D3DRS_CULLMODE | 1 |
| D3DRS_ZENABLE | 1 |

### Formats

| Token | Occurrences |
| --- | --- |
| D3DFMT_A8R8G8B8 | 28 |
| D3DFMT_X8R8G8B8 | 14 |
| D3DFMT_D24S8 | 11 |
| D3DFMT_R5G6B5 | 5 |
| D3DFMT_A8 | 4 |
| D3DFMT_DXT1 | 4 |
| D3DFMT_DXT3 | 4 |
| D3DFMT_DXT2 | 3 |
| D3DFMT_DXT4 | 3 |
| D3DFMT_DXT5 | 3 |
| D3DFMT_INDEX16 | 3 |
| D3DFMT_UNKNOWN | 3 |
| D3DFMT_A16B16G16R16 | 2 |
| D3DFMT_A16B16G16R16F | 2 |
| D3DFMT_A1R5G5B5 | 2 |
| D3DFMT_A2B10G10R10 | 2 |
| D3DFMT_A2R10G10B10 | 2 |
| D3DFMT_A2W10V10U10 | 2 |
| D3DFMT_A32B32G32R32F | 2 |
| D3DFMT_A4L4 | 2 |
| D3DFMT_A4R4G4B4 | 2 |
| D3DFMT_A8B8G8R8 | 2 |
| D3DFMT_A8L8 | 2 |
| D3DFMT_A8P8 | 2 |
| D3DFMT_A8R3G3B2 | 2 |
| D3DFMT_CxV8U8 | 2 |
| D3DFMT_D16 | 2 |
| D3DFMT_G16R16 | 2 |
| D3DFMT_G16R16F | 2 |
| D3DFMT_G32R32F | 2 |
| D3DFMT_G8R8_G8B8 | 2 |
| D3DFMT_L16 | 2 |
| D3DFMT_L6V5U5 | 2 |
| D3DFMT_L8 | 2 |
| D3DFMT_P8 | 2 |
| D3DFMT_Q16W16V16U16 | 2 |
| D3DFMT_Q8W8V8U8 | 2 |
| D3DFMT_R16F | 2 |
| D3DFMT_R32F | 2 |
| D3DFMT_R3G3B2 | 2 |
| D3DFMT_R8G8B8 | 2 |
| D3DFMT_R8G8_B8G8 | 2 |
| D3DFMT_UYVY | 2 |
| D3DFMT_V16U16 | 2 |
| D3DFMT_V8U8 | 2 |
| D3DFMT_X1R5G5B5 | 2 |
| D3DFMT_X4R4G4B4 | 2 |
| D3DFMT_X8B8G8R8 | 2 |
| D3DFMT_X8L8V8U8 | 2 |
| D3DFMT_YUY2 | 2 |
| D3DFMT_D24X8 | 1 |
| D3DFMT_D32 | 1 |

### Sampler states

| Token | Occurrences |
| --- | --- |

### Texture-stage states

| Token | Occurrences |
| --- | --- |
| D3DTSS_TEXTURETRANSFORMFLAGS | 1 |

### Pools

| Token | Occurrences |
| --- | --- |
| D3DPOOL_SYSTEMMEM | 7 |
| D3DPOOL_MANAGED | 6 |
| D3DPOOL_DEFAULT | 1 |

### Usage

| Token | Occurrences |
| --- | --- |
| D3DUSAGE_RENDERTARGET | 11 |
| D3DUSAGE_WRITEONLY | 8 |
| D3DUSAGE_DEPTHSTENCIL | 4 |
| D3DUSAGE_DYNAMIC | 1 |

### Locks

| Token | Occurrences |
| --- | --- |
| D3DLOCK_READONLY | 5 |
| D3DLOCK_DISCARD | 3 |

### Primitive types

| Token | Occurrences |
| --- | --- |
| D3DPT_TRIANGLELIST | 17 |
| D3DPT_TRIANGLEFAN | 1 |

### Declaration types

| Token | Occurrences |
| --- | --- |
| D3DDECLTYPE_FLOAT2 | 10 |
| D3DDECLTYPE_FLOAT3 | 7 |
| D3DDECLTYPE_SHORT2 | 4 |
| D3DDECLTYPE_D3DCOLOR | 3 |
| D3DDECLTYPE_SHORT4 | 3 |
| D3DDECLTYPE_UNUSED | 2 |
| D3DDECLTYPE_FLOAT1 | 1 |
| D3DDECLTYPE_FLOAT4 | 1 |

### Declaration usages

| Token | Occurrences |
| --- | --- |
| D3DDECLUSAGE_TEXCOORD | 14 |
| D3DDECLUSAGE_POSITION | 13 |
| D3DDECLUSAGE_NORMAL | 6 |
| D3DDECLUSAGE_TANGENT | 3 |
| D3DDECLUSAGE_BINORMAL | 1 |
| D3DDECLUSAGE_COLOR | 1 |

## D3DX groups

### texture loading (40 sites)

| Method | Sites |
| --- | --- |
| D3DX free::D3DXLoadSurfaceFromSurface | 14 |
| D3DX free::D3DXCreateTexture | 6 |
| D3DX free::D3DXSaveSurfaceToFileA | 5 |
| D3DX free::D3DXFilterTexture | 3 |
| D3DX free::D3DXSaveTextureToFileA | 3 |
| D3DX free::D3DXGetImageInfoFromFileInMemory | 2 |
| D3DX free::D3DXLoadSurfaceFromMemory | 2 |
| D3DX free::D3DXCreateRenderToSurface | 1 |
| D3DX free::D3DXCreateTextureFromFileExA | 1 |
| D3DX free::D3DXCreateTextureFromFileInMemory | 1 |
| D3DX free::D3DXFillTexture | 1 |
| D3DX free::D3DXGetImageInfoFromFileA | 1 |

### math (21 sites)

| Method | Sites |
| --- | --- |
| D3DX free::D3DXFloat32To16Array | 10 |
| D3DX free::D3DXMatrixMultiply | 3 |
| D3DX free::D3DXMatrixInverse | 2 |
| D3DX free::D3DXMatrixOrthoLH | 2 |
| D3DX free::D3DXMatrixRotationZ | 2 |
| D3DX free::D3DXMatrixLookAtLH | 1 |
| D3DX free::D3DXMatrixTranspose | 1 |

### mesh (57 sites)

| Method | Sites |
| --- | --- |
| ID3DXMesh::Release | 10 |
| ID3DXMesh::GetNumFaces | 7 |
| ID3DXMesh::GetIndexBuffer | 5 |
| ID3DXMesh::GetNumVertices | 5 |
| ID3DXMesh::GetVertexBuffer | 5 |
| ID3DXMesh::CloneMeshFVF | 4 |
| D3DX free::D3DXComputeBoundingSphere | 3 |
| D3DX free::D3DXCreateSphere | 3 |
| ID3DXMesh::LockIndexBuffer | 3 |
| ID3DXMesh::UnlockIndexBuffer | 3 |
| D3DX free::D3DXComputeBoundingBox | 1 |
| D3DX free::D3DXCreateBox | 1 |
| D3DX free::D3DXCreateCylinder | 1 |
| ID3DXMesh::AddRef | 1 |
| ID3DXMesh::CloneMesh | 1 |
| ID3DXMesh::GenerateAdjacency | 1 |
| ID3DXMesh::LockVertexBuffer | 1 |
| ID3DXMesh::OptimizeInplace | 1 |
| ID3DXMesh::UnlockVertexBuffer | 1 |

### effect (158 sites)

| Method | Sites |
| --- | --- |
| ID3DXEffect::SetTexture | 10 |
| ID3DXEffect::SetTechnique | 7 |
| ID3DXEffect::CommitChanges | 6 |
| ID3DXEffect::EndPass | 6 |
| ID3DXEffect::FindNextValidTechnique | 6 |
| ID3DXEffect::GetParameterDesc | 6 |
| ID3DXEffect::SetArrayRange | 6 |
| ID3DXEffect::SetFloatArray | 6 |
| ID3DXEffect::Begin | 5 |
| ID3DXEffect::BeginPass | 5 |
| ID3DXEffect::End | 5 |
| ID3DXEffect::GetParameterByName | 5 |
| ID3DXEffect::GetTechniqueByName | 5 |
| ID3DXEffect::SetValue | 5 |
| D3DX free::D3DXCreateEffect | 4 |
| D3DX free::D3DXCreateEffectCompiler | 4 |
| ID3DXEffect::GetAnnotationByName | 4 |
| ID3DXEffect::GetTechniqueDesc | 4 |
| ID3DXEffect::OnLostDevice | 4 |
| ID3DXEffect::SetFloat | 4 |
| ID3DXEffect::SetInt | 4 |
| ID3DXEffect::SetVector | 4 |
| ID3DXEffect::GetInt | 3 |
| ID3DXEffect::SetBool | 3 |
| ID3DXEffect::GetBool | 2 |
| ID3DXEffect::GetParameterElement | 2 |
| ID3DXEffect::GetString | 2 |
| ID3DXEffect::OnResetDevice | 2 |
| ID3DXEffect::Release | 2 |
| ID3DXEffect::SetMatrix | 2 |
| ID3DXEffect::SetMatrixArray | 2 |
| ID3DXEffect::SetMatrixPointerArray | 2 |
| ID3DXEffect::SetStateManager | 2 |
| ID3DXEffect::SetVectorArray | 2 |
| ID3DXEffectCompiler::GetParameterDesc | 2 |
| ID3DXEffect::AddRef | 1 |
| ID3DXEffect::GetIntArray | 1 |
| ID3DXEffect::GetParameter | 1 |
| ID3DXEffect::GetPass | 1 |
| ID3DXEffect::GetTechnique | 1 |
| ID3DXEffect::IsParameterUsed | 1 |
| ID3DXEffect::SetBoolArray | 1 |
| ID3DXEffect::SetIntArray | 1 |
| ID3DXEffect::ValidateTechnique | 1 |
| ID3DXEffectCompiler::CompileEffect | 1 |
| ID3DXEffectCompiler::GetAnnotationByName | 1 |
| ID3DXEffectCompiler::GetParameterByName | 1 |
| ID3DXEffectCompiler::GetParameterElement | 1 |
| ID3DXEffectCompiler::GetString | 1 |
| ID3DXEffectCompiler::Release | 1 |

### font/sprite (0 sites)

| Method | Sites |
| --- | --- |

### other (37 sites)

| Method | Sites |
| --- | --- |
| ID3DXBuffer::GetBufferPointer | 12 |
| ID3DXBuffer::Release | 5 |
| ID3DXBuffer::GetBufferSize | 4 |
| ID3DXRenderToSurface::Release | 3 |
| D3DX free::D3DXAssembleShader | 2 |
| D3DX free::D3DXCreateBuffer | 2 |
| ID3DXInclude::Close | 2 |
| ID3DXRenderToSurface::EndScene | 2 |
| D3DX free::D3DXCreateMesh | 1 |
| D3DX free::D3DXCreateMeshFVF | 1 |
| D3DX free::D3DXGetShaderVersion | 1 |
| ID3DXInclude::Open | 1 |
| ID3DXRenderToSurface::BeginScene | 1 |

## Top files

| File | Sites |
| --- | --- |
| Code/BF2/Geom/QuadPatchEditable.cpp | 79 |
| Code/BF2/Geom/XpLightManager.cpp | 59 |
| Code/BF2/RendDX9/Ra/RaShader.cpp | 52 |
| Code/BF2/Geom/SurroundingPatchEditable.cpp | 45 |
| Code/BF2/RendDX9/StateManager.cpp | 45 |
| Code/BF2/RendDX9/RendDX9Init.cpp | 42 |
| Code/BF2/RendDX9/Effect.cpp | 41 |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp | 36 |
| Code/BF2/RendDX9/Texture.cpp | 34 |
| Code/BF2/Geom/SwiffRender.cpp | 32 |
| Code/BF2/RendDX9/Ra/EffectManager.cpp | 31 |
| Code/BF2/RendDX9/BF2Render.cpp | 30 |
| Code/BF2/RendDX9/FXParameter.cpp | 27 |
| Code/BF2/Geom/RoadEditable.cpp | 24 |
| Code/BF2/RendDX9/BufferManager.cpp | 23 |
| Code/BF2/RendDX9/TextureManager.cpp | 23 |
| Code/BF2/Geom/Terrain.cpp | 22 |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp | 21 |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp | 14 |
| Code/BF2/RendDX9/RenderQueues.cpp | 14 |
| Code/BF2/GeomMisc/MenuRend/Video.cpp | 13 |
| Code/BF2/GeomMisc/SplineControlPoint.cpp | 13 |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp | 12 |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.cpp | 10 |
| Code/BF2/RendDX9/QueryManager.cpp | 7 |

## Raw pointer crossings

These declarations are migration boundaries, not a claim that every one is exported. The exhaustive `d3d9_crossings.csv` includes parameters, returns, storage, annotated opaque fields, all void-pointer member candidates and pointer overlays. Header interfaces are visible beyond their owning TU; private TU structs and offset views still require a consistent resource representation. Opaque candidates include engine pointers and vtables: they require owner review and are not all GPU resources. Integer-encoded pointer storage cannot be exhaustively typed lexically.

The main shared contracts are `EngineTexture::baseTexture` and `TextureLockRecord::staging` in `RendDX9/Texture.h`, `RendDX9Base::d3d9/device` and typed state/buffer helper parameters in `RendDX9/RendDX9Base.h`, and the `RaStreamDataEditable` vertex/index buffer fields. TU-private crossings include `VertexBuffer::buffer`, `IndexBuffer::buffer`, vertex declarations, queries, render/depth surfaces, and the opaque compiled-effect handles. `StateManager::streamVB`, `indices`, `vertexDecl`, `vertexShader` and `pixelShader` are void-pointer resource mirrors; its texture `StateCell` values carry raw texture references through integer words (see `blockCopyTextureRef` and the reset/pop walks). A Windows backend must preserve those reference operations before any Metal representation change. `IRenderer` and `IRenderView` have unresolved positional slots; their declarations alone do not prove an absence of additional opaque crossings. `RendDX9Base.h::renderDevice()` also returns the device as `void*`; this opaque helper return must migrate with its typed/cast callers.

| Location | Type | Name / kind | Source |
| --- | --- | --- | --- |
| Code/BF2/Geom/BundledMesh.cpp:34 | opaque; see annotation | annotated opaque storage | void* effectD3dxHandle(void* effect); // Effect.cpp - the wrapper's +0x08 ID3DXEffect* |
| Code/BF2/Geom/BundledMeshTemplate.cpp:41 | IDirect3DVertexBuffer9 | vertexBufferD3D | IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper); |
| Code/BF2/Geom/BundledMeshTemplate.cpp:42 | IDirect3DIndexBuffer9 | indexBufferD3D | IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper); |
| Code/BF2/Geom/BundledMeshTemplate.cpp:43 | IDirect3DVertexDeclaration9 | vertexDeclarationD3D | IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper); |
| Code/BF2/Geom/BundledMeshTemplate.cpp:298 | IDirect3DResource9 | r | { IDirect3DResource9* r=dx9rend::vertexBufferD3D(m_vertexBuffer); if(r) r->PreLoad(); } // wrapper +0x08 |
| Code/BF2/Geom/BundledMeshTemplate.cpp:300 | IDirect3DResource9 | r | { IDirect3DResource9* r=dx9rend::indexBufferD3D(m_indexBuffer); if(r) r->PreLoad(); } // wrapper +0x08 |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:313 | IDirect3DSurface9 | oldDepthStencil | IDirect3DSurface9* oldDepthStencil; |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:314 | IDirect3DSurface9 | newDepthStencil | IDirect3DSurface9* newDepthStencil; |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:378 | IDirect3DSurface9 | surface | IDirect3DSurface9* surface = |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:402 | IDirect3DSurface9 | conversionSurface | IDirect3DSurface9* const conversionSurface = |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:404 | IDirect3DSurface9 | lightingSurface | IDirect3DSurface9* const lightingSurface = |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:184 | IDirect3DDevice9 | renderDevice | inline IDirect3DDevice9* renderDevice() |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:955 | ID3DXMesh | raw pointer overlay/cast | D3DXCreateSphere(renderDevice(), 0.1f, 20, 20, reinterpret_cast<ID3DXMesh**>(&m_lightMesh), 0); |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:956 | ID3DXMesh | clone | ID3DXMesh* clone = 0; |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:961 | ID3DXMesh | mesh | ID3DXMesh* const mesh = static_cast<ID3DXMesh*>(m_lightMesh); |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:1821 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = renderDevice(); |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:1822 | IDirect3DSurface9 | savedDepth | IDirect3DSurface9* savedDepth; |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:2481 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = renderDevice(); |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:2482 | IDirect3DSurface9 | savedDepth | IDirect3DSurface9* savedDepth; |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.h:262 | IDirect3DSurface9 | m_depthStencil | IDirect3DSurface9* m_depthStencil; // +0x09C 1024 x 1024 D24S8 |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.h:263 | IUnknown | m_lightMesh | IUnknown* m_lightMesh; // +0x0A0 the D3DX sphere (ID3DXMesh), radius 0.1 |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.h:265 | IUnknown | m_unresolvedA8 | IUnknown* m_unresolvedA8; // +0x0A8 0, Release()d by the destructor [UNRESOLVED] type |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.h:267 | IDirect3DVertexBuffer9 | m_lightMeshVertices | IDirect3DVertexBuffer9* m_lightMeshVertices; // +0x0B0 |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.h:268 | IDirect3DIndexBuffer9 | m_lightMeshIndices | IDirect3DIndexBuffer9* m_lightMeshIndices; // +0x0B4 |
| Code/BF2/Geom/LightmapGeneration/MeshLightSampling.cpp:181 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_texture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/LightmapGeneration/MeshLightSampling.cpp:182 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(converted, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/NameTagRenderer.cpp:71 | IDirect3DDevice9 | device | inline IDirect3DDevice9* device() |
| Code/BF2/Geom/NameTagRenderer.cpp:440 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_stagingTexture, 0, 0x7FFFFFFFu); |
| Code/BF2/Geom/NameTagRenderer.cpp:441 | IDirect3DSurface9 | target | IDirect3DSurface9* const target = dx9rend::textureGetLevelSurface(m_nameTexture, 0, 0x7FFFFFFFu); |
| Code/BF2/Geom/NameTagRenderer.cpp:482 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = |
| Code/BF2/Geom/NameTagRenderer.cpp:484 | IDirect3DSurface9 | target | IDirect3DSurface9* const target = |
| Code/BF2/Geom/Overgrowth/OverGrowth.cpp:1036 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface( |
| Code/BF2/Geom/Overgrowth/OverGrowth.cpp:1096 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface( |
| Code/BF2/Geom/QuadPatchEditable.cpp:101 | IDirect3DSurface9 | target | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/Geom/QuadPatchEditable.cpp:101 | IDirect3DSurface9 | depthStencil | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/Geom/QuadPatchEditable.cpp:646 | IDirect3DTexture9 | texture | IDirect3DTexture9* texture = 0; |
| Code/BF2/Geom/QuadPatchEditable.cpp:961 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1033 | IDirect3DSurface9 | lowSurface | IDirect3DSurface9* const lowSurface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1038 | IDirect3DSurface9 | surface0 | IDirect3DSurface9* const surface0 = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1042 | IDirect3DSurface9 | surface1 | IDirect3DSurface9* const surface1 = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1257 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1258 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1280 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1281 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(engineTexture(colormap), 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1296 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1316 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1355 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1421 | IDirect3DSurface9 | surfaces | IDirect3DSurface9* surfaces[2]; |
| Code/BF2/Geom/QuadPatchEditable.cpp:1715 | IDirect3DSurface9 | editSurface | IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1718 | IDirect3DSurface9 | colormapSurface | IDirect3DSurface9* const colormapSurface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1823 | IDirect3DSurface9 | colormapSurface | IDirect3DSurface9* const colormapSurface = |
| Code/BF2/Geom/QuadPatchEditable.cpp:1827 | IDirect3DSurface9 | editSurface | IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1834 | IDirect3DSurface9 | textureSurface | IDirect3DSurface9* const textureSurface = dx9rend::textureGetLevelSurface(record->colorTexture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:1976 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(texture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2176 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2177 | IDirect3DSurface9 | renderTarget | IDirect3DSurface9* renderTarget = 0; |
| Code/BF2/Geom/QuadPatchEditable.cpp:2178 | IDirect3DSurface9 | depthStencil | IDirect3DSurface9* depthStencil = 0; |
| Code/BF2/Geom/QuadPatchEditable.cpp:2209 | IDirect3DSurface9 | bigSurface | IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2213 | IDirect3DSurface9 | maskSurface | IDirect3DSurface9* const maskSurface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2281 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2323 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_roadMask, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2355 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(m_materialOverlay, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2615 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2616 | IDirect3DSurface9 | renderTarget | IDirect3DSurface9* renderTarget = 0; |
| Code/BF2/Geom/QuadPatchEditable.cpp:2617 | IDirect3DSurface9 | depthStencil | IDirect3DSurface9* depthStencil = 0; |
| Code/BF2/Geom/QuadPatchEditable.cpp:2650 | IDirect3DSurface9 | bigSurface | IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/QuadPatchEditable.cpp:2654 | IDirect3DSurface9 | flippedSurface | IDirect3DSurface9* const flippedSurface = dx9rend::textureGetLevelSurface(flipped, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/RoadEditable.cpp:830 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = rendererBase()->device; // renderer +0x0C |
| Code/BF2/Geom/RoadEditable.cpp:864 | IDirect3DSurface9 | rendering | IDirect3DSurface9* rendering = 0; |
| Code/BF2/Geom/RoadEditable.cpp:868 | ID3DXRenderToSurface | target | ID3DXRenderToSurface* target = 0; |
| Code/BF2/Geom/RoadEditable.cpp:951 | IDirect3DSurface9 | heights | IDirect3DSurface9* heights = 0; |
| Code/BF2/Geom/SkinnedMesh.cpp:55 | opaque; see annotation | annotated opaque storage | void* effectD3dxHandle(void* effect); // Effect.cpp - the wrapper's +0x08 ID3DXEffect* |
| Code/BF2/Geom/SkinnedMesh.cpp:667 | ID3DXEffect | d3dx | ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>(dx9rend::effectD3dxHandle(effect)); |
| Code/BF2/Geom/SkinnedMeshTemplate.cpp:33 | IDirect3DVertexBuffer9 | vertexBufferD3D | IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper); |
| Code/BF2/Geom/SkinnedMeshTemplate.cpp:34 | IDirect3DIndexBuffer9 | indexBufferD3D | IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper); |
| Code/BF2/Geom/SkinnedMeshTemplate.cpp:255 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = Ra::raRendererDevice(); // renderer +0x0C |
| Code/BF2/Geom/SkinnedMeshTemplate.cpp:1005 | IDirect3DVertexBuffer9 | vertexBufferObject | IDirect3DVertexBuffer9* vertexBufferObject(void* wrapper) |
| Code/BF2/Geom/SkinnedMeshTemplate.cpp:1010 | IDirect3DIndexBuffer9 | indexBufferObject | IDirect3DIndexBuffer9* indexBufferObject(void* wrapper) |
| Code/BF2/Geom/SkinnedMeshTemplate.h:72 | IDirect3DVertexDeclaration9 | declaration | IDirect3DVertexDeclaration9* declaration; // +0x00 |
| Code/BF2/Geom/SkinnedMeshTemplate.h:108 | IDirect3DVertexBuffer9 | vertexBuffer | IDirect3DVertexBuffer9* vertexBuffer; // +0x194 |
| Code/BF2/Geom/SkinnedMeshTemplate.h:110 | IDirect3DIndexBuffer9 | indexBuffer | IDirect3DIndexBuffer9* indexBuffer; // +0x19C |
| Code/BF2/Geom/SkyDome.cpp:236 | IDirect3DQuery9 | d3d | IDirect3DQuery9* const d3d = queryField<IDirect3DQuery9*>(query, 0x08); |
| Code/BF2/Geom/SkyDome.cpp:271 | IDirect3DQuery9 | d3d | IDirect3DQuery9* const d3d = queryField<IDirect3DQuery9*>(query, 0x08); |
| Code/BF2/Geom/SkyDome.cpp:515 | IDirect3DQuery9 | d3d | IDirect3DQuery9** const d3d = &queryField<IDirect3DQuery9*>(m_flareQuery, 0x08); |
| Code/BF2/Geom/StaticMesh.cpp:86 | opaque; see annotation | annotated opaque storage | void* effectD3dxHandle(void* effect); // Effect.cpp - the wrapper's +0x08 ID3DXEffect* |
| Code/BF2/Geom/StaticMesh.cpp:858 | IDirect3DBaseTexture9 | incoming | void staticMeshSwapD3DTexture(void*& slot, IDirect3DBaseTexture9* incoming) |
| Code/BF2/Geom/StaticMesh.cpp:860 | IDirect3DBaseTexture9 | current | IDirect3DBaseTexture9* const current = static_cast<IDirect3DBaseTexture9*>(slot); |
| Code/BF2/Geom/StaticMesh.cpp:2533 | ID3DXEffect | d3dx | ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>(dx9rend::effectD3dxHandle(effect)); |
| Code/BF2/Geom/StaticMeshMaterial.cpp:57 | IDirect3DVertexDeclaration9 | vertexDeclarationD3D | IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper); |
| Code/BF2/Geom/StaticMeshMaterial.cpp:706 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = Ra::raRendererDevice(); // renderer +0x0C |
| Code/BF2/Geom/StaticMeshMaterial.cpp:707 | IDirect3DVertexDeclaration9 | declaration | IDirect3DVertexDeclaration9* const declaration = Ra::raVertexDeclarationCached( |
| Code/BF2/Geom/StaticMeshMaterial.cpp:1181 | IDirect3DVertexDeclaration9 | decl | IDirect3DVertexDeclaration9* const decl = |
| Code/BF2/Geom/StaticMeshTemplate.cpp:160 | IDirect3DResource9 | resource | IDirect3DResource9* resource; // +0x08 |
| Code/BF2/Geom/StaticMeshTemplate.h:119 | IDirect3DVertexDeclaration9 | declaration | IDirect3DVertexDeclaration9* declaration; // node +0x10 |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:83 | IDirect3DSurface9 | target | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:83 | IDirect3DSurface9 | depthStencil | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:747 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:748 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:764 | IDirect3DSurface9 | colormapSurface | IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:765 | IDirect3DSurface9 | editSurface | IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:871 | IDirect3DSurface9 | scratchSurface | IDirect3DSurface9* const scratchSurface = dx9rend::textureGetLevelSurface(scratch, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:874 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(*texture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:959 | IDirect3DSurface9 | colormapSurface | IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:962 | IDirect3DSurface9 | editSurface | IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1053 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface(engineTexture(lowDetailmap), 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1142 | IDirect3DSurface9 | colormapSurface | IDirect3DSurface9* const colormapSurface = dx9rend::textureGetLevelSurface(m_colormap, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1145 | IDirect3DSurface9 | editSurface | IDirect3DSurface9* const editSurface = dx9rend::textureGetLevelSurface(m_colormapEdit, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1152 | IDirect3DSurface9 | textureSurface | IDirect3DSurface9* const textureSurface = dx9rend::textureGetLevelSurface(record->colorTexture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1372 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1373 | IDirect3DSurface9 | renderTarget | IDirect3DSurface9* renderTarget = 0; |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1374 | IDirect3DSurface9 | depthStencil | IDirect3DSurface9* depthStencil = 0; |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1416 | IDirect3DSurface9 | bigSurface | IDirect3DSurface9* const bigSurface = dx9rend::textureGetLevelSurface(big, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:1420 | IDirect3DSurface9 | flippedSurface | IDirect3DSurface9* const flippedSurface = dx9rend::textureGetLevelSurface(flipped, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/SwiffRender.cpp:254 | IUnknown | texture | IUnknown* texture; // +4 IDirect3DBaseTexture9*, AddRef'd by init |
| Code/BF2/Geom/SwiffRender.cpp:315 | IUnknown | texture | void recordInit(SwiffTextureRecord* rec, int id, IUnknown* texture, const char* name, |
| Code/BF2/Geom/SwiffRender.cpp:529 | IDirect3DTexture9 | texture | IDirect3DTexture9* texture = 0; |
| Code/BF2/Geom/SwiffRender.cpp:1034 | IDirect3DTexture9 | tex | IDirect3DTexture9* tex = 0; |
| Code/BF2/Geom/SwiffRender.cpp:1038 | IDirect3DTexture9 | t | IDirect3DTexture9* t = tex; |
| Code/BF2/Geom/SwiffRender.cpp:1039 | IDirect3DSurface9 | surface | IDirect3DSurface9* surface = 0; |
| Code/BF2/Geom/SwiffRender.cpp:1083 | IDirect3DTexture9 | tex | IDirect3DTexture9* tex = 0; |
| Code/BF2/Geom/SwiffRender.cpp:1087 | IDirect3DTexture9 | t | IDirect3DTexture9* t = tex; |
| Code/BF2/Geom/SwiffRender.cpp:1088 | IDirect3DSurface9 | surface | IDirect3DSurface9* surface = 0; |
| Code/BF2/Geom/SwiffRender.cpp:1165 | IDirect3DDevice9 | device | IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(renderDevice()); |
| Code/BF2/Geom/SwiffRender.cpp:1279 | IDirect3DDevice9 | device | IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(renderDevice()); |
| Code/BF2/Geom/Terrain.cpp:1335 | ID3DXMesh | mesh | ID3DXMesh* mesh = 0; |
| Code/BF2/Geom/Terrain.cpp:1337 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device; |
| Code/BF2/Geom/Terrain.cpp:1358 | ID3DXBuffer | adjacency | ID3DXBuffer* adjacency = 0; |
| Code/BF2/Geom/Terrain.cpp:1365 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device; |
| Code/BF2/Geom/TerrainDraw.cpp:778 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = dx9rend::textureGetLevelSurface( |
| Code/BF2/Geom/TerrainDraw.cpp:780 | IDirect3DSurface9 | backBuffer | IDirect3DSurface9* const backBuffer = dx9rend::rendererBackBufferSurface(); // Renderer +0x76C |
| Code/BF2/Geom/TerrainDraw.cpp:1220 | IUnknown | raw pointer overlay/cast | IUnknown*& slot = *reinterpret_cast<IUnknown**>(&entry->lighting.lightMap.value); |
| Code/BF2/Geom/TerrainDraw.cpp:1220 | IUnknown | slot | IUnknown*& slot = *reinterpret_cast<IUnknown**>(&entry->lighting.lightMap.value); |
| Code/BF2/Geom/TerrainDraw.cpp:1223 | IUnknown | base | IUnknown* const base = static_cast<dx9rend::EngineTexture*>(lightmap)->baseTexture; |
| Code/BF2/Geom/TerrainLightSampling.cpp:439 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface(m_texture, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/TerrainLightSampling.cpp:440 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = dx9rend::textureGetLevelSurface(converted, 0, 0x7FFFFFFF); |
| Code/BF2/Geom/UndergrowthSystemRender.cpp:851 | IDirect3DTexture9 | colorTexture | IDirect3DTexture9* const colorTexture = |
| Code/BF2/Geom/UndergrowthSystemRender.cpp:853 | IDirect3DTexture9 | lightTexture | IDirect3DTexture9* const lightTexture = |
| Code/BF2/Geom/XpLightManager.cpp:282 | IDirect3DSurface9 | shadowDepthSurface | IDirect3DSurface9* shadowDepthSurface; // +0x228 |
| Code/BF2/Geom/XpLightManager.cpp:283 | IDirect3DSurface9 | shadowDepthSurface2 | IDirect3DSurface9* shadowDepthSurface2; // +0x22C |
| Code/BF2/Geom/XpLightManager.cpp:286 | IDirect3DSurface9 | pointShadowDepthSurface | IDirect3DSurface9* pointShadowDepthSurface; // +0x238 |
| Code/BF2/Geom/XpLightManager.cpp:341 | IDirect3DVertexBuffer9 | sphereVertices | IDirect3DVertexBuffer9* sphereVertices; // +0x8D8 the debug meshes |
| Code/BF2/Geom/XpLightManager.cpp:342 | IDirect3DIndexBuffer9 | sphereIndices | IDirect3DIndexBuffer9* sphereIndices; // +0x8DC |
| Code/BF2/Geom/XpLightManager.cpp:345 | IDirect3DVertexBuffer9 | cylinderVertices | IDirect3DVertexBuffer9* cylinderVertices; // +0x8E8 |
| Code/BF2/Geom/XpLightManager.cpp:346 | IDirect3DIndexBuffer9 | cylinderIndices | IDirect3DIndexBuffer9* cylinderIndices; // +0x8EC |
| Code/BF2/Geom/XpLightManager.cpp:349 | IDirect3DVertexBuffer9 | boxVertices | IDirect3DVertexBuffer9* boxVertices; // +0x8F8 |
| Code/BF2/Geom/XpLightManager.cpp:350 | IDirect3DIndexBuffer9 | boxIndices | IDirect3DIndexBuffer9* boxIndices; // +0x8FC |
| Code/BF2/Geom/XpLightManager.cpp:353 | IDirect3DDevice9 | device | IDirect3DDevice9* device; // +0x908 |
| Code/BF2/Geom/XpLightManager.cpp:457 | IDirect3DSurface9 | surface | IDirect3DSurface9* const surface = |
| Code/BF2/Geom/XpLightManager.cpp:541 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = |
| Code/BF2/Geom/XpLightManager.cpp:545 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = |
| Code/BF2/Geom/XpLightManager.cpp:565 | IDirect3DSurface9 | surface | void releaseSurface(IDirect3DSurface9*& surface) |
| Code/BF2/Geom/XpLightManager.cpp:582 | IDirect3DSurface9 | secondaryDepth | IDirect3DSurface9* const secondaryDepth = |
| Code/BF2/Geom/XpLightManager.cpp:613 | IDirect3DSurface9 | surface | IDirect3DSurface9* surface = 0; |
| Code/BF2/Geom/XpLightManager.cpp:630 | IDirect3DSurface9 | surface | void setLightManagerDepthStencil(IDirect3DSurface9* surface) |
| Code/BF2/Geom/XpLightManager.cpp:677 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = lm(self).device; |
| Code/BF2/Geom/XpLightManager.cpp:736 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = lm(self).device; |
| Code/BF2/Geom/XpLightManager.cpp:2968 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = |
| Code/BF2/Geom/XpLightManager.cpp:3749 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = lm(manager).device; |
| Code/BF2/Geom/XpLightManager.cpp:4372 | IUnknown | objects | IUnknown** const objects[] = |
| Code/BF2/Geom/XpLightManager.cpp:4374 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.boxVertices), reinterpret_cast<IUnknown**>(&d.boxIndices), |
| Code/BF2/Geom/XpLightManager.cpp:4374 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.boxVertices), reinterpret_cast<IUnknown**>(&d.boxIndices), |
| Code/BF2/Geom/XpLightManager.cpp:4375 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.sphereVertices), reinterpret_cast<IUnknown**>(&d.sphereIndices), |
| Code/BF2/Geom/XpLightManager.cpp:4375 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.sphereVertices), reinterpret_cast<IUnknown**>(&d.sphereIndices), |
| Code/BF2/Geom/XpLightManager.cpp:4376 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.cylinderVertices), |
| Code/BF2/Geom/XpLightManager.cpp:4377 | IUnknown | raw pointer overlay/cast | reinterpret_cast<IUnknown**>(&d.cylinderIndices) |
| Code/BF2/Geom/XpLightManager.cpp:4381 | IUnknown | object | IUnknown*& object = *objects[i]; |
| Code/BF2/Geom/XpLightManager.cpp:4442 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = lm(self).device; |
| Code/BF2/Geom/XpLightManager.cpp:4444 | ID3DXMesh | sphereSource | ID3DXMesh* sphereSource = 0; |
| Code/BF2/Geom/XpLightManager.cpp:4445 | ID3DXMesh | sphere | ID3DXMesh* sphere = 0; |
| Code/BF2/Geom/XpLightManager.cpp:4449 | ID3DXMesh | cylinderSource | ID3DXMesh* cylinderSource = 0; |
| Code/BF2/Geom/XpLightManager.cpp:4450 | ID3DXMesh | cylinder | ID3DXMesh* cylinder = 0; |
| Code/BF2/Geom/XpLightManager.cpp:4454 | ID3DXMesh | boxSource | ID3DXMesh* boxSource = 0; |
| Code/BF2/Geom/XpLightManager.cpp:4455 | ID3DXMesh | box | ID3DXMesh* box = 0; |
| Code/BF2/Geom/XpLightManager.cpp:5641 | IDirect3DSurface9 | surface | void lightManagerSetDepthStencil(IDirect3DSurface9* surface) |
| Code/BF2/Geom/XpLightManager.cpp:6955 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = |
| Code/BF2/Geom/XpLightManager.h:341 | IDirect3DSurface9 | surface | void lightManagerSetDepthStencil(IDirect3DSurface9* surface); |
| Code/BF2/GeomMisc/FSQuadDrawer.cpp:379 | IDirect3DSurface9 | savedRenderTarget | IDirect3DSurface9* savedRenderTarget = dx9rend::stateManagerRenderTarget0(); |
| Code/BF2/GeomMisc/FSQuadDrawer.cpp:386 | IDirect3DSurface9 | destinationSurface | IDirect3DSurface9* const destinationSurface = |
| Code/BF2/GeomMisc/FSQuadDrawer.cpp:407 | IDirect3DSurface9 | rendererDepth | IDirect3DSurface9* const rendererDepth = dx9rend::rendererDepthStencilSurface(); |
| Code/BF2/GeomMisc/FontSystem/FontSystem.cpp:525 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination = |
| Code/BF2/GeomMisc/FontSystem/FontSystem.cpp:527 | IDirect3DSurface9 | source | IDirect3DSurface9* const source = dx9rend::textureGetLevelSurface( |
| Code/BF2/GeomMisc/FontSystem/FontSystem.cpp:552 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/GeomMisc/MenuRend/FSMoviePlayer.cpp:296 | ID3DXEffect | fx | ID3DXEffect* fx = static_cast<ID3DXEffect*>(dx9rend::effectD3dxHandle(m_effect)); |
| Code/BF2/GeomMisc/MenuRend/Video.cpp:34 | opaque; see annotation | annotated opaque storage | namespace dx9rend { void* renderDevice(); } // RendDX9Base.h - the live IDirect3DDevice9 |
| Code/BF2/GeomMisc/MenuRend/Video.cpp:115 | IDirect3DTexture9 | m_texture | IDirect3DTexture9* m_texture; // shipped +0x30 |
| Code/BF2/GeomMisc/MenuRend/Video.cpp:116 | IDirect3DSurface9 | m_surface | IDirect3DSurface9* m_surface; // shipped +0x34 |
| Code/BF2/GeomMisc/MenuRend/Video.cpp:136 | IDirect3DDevice9 | dev | IDirect3DDevice9* dev = static_cast<IDirect3DDevice9*>(::dice::hfe::dx9rend::renderDevice()); |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:30 | ID3DXMesh | s_sphere | ID3DXMesh* s_sphere = 0; // dword_1024B59C |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:85 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:86 | ID3DXMesh | source | ID3DXMesh* source = 0; |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:165 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = static_cast<IDirect3DDevice9*>(dx9rend::renderDevice()); |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:166 | IDirect3DVertexBuffer9 | vertices | IDirect3DVertexBuffer9* vertices = 0; |
| Code/BF2/GeomMisc/SplineControlPoint.cpp:167 | IDirect3DIndexBuffer9 | indices | IDirect3DIndexBuffer9* indices = 0; |
| Code/BF2/RendDX9/BF2Render.cpp:518 | IDirect3DSurface9 | m_backBuffer | IDirect3DSurface9* m_backBuffer; // +0x76C the device back buffer reference |
| Code/BF2/RendDX9/BF2Render.cpp:519 | IDirect3DSurface9 | m_depthStencil | IDirect3DSurface9* m_depthStencil; // +0x770 the depth-stencil reference |
| Code/BF2/RendDX9/BF2Render.cpp:855 | IDirect3DSurface9 | backBuffer | IDirect3DSurface9*& backBuffer = renderer->m_backBuffer; // +0x76C |
| Code/BF2/RendDX9/BF2Render.cpp:856 | IDirect3DSurface9 | depth | IDirect3DSurface9*& depth = renderer->m_depthStencil; // +0x770 |
| Code/BF2/RendDX9/BF2Render.cpp:2233 | IDirect3DDevice9 | dev | IDirect3DDevice9* dev = static_cast<RendDX9Base*>(g_rendererBase)->device; |
| Code/BF2/RendDX9/BF2Render.cpp:2475 | IDirect3DDevice9 | dev | IDirect3DDevice9* dev = static_cast<RendDX9Base*>(g_rendererBase)->device; |
| Code/BF2/RendDX9/BF2Render.cpp:2752 | IDirect3DSurface9 | fallback | IDirect3DSurface9*& fallback = static_cast<RendDX9Base*>(g_rendererBase)->tempBackBuffer; |
| Code/BF2/RendDX9/BF2Render.cpp:2757 | IDirect3DSurface9 | selected | IDirect3DSurface9* selected = fallback; |
| Code/BF2/RendDX9/BF2Render.cpp:3162 | IDirect3DSurface9 | rendererBackBufferSurface | IDirect3DSurface9* rendererBackBufferSurface() |
| Code/BF2/RendDX9/BF2Render.cpp:3167 | IDirect3DSurface9 | rendererDepthStencilSurface | IDirect3DSurface9* rendererDepthStencilSurface() |
| Code/BF2/RendDX9/BF2Render.cpp:3293 | IDirect3DSurface9 | selected | IDirect3DSurface9* selected = static_cast<RendDX9Base*>(g_rendererBase)->tempBackBuffer; |
| Code/BF2/RendDX9/BF2Render.cpp:3414 | IDirect3DSurface9 | target | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/RendDX9/BF2Render.cpp:3414 | IDirect3DSurface9 | depthStencil | void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil, |
| Code/BF2/RendDX9/BF2Render.cpp:3451 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(renderer)->device; |
| Code/BF2/RendDX9/BF2Render.cpp:3452 | IDirect3DSurface9 | previousDepthStencil | IDirect3DSurface9* previousDepthStencil = 0; |
| Code/BF2/RendDX9/BF2Render.cpp:3485 | IDirect3DSurface9 | tempBackBuffer | IDirect3DSurface9*& tempBackBuffer = reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer; |
| Code/BF2/RendDX9/BF2Render.cpp:3525 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(self)->device; |
| Code/BF2/RendDX9/BF2Render.cpp:3526 | IDirect3DSurface9 | target | IDirect3DSurface9* target = 0; |
| Code/BF2/RendDX9/BF2Render.cpp:3527 | IDirect3DSurface9 | depthStencil | IDirect3DSurface9* depthStencil = 0; |
| Code/BF2/RendDX9/BF2Render.cpp:3637 | IDirect3DSurface9 | tempBackBuffer | IDirect3DSurface9*& tempBackBuffer = reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer; |
| Code/BF2/RendDX9/BufferManager.cpp:102 | IDirect3DDevice9 | device | explicit BufferManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/BufferManager.cpp:104 | IDirect3DDevice9 | device | IDirect3DDevice9* device() const { return m_device; } |
| Code/BF2/RendDX9/BufferManager.cpp:123 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; // +0x04 |
| Code/BF2/RendDX9/BufferManager.cpp:138 | IDirect3DDevice9 | device | BufferManager::BufferManager(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/BufferManager.cpp:156 | IDirect3DDevice9 | device | bool createBufferManager(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/BufferManager.cpp:187 | IDirect3DVertexBuffer9 | buffer | IDirect3DVertexBuffer9* buffer; // +0x08 |
| Code/BF2/RendDX9/BufferManager.cpp:244 | IDirect3DVertexBuffer9 | vb | void vertexBufferSetBuffer(VertexBuffer* self, IDirect3DVertexBuffer9* vb) |
| Code/BF2/RendDX9/BufferManager.cpp:287 | IDirect3DVertexBuffer9 | vb | IDirect3DVertexBuffer9* vb = 0; |
| Code/BF2/RendDX9/BufferManager.cpp:406 | IDirect3DVertexBuffer9 | vertexBufferD3D | IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper) |
| Code/BF2/RendDX9/BufferManager.cpp:455 | IDirect3DIndexBuffer9 | buffer | IDirect3DIndexBuffer9* buffer; // +0x08 |
| Code/BF2/RendDX9/BufferManager.cpp:469 | IDirect3DIndexBuffer9 | ib | void indexBufferSetBuffer(IndexBuffer* self, IDirect3DIndexBuffer9* ib) |
| Code/BF2/RendDX9/BufferManager.cpp:499 | IDirect3DIndexBuffer9 | ib | IDirect3DIndexBuffer9* ib = 0; |
| Code/BF2/RendDX9/BufferManager.cpp:561 | IDirect3DIndexBuffer9 | indexBufferD3D | IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper) |
| Code/BF2/RendDX9/BufferManager.cpp:592 | IDirect3DVertexDeclaration9 | decl | IDirect3DVertexDeclaration9* decl; // +0x08 |
| Code/BF2/RendDX9/BufferManager.cpp:634 | IDirect3DVertexDeclaration9 | decl | IDirect3DVertexDeclaration9* decl = 0; |
| Code/BF2/RendDX9/BufferManager.cpp:659 | IDirect3DVertexDeclaration9 | vertexDeclarationD3D | IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper) |
| Code/BF2/RendDX9/Effect.cpp:199 | ID3DXEffect | d3dxEffect | bool bind(ID3DXEffect* d3dxEffect); |
| Code/BF2/RendDX9/Effect.cpp:214 | ID3DXEffect | d3dxEffect | ID3DXEffect* d3dxEffect() const { return m_d3dxEffect; } |
| Code/BF2/RendDX9/Effect.cpp:243 | ID3DXEffect | m_d3dxEffect | ID3DXEffect* m_d3dxEffect; // +0x08 |
| Code/BF2/RendDX9/Effect.cpp:338 | ID3DXEffect | d3dxEffect | bool Effect::bind(ID3DXEffect* d3dxEffect) |
| Code/BF2/RendDX9/Effect.cpp:780 | ID3DXEffect | fx | ID3DXEffect* fx = self->d3dxEffect(); |
| Code/BF2/RendDX9/Effect.cpp:804 | ID3DXEffect | fx | ID3DXEffect* fx = self->d3dxEffect(); |
| Code/BF2/RendDX9/Effect.cpp:1098 | ID3DXEffect | effect | ID3DXEffect* effect = 0; |
| Code/BF2/RendDX9/Effect.cpp:1099 | ID3DXBuffer | errors | ID3DXBuffer* errors = 0; |
| Code/BF2/RendDX9/Effect.cpp:1110 | ID3DXBuffer | extendedErrors | ID3DXBuffer* extendedErrors = 0; |
| Code/BF2/RendDX9/FXParameter.cpp:128 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/FXParameter.cpp:223 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/FXParameter.cpp:244 | IDirect3DBaseTexture9 | d3dTexture | IDirect3DBaseTexture9* d3dTexture = 0; |
| Code/BF2/RendDX9/FXParameter.cpp:371 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/FXParameter.cpp:378 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/FXParameter.cpp:385 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/FXParameter.cpp:409 | ID3DXEffect | fx | ID3DXEffect* const fx = static_cast<ID3DXEffect*>(effectD3dxHandle(m_effect)); |
| Code/BF2/RendDX9/QueryManager.cpp:50 | IDirect3DQuery9 | d3dQuery | IDirect3DQuery9* d3dQuery() const { return m_d3dQuery; } |
| Code/BF2/RendDX9/QueryManager.cpp:63 | IDirect3DQuery9 | m_d3dQuery | IDirect3DQuery9* m_d3dQuery; // +0x08 ctor: 0 |
| Code/BF2/RendDX9/QueryManager.cpp:148 | IDirect3DDevice9 | device | explicit QueryManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/QueryManager.cpp:188 | IDirect3DDevice9 | device | bool probeQueryType(IDirect3DDevice9* device, D3DQUERYTYPE type) |
| Code/BF2/RendDX9/QueryManager.cpp:190 | IDirect3DQuery9 | probe | IDirect3DQuery9* probe = 0; |
| Code/BF2/RendDX9/QueryManager.cpp:206 | IDirect3DDevice9 | device | QueryManager::QueryManager(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/QueryManager.cpp:314 | IDirect3DDevice9 | device | bool createQueryManager(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:139 | opaque; see annotation | annotated opaque storage | typedef std::map<std::string, void*, RaStricmpLess> RaEffectCache; // -> ID3DXEffect* |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:158 | opaque; see annotation | annotated opaque storage | void* vptr; // +0x00 &off_101E139C - the ID3DXInclude table, installed below |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:182 | IDirect3DDevice9 | device | explicit EffectManager(IDirect3DDevice9* device, bool disableShaderCache); |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:213 | ID3DXBuffer | compileEffectSource | ID3DXBuffer* compileEffectSource(const char* source, unsigned int sourceLen, |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:215 | ID3DXBuffer | errors | ID3DXBuffer** errors); |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:231 | ID3DXBuffer | blob | bool writeEffectCache(ID3DXBuffer* blob, const std::string& path); |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:261 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; // +0x38 |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:292 | IDirect3DDevice9 | device | EffectManager::EffectManager(IDirect3DDevice9* device, bool disableShaderCache) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:721 | IDirect3DDevice9 | device | bool createEffectManager(IDirect3DDevice9* device, bool disableShaderCache) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1060 | ID3DXBuffer | blob | bool EffectManager::writeEffectCache(ID3DXBuffer* blob, const std::string& path) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1179 | ID3DXInclude | include | const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags, |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1180 | ID3DXEffectCompiler | compilerOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1180 | ID3DXBuffer | parseErrorsOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1194 | ID3DXInclude | include | const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags, |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1195 | ID3DXEffectCompiler | compilerOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1195 | ID3DXBuffer | parseErrorsOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1242 | ID3DXBuffer | EffectManager | ID3DXBuffer* EffectManager::compileEffectSource(const char* source, unsigned int sourceLen, |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1244 | ID3DXBuffer | errors | ID3DXBuffer** errors) |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1254 | ID3DXBuffer | blob | ID3DXBuffer* blob = 0; |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1258 | ID3DXEffectCompiler | compiler | ID3DXEffectCompiler* compiler = 0; |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1417 | ID3DXEffect | effect | ID3DXEffect* effect = 0; |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1418 | ID3DXBuffer | errors | ID3DXBuffer* errors = 0; |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1425 | ID3DXBuffer | buffer | ID3DXBuffer* buffer = 0; |
| Code/BF2/RendDX9/Ra/EffectManager.cpp:1541 | ID3DXBuffer | blob | ID3DXBuffer* const blob = compileEffectSource( |
| Code/BF2/RendDX9/Ra/RaShader.cpp:249 | IDirect3DBaseTexture9 | raw pointer overlay/cast | *reinterpret_cast<IDirect3DBaseTexture9**>(holderPayload(self)))); |
| Code/BF2/RendDX9/Ra/RaShader.cpp:1108 | ID3DXEffect | effect | ID3DXEffect* const effect = static_cast<ID3DXEffect*>(m_effect); |
| Code/BF2/RendDX9/Ra/RaShader.cpp:1351 | ID3DXEffect | effect | ID3DXEffect* const effect = static_cast<ID3DXEffect*>(loadEffect()); |
| Code/BF2/RendDX9/Ra/RaShader.cpp:1387 | IDirect3DPixelShader9 | pixelShader | if (IDirect3DPixelShader9* const pixelShader = |
| Code/BF2/RendDX9/Ra/RaShader.h:166 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; // +0x14C |
| Code/BF2/RendDX9/Ra/RaShader.h:176 | opaque; see annotation | annotated opaque storage | void* m_effect; // +0x16C the ID3DXEffect |
| Code/BF2/RendDX9/Ra/RaShader.h:353 | IDirect3DVertexDeclaration9 | raVertexDeclarationCached | IDirect3DVertexDeclaration9* raVertexDeclarationCached(const std::vector<D3DVERTEXELEMENT9>& elements, |
| Code/BF2/RendDX9/Ra/RaShader.h:354 | IDirect3DDevice9 | device | IDirect3DDevice9* device); |
| Code/BF2/RendDX9/Ra/RaShader.h:355 | IDirect3DDevice9 | raRendererDevice | IDirect3DDevice9* raRendererDevice(); // the renderer base's +0x0C |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:282 | ID3DXEffectCompiler | fx | D3DXHANDLE fxGetParameterByName(ID3DXEffectCompiler* fx, D3DXHANDLE parent, const char* name) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:288 | ID3DXEffectCompiler | fx | bool fxGetElementCount(ID3DXEffectCompiler* fx, D3DXHANDLE handle, unsigned int& elements) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:298 | ID3DXEffectCompiler | fx | const char* fxGetElementString(ID3DXEffectCompiler* fx, D3DXHANDLE handle, unsigned int index) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:313 | ID3DXInclude | include | const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags, |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:314 | ID3DXEffectCompiler | compilerOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:314 | ID3DXBuffer | parseErrorsOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:328 | ID3DXInclude | include | const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags, |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:329 | ID3DXEffectCompiler | compilerOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:329 | ID3DXBuffer | parseErrorsOut | ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut) |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:775 | ID3DXEffectCompiler | compiler | ID3DXEffectCompiler* compiler = 0; |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:776 | ID3DXBuffer | errors | ID3DXBuffer* errors = 0; |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:886 | ID3DXEffectCompiler | fx | ID3DXEffectCompiler* const fx = static_cast<ID3DXEffectCompiler*>(effectCompiler); |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:931 | ID3DXEffectCompiler | fx | ID3DXEffectCompiler* const fx = static_cast<ID3DXEffectCompiler*>(effectCompiler); |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.cpp:40 | IDirect3DDevice9 | device | IDirect3DDevice9* device) |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.cpp:135 | IDirect3DDevice9 | device | IDirect3DDevice9* const device = raRendererDevice(); // renderer +0x0C |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.cpp:136 | IDirect3DVertexDeclaration9 | inserted | IDirect3DVertexDeclaration9*& inserted = m_declarations[key]; |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.cpp:173 | IDirect3DIndexBuffer9 | indices | IDirect3DIndexBuffer9* const indices = m_indexBuffer; |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.h:30 | IDirect3DDevice9 | device | IDirect3DDevice9* device); |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.h:48 | IDirect3DVertexBuffer9 | m_vertexBuffer | IDirect3DVertexBuffer9* m_vertexBuffer; // +0x004 |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.h:49 | IDirect3DIndexBuffer9 | m_indexBuffer | IDirect3DIndexBuffer9* m_indexBuffer; // +0x008 |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.h:56 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; // +0x170 |
| Code/BF2/RendDX9/Ra/RaStreamDataEditable.h:69 | opaque; see annotation | annotated opaque storage | std::map<unsigned int, IDirect3DVertexDeclaration9*> m_declarations; // +0x198 |
| Code/BF2/RendDX9/Ra/RaVertexStreams.cpp:36 | IDirect3DDevice9 | raRendererDevice | IDirect3DDevice9* raRendererDevice() |
| Code/BF2/RendDX9/Ra/RaVertexStreams.cpp:227 | IDirect3DVertexDeclaration9 | raVertexDeclarationCached | IDirect3DVertexDeclaration9* raVertexDeclarationCached(const std::vector<D3DVERTEXELEMENT9>& elements, |
| Code/BF2/RendDX9/Ra/RaVertexStreams.cpp:228 | IDirect3DDevice9 | device | IDirect3DDevice9* device) |
| Code/BF2/RendDX9/Ra/RaVertexStreams.cpp:243 | IDirect3DVertexDeclaration9 | declaration | IDirect3DVertexDeclaration9* declaration = 0; |
| Code/BF2/RendDX9/RendDX9Base.h:167 | IDirect3D9 | d3d9 | /* 0x008 */ IDirect3D9* d3d9; // Direct3DCreate9(0x20) |
| Code/BF2/RendDX9/RendDX9Base.h:168 | IDirect3DDevice9 | device | /* 0x00C */ IDirect3DDevice9* device; // CreateDevice's out-parameter |
| Code/BF2/RendDX9/RendDX9Base.h:221 | IDirect3DSurface9 | tempBackBuffer | /* 0x0F4 */ IDirect3DSurface9* tempBackBuffer; |
| Code/BF2/RendDX9/RendDX9Base.h:421 | IDirect3DDevice9 | device | void* createRaShaderConstants(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/RendDX9Base.h:430 | IDirect3DBaseTexture9 | texture | IDirect3DBaseTexture9* texture); |
| Code/BF2/RendDX9/RendDX9Base.h:564 | IDirect3DSurface9 | surface | void stateManagerSetRenderTarget(unsigned int index, IDirect3DSurface9* surface); |
| Code/BF2/RendDX9/RendDX9Base.h:567 | IDirect3DSurface9 | stateManagerRenderTarget0 | IDirect3DSurface9* stateManagerRenderTarget0(); |
| Code/BF2/RendDX9/RendDX9Base.h:570 | IDirect3DSurface9 | surface | void stateManagerSetDepthStencilSurface(IDirect3DSurface9* surface); |
| Code/BF2/RendDX9/RendDX9Base.h:571 | IDirect3DIndexBuffer9 | indices | void stateManagerSetIndices(IDirect3DIndexBuffer9* indices); |
| Code/BF2/RendDX9/RendDX9Base.h:572 | IDirect3DVertexBuffer9 | vertexBuffer | void stateManagerSetStreamSource(unsigned int stream, IDirect3DVertexBuffer9* vertexBuffer, |
| Code/BF2/RendDX9/RendDX9Base.h:574 | IDirect3DVertexDeclaration9 | declaration | void stateManagerSetVertexDeclaration(IDirect3DVertexDeclaration9* declaration); |
| Code/BF2/RendDX9/RendDX9Base.h:578 | IDirect3DVertexShader9 | shader | void stateManagerSetVertexShader(IDirect3DVertexShader9* shader); |
| Code/BF2/RendDX9/RendDX9Base.h:579 | IDirect3DPixelShader9 | shader | void stateManagerSetPixelShader(IDirect3DPixelShader9* shader); |
| Code/BF2/RendDX9/RendDX9Base.h:618 | IDirect3DSurface9 | rendererBackBufferSurface | IDirect3DSurface9* rendererBackBufferSurface(); // +0x76C, the device back buffer reference |
| Code/BF2/RendDX9/RendDX9Base.h:619 | IDirect3DSurface9 | rendererDepthStencilSurface | IDirect3DSurface9* rendererDepthStencilSurface(); // +0x770, the depth-stencil reference |
| Code/BF2/RendDX9/RendDX9Draw.cpp:24 | IDirect3DDevice9 | device | inline IDirect3DDevice9* device() |
| Code/BF2/RendDX9/RendDX9Draw.cpp:95 | IDirect3DSwapChain9 | chain | IDirect3DSwapChain9* chain = b->swapChains[b->currentSwapChain]; // +0xE4[+0xF0] |
| Code/BF2/RendDX9/RendDX9Init.cpp:35 | IDirect3DDevice9 | device | namespace Ra { bool createEffectManager(IDirect3DDevice9* device, bool disableShaderCache); } |
| Code/BF2/RendDX9/RendDX9Init.cpp:46 | IDirect3DDevice9 | device | bool createBufferManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/RendDX9Init.cpp:47 | IDirect3DDevice9 | device | bool createQueryManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/RendDX9Init.cpp:48 | IDirect3DDevice9 | device | bool createTextureManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/RendDX9Init.cpp:50 | IDirect3DDevice9 | device | bool createShaderManager(void* base, IDirect3DDevice9* device); |
| Code/BF2/RendDX9/RendDX9Init.cpp:126 | IDirect3D9 | d3d9 | IDirect3D9* d3d9; // +0x580 |
| Code/BF2/RendDX9/RendDX9Init.cpp:181 | IDirect3D9 | d3d | IDirect3D9* const d3d = deviceCaps()->d3d9; // +0x580 |
| Code/BF2/RendDX9/RendDX9Init.cpp:216 | IDirect3DSwapChain9 | g_primarySwapChain | IDirect3DSwapChain9* g_primarySwapChain = 0; |
| Code/BF2/RendDX9/RendDX9Init.cpp:220 | IDirect3DSwapChain9 | chain | void swapChainVectorPushBack(RendDX9Base* b, IDirect3DSwapChain9* chain) |
| Code/BF2/RendDX9/RendDX9Init.cpp:701 | IDirect3DSwapChain9 | swapChain | IDirect3DSwapChain9* swapChain = 0; |
| Code/BF2/RendDX9/RendDX9Init.cpp:800 | IDirect3DSwapChain9 | swapChain0 | IDirect3DSwapChain9* swapChain0 = 0; |
| Code/BF2/RendDX9/RendDX9Init.cpp:823 | IDirect3DSwapChain9 | extra | IDirect3DSwapChain9* extra = 0; |
| Code/BF2/RendDX9/RendDX9Init.cpp:1331 | IUnknown | object | inline void releaseIfSet(IUnknown* object) |
| Code/BF2/RendDX9/RendDX9Init.cpp:1350 | ID3DXBuffer | vsBuffer | ID3DXBuffer* vsBuffer = 0; // [ebp-8] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1351 | ID3DXBuffer | vsErrors | ID3DXBuffer* vsErrors = 0; // [ebp-0Ch] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1360 | IDirect3DVertexShader9 | vs | IDirect3DVertexShader9* vs = 0; // [ebp-10h] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1364 | ID3DXBuffer | psBuffer | ID3DXBuffer* psBuffer = 0; // [ebp-14h] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1365 | ID3DXBuffer | psErrors | ID3DXBuffer* psErrors = 0; // [ebp-18h] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1374 | IDirect3DPixelShader9 | ps | IDirect3DPixelShader9* ps = 0; // [ebp-1Ch] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1378 | IDirect3DVertexDeclaration9 | declaration | IDirect3DVertexDeclaration9* declaration = 0; // [ebp-20h] |
| Code/BF2/RendDX9/RendDX9Init.cpp:1403 | IDirect3DSurface9 | target | IDirect3DSurface9* const target = |
| Code/BF2/RendDX9/RendDX9Init.cpp:1407 | IDirect3DSurface9 | backBuffer | IDirect3DSurface9* const backBuffer = rendererBackBufferSurface(); // +0x76C |
| Code/BF2/RendDX9/RendDX9Init.cpp:1412 | IDirect3DSurface9 | readback | IDirect3DSurface9* readback = 0; // [ebp-4] |
| Code/BF2/RendDX9/RenderQueues.cpp:80 | opaque; see annotation | annotated opaque storage | const unsigned int kRaTextureArrayWord = 0x2C8 / 4; // the 128 IDirect3DBaseTexture9* the |
| Code/BF2/RendDX9/RenderQueues.cpp:240 | IDirect3DBaseTexture9 | raw pointer overlay/cast | IDirect3DBaseTexture9* const texture = *reinterpret_cast<IDirect3DBaseTexture9**>( |
| Code/BF2/RendDX9/RenderQueues.cpp:240 | IDirect3DBaseTexture9 | texture | IDirect3DBaseTexture9* const texture = *reinterpret_cast<IDirect3DBaseTexture9**>( |
| Code/BF2/RendDX9/RenderQueues.cpp:407 | IDirect3DBaseTexture9 | texture | IDirect3DBaseTexture9* texture) |
| Code/BF2/RendDX9/RenderQueues.cpp:409 | IDirect3DBaseTexture9 | field | IDirect3DBaseTexture9** const field = |
| Code/BF2/RendDX9/RenderQueues.cpp:571 | IDirect3DDevice9 | device | RaShaderConstants* constructRaShaderConstants(void* storage, IDirect3DDevice9* device) |
| Code/BF2/RendDX9/RenderQueues.cpp:708 | IDirect3DBaseTexture9 | texture | IDirect3DBaseTexture9* const texture = static_cast<IDirect3DBaseTexture9*>( |
| Code/BF2/RendDX9/RenderQueues.cpp:730 | IDirect3DBaseTexture9 | raw pointer overlay/cast | IDirect3DBaseTexture9** array = reinterpret_cast<IDirect3DBaseTexture9**>( |
| Code/BF2/RendDX9/RenderQueues.cpp:730 | IDirect3DBaseTexture9 | array | IDirect3DBaseTexture9** array = reinterpret_cast<IDirect3DBaseTexture9**>( |
| Code/BF2/RendDX9/RenderQueues.cpp:738 | IUnknown | value | IUnknown* const value = static_cast<IUnknown*>(raPointer(constants, i)); |
| Code/BF2/RendDX9/RenderQueues.cpp:1306 | IDirect3DDevice9 | device | void* createRaShaderConstants(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/RenderQueues.cpp:1322 | IDirect3DBaseTexture9 | texture | void raShaderConstantsSetTexture(unsigned int holderOffset, IDirect3DBaseTexture9* texture) |
| Code/BF2/RendDX9/ResourceManager.cpp:87 | IDirect3DSurface9 | m_reservedSurface | IDirect3DSurface9* m_reservedSurface; // +0x0C, CreateRenderTarget out parameter |
| Code/BF2/RendDX9/ShaderManager.cpp:170 | opaque; see annotation | annotated opaque storage | void* effectD3dxHandle(void* effect); // wrapper+0x08, the ID3DXEffect* (the listener walks) |
| Code/BF2/RendDX9/ShaderManager.cpp:486 | ID3DXEffect | effect | ID3DXEffect* effect; // +0x10 |
| Code/BF2/RendDX9/ShaderManager.cpp:537 | ID3DXEffect | d3dx | ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>( |
| Code/BF2/RendDX9/ShaderManager.cpp:589 | IDirect3DDevice9 | device | bool createShaderManager(void* base, IDirect3DDevice9* device) |
| Code/BF2/RendDX9/ShaderManager.cpp:873 | ID3DXEffect | fx | ID3DXEffect* fx = static_cast<ID3DXEffect*>(sm->rawEffectA0); |
| Code/BF2/RendDX9/ShaderManager.cpp:878 | ID3DXEffect | fx | ID3DXEffect* fx = static_cast<ID3DXEffect*>( |
| Code/BF2/RendDX9/StateManager.cpp:116 | IDirect3DDevice9 | device | /* 0x000C */ IDirect3DDevice9* device; // AddRef'd by the ctor |
| Code/BF2/RendDX9/StateManager.cpp:136 | IDirect3DSurface9 | renderTarget | /* 0x1E90 */ IDirect3DSurface9* renderTarget; // refcounted (slot +0x60) |
| Code/BF2/RendDX9/StateManager.cpp:139 | IDirect3DSurface9 | depthStencil | /* 0x1E98 */ IDirect3DSurface9* depthStencil; // refcounted (slot +0x64) |
| Code/BF2/RendDX9/StateManager.cpp:194 | IDirect3DDevice9 | rendererDevice | inline IDirect3DDevice9* rendererDevice() |
| Code/BF2/RendDX9/StateManager.cpp:229 | IDirect3DDevice9 | dev | IDirect3DDevice9* dev = rendererDevice(); |
| Code/BF2/RendDX9/StateManager.cpp:820 | IDirect3DSurface9 | surface | int __stdcall smSetRenderTarget(StateManager* self, unsigned int index, IDirect3DSurface9* surface) |
| Code/BF2/RendDX9/StateManager.cpp:822 | IDirect3DSurface9 | value | IDirect3DSurface9** value = |
| Code/BF2/RendDX9/StateManager.cpp:823 | IDirect3DSurface9 | raw pointer overlay/cast | reinterpret_cast<IDirect3DSurface9**>(reinterpret_cast<unsigned char*>(self) |
| Code/BF2/RendDX9/StateManager.cpp:841 | IDirect3DSurface9 | surface | int __stdcall smSetDepthStencilSurface(StateManager* self, IDirect3DSurface9* surface) |
| Code/BF2/RendDX9/StateManager.cpp:861 | IDirect3DSurface9 | topSurfaceOf | IDirect3DSurface9* topSurfaceOf(void* baseTexture) |
| Code/BF2/RendDX9/StateManager.cpp:863 | IDirect3DBaseTexture9 | base | IDirect3DBaseTexture9* base = static_cast<IDirect3DBaseTexture9*>(baseTexture); |
| Code/BF2/RendDX9/StateManager.cpp:864 | IDirect3DSurface9 | surface | IDirect3DSurface9* surface = 0; |
| Code/BF2/RendDX9/StateManager.cpp:1221 | IDirect3DSurface9 | surface | void stateManagerSetRenderTarget(unsigned int index, IDirect3DSurface9* surface) |
| Code/BF2/RendDX9/StateManager.cpp:1226 | IDirect3DSurface9 | surface | void stateManagerSetDepthStencilSurface(IDirect3DSurface9* surface) |
| Code/BF2/RendDX9/StateManager.cpp:1233 | IDirect3DSurface9 | stateManagerRenderTarget0 | IDirect3DSurface9* stateManagerRenderTarget0() |
| Code/BF2/RendDX9/StateManager.cpp:1321 | IDirect3DIndexBuffer9 | indices | void stateManagerSetIndices(IDirect3DIndexBuffer9* indices) |
| Code/BF2/RendDX9/StateManager.cpp:1326 | IDirect3DVertexBuffer9 | vertexBuffer | void stateManagerSetStreamSource(unsigned int stream, IDirect3DVertexBuffer9* vertexBuffer, |
| Code/BF2/RendDX9/StateManager.cpp:1332 | IDirect3DVertexDeclaration9 | declaration | void stateManagerSetVertexDeclaration(IDirect3DVertexDeclaration9* declaration) |
| Code/BF2/RendDX9/StateManager.cpp:1341 | IDirect3DVertexShader9 | shader | void stateManagerSetVertexShader(IDirect3DVertexShader9* shader) |
| Code/BF2/RendDX9/StateManager.cpp:1346 | IDirect3DPixelShader9 | shader | void stateManagerSetPixelShader(IDirect3DPixelShader9* shader) |
| Code/BF2/RendDX9/Texture.cpp:46 | IDirect3DDevice9 | baseDevice | IDirect3DDevice9* baseDevice() |
| Code/BF2/RendDX9/Texture.cpp:164 | IDirect3DBaseTexture9 | d3d | bool textureAttach(EngineTexture* tex, IDirect3DBaseTexture9* d3d, const unsigned int desc[7]) |
| Code/BF2/RendDX9/Texture.cpp:214 | IDirect3DSurface9 | textureGetLevelSurface | IDirect3DSurface9* textureGetLevelSurface(EngineTexture* tex, unsigned int mip, unsigned int face) |
| Code/BF2/RendDX9/Texture.cpp:216 | IDirect3DSurface9 | surf | IDirect3DSurface9* surf = 0; |
| Code/BF2/RendDX9/Texture.cpp:241 | IDirect3DSurface9 | createStagingSurface | IDirect3DSurface9* createStagingSurface(unsigned int w, unsigned int h, unsigned int format) |
| Code/BF2/RendDX9/Texture.cpp:243 | IDirect3DDevice9 | device | IDirect3DDevice9* device = baseDevice(); |
| Code/BF2/RendDX9/Texture.cpp:244 | IDirect3DTexture9 | staging | IDirect3DTexture9* staging = 0; |
| Code/BF2/RendDX9/Texture.cpp:247 | IDirect3DSurface9 | surf | IDirect3DSurface9* surf = 0; |
| Code/BF2/RendDX9/Texture.cpp:259 | IDirect3DSurface9 | dst | void updateSurfaceFromStaging(IDirect3DSurface9* dst, IDirect3DSurface9* src) |
| Code/BF2/RendDX9/Texture.cpp:259 | IDirect3DSurface9 | src | void updateSurfaceFromStaging(IDirect3DSurface9* dst, IDirect3DSurface9* src) |
| Code/BF2/RendDX9/Texture.cpp:261 | IDirect3DDevice9 | device | IDirect3DDevice9* device = baseDevice(); |
| Code/BF2/RendDX9/Texture.cpp:292 | IDirect3DSurface9 | real | IDirect3DSurface9* real = textureGetLevelSurface(tex, mip, face); |
| Code/BF2/RendDX9/Texture.cpp:293 | IDirect3DSurface9 | stage | IDirect3DSurface9* stage = createStagingSurface(rec->width, rec->height, tex->format); |
| Code/BF2/RendDX9/Texture.cpp:371 | IDirect3DSurface9 | real | IDirect3DSurface9* real = textureGetLevelSurface(tex, rec->mip, rec->face); |
| Code/BF2/RendDX9/Texture.h:31 | IDirect3DBaseTexture9 | baseTexture | IDirect3DBaseTexture9* baseTexture; // +0x08 |
| Code/BF2/RendDX9/Texture.h:68 | IDirect3DSurface9 | staging | IDirect3DSurface9* staging; // +0x1C default-pool path only, else 0 |
| Code/BF2/RendDX9/Texture.h:79 | IDirect3DBaseTexture9 | d3d | bool textureAttach(EngineTexture* tex, IDirect3DBaseTexture9* d3d, |
| Code/BF2/RendDX9/Texture.h:85 | IDirect3DSurface9 | textureGetLevelSurface | IDirect3DSurface9* textureGetLevelSurface(EngineTexture* tex, unsigned int mip, |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/ITextureAtlas.h:133 | IDirect3DDevice9 | device | static ITextureAtlas* __fastcall create(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:86 | IDirect3DTexture9 | atlasTexture | IDirect3DTexture9* atlasTexture = 0; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:100 | IDirect3DTexture9 | texture | IDirect3DTexture9* texture; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:134 | IDirect3DSurface9 | srcSurface | IDirect3DSurface9* srcSurface = 0; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:135 | IDirect3DSurface9 | dstSurface | IDirect3DSurface9* dstSurface = 0; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:190 | IDirect3DTexture9 | dxtTexture | IDirect3DTexture9* dxtTexture; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:194 | IDirect3DSurface9 | srcSurface | IDirect3DSurface9* srcSurface = 0; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:195 | IDirect3DSurface9 | dstSurface | IDirect3DSurface9* dstSurface = 0; |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:252 | IDirect3DDevice9 | device | TextureAtlasD3D9::TextureAtlasD3D9(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:276 | IDirect3DDevice9 | device | ITextureAtlas* __fastcall ITextureAtlas::create(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.h:31 | IDirect3DDevice9 | device | explicit TextureAtlasD3D9(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.h:41 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; |
| Code/BF2/RendDX9/TextureManager.cpp:173 | IDirect3DDevice9 | device | explicit TextureManager(IDirect3DDevice9* device); |
| Code/BF2/RendDX9/TextureManager.cpp:232 | IDirect3DSurface9 | source | bool copyRenderTargetToSurface(IDirect3DSurface9* source, IDirect3DSurface9* destination) |
| Code/BF2/RendDX9/TextureManager.cpp:232 | IDirect3DSurface9 | destination | bool copyRenderTargetToSurface(IDirect3DSurface9* source, IDirect3DSurface9* destination) |
| Code/BF2/RendDX9/TextureManager.cpp:300 | IDirect3DDevice9 | m_device | IDirect3DDevice9* m_device; // +0x0C |
| Code/BF2/RendDX9/TextureManager.cpp:384 | IDirect3DDevice9 | rendererDevice | IDirect3DDevice9* rendererDevice() |
| Code/BF2/RendDX9/TextureManager.cpp:401 | IDirect3DDevice9 | device | TextureManager::TextureManager(IDirect3DDevice9* device) |
| Code/BF2/RendDX9/TextureManager.cpp:614 | IDirect3DTexture9 | tex2D | IDirect3DTexture9* tex2D = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:615 | IDirect3DVolumeTexture9 | texVol | IDirect3DVolumeTexture9* texVol = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:685 | IDirect3DBaseTexture9 | base | IDirect3DBaseTexture9* base = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:729 | IDirect3DCubeTexture9 | cube | IDirect3DCubeTexture9* cube = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:746 | IDirect3DBaseTexture9 | base | IDirect3DBaseTexture9* base = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:1469 | IDirect3DSurface9 | sourceSurface | IDirect3DSurface9* sourceSurface = 0; |
| Code/BF2/RendDX9/TextureManager.cpp:1479 | IDirect3DSurface9 | destinationSurface | IDirect3DSurface9* const destinationSurface = textureGetLevelSurface(destination, level, face); |
| Code/BF2/RendDX9/TextureManager.cpp:1586 | IDirect3DDevice9 | device | bool createTextureManager(IDirect3DDevice9* device) |
| Code/BF2/RendMisc/EnvMapSystem/EnvMapManager.cpp:290 | IDirect3DSurface9 | destination | IDirect3DSurface9* const destination= |
| Code/BF2/RendMisc/EnvMapSystem/EnvMapManager.cpp:292 | IDirect3DSurface9 | source | IDirect3DSurface9* const source=dx9rend::textureGetLevelSurface(target,0u,face); |

Additional untyped member candidates (including the state mirrors named above) are listed in `d3d9_crossings.csv` with kind `opaque member candidate`.

## Additional source files outside the requested trees

| File | Calls/candidates | Role |
| --- | --- | --- |
| Code/BF2/External/gameswf/gameswf_render_handler_d3d.cpp | 142 | vendor |
| Code/BF2/External/gameswf/gameswf_render_handler_xbox.cpp | 10 | vendor |
| Code/BF2/External/imgui/backends/imgui_impl_dx9.cpp | 111 | vendor |
| Code/BF2/External/imgui/backends/imgui_impl_dx9.h | 0 | vendor |
| Code/BF2/Platform/Gfx/D3D9Subset.h | 0 | engine/shared |
| Code/BF2/Platform/Gfx/D3DX9Subset.h | 0 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/Device.cpp | 11 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/DeviceFrontiers.inc | 0 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/DeviceResources.inc | 8 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/DeviceState.inc | 0 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/Frontiers.cpp | 0 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/Resources.inc | 8 | engine/shared |
| Code/BF2/Platform/Gfx/Metal/StateCache.inc | 0 | engine/shared |
| Code/Build/textureatlas_diff/host.cpp | 3 | test/tool |
| Code/Tests/buffer_manager_collect_runtime.cpp | 0 | test/tool |
| Code/Tests/effect_stock_d3d_runtime.cpp | 35 | test/tool |
| Code/Tests/env_map_runtime.cpp | 0 | test/tool |
| Code/Tests/fsmovie_d3d_runtime.cpp | 23 | test/tool |
| Code/Tests/light_manager_runtime.cpp | 2 | test/tool |
| Code/Tests/macos_metal_device/groups.inc | 192 | test/tool |
| Code/Tests/macos_metal_device/main.cpp | 59 | test/tool |
| Code/Tests/particle_renderer_fixture_support.h | 0 | test/tool |
| Code/Tests/quad_drawer_states_runtime.cpp | 0 | test/tool |
| Code/Tests/renddx9_detail_helpers_runtime.cpp | 0 | test/tool |
| Code/Tests/resource_manager_lifecycle_runtime.cpp | 0 | test/tool |
| Code/Tests/road_objects_runtime.cpp | 0 | test/tool |
| Code/Tests/video_bink_d3d_runtime.cpp | 10 | test/tool |
| Code/Tools/BF2Editor/App/EditorApp.cpp | 2 | test/tool |
| Code/Tools/BF2Editor/App/EngineHost.cpp | 4 | test/tool |
| Code/Tools/BF2Editor/App/EngineHost.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/FrameCapture.cpp | 8 | test/tool |
| Code/Tools/BF2Editor/App/FrameCapture.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/FrozenFrame.cpp | 9 | test/tool |
| Code/Tools/BF2Editor/App/FrozenFrame.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/ImGuiHost.cpp | 0 | test/tool |
| Code/Tools/BF2Editor/App/ImGuiHost.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/MinimapPanel.cpp | 4 | test/tool |
| Code/Tools/BF2Editor/App/MinimapPanel.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/SceneOverlay.cpp | 1 | test/tool |
| Code/Tools/BF2Editor/App/SceneOverlay.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/TexturePalette.cpp | 5 | test/tool |
| Code/Tools/BF2Editor/App/TexturePalette.h | 0 | test/tool |
| Code/Tools/BF2Editor/App/ViewRecorder.cpp | 21 | test/tool |
| Code/Tools/BF2Editor/App/ViewRecorder.h | 0 | test/tool |

Outside counts are excluded from all tables above; filter `scope=outside` in the two call CSVs.

## Proposed seam groups (proposal)

Preserve each listed call and its arguments 1:1 in a Windows D3D9 backend first. The proposed grouping is organizational; it does not authorize removing inherited COM operations, changing lifetimes, coalescing calls, changing flags, or replacing effect behavior. Metal comes after the Windows wrapper is validated.

| Group | Exact observed methods |
| --- | --- |
| D3DX helpers (math) | D3DX free::D3DXFloat32To16Array, D3DX free::D3DXMatrixInverse, D3DX free::D3DXMatrixLookAtLH, D3DX free::D3DXMatrixMultiply, D3DX free::D3DXMatrixOrthoLH, D3DX free::D3DXMatrixRotationZ, D3DX free::D3DXMatrixTranspose |
| D3DX helpers (mesh) | D3DX free::D3DXComputeBoundingBox, D3DX free::D3DXComputeBoundingSphere, D3DX free::D3DXCreateBox, D3DX free::D3DXCreateCylinder, D3DX free::D3DXCreateSphere, ID3DXMesh::AddRef, ID3DXMesh::CloneMesh, ID3DXMesh::CloneMeshFVF, ID3DXMesh::GenerateAdjacency, ID3DXMesh::GetIndexBuffer, ID3DXMesh::GetNumFaces, ID3DXMesh::GetNumVertices, ID3DXMesh::GetVertexBuffer, ID3DXMesh::LockIndexBuffer, ID3DXMesh::LockVertexBuffer, ID3DXMesh::OptimizeInplace, ID3DXMesh::Release, ID3DXMesh::UnlockIndexBuffer, ID3DXMesh::UnlockVertexBuffer |
| D3DX helpers (other) | D3DX free::D3DXAssembleShader, D3DX free::D3DXCreateBuffer, D3DX free::D3DXCreateMesh, D3DX free::D3DXCreateMeshFVF, D3DX free::D3DXGetShaderVersion, ID3DXBuffer::GetBufferPointer, ID3DXBuffer::GetBufferSize, ID3DXBuffer::Release, ID3DXInclude::Close, ID3DXInclude::Open, ID3DXRenderToSurface::BeginScene, ID3DXRenderToSurface::EndScene, ID3DXRenderToSurface::Release |
| D3DX helpers (texture loading) | D3DX free::D3DXCreateRenderToSurface, D3DX free::D3DXCreateTexture, D3DX free::D3DXCreateTextureFromFileExA, D3DX free::D3DXCreateTextureFromFileInMemory, D3DX free::D3DXFillTexture, D3DX free::D3DXFilterTexture, D3DX free::D3DXGetImageInfoFromFileA, D3DX free::D3DXGetImageInfoFromFileInMemory, D3DX free::D3DXLoadSurfaceFromMemory, D3DX free::D3DXLoadSurfaceFromSurface, D3DX free::D3DXSaveSurfaceToFileA, D3DX free::D3DXSaveTextureToFileA |
| declarations | IDirect3DDevice9::CreateVertexDeclaration, IDirect3DDevice9::SetVertexDeclaration, IDirect3DVertexDeclaration9::GetDeclaration |
| draws | IDirect3DDevice9::BeginScene, IDirect3DDevice9::DrawIndexedPrimitive, IDirect3DDevice9::DrawPrimitive, IDirect3DDevice9::DrawPrimitiveUP, IDirect3DDevice9::EndScene, IDirect3DSwapChain9::Present |
| effects | D3DX free::D3DXCreateEffect, D3DX free::D3DXCreateEffectCompiler, ID3DXEffect::AddRef, ID3DXEffect::Begin, ID3DXEffect::BeginPass, ID3DXEffect::CommitChanges, ID3DXEffect::End, ID3DXEffect::EndPass, ID3DXEffect::FindNextValidTechnique, ID3DXEffect::GetAnnotationByName, ID3DXEffect::GetBool, ID3DXEffect::GetInt, ID3DXEffect::GetIntArray, ID3DXEffect::GetParameter, ID3DXEffect::GetParameterByName, ID3DXEffect::GetParameterDesc, ID3DXEffect::GetParameterElement, ID3DXEffect::GetPass, ID3DXEffect::GetString, ID3DXEffect::GetTechnique, ID3DXEffect::GetTechniqueByName, ID3DXEffect::GetTechniqueDesc, ID3DXEffect::IsParameterUsed, ID3DXEffect::OnLostDevice, ID3DXEffect::OnResetDevice, ID3DXEffect::Release, ID3DXEffect::SetArrayRange, ID3DXEffect::SetBool, ID3DXEffect::SetBoolArray, ID3DXEffect::SetFloat, ID3DXEffect::SetFloatArray, ID3DXEffect::SetInt, ID3DXEffect::SetIntArray, ID3DXEffect::SetMatrix, ID3DXEffect::SetMatrixArray, ID3DXEffect::SetMatrixPointerArray, ID3DXEffect::SetStateManager, ID3DXEffect::SetTechnique, ID3DXEffect::SetTexture, ID3DXEffect::SetValue, ID3DXEffect::SetVector, ID3DXEffect::SetVectorArray, ID3DXEffect::ValidateTechnique, ID3DXEffectCompiler::CompileEffect, ID3DXEffectCompiler::GetAnnotationByName, ID3DXEffectCompiler::GetParameterByName, ID3DXEffectCompiler::GetParameterDesc, ID3DXEffectCompiler::GetParameterElement, ID3DXEffectCompiler::GetString, ID3DXEffectCompiler::Release |
| lock/unlock | IDirect3DCubeTexture9::LockRect, IDirect3DCubeTexture9::UnlockRect, IDirect3DIndexBuffer9::Lock, IDirect3DIndexBuffer9::Unlock, IDirect3DSurface9::LockRect, IDirect3DSurface9::UnlockRect, IDirect3DTexture9::LockRect, IDirect3DTexture9::UnlockRect, IDirect3DVertexBuffer9::Lock, IDirect3DVertexBuffer9::Unlock, IDirect3DVolumeTexture9::LockBox, IDirect3DVolumeTexture9::UnlockBox, unresolved::Lock, unresolved::Unlock |
| queries | IDirect3DBaseTexture9::QueryInterface, IDirect3DCubeTexture9::QueryInterface, IDirect3DDevice9::CreateQuery, IDirect3DQuery9::GetData, IDirect3DQuery9::Issue, IDirect3DQuery9::Release, IDirect3DTexture9::QueryInterface, IDirect3DVolumeTexture9::QueryInterface |
| render targets | IDirect3DCubeTexture9::GetCubeMapSurface, IDirect3DDevice9::Clear, IDirect3DDevice9::CreateAdditionalSwapChain, IDirect3DDevice9::CreateDepthStencilSurface, IDirect3DDevice9::CreateOffscreenPlainSurface, IDirect3DDevice9::CreateRenderTarget, IDirect3DDevice9::GetBackBuffer, IDirect3DDevice9::GetDepthStencilSurface, IDirect3DDevice9::GetRenderTargetData, IDirect3DDevice9::GetSwapChain, IDirect3DDevice9::GetViewport, IDirect3DDevice9::SetDepthStencilSurface, IDirect3DDevice9::SetRenderTarget, IDirect3DDevice9::SetScissorRect, IDirect3DDevice9::SetViewport, IDirect3DDevice9::StretchRect, IDirect3DDevice9::UpdateSurface, IDirect3DTexture9::GetSurfaceLevel, unresolved::GetBackBuffer |
| resources, lifetime and device lifecycle | D3D9 free::Direct3DCreate9, IDirect3D9::AddRef, IDirect3D9::CheckDeviceFormat, IDirect3D9::CheckDeviceMultiSampleType, IDirect3D9::CheckDeviceType, IDirect3D9::CreateDevice, IDirect3D9::EnumAdapterModes, IDirect3D9::GetAdapterCount, IDirect3D9::GetAdapterDisplayMode, IDirect3D9::GetAdapterIdentifier, IDirect3D9::GetAdapterModeCount, IDirect3D9::GetDeviceCaps, IDirect3DBaseTexture9::AddRef, IDirect3DBaseTexture9::GetType, IDirect3DBaseTexture9::PreLoad, IDirect3DBaseTexture9::Release, IDirect3DBaseTexture9::SetPriority, IDirect3DCubeTexture9::Release, IDirect3DDevice9::AddRef, IDirect3DDevice9::CreateCubeTexture, IDirect3DDevice9::CreateIndexBuffer, IDirect3DDevice9::CreatePixelShader, IDirect3DDevice9::CreateTexture, IDirect3DDevice9::CreateVertexBuffer, IDirect3DDevice9::CreateVertexShader, IDirect3DDevice9::CreateVolumeTexture, IDirect3DDevice9::GetDeviceCaps, IDirect3DDevice9::Release, IDirect3DDevice9::Reset, IDirect3DDevice9::TestCooperativeLevel, IDirect3DIndexBuffer9::AddRef, IDirect3DIndexBuffer9::GetDesc, IDirect3DIndexBuffer9::Release, IDirect3DPixelShader9::GetFunction, IDirect3DResource9::PreLoad, IDirect3DSurface9::AddRef, IDirect3DSurface9::GetDesc, IDirect3DSurface9::Release, IDirect3DTexture9::AddRef, IDirect3DTexture9::GetLevelCount, IDirect3DTexture9::GetLevelDesc, IDirect3DTexture9::Release, IDirect3DVertexBuffer9::AddRef, IDirect3DVertexBuffer9::GetDesc, IDirect3DVertexBuffer9::Release, IDirect3DVertexDeclaration9::AddRef, IDirect3DVertexDeclaration9::Release, IDirect3DVolumeTexture9::Release, IUnknown::AddRef, IUnknown::Release, unresolved::Release |
| state | IDirect3DDevice9::SetClipPlane, IDirect3DDevice9::SetIndices, IDirect3DDevice9::SetPixelShader, IDirect3DDevice9::SetPixelShaderConstantB, IDirect3DDevice9::SetPixelShaderConstantF, IDirect3DDevice9::SetPixelShaderConstantI, IDirect3DDevice9::SetRenderState, IDirect3DDevice9::SetSamplerState, IDirect3DDevice9::SetStreamSource, IDirect3DDevice9::SetTexture, IDirect3DDevice9::SetTextureStageState, IDirect3DDevice9::SetVertexShader, IDirect3DDevice9::SetVertexShaderConstantB, IDirect3DDevice9::SetVertexShaderConstantF, IDirect3DDevice9::SetVertexShaderConstantI |

## Self-check and limitations

Independent syntax cross-check: 0 unmatched capitalized member-call sites; 0 raw `d3dxSlot` occurrences versus 0 dispatch rows. D3DX import-manifest check unavailable: `D3dx9_25.def` was retired in favor of the SDK import library. Details are in `coverage.json`. These checks catch extraction omissions but do not establish the type of opaque receivers.

12 requested-tree sites remain unresolved. Ten deterministic examples (or all when fewer):

| Location | Receiver | Method | Arguments |
| --- | --- | --- | --- |
| Code/BF2/Geom/SwiffRender.cpp:335 | rec->texture | Release |  |
| Code/BF2/Geom/SwiffRender.cpp:351 | rec->texture | Release |  |
| Code/BF2/RendDX9/BufferManager.cpp:248 | self->buffer | Release |  |
| Code/BF2/RendDX9/BufferManager.cpp:362 | self->buffer | Lock | byteOffset, byteSize, &mapped, flags |
| Code/BF2/RendDX9/BufferManager.cpp:372 | self->buffer | Unlock |  |
| Code/BF2/RendDX9/BufferManager.cpp:434 | self->buffer | Release |  |
| Code/BF2/RendDX9/BufferManager.cpp:476 | self->buffer | Release |  |
| Code/BF2/RendDX9/BufferManager.cpp:539 | self->buffer | Lock | offsetIndices * bytesPerIndex, countIndices * bytesPerIndex, &mapped, flags |
| Code/BF2/RendDX9/BufferManager.cpp:548 | self->buffer | Unlock |  |
| Code/BF2/RendDX9/BufferManager.cpp:574 | self->buffer | Release |  |

Twenty rows are selected without replacement with Python `random.Random(1509)` from the sorted requested-tree rows. `spot_checks.csv` holds them and `spot_checks.txt` holds numbered source excerpts. The review result is recorded in `docs/macos-r0-d3d9-inventory.md`; sampling checks precision, not exhaustive recall. Manual review of the source snapshot at `df2262e`: all 20 rows matched their location, receiver interface/method and arguments. That review applies to this baseline only; re-run it when inputs change.

| Location | Interface::method | Resolved |
| --- | --- | --- |
| Code/BF2/Geom/UndergrowthSystemRender.cpp:867 | IDirect3DTexture9::LockRect | yes |
| Code/BF2/RendDX9/Texture.cpp:328 | IUnknown::Release | yes |
| Code/BF2/Geom/RoadEditable.cpp:872 | IDirect3DSurface9::Release | yes |
| Code/BF2/Geom/Terrain.cpp:1371 | ID3DXMesh::LockIndexBuffer | yes |
| Code/BF2/Geom/QuadPatchEditable.cpp:1075 | IDirect3DSurface9::Release | yes |
| Code/BF2/Geom/RoadEditable.cpp:906 | IDirect3DDevice9::SetVertexShaderConstantF | yes |
| Code/BF2/RendDX9/RendDX9Init.cpp:801 | IDirect3DDevice9::GetSwapChain | yes |
| Code/BF2/Geom/HemiMapManager/HemiMapManager.cpp:416 | IDirect3DSurface9::Release | yes |
| Code/BF2/RendDX9/TextureAtlas/TextureAtlasBuilder/TextureAtlasD3D9.cpp:151 | IDirect3DSurface9::Release | yes |
| Code/BF2/Geom/XpLightManager.cpp:623 | IDirect3DSurface9::AddRef | yes |
| Code/BF2/RendDX9/FXParameter.cpp:302 | ID3DXEffect::SetBool | yes |
| Code/BF2/RendDX9/Ra/RaShaderPermutator.cpp:687 | ID3DXInclude::Close | yes |
| Code/BF2/RendDX9/RendDX9Init.cpp:182 | IDirect3D9::CheckDeviceFormat | yes |
| Code/BF2/RendDX9/RendDX9Init.cpp:1400 | IDirect3DDevice9::EndScene | yes |
| Code/BF2/Geom/LightmapGeneration/LightmapGeneration.cpp:1021 | IDirect3DVertexBuffer9::Release | yes |
| Code/BF2/Geom/QuadPatchEditable.cpp:1447 | IDirect3DSurface9::Release | yes |
| Code/BF2/RendDX9/Ra/RaShader.cpp:1300 | ID3DXEffect::GetInt | yes |
| Code/BF2/RendDX9/Ra/RaShader.cpp:1406 | ID3DXEffect::GetParameterByName | yes |
| Code/BF2/Geom/SurroundingPatchEditable.cpp:793 | IDirect3DSurface9::UnlockRect | yes |
| Code/BF2/GeomMisc/MenuRend/Video.cpp:155 | IDirect3DSurface9::GetDesc | yes |

No engine source was changed. No binary recovery, renderer build, GPU execution, Windows wrapper validation, server test or Metal rendering is claimed.
