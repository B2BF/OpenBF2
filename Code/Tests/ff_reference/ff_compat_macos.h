// [CHOSEN, not recovered] The D3D9 names ff_run.inc uses that the port's D3D9Subset.h does not
// declare (values from the public d3d9types.h), so macos_metal_device can include ff_run.inc.
#pragma once
#include "../../BF2/Platform/Gfx/D3D9Subset.h"

enum D3DLIGHTTYPE { D3DLIGHT_POINT = 1, D3DLIGHT_SPOT = 2, D3DLIGHT_DIRECTIONAL = 3 };
struct D3DLIGHT9 {
    D3DLIGHTTYPE Type; D3DCOLORVALUE Diffuse, Specular, Ambient; D3DVECTOR Position, Direction;
    float Range, Falloff, Attenuation0, Attenuation1, Attenuation2, Theta, Phi;
};
#define FFC_RS(n) static_cast<D3DRENDERSTATETYPE>(n)
#define D3DRS_FOGENABLE FFC_RS(28)
#define D3DRS_SPECULARENABLE FFC_RS(29)
#define D3DRS_FOGTABLEMODE FFC_RS(35)
#define D3DRS_FOGSTART FFC_RS(36)
#define D3DRS_FOGEND FFC_RS(37)
#define D3DRS_FOGDENSITY FFC_RS(38)
#define D3DRS_RANGEFOGENABLE FFC_RS(48)
#define D3DRS_CLIPPING FFC_RS(136)
#define D3DRS_LIGHTING FFC_RS(137)
#define D3DRS_AMBIENT FFC_RS(139)
#define D3DRS_FOGVERTEXMODE FFC_RS(140)
#define D3DRS_COLORVERTEX FFC_RS(141)
#define D3DRS_LOCALVIEWER FFC_RS(142)
#define D3DRS_NORMALIZENORMALS FFC_RS(143)
#define D3DRS_DIFFUSEMATERIALSOURCE FFC_RS(145)
#define D3DRS_SPECULARMATERIALSOURCE FFC_RS(146)
#define D3DRS_AMBIENTMATERIALSOURCE FFC_RS(147)
#define D3DRS_EMISSIVEMATERIALSOURCE FFC_RS(148)
#define D3DRS_VERTEXBLEND FFC_RS(151)
#define D3DRS_INDEXEDVERTEXBLENDENABLE FFC_RS(167)
#define D3DTSS_COLOROP static_cast<D3DTEXTURESTAGESTATETYPE>(1)
#define D3DTSS_TEXCOORDINDEX static_cast<D3DTEXTURESTAGESTATETYPE>(11)
#define D3DTOP_DISABLE 1
#define D3DTOP_MODULATE 4
#define D3DFOG_NONE 0
#define D3DTS_TEXTURE1 static_cast<D3DTRANSFORMSTATETYPE>(17)
#define D3DTS_WORLDMATRIX(n) static_cast<D3DTRANSFORMSTATETYPE>(256 + (n))
#define D3DPV_DONOTCOPYDATA 1
#define FF_RUN_USAGE 0   // hardware vertex processing device (ff_run.inc)
#define D3DFVF_TEXCOORDSIZE4(i) (2u << ((i) * 2 + 16))
