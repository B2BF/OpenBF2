// [CHOSEN, not recovered] D3D9 shader assembler for BF2's asm blocks. See D3DAsm.h.
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "D3DAsm.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

namespace bf2fx {
namespace {

enum RegType : uint32_t { Temp = 0, Input = 1, Const = 2, Texture = 3, Addr = 3, RastOut = 4, AttrOut = 5, TexCrdOut = 6 };

struct Opcode { const char* name; uint32_t code; int dst, src; int minor; /* -1 any, 13: ps<=1.3, 14: ps 1.4, 99: vs */ };
// Public D3D9 opcode numbers.
const Opcode kOps[] = {
    {"nop",0,0,0,-1},{"mov",1,1,1,-1},{"add",2,1,2,-1},{"sub",3,1,2,-1},{"mad",4,1,3,-1},{"mul",5,1,2,-1},
    {"rcp",6,1,1,99},{"rsq",7,1,1,99},{"dp3",8,1,2,-1},{"dp4",9,1,2,-1},{"min",10,1,2,99},{"max",11,1,2,99},
    {"slt",12,1,2,99},{"sge",13,1,2,99},{"exp",14,1,1,99},{"log",15,1,1,99},{"lit",16,1,1,99},{"dst",17,1,2,99},
    {"lrp",18,1,3,-1},{"frc",19,1,1,99},{"m4x4",20,1,2,99},{"m4x3",21,1,2,99},{"m3x4",22,1,2,99},{"m3x3",23,1,2,99},
    {"m3x2",24,1,2,99},{"expp",78,1,1,99},{"logp",79,1,1,99},{"cnd",80,1,3,-1},{"cmp",88,1,3,-1},{"bem",89,1,2,14},
    {"texcoord",64,1,0,13},{"texkill",65,1,0,-1},{"tex",66,1,0,13},{"texbem",67,1,1,13},{"texbeml",68,1,1,13},
    {"texreg2ar",69,1,1,13},{"texreg2gb",70,1,1,13},{"texm3x2pad",71,1,1,13},{"texm3x2tex",72,1,1,13},
    {"texm3x3pad",73,1,1,13},{"texm3x3tex",74,1,1,13},{"texm3x3spec",76,1,2,13},{"texm3x3vspec",77,1,1,13},
    {"texreg2rgb",82,1,1,13},{"texdp3tex",83,1,1,13},{"texm3x2depth",84,1,1,13},{"texdp3",85,1,1,13},
    {"texm3x3",86,1,1,13},{"texdepth",87,1,0,14},{"texld",66,1,1,14},{"texcrd",64,1,1,14},{"phase",0xFFFD,0,0,14},
};

struct Reg { uint32_t type = 0, index = 0; bool ok = false; };

uint32_t comp(char c) {
    switch(std::tolower(static_cast<unsigned char>(c))) {
        case 'x': case 'r': return 0; case 'y': case 'g': return 1;
        case 'z': case 'b': return 2; case 'w': case 'a': return 3;
    }
    return 99;
}

class Assembler {
public:
    explicit Assembler(Bytes& out):out_(out) {}
    bool run(const std::string& text,std::string& error) {
        std::istringstream lines(text);std::string line;bool version=false;int n=0;
        while(std::getline(lines,line)) {
            ++n;
            auto semi=line.find(';');if(semi!=std::string::npos) line.resize(semi);
            std::string trimmed=trim(line);if(trimmed.empty()) continue;
            if(!version) {
                if(!parseVersion(trimmed)) return fail(error,n,"Expected a ps_1_x/vs_1_1 version, got '"+trimmed+"'");
                version=true;continue;
            }
            if(!instruction(trimmed)) return fail(error,n,err_+" in '"+trimmed+"'");
        }
        if(!version) return fail(error,0,"Empty asm block");
        put(0x0000FFFF);
        return true;
    }
private:
    Bytes& out_;
    bool pixel_=true;uint32_t major_=1,minor_=1;
    std::string err_;
    void put(uint32_t w) { for(int i=0;i<4;++i) out_.push_back(uint8_t(w>>(i*8))); }
    static std::string trim(const std::string& s) {
        size_t b=s.find_first_not_of(" \t\r"),e=s.find_last_not_of(" \t\r");
        return b==std::string::npos?"":s.substr(b,e-b+1);
    }
    static std::string strip(const std::string& s) {std::string o;for(char c:s) if(!std::isspace(static_cast<unsigned char>(c))) o+=c;return o;}
    bool fail(std::string& error,int line,const std::string& m) {error="asm line "+std::to_string(line)+": "+m;return false;}
    bool bad(const std::string& m) {err_=m;return false;}
    bool parseVersion(const std::string& line) {
        std::string s;for(char c:strip(line)) s+=char(std::tolower(static_cast<unsigned char>(c)));
        for(auto& c:s) if(c=='.') c='_';
        if(s.size()!=6 || s[2]!='_' || s[4]!='_' || (s.compare(0,2,"ps") && s.compare(0,2,"vs"))) return false;
        pixel_=s[0]=='p';major_=uint32_t(s[3]-'0');minor_=uint32_t(s[5]-'0');
        if(pixel_ ? (major_!=1 || minor_<1 || minor_>4) : (major_!=1 || minor_!=1)) return false;
        put((pixel_?0xFFFF0000u:0xFFFE0000u)|(major_<<8)|minor_);
        return true;
    }
    bool ps14() const { return pixel_ && minor_==4; }
    // Register limits of the ps_1_x / vs_1_1 models.
    bool checkReg(const Reg& r,bool dst,bool definition=false) {
        uint32_t limit=0;
        if(pixel_) {
            switch(r.type) {
                case Temp: limit=ps14()?6:2;break;
                case Input: limit=2;break;
                case Const: limit=8;break;
                case Texture: limit=ps14()?6:4;break;
                default: return bad("register type not available in pixel shaders");
            }
            if(dst && r.type==Input) return bad("v# cannot be written");
            if(dst && r.type==Const && !definition) return bad("c# cannot be written");
        } else {
            switch(r.type) {
                case Temp: limit=12;break; case Input: limit=16;break; case Const: limit=96;break; case Addr: limit=1;break;
                case RastOut: limit=3;break; case AttrOut: limit=2;break; case TexCrdOut: limit=8;break;
                default: return bad("register type");
            }
        }
        if(r.index>=limit) return bad("register index out of range");
        return true;
    }
    Reg reg(std::string s) {
        Reg r;std::string l;for(char c:s) l+=char(std::tolower(static_cast<unsigned char>(c)));
        auto num=[&](size_t from,uint32_t& v)->bool {
            if(from>=l.size()) return false;
            size_t i=from;bool br=l[i]=='[';if(br) ++i;
            if(i>=l.size() || !std::isdigit(static_cast<unsigned char>(l[i]))) return false;
            v=uint32_t(std::strtoul(l.c_str()+i,nullptr,10));
            while(i<l.size() && std::isdigit(static_cast<unsigned char>(l[i]))) ++i;
            if(br) {if(i>=l.size() || l[i]!=']') return false;++i;}
            return i==l.size();
        };
        if(!pixel_) {
            if(l=="opos") {r.type=RastOut;r.index=0;r.ok=true;return r;}
            if(l=="ofog") {r.type=RastOut;r.index=1;r.ok=true;return r;}
            if(l=="opts") {r.type=RastOut;r.index=2;r.ok=true;return r;}
            if(l.rfind("od",0)==0) {r.type=AttrOut;r.ok=num(2,r.index);return r;}
            if(l.rfind("ot",0)==0) {r.type=TexCrdOut;r.ok=num(2,r.index);return r;}
            if(l.rfind("a",0)==0) {r.type=Addr;r.ok=num(1,r.index);return r;}
        }
        switch(l.empty()?0:l[0]) {
            case 'r': r.type=Temp;break; case 'v': r.type=Input;break; case 'c': r.type=Const;break;
            case 't': if(!pixel_) return r; r.type=Texture;break;
            default: return r;
        }
        r.ok=num(1,r.index);return r;
    }
    static uint32_t regBits(const Reg& r) { return ((r.type&7u)<<28)|(((r.type>>3)&3u)<<11)|(r.index&0x7FFu); }
    bool dst(std::string s,uint32_t mods,uint32_t shift,uint32_t& token,bool definition=false) {
        uint32_t mask=0xF;auto dot=s.find('.');
        if(dot!=std::string::npos) {
            mask=0;for(char c:s.substr(dot+1)) {uint32_t k=comp(c);if(k>3) return bad("bad write mask");mask|=1u<<k;}
            s.resize(dot);
        }
        Reg r=reg(s);if(!r.ok) return bad("bad destination register '"+s+"'");
        if(!checkReg(r,true,definition)) return false;
        token=0x80000000u|regBits(r)|(mask<<16)|(mods<<20)|(shift<<24);
        return true;
    }
    bool src(std::string s,uint32_t& token) {
        uint32_t mod=0;bool neg=false,comp1=false;
        if(!s.empty() && s[0]=='-') {neg=true;s.erase(0,1);}
        if(s.size()>2 && s[0]=='1' && s[1]=='-') {comp1=true;s.erase(0,2);}
        uint32_t swz=0xE4;auto dot=s.find('.');
        if(dot!=std::string::npos) {
            std::string sw=s.substr(dot+1);s.resize(dot);
            if(sw.empty() || sw.size()>4) return bad("bad swizzle");
            uint32_t c[4];for(size_t i=0;i<4;++i) {c[i]=comp(sw[std::min(i,sw.size()-1)]);if(c[i]>3) return bad("bad swizzle");}
            swz=c[0]|(c[1]<<2)|(c[2]<<4)|(c[3]<<6);
        }
        std::string suffix;auto us=s.find('_');
        if(us!=std::string::npos) {suffix=s.substr(us+1);s.resize(us);for(auto& ch:suffix) ch=char(std::tolower(static_cast<unsigned char>(ch)));}
        if(suffix.empty()) mod=neg?1:0;
        else if(suffix=="bias") mod=neg?3:2;
        else if(suffix=="bx2") mod=neg?5:4;
        else if(suffix=="x2") mod=neg?8:7;
        else if(suffix=="dz" && !neg) mod=9;
        else if(suffix=="dw" && !neg) mod=10;
        else return bad("bad source modifier _"+suffix);
        if(comp1) {if(mod) return bad("1- with another modifier");mod=6;}
        Reg r=reg(s);if(!r.ok) return bad("bad source register '"+s+"'");
        if(!checkReg(r,false)) return false;
        token=0x80000000u|regBits(r)|(swz<<16)|(mod<<24);
        return true;
    }
    static std::vector<std::string> split(const std::string& s) {
        std::vector<std::string> v;std::string cur;
        for(char c:s) {if(c==',') {v.push_back(cur);cur.clear();} else cur+=c;}
        if(!cur.empty() || !v.empty()) v.push_back(cur);
        return v;
    }
    bool instruction(std::string line) {
        bool coissue=false;
        if(line[0]=='+') {coissue=true;line=trim(line.substr(1));}
        size_t sp=0;while(sp<line.size() && !std::isspace(static_cast<unsigned char>(line[sp]))) ++sp;
        std::string op=line.substr(0,sp);for(auto& c:op) c=char(std::tolower(static_cast<unsigned char>(c)));
        std::string rest=strip(line.substr(sp));
        if(op=="def") return def(rest);
        if(op.rfind("dcl_",0)==0) return dcl(op.substr(4),rest);
        // Instruction modifiers: _sat, _x2/_x4/_x8, _d2/_d4/_d8 (opcode names have no '_').
        uint32_t mods=0,shift=0;
        auto us=op.find('_');std::string base=op.substr(0,us);
        while(us!=std::string::npos) {
            auto next=op.find('_',us+1);std::string m=op.substr(us+1,next==std::string::npos?std::string::npos:next-us-1);
            if(m=="sat") mods|=1; else if(m=="x2") shift=1; else if(m=="x4") shift=2; else if(m=="x8") shift=3;
            else if(m=="d2") shift=15; else if(m=="d4") shift=14; else if(m=="d8") shift=13;
            else return bad("unknown instruction modifier _"+m);
            us=next;
        }
        const Opcode* info=nullptr;
        for(const auto& o:kOps) if(base==o.name) {
            if(o.minor==-1 || (o.minor==99 && !pixel_) || (o.minor==13 && pixel_ && minor_<=3) || (o.minor==14 && ps14())) {info=&o;break;}
        }
        if(!info) return bad("instruction '"+base+"' is not available in this shader version");
        auto ops=split(rest);
        // ps_1_1..1_3 tex/texcoord name only a destination; ps_1_4 texld/texcrd also a source.
        if(int(ops.size())!=info->dst+info->src) return bad("wrong operand count");
        uint32_t token=info->code|(coissue?0x40000000u:0);
        if(info->code==0xFFFD) {put(0x0000FFFD);return true;}
        std::vector<uint32_t> params;
        if(info->dst) {uint32_t t;if(!dst(ops[0],mods,shift,t)) return false;params.push_back(t);}
        for(size_t i=size_t(info->dst);i<ops.size();++i) {uint32_t t;if(!src(ops[i],t)) return false;params.push_back(t);}
        put(token);for(auto p:params) put(p);
        return true;
    }
    bool def(const std::string& rest) {
        auto ops=split(rest);if(ops.size()!=5) return bad("def needs a register and four values");
        uint32_t t;if(!dst(ops[0],0,0,t,true)) return false;
        if(((t>>28)&7)!=Const) return bad("def needs a constant register");
        put(0x51);put(t);
        for(int i=1;i<5;++i) {
            char* end=nullptr;float f=std::strtof(ops[i].c_str(),&end);
            if(end==ops[i].c_str() || *end) return bad("bad def value");
            uint32_t w;std::memcpy(&w,&f,4);put(w);
        }
        return true;
    }
    bool dcl(const std::string& usage,const std::string& rest) {
        if(pixel_) return bad("dcl in ps_1_x");
        static const char* names[]={"position","blendweight","blendindices","normal","psize","texcoord","tangent",
            "binormal","tessfactor","positiont","color","fog","depth","sample"};
        std::string u=usage;uint32_t index=0;
        while(!u.empty() && std::isdigit(static_cast<unsigned char>(u.back()))) u.pop_back();
        if(u.size()<usage.size()) index=uint32_t(std::strtoul(usage.c_str()+u.size(),nullptr,10));
        uint32_t code=99;for(uint32_t i=0;i<14;++i) if(u==names[i]) code=i;
        if(code==99) return bad("unknown dcl usage "+usage);
        uint32_t t;if(!dst(rest,0,0,t)) return false;
        put(0x1F);put(0x80000000u|code|(index<<16));put(t);
        return true;
    }
};

} // namespace

bool assemble(const std::string& text,Bytes& out,std::string& error) {
    out.clear();Assembler a(out);
    if(!a.run(text,error)) {out.clear();return false;}
    return true;
}

} // namespace bf2fx
#endif
