// [CHOSEN, not recovered] Checks of Direct3D 9 shader bytecode against rules D3D9's shader
// validator (D3DX assembler, CreateVertexShader/CreatePixelShader) enforces and MojoShader does not:
//  - X5326 "Read of uninitialized components": every component an instruction reads from r# must
//    have been written by an earlier instruction. Straight-line over the token stream (a write
//    inside an if or a loop body counts from there on), per component, with the source swizzle
//    and the destination write mask deciding which components are read.
//  - X5346/X5393: vs_1_1 and vs_2_x read at most one distinct c# per instruction (not sincos).
//  - X5202: ps_2_0 source swizzles are .xyzw, .x, .y, .z, .w, .yzxw, .zxyw or .wzyx only.
//  - X5406: ps_2_x writes oC#/oDepth with mov only.
//  - Register and slot limits: r# count (ps_1_1-1_3 2, ps_1_4 6, vs_1_1/vs_2_0/ps_2_0 12) and
//    instruction slots (vs_1_1 128, vs_2_0 256, ps_2_0 64 arithmetic + 32 texture), counted with
//    the documented per-instruction slot costs; approximate where D3DX's own expansion differs.
// Public D3D9 token format only.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace nativefx {

struct D3dbcCheckResult {
    unsigned reads = 0;          // r# components read
    unsigned uninitialized = 0;  // of those, read before any write
    unsigned constLimit = 0;     // instructions reading two or more distinct c# (vs_1_1, vs_2_x)
    unsigned swizzle = 0;        // ps_2_0 source operands with a swizzle ps_2_0 cannot encode
    unsigned outputWrite = 0;    // ps_2_x instructions other than mov writing oC#/oDepth
    unsigned temps = 0;          // highest r# index + 1
    unsigned slots = 0, texSlots = 0;  // instruction slots (ps: arithmetic, texture)
    bool tempLimit = false, slotLimit = false;
    bool malformed = false;
    std::string first;           // first uninitialized read, "instruction N: opcode O reads rK.c"
    std::string firstRule;       // first encoding finding
    unsigned encodingFindings() const { return constLimit + swizzle + outputWrite + tempLimit + slotLimit; }
    bool clean() const { return !uninitialized && !encodingFindings() && !malformed; }
};

