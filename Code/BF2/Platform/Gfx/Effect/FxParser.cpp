// [CHOSEN, not recovered] D3DX9 effect source reader for the macOS port. See FxParser.h.
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "FxParser.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace bf2fx {
namespace src {

std::string lower(std::string s) {
    for(auto& c:s) c=char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool iequals(const std::string& a,const std::string& b) {
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();++i)
        if(std::tolower(static_cast<unsigned char>(a[i]))!=std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}
bool Variable::has(const char* m) const {
    return std::find(modifiers.begin(),modifiers.end(),std::string(m))!=modifiers.end();
}
bool Param::uniform() const {
    return std::find(modifiers.begin(),modifiers.end(),std::string("uniform"))!=modifiers.end();
}
bool Param::output() const {
    for(const auto& m:modifiers) if(m=="out" || m=="inout") return true;
    return false;
}
const StructDef* Effect::findStruct(const std::string& name) const {
    for(const auto& s:structs) if(s.name==name) return &s;
    return nullptr;
}
const Function* Effect::findFunction(const std::string& name) const {
    const Function* found=nullptr;
    for(const auto& f:functions) if(f.name==name && (f.hasBody || !found)) found=&f;
    return found;
}

namespace {

struct Token {
    enum Kind { End, Ident, Number, String, Punct, AsmBody } kind = End;
    std::string text;
    size_t begin = 0, end = 0;
    int line = 0;
};

struct ParseError : std::runtime_error {
    size_t offset; int line;
    ParseError(const std::string& m,size_t o,int l):std::runtime_error(m),offset(o),line(l) {}
};

std::vector<Token> lex(const std::string& s) {
    std::vector<Token> out;
    size_t i=0,n=s.size();int line=1;bool lineStart=true;
    auto push=[&](Token::Kind k,size_t b,size_t e) {Token t;t.kind=k;t.text=s.substr(b,e-b);t.begin=b;t.end=e;t.line=line;out.push_back(t);};
    while(i<n) {
        char c=s[i];
        if(c=='\n') {++line;++i;lineStart=true;continue;}
        if(std::isspace(static_cast<unsigned char>(c))) {++i;continue;}
        if(c=='#' && lineStart) { // #line / #pragma left by the preprocessor.
            while(i<n && s[i]!='\n') ++i;
            continue;
        }
        lineStart=false;
        if(c=='/' && i+1<n && s[i+1]=='/') {while(i<n && s[i]!='\n') ++i;continue;}
        if(c=='/' && i+1<n && s[i+1]=='*') {
            i+=2;while(i+1<n && !(s[i]=='*' && s[i+1]=='/')) {if(s[i]=='\n') ++line;++i;}
            i=std::min(n,i+2);continue;
        }
        size_t b=i;
        if(std::isalpha(static_cast<unsigned char>(c)) || c=='_') {
            while(i<n && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i]=='_')) ++i;
            push(Token::Ident,b,i);
            if(iequals(out.back().text,"asm")) {
                size_t j=i;int l=line;
                while(j<n && std::isspace(static_cast<unsigned char>(s[j]))) {if(s[j]=='\n') ++l;++j;}
                if(j<n && s[j]=='{') {
                    size_t k=j+1;int depth=1;
                    while(k<n && depth) {
                        if(s[k]=='/' && k+1<n && s[k+1]=='/') {while(k<n && s[k]!='\n') ++k;continue;}
                        if(s[k]=='{') ++depth; else if(s[k]=='}') --depth;
                        if(s[k]=='\n') ++l;
                        ++k;
                    }
                    if(depth) throw ParseError("Unterminated asm block",b,line);
                    Token t;t.kind=Token::AsmBody;t.text=s.substr(j+1,k-1-(j+1));t.begin=b;t.end=k;t.line=line;
                    out.back()=t;line=l;i=k;
                }
            }
            continue;
        }
        if(std::isdigit(static_cast<unsigned char>(c)) || (c=='.' && i+1<n && std::isdigit(static_cast<unsigned char>(s[i+1])))) {
            if(c=='0' && i+1<n && (s[i+1]=='x' || s[i+1]=='X')) {
                i+=2;while(i<n && std::isxdigit(static_cast<unsigned char>(s[i]))) ++i;
            } else {
                while(i<n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;
                if(i<n && s[i]=='.') {++i;while(i<n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;}
                if(i<n && (s[i]=='e' || s[i]=='E')) {
                    size_t j=i+1;if(j<n && (s[j]=='+' || s[j]=='-')) ++j;
                    if(j<n && std::isdigit(static_cast<unsigned char>(s[j]))) {i=j;while(i<n && std::isdigit(static_cast<unsigned char>(s[i]))) ++i;}
                }
            }
            while(i<n && std::strchr("fFhHlLuU",s[i])) ++i;
            push(Token::Number,b,i);continue;
        }
        if(c=='"') {
            ++i;while(i<n && s[i]!='"') {if(s[i]=='\\') ++i;if(i<n && s[i]=='\n') ++line;++i;}
            if(i>=n) throw ParseError("Unterminated string",b,line);
            ++i;push(Token::String,b,i);continue;
        }
        static const char* two[]={"<=",">=","==","!=","&&","||","++","--","+=","-=","*=","/=","::",nullptr};
        bool done=false;
        for(int k=0;two[k];++k) if(s.compare(i,2,two[k])==0) {i+=2;push(Token::Punct,b,i);done=true;break;}
        if(done) continue;
        ++i;push(Token::Punct,b,i);
    }
    Token e;e.kind=Token::End;e.begin=e.end=n;e.line=line;out.push_back(e);
    return out;
}

bool scalarName(const std::string& s,Base& base,uint32_t& rows,uint32_t& cols,bool& vector,bool& matrix) {
    static const struct {const char* name;Base base;} names[]={{"bool",Base::Bool},{"int",Base::Int},{"uint",Base::Uint},
        {"dword",Base::Uint},{"half",Base::Half},{"float",Base::Float},{"double",Base::Double}};
    for(const auto& nm:names) {
        size_t len=std::strlen(nm.name);
        if(s.compare(0,len,nm.name)!=0) continue;
        std::string rest=s.substr(len);
        base=nm.base;rows=1;cols=1;vector=false;matrix=false;
        if(rest.empty()) return true;
        if(rest.size()==1 && rest[0]>='1' && rest[0]<='4') {cols=uint32_t(rest[0]-'0');vector=true;return true;}
        if(rest.size()==3 && rest[0]>='1' && rest[0]<='4' && rest[1]=='x' && rest[2]>='1' && rest[2]<='4') {
            rows=uint32_t(rest[0]-'0');cols=uint32_t(rest[2]-'0');matrix=true;return true;
        }
        return false;
    }
    return false;
}

bool objectName(const std::string& s,Base& base) {
    static const struct {const char* name;Base base;} names[]={{"string",Base::String},{"texture",Base::Texture},
        {"texture1d",Base::Texture1D},{"texture2d",Base::Texture2D},{"texture3d",Base::Texture3D},
        {"texturecube",Base::TextureCube},{"sampler",Base::Sampler},{"sampler1d",Base::Sampler1D},
        {"sampler2d",Base::Sampler2D},{"sampler3d",Base::Sampler3D},{"samplercube",Base::SamplerCube},
        {"vertexshader",Base::VertexShader},{"pixelshader",Base::PixelShader}};
    std::string l=lower(s);
    for(const auto& nm:names) if(l==nm.name) {base=nm.base;return true;}
    return false;
}

bool isModifier(const std::string& s) {
    static const char* mods[]={"static","const","uniform","extern","shared","volatile","row_major","column_major",
        "inline","precise","nointerpolation","linear","centroid","noperspective","in","out","inout",nullptr};
    for(int i=0;mods[i];++i) if(s==mods[i]) return true;
    return false;
}

class Parser {
public:
    Parser(const std::string& text,Effect& e):toks_(lex(text)),fx_(e) {}
    void run() {
        while(!at(Token::End)) topLevel();
    }
private:
    std::vector<Token> toks_;
    size_t p_=0;
    Effect& fx_;

    const Token& cur() const { return toks_[p_]; }
    const Token& peek(size_t k=1) const { return toks_[std::min(p_+k,toks_.size()-1)]; }
    bool at(Token::Kind k) const { return cur().kind==k; }
    bool is(const char* punct) const { return cur().kind==Token::Punct && cur().text==punct; }
    bool isIdent(const char* name) const { return cur().kind==Token::Ident && cur().text==name; }
    bool isIdentI(const char* name) const { return cur().kind==Token::Ident && iequals(cur().text,name); }
    [[noreturn]] void fail(const std::string& m) const {
        throw ParseError(m+" near '"+cur().text+"'",cur().begin,cur().line);
    }
    void expect(const char* punct) { if(!is(punct)) fail(std::string("Expected '")+punct+"'"); ++p_; }
    bool accept(const char* punct) { if(is(punct)) {++p_;return true;} return false; }
    std::string ident() { if(!at(Token::Ident)) fail("Expected identifier"); return toks_[p_++].text; }

    // Type recognition at the current token, without consuming.
    bool typeAhead() const {
        if(!at(Token::Ident)) return false;
        const std::string& s=cur().text;Base b;uint32_t r,c;bool v,m;
        if(scalarName(s,b,r,c,v,m) || objectName(s,b) || s=="vector" || s=="matrix" || s=="void" || s=="struct") return true;
        if(fx_.typedefs.count(s) || fx_.findStruct(s)) return true;
        return false;
    }
    TypeSpec type() {
        TypeSpec t;std::string s=ident();
        if(s=="struct") s=ident();
        Base b;uint32_t r,c;bool v,m;
        if(s=="void") {t.base=Base::Void;return t;}
        if(scalarName(s,b,r,c,v,m)) {t.base=b;t.rows=r;t.cols=c;t.vector=v;t.matrix=m;return t;}
        if(objectName(s,b)) {t.base=b;
            if((b==Base::Texture || b==Base::Sampler) && is("<")) { // texture<float4> style: ignore element type.
                int depth=0;do {if(is("<")) ++depth; else if(is(">")) --depth;++p_;} while(depth && !at(Token::End));
            }
            return t;}
        if(s=="vector") {
            t.base=Base::Float;t.cols=4;t.vector=true;
            if(accept("<")) {TypeSpec e=type();t.base=e.base;expect(",");t.cols=uint32_t(intLiteral());expect(">");}
            return t;
        }
        if(s=="matrix") {
            t.base=Base::Float;t.rows=4;t.cols=4;t.matrix=true;
            if(accept("<")) {TypeSpec e=type();t.base=e.base;expect(",");t.rows=uint32_t(intLiteral());expect(",");t.cols=uint32_t(intLiteral());expect(">");}
            return t;
        }
        auto td=fx_.typedefs.find(s);
        if(td!=fx_.typedefs.end()) return td->second;
        if(fx_.findStruct(s)) {t.base=Base::Struct;t.structName=s;return t;}
        --p_;fail("Unknown type");
    }
    long intLiteral() {
        if(!at(Token::Number)) fail("Expected integer");
        return std::strtol(toks_[p_++].text.c_str(),nullptr,0);
    }
    std::vector<std::string> modifiers() {
        std::vector<std::string> m;
        while(at(Token::Ident) && isModifier(cur().text)) m.push_back(toks_[p_++].text);
        return m;
    }
    static void applyMajor(TypeSpec& t,const std::vector<std::string>& m) {
        for(const auto& x:m) {if(x=="row_major") t.rowMajor=true;if(x=="column_major") t.columnMajor=true;}
    }

    void topLevel() {
        if(accept(";")) return;
        if(isIdent("struct") && ((peek().kind==Token::Ident && peek(2).kind==Token::Punct && peek(2).text=="{") ||
                                 (peek().kind==Token::Punct && peek().text=="{"))) {structDef();return;}
        if(isIdent("typedef")) {typedefDecl();return;}
        if(isIdentI("technique") || isIdentI("technique9")) {technique();return;}
        size_t begin=cur().begin;int line=cur().line;
        auto mods=modifiers();
        TypeSpec t=type();applyMajor(t,mods);
        // Remaining modifiers after the type (rare): ignored.
        size_t nameBegin=cur().begin;std::string name=ident();
        if(is("(")) {function(mods,t,name,begin,line);fx_.functions.back().nameBegin=nameBegin;return;}
        declarators(mods,t,name,begin,line);
    }

    void structDef() {
        StructDef d;d.begin=cur().begin;++p_;
        if(at(Token::Ident)) d.name=ident(); else d.name="<anonymous@"+std::to_string(d.begin)+">";
        // Register before parsing members so that self-references are diagnosed as types.
        expect("{");
        while(!accept("}")) {
            StructMember m;m.modifiers=modifiers();m.type=type();applyMajor(m.type,m.modifiers);
            for(;;) {
                StructMember mm=m;mm.name=ident();
                if(accept("[")) {mm.arraySize=expr();expect("]");}
                if(accept(":")) mm.semantic=ident();
                d.members.push_back(mm);
                if(!accept(",")) break;
            }
            expect(";");
        }
        d.end=cur().end;fx_.structs.push_back(d);
        if(at(Token::Ident)) { // struct S {...} var;
            TypeSpec t;t.base=Base::Struct;t.structName=d.name;std::string name=ident();
            declarators({},t,name,d.begin,0);return;
        }
        expect(";");
    }

    void typedefDecl() {
        ++p_;auto mods=modifiers();TypeSpec t=type();applyMajor(t,mods);
        for(;;) {std::string n=ident();fx_.typedefs[n]=t;if(!accept(",")) break;}
        expect(";");
    }

    void annotations(std::vector<Variable>& out) {
        expect("<");
        while(!accept(">")) {
            Variable a;a.begin=cur().begin;a.line=cur().line;a.modifiers=modifiers();a.type=type();a.name=ident();
            if(accept("[")) {if(!is("]")) a.arraySize=expr(); else a.unsizedArray=true;expect("]");}
            expect("=");
            if(is("{")) a.init=list(true); else a.init=expr(true);
            expect(";");a.end=cur().begin;out.push_back(a);
        }
    }

    void declarators(const std::vector<std::string>& mods,const TypeSpec& t,std::string name,size_t begin,int line) {
        size_t first=fx_.globals.size();
        for(;;) {
            Variable v;v.modifiers=mods;v.type=t;v.name=name;v.begin=begin;v.line=line;
            if(accept("[")) {if(!is("]")) v.arraySize=expr(); else v.unsizedArray=true;expect("]");}
            while(accept(":")) {
                if(isIdent("register")) {
                    Reservation r;r.begin=cur().begin;++p_;expect("(");
                    std::vector<std::string> args;std::string arg;
                    while(!is(")")) {if(accept(",")) {args.push_back(arg);arg.clear();continue;} arg+=toks_[p_++].text;}
                    args.push_back(arg);r.end=cur().end;expect(")");
                    if(args.size()==2) {r.profile=args[0];r.reg=args[1];fx_.reservations.push_back(r);}
                }
                else v.semantic=ident();
            }
            if(is("<")) {
                size_t ab=cur().begin;annotations(v.annotations);
                fx_.effectOnly.push_back({ab,toks_[p_-1].end});
            }
            if(accept("=")) {
                size_t ib=toks_[p_-1].begin;
                if(isIdent("sampler_state") || isIdentI("sampler_state")) {
                    v.init=samplerState();fx_.effectOnly.push_back({ib,toks_[p_-1].end});
                } else if(is("{")) v.init=list(false);
                else v.init=expr();
            }
            fx_.globals.push_back(v);
            if(!accept(",")) break;
            name=ident();
        }
        size_t end=cur().end;expect(";");
        for(size_t i=first;i<fx_.globals.size();++i) fx_.globals[i].end=end;
        // Effect-only declarations: shaders (compiled separately) and strings.
        Base b=t.base;
        if(b==Base::VertexShader || b==Base::PixelShader || b==Base::String) fx_.effectOnly.push_back({begin,end});
    }

    void function(const std::vector<std::string>& mods,const TypeSpec& ret,const std::string& name,size_t begin,int line) {
        Function f;f.modifiers=mods;f.returns=ret;f.name=name;f.begin=begin;f.line=line;
        expect("(");
        if(isIdent("void") && peek().kind==Token::Punct && peek().text==")") ++p_;
        while(!accept(")")) {
            Param prm;prm.begin=cur().begin;prm.modifiers=modifiers();prm.type=type();applyMajor(prm.type,prm.modifiers);
            auto more=modifiers();prm.modifiers.insert(prm.modifiers.end(),more.begin(),more.end());
            prm.name=ident();
            if(accept("[")) {prm.arraySize=expr();expect("]");}
            while(accept(":")) {
                if(isIdent("register")) {++p_;expect("(");while(!accept(")")) ++p_;}
                else prm.semantic=ident();
            }
            prm.end=toks_[p_-1].end;
            if(accept("=")) prm.defaultValue=expr();
            f.params.push_back(prm);
            if(!accept(",")) {expect(")");break;}
        }
        if(accept(":")) f.semantic=ident();
        if(accept(";")) {f.end=toks_[p_-1].end;fx_.functions.push_back(f);return;}
        if(!is("{")) fail("Expected function body");
        f.hasBody=true;f.bodyBegin=cur().begin;
        int depth=0;
        do {if(is("{")) ++depth; else if(is("}")) --depth;if(at(Token::End)) fail("Unterminated function body");++p_;} while(depth);
        f.bodyEnd=f.end=toks_[p_-1].end;
        fx_.functions.push_back(f);
    }

    void technique() {
        Technique t;t.begin=cur().begin;t.line=cur().line;++p_;
        if(at(Token::Ident)) t.name=ident();
        if(is("<")) annotations(t.annotations);
        expect("{");
        while(!accept("}")) {
            if(!isIdentI("pass")) fail("Expected pass");
            Pass ps;ps.line=cur().line;++p_;
            if(at(Token::Ident)) ps.name=ident();
            if(is("<")) annotations(ps.annotations);
            expect("{");
            while(!accept("}")) ps.states.push_back(stateAssign());
            t.passes.push_back(ps);
        }
        t.end=toks_[p_-1].end;accept(";");
        fx_.effectOnly.push_back({t.begin,t.end});
        fx_.techniques.push_back(t);
    }

    StateAssign stateAssign() {
        StateAssign s;s.begin=cur().begin;s.line=cur().line;s.name=ident();
        if(accept("[")) {s.index=expr();expect("]");}
        expect("=");
        s.value=stateValue();
        s.end=cur().end;expect(";");
        return s;
    }
    ExprPtr stateValue() {
        if(isIdentI("sampler_state")) return samplerState();
        if(is("{")) return list(false);
        return expr();
    }
    ExprPtr samplerState() {
        auto e=std::make_shared<Expr>();e->kind=Expr::SamplerState;e->begin=cur().begin;++p_;
        expect("{");
        while(!accept("}")) e->states.push_back(stateAssign());
        e->end=toks_[p_-1].end;return e;
    }
    ExprPtr list(bool annotation) {
        auto e=std::make_shared<Expr>();e->kind=Expr::List;e->begin=cur().begin;expect("{");
        while(!accept("}")) {
            if(is("{")) e->args.push_back(list(annotation)); else e->args.push_back(expr(annotation));
            if(!accept(",")) {expect("}");break;}
        }
        e->end=toks_[p_-1].end;return e;
    }

    // Expression grammar (C precedence). noGreater stops at '>' (annotation blocks).
    bool noGreater_=false;
    ExprPtr expr(bool noGreater=false) {
        bool saved=noGreater_;noGreater_=noGreater;
        auto e=ternary();noGreater_=saved;return e;
    }
    ExprPtr make(Expr::Kind k,size_t begin) { auto e=std::make_shared<Expr>();e->kind=k;e->begin=begin;return e; }
    ExprPtr ternary() {
        auto c=binary(0);
        if(accept("?")) {
            auto t=make(Expr::Ternary,c->begin);t->args.push_back(c);t->args.push_back(ternary());expect(":");
            t->args.push_back(ternary());t->end=toks_[p_-1].end;return t;
        }
        return c;
    }
    int precedence(const Token& t) const {
        if(t.kind!=Token::Punct) return -1;
        const std::string& s=t.text;
        if(s=="||") return 1; if(s=="&&") return 2; if(s=="|") return 3; if(s=="^") return 4; if(s=="&") return 5;
        if(s=="==" || s=="!=") return 6;
        if(s=="<" || s=="<=" || s==">=") return 7;
        if(s==">") return noGreater_?-1:7;
        if(s=="+" || s=="-") return 9; if(s=="*" || s=="/" || s=="%") return 10;
        return -1;
    }
    ExprPtr binary(int minPrec) {
        auto lhs=unary();
        for(;;) {
            int prec=precedence(cur());
            if(prec<0 || prec<minPrec) break;
            std::string op=toks_[p_++].text;
            auto rhs=binary(prec+1);
            auto b=make(Expr::Binary,lhs->begin);b->text=op;b->args={lhs,rhs};b->end=rhs->end;lhs=b;
        }
        return lhs;
    }
    ExprPtr unary() {
        size_t b=cur().begin;
        if(is("-") || is("+") || is("!") || is("~")) {
            std::string op=toks_[p_++].text;auto u=make(Expr::Unary,b);u->text=op;u->args.push_back(unary());u->end=u->args[0]->end;return u;
        }
        if(is("(") ) {
            // Cast: '(' type ')' unary.
            size_t save=p_;++p_;
            if(typeAhead()) {
                try {TypeSpec t=type();
                    if(accept(")")) {auto c=make(Expr::Cast,b);c->type=t;c->args.push_back(unary());c->end=c->args[0]->end;return c;}
                } catch(const ParseError&) {}
            }
            p_=save;
        }
        return postfix(primary());
    }
    ExprPtr postfix(ExprPtr e) {
        for(;;) {
            if(accept("[")) {auto i=make(Expr::Index,e->begin);i->args={e,expr()};expect("]");i->end=toks_[p_-1].end;e=i;continue;}
            if(is(".") && peek().kind==Token::Ident) {++p_;auto m=make(Expr::Member,e->begin);m->text=ident();m->args={e};m->end=toks_[p_-1].end;e=m;continue;}
            break;
        }
        return e;
    }
    ExprPtr primary() {
        const Token& t=cur();size_t b=t.begin;
        if(t.kind==Token::Number) {
            auto e=make(Expr::Number,b);std::string s=t.text;
            bool hex=s.size()>1 && s[0]=='0' && (s[1]=='x' || s[1]=='X');
            while(!s.empty() && !hex && std::strchr("fFhHlLuU",s.back())) {if(s.back()=='f' || s.back()=='F' || s.back()=='h' || s.back()=='H') e->integer=false;s.pop_back();}
            if(hex) {e->number=double(std::strtoull(s.c_str(),nullptr,16));e->integer=true;}
            else {e->number=std::strtod(s.c_str(),nullptr);e->integer=t.text.find_first_of(".eEfFhH")==std::string::npos;}
            ++p_;e->end=t.end;return e;
        }
        if(t.kind==Token::String) {
            auto e=make(Expr::String,b);
            while(at(Token::String)) {
                const std::string& s=cur().text;
                for(size_t i=1;i+1<s.size();++i) {
                    char c=s[i];
                    if(c=='\\' && i+2<s.size()) {
                        char n=s[++i];
                        c=n=='n'?'\n':n=='t'?'\t':n=='r'?'\r':n=='0'?'\0':n;
                    }
                    e->text+=c;
                }
                ++p_;
            }
            e->end=toks_[p_-1].end;return e;
        }
        if(t.kind==Token::AsmBody) {auto e=make(Expr::Asm,b);e->text=t.text;++p_;e->end=t.end;return e;}
        if(is("(")) {
            ++p_;auto e=expr();
            // Comma expressions do not occur in BF2's effects.
            expect(")");e->paren=true;return e;
        }
        if(is("{")) return list(false);
        if(is("<")) {
            ++p_;auto e=make(Expr::Reference,b);e->text=ident();
            if(accept("[")) {e->args.push_back(expr(true));expect("]");}
            expect(">");e->end=toks_[p_-1].end;return e;
        }
        if(t.kind==Token::Ident) {
            if(iequals(t.text,"compile")) {
                ++p_;auto e=make(Expr::Compile,b);e->text=ident();
                auto fn=make(Expr::Ident,cur().begin);fn->text=ident();e->args.push_back(fn);
                expect("(");
                while(!accept(")")) {e->args.push_back(expr());if(!accept(",")) {expect(")");break;}}
                e->end=toks_[p_-1].end;return e;
            }
            if(iequals(t.text,"true") || iequals(t.text,"false")) {
                auto e=make(Expr::Bool,b);e->number=iequals(t.text,"true")?1:0;e->integer=true;e->text=t.text;++p_;e->end=t.end;return e;
            }
            if(iequals(t.text,"null")) {auto e=make(Expr::Null,b);e->text=t.text;++p_;e->end=t.end;return e;}
            if(iequals(t.text,"sampler_state")) return samplerState();
            Base base;uint32_t r,c;bool v,m;
            if((scalarName(t.text,base,r,c,v,m) || t.text=="vector" || t.text=="matrix" || fx_.typedefs.count(t.text)) &&
               peek().kind==Token::Punct && (peek().text=="(" || peek().text=="<")) {
                auto e=make(Expr::Construct,b);e->type=type();expect("(");
                while(!accept(")")) {e->args.push_back(expr());if(!accept(",")) {expect(")");break;}}
                e->end=toks_[p_-1].end;return e;
            }
            ++p_;
            if(is("(")) {
                auto e=make(Expr::Call,b);e->text=t.text;++p_;
                while(!accept(")")) {e->args.push_back(expr());if(!accept(",")) {expect(")");break;}}
                e->end=toks_[p_-1].end;return e;
            }
            auto e=make(Expr::Ident,b);e->text=t.text;e->end=t.end;return e;
        }
        fail("Unexpected token");
    }
};

} // namespace

bool parse(const std::string& text,Effect& effect,Error& error) {
    try {
        Parser p(text,effect);p.run();
    } catch(const ParseError& e) {
        error.message=e.what();error.offset=e.offset;error.line=e.line;return false;
    }
    return true;
}

std::string shaderText(const std::string& text,const Effect& effect) {
    std::string out=text;
    for(const auto& r:effect.effectOnly)
        for(size_t i=r.begin;i<r.end && i<out.size();++i) if(out[i]!='\n') out[i]=' ';
    return out;
}

} // namespace src
} // namespace bf2fx
#endif