inline D3dbcCheckResult checkD3dbc(const std::vector<uint8_t>& bytes) {
    D3dbcCheckResult res;
    const size_t n = bytes.size() / 4;
    auto tok = [&](size_t i) {
        return uint32_t(bytes[4 * i]) | uint32_t(bytes[4 * i + 1]) << 8 | uint32_t(bytes[4 * i + 2]) << 16 |
               uint32_t(bytes[4 * i + 3]) << 24;
    };
    if (n < 2) { res.malformed = true; return res; }
    const uint32_t version = tok(0);
    const bool pixel = (version >> 16) == 0xffff;
    if (!pixel && (version >> 16) != 0xfffe) { res.malformed = true; return res; }
    const unsigned major = (version >> 8) & 0xff, minor = version & 0xff;
    std::vector<uint8_t> written(4096, 0);  // per temp register: component mask written so far
    unsigned samplerDim[16] = {};           // dcl'd sampler type (2 = 2D, 3 = cube, 4 = volume)
    auto regType = [](uint32_t t) { return ((t >> 28) & 7) | ((t >> 8) & 0x18); };
    auto swz = [](uint32_t t, unsigned c) { return (t >> (16 + 2 * c)) & 3; };
    unsigned index = 0;
    size_t i = 1;
    while (i < n) {
        const uint32_t op = tok(i);
        if (op == 0x0000ffff) {
            const unsigned tempMax = pixel ? (major == 1 ? (minor < 4 ? 2 : 6) : major == 2 && minor == 0 ? 12 : 32)
                                           : (major == 1 || (major == 2 && minor == 0) ? 12 : 32);
            res.tempLimit = res.temps > tempMax;
            if (pixel) res.slotLimit = major == 2 && minor == 0 && (res.slots > 64 || res.texSlots > 32);
            else res.slotLimit = (major == 1 && res.slots > 128) || (major == 2 && minor == 0 && res.slots > 256);
            if (res.tempLimit && res.firstRule.empty()) res.firstRule = "uses r" + std::to_string(res.temps - 1);
            if (res.slotLimit && res.firstRule.empty()) res.firstRule = std::to_string(res.slots) + " arithmetic slots";
            return res;
        }
        const uint32_t code = op & 0xffff;
        if (code == 0xfffe) { i += 1 + ((op >> 16) & 0x7fff); continue; }  // comment
        ++index;
        // Parameter tokens: SM2+ store the length; SM1 needs the opcode's operand count.
        size_t len;
        if (major >= 2) len = (op >> 24) & 0xf;
        else {
            switch (code) {
                case 0: case 0xfffd: len = 0; break;                                   // nop, phase
                case 1: case 6: case 7: case 14: case 15: case 16: case 19: case 78: case 79: len = 2; break;
                case 2: case 3: case 5: case 8: case 9: case 10: case 11: case 12: case 13: case 17:
                case 20: case 21: case 22: case 23: case 24: case 89: len = 3; break;
                case 4: case 18: case 80: case 88: len = 4; break;
                case 31: len = 2; break;                                               // dcl (vs_1_1)
                case 81: len = 5; break;                                               // def
                case 64: case 66: len = (pixel && minor >= 4) ? 2 : 1; break;          // texcoord/tex
                case 65: case 87: len = 1; break;                                      // texkill, texdepth
                case 76: len = 3; break;                                               // texm3x3spec
                case 67: case 68: case 69: case 70: case 71: case 72: case 73: case 74: case 75:
                case 77: case 82: case 83: case 84: case 85: case 86: len = 2; break;
                default: res.malformed = true; return res;
            }
        }
        if (i + 1 + len > n) { res.malformed = true; return res; }
        // Operand tokens without SM2+ relative-address tokens (bit 13 on the operand before).
        std::vector<uint32_t> params;
        for (size_t k = 0; k < len; ++k) {
            const uint32_t t = tok(i + 1 + k);
            params.push_back(t);
            if (major >= 2 && (t & 0x80000000u) && (t & 0x2000u) && code != 31 && code != 81 && code != 47 && code != 48) ++k;
        }
        const uint32_t* p = params.data();
        i += 1 + len;
        len = params.size();
        // Instructions without a destination + sources layout.
        if (code == 31) {  // dcl: record sampler dimensions
            if (len >= 2 && regType(p[1]) == 10) samplerDim[p[1] & 15] = (p[0] >> 27) & 15;
            continue;
        }
        if (code == 81 || code == 47 || code == 48 || code == 0 || code == 0xfffd) continue;  // def/defi/defb
        for (size_t k = 0; k < len; ++k) if (regType(p[k]) == 0 && (p[k] & 0x7ff) + 1 > res.temps) res.temps = (p[k] & 0x7ff) + 1;
        {
            unsigned cost = 1;
            switch (code) {
                case 20: case 22: cost = 4; break;            // m4x4, m3x4
                case 21: case 23: case 16: case 36: case 32: cost = 3; break;  // m4x3, m3x3, lit, nrm, pow
                case 24: case 33: case 18: cost = 2; break;   // m3x2, crs, lrp
                case 19: cost = pixel ? 1 : 3; break;          // frc (vs macro)
                case 34: cost = 3; break;                      // sgn
                case 37: cost = 8; break;                      // sincos
                case 14: case 15: cost = pixel ? 1 : 10; break;  // exp, log (vs macros)
                default: break;
            }
            const bool tex = pixel && major == 2 && (code == 66 || code == 65 || code == 93 || code == 95);
            if (tex) res.texSlots += 1; else if (code < 38 || code > 45) res.slots += cost;
        }
        if (code >= 25 && code <= 30) continue;     // call, callnz, loop, ret, endloop, label
        if ((code >= 38 && code <= 45) || code == 96) {  // rep..breakc, breakp: sources only
            for (size_t k = 0; k < len; ++k) if (regType(p[k]) == 0) {
                const unsigned r = p[k] & 0x7ff;
                for (unsigned c = 0; c < 4; ++c) {
                    const unsigned s = swz(p[k], c);
                    ++res.reads;
                    if (!(written[r] & (1u << s))) {
                        if (!res.uninitialized++) res.first = "instruction " + std::to_string(index) + ": opcode " + std::to_string(code) + " reads r" + std::to_string(r) + "." + "xyzw"[s];
                    }
                }
            }
            continue;
        }
        if (len == 0) continue;
        const uint32_t dst = p[0];
        const unsigned mask = (dst >> 16) & 15;
        // Encoding rules (sources are p[1..]; texkill/texdepth have none).
        auto rule = [&](const char* what) {
            if (res.firstRule.empty()) res.firstRule = "instruction " + std::to_string(index) + ": opcode " + std::to_string(code) + " " + what;
        };
        if (!pixel && major < 3 && code != 37) {
            unsigned distinct = 0, seen[4] = {};
            for (size_t k = 1; k < len; ++k) if (regType(p[k]) == 2) {
                const unsigned r = p[k] & 0x7ff;
                bool dup = false;
                for (unsigned d = 0; d < distinct; ++d) dup |= seen[d] == r;
                if (!dup && distinct < 4) seen[distinct++] = r;
            }
            if (distinct > 1) { ++res.constLimit; rule("reads two or more distinct c#"); }
        }
        if (pixel && major == 2 && minor == 0) {
            for (size_t k = 1; k < len; ++k) {
                const unsigned sw = (p[k] >> 16) & 0xff;
                if (sw != 0xe4 && sw != 0x00 && sw != 0x55 && sw != 0xaa && sw != 0xff && sw != 0xc9 && sw != 0xd2 && sw != 0x1b) {
                    ++res.swizzle; rule("source swizzle not encodable in ps_2_0");
                }
            }
        }
        if (pixel && major == 2 && code != 1 && code != 65 && (regType(dst) == 8 || regType(dst) == 9)) {
            ++res.outputWrite; rule("writes oC#/oDepth without mov");
        }
        // texkill: its operand is a destination token that is read (the components masked).
        // ps_1_x texture ops read texture registers (t#), not temps; texdepth reads r5.xy.
        std::vector<std::pair<uint32_t, unsigned>> reads;  // (source token, component mask read)
        auto perComponent = [&](uint32_t src, unsigned dmask) {
            unsigned m = 0;
            for (unsigned c = 0; c < 4; ++c) if (dmask & (1u << c)) m |= 1u << swz(src, c);
            return m;
        };
        auto first = [&](uint32_t src, unsigned count) {
            unsigned m = 0;
            for (unsigned c = 0; c < count; ++c) m |= 1u << swz(src, c);
            return m;
        };
        if (code == 65) {
            if (regType(dst) == 0) reads.push_back({dst | 0xe40000u, pixel && major < 2 ? 7u : mask});
        } else if (code == 87) {
            if (regType(dst) == 0) reads.push_back({dst | 0xe40000u, 3u});
        } else {
            for (size_t k = 1; k < len; ++k) {
                const uint32_t s = p[k];
                unsigned m;
                switch (code) {
                    case 8: case 36: m = first(s, 3); break;                        // dp3, nrm
                    case 9: m = first(s, 4); break;                                 // dp4
                    case 33: m = first(s, 3); break;                                // crs
                    case 90: m = k < 3 ? first(s, 2) : 1u << swz(s, 3); break;      // dp2add
                    case 6: case 7: case 14: case 15: case 78: case 79: case 37: case 32:
                        m = 1u << swz(s, 3); break;                                 // scalar (replicate)
                    case 16: m = (1u << swz(s, 0)) | (1u << swz(s, 1)) | (1u << swz(s, 3)); break;  // lit
                    case 17: m = k == 1 ? (1u << swz(s, 1)) | (1u << swz(s, 2)) : (1u << swz(s, 1)) | (1u << swz(s, 3)); break;  // dst
                    case 20: case 21: case 22: case 23: case 24: {                  // m4x4 .. m3x2
                        const unsigned cols = code <= 21 ? 4 : 3;
                        const unsigned rows = code == 20 ? 4 : code == 21 ? 3 : code == 22 ? 4 : code == 23 ? 3 : 2;
                        if (k == 1) m = first(s, cols);
                        else {
                            for (unsigned row = 1; row < rows; ++row) if (mask & (1u << row))
                                reads.push_back({s + row, first(s, cols)});
                            m = (mask & 1) ? first(s, cols) : 0;
                        }
                        break;
                    }
                    case 66: case 93: case 95:                                      // texld, texldd, texldl
                        if (major >= 2 && k == 1) {
                            const unsigned ctrl = (op >> 16) & 0xff;
                            unsigned dim = 2;
                            if (len > 2 && regType(p[2]) == 10) dim = samplerDim[p[2] & 15];
                            unsigned count = dim == 3 || dim == 4 ? 3 : 2;
                            if (ctrl || code == 95) count = 4;                      // texldp/texldb, texldl
                            m = first(s, count);
                        } else m = major >= 2 ? first(s, 4) : 0;
                        break;
                    default: m = perComponent(s, mask); break;
                }
                if (m) reads.push_back({s, m});
            }
        }
        for (const auto& rd : reads) {
            if (regType(rd.first) != 0) continue;
            const unsigned r = rd.first & 0x7ff;
            for (unsigned c = 0; c < 4; ++c) if (rd.second & (1u << c)) {
                ++res.reads;
                if (!(written[r] & (1u << c)) && !res.uninitialized++)
                    res.first = "instruction " + std::to_string(index) + ": opcode " + std::to_string(code) + " reads r" + std::to_string(r) + "." + "xyzw"[c];
            }
        }
        if (code != 65 && code != 87 && regType(dst) == 0) {
            const unsigned r = dst & 0x7ff;
            written[r] |= mask;
        }
    }
    res.malformed = true;  // no end token
    return res;
}

} // namespace nativefx
