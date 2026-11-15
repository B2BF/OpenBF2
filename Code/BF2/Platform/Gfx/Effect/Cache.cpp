// [CHOSEN, not recovered] Owner-requested macOS CPU effect cache.
#if defined(__APPLE__)
#include "Cache.h"
#include "Wire.h"
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <unordered_map>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace bf2fx {
namespace fs=std::filesystem;
std::string sha256(const Bytes& bytes) {
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(bytes.data(),CC_LONG(bytes.size()),digest);
    const char* hex="0123456789abcdef"; std::string out;
    for(auto b:digest) { out+=hex[b>>4]; out+=hex[b&15]; } return out;
}
Bytes readFile(const std::string& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f) throw std::runtime_error("Cannot read "+path);
    auto n=f.tellg(); if(n<0 || n>128*1024*1024) throw std::runtime_error("File size limit: "+path);
    Bytes b(static_cast<size_t>(n)); f.seekg(0); f.read(reinterpret_cast<char*>(b.data()),b.size());
    if(!f) throw std::runtime_error("Cannot read "+path); return b;
}
void writeFile(const std::string& path,const Bytes& bytes) {
    std::ofstream f(path,std::ios::binary|std::ios::trunc); f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    f.close(); if(!f) throw std::runtime_error("Cannot write "+path);
}
static void atomicWrite(const fs::path& path,const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::string pattern=path.string()+".tmp.XXXXXX"; std::vector<char> name(pattern.begin(),pattern.end()); name.push_back(0);
    int fd=mkstemp(name.data()); if(fd<0) throw std::runtime_error("Cannot create cache temporary");
    size_t n=0; bool ok=true;
    while(n<bytes.size()) { auto r=::write(fd,bytes.data()+n,bytes.size()-n); if(r<0 && errno==EINTR) continue;
        if(r<=0) {ok=false;break;} n+=size_t(r); }
    if(fsync(fd)) ok=false; if(::close(fd)) ok=false;
    if(ok && ::rename(name.data(),path.c_str())==0) return;
    ::unlink(name.data()); throw std::runtime_error("Cannot publish cache file");
}
static std::string stripComments(const Bytes& bytes) {
    std::string s(bytes.begin(),bytes.end()); bool line=false,block=false,quoted=false; char quote=0;
    for(size_t i=0;i<s.size();++i) {
        char c=s[i],next=i+1<s.size()?s[i+1]:0;
        if(line) { if(c=='\n') line=false; else s[i]=' '; }
        else if(block) { if(c=='*' && next=='/') {s[i]=s[i+1]=' ';++i;block=false;} else if(c!='\n') s[i]=' '; }
        else if(quoted) { if(c=='\\') ++i; else if(c==quote) quoted=false; }
        else if(c=='"' || c=='\'') {quoted=true;quote=c;}
        else if(c=='/' && (next=='/' || next=='*')) {line=next=='/';block=next=='*';s[i]=s[i+1]=' ';++i;}
        else if(c=='\\' && (next=='\n' || next=='\r')) {
            s[i]=s[i+1]=' ';++i; if(next=='\r' && i+1<s.size() && s[i+1]=='\n') s[++i]=' ';
        }
    } return s;
}
// The operand of an #include line as std::regex R"(^\s*#\s*include\s+(.+?)\s*$)" (ECMAScript:
// '.' does not match '\r') captures it; false when the line is no include directive. Hand-written
// because the regex cost about 1.7 ms per effect request (it ran on every line of the source and
// its includes, thousands of requests per level load).
static bool includeOperand(const std::string& line,std::string& operand) {
    auto ws=[](char c){return c==' '||c=='\t'||c=='\n'||c=='\v'||c=='\f'||c=='\r';};
    const size_t n=line.size(); size_t i=0;
    while(i<n && ws(line[i])) ++i;
    if(i==n || line[i]!='#') return false;
    ++i; while(i<n && ws(line[i])) ++i;
    if(line.compare(i,7,"include")!=0) return false;
    i+=7; const size_t spaces=i;
    while(i<n && ws(line[i])) ++i;
    if(i==spaces) return false;
    size_t end=n; while(end>i && ws(line[end-1])) --end;
    if(end==i) { // only whitespace follows: the group takes back one of it that is not '\r'
        for(size_t j=n;j-->spaces+1;) if(line[j]!='\r' && line[j]!='\n') { operand=line.substr(j,1); return true; }
        return false;
    }
    operand=line.substr(i,end-i);
    return operand.find('\r')==std::string::npos && operand.find('\n')==std::string::npos;
}
// One #include directive of a file: its kind and name, or the error capture() reports at that line
// (the last entry then).
struct Directive { uint32_t kind=0; std::string name,error; };
// The #include directives of a file, by the SHA-256 of its bytes: a pure function of them, kept for
// the process because nearly every effect request includes the same few files again.
static std::shared_ptr<const std::vector<Directive>> directives(const std::string& hash,const Bytes& bytes) {
    static std::mutex mutex;
    static std::unordered_map<std::string,std::shared_ptr<const std::vector<Directive>>> memo;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it=memo.find(hash); if(it!=memo.end()) return it->second;
    }
    auto list=std::make_shared<std::vector<Directive>>();
    std::istringstream lines(stripComments(bytes)); std::string line,operand;
    while(std::getline(lines,line)) if(includeOperand(line,operand)) {
        Directive d;
        if(operand.size()<2 || (operand[0]!='"' && operand[0]!='<')) d.error="BF2FX_UNSUPPORTED_MACRO_INCLUDE: "+operand;
        else {
            char end=operand[0]=='"'?'"':'>'; auto endpos=operand.find(end,1);
            if(endpos==std::string::npos || operand.find_first_not_of(" \t\r",endpos+1)!=std::string::npos)
                d.error="Invalid literal include";
            else { d.kind=operand[0]=='<'?1:0; d.name=operand.substr(1,endpos-1); }
        }
        list->push_back(d);
        if(!d.error.empty()) break;
    }
    std::lock_guard<std::mutex> lock(mutex);
    return memo.emplace(hash,std::move(list)).first->second;
}
Result capture(const void* source,size_t size,const std::vector<Define>& defines,uint32_t flags,
               const std::string& dllHash,const std::string& translator,IncludeResolver* resolver,Request& out) {
    try {
        if(!source || !size || size>16*1024*1024 || dllHash.size()!=64 || translator.empty())
            return {Error::InvalidInput,"Invalid source/compiler identity"};
        Request r; auto p=static_cast<const uint8_t*>(source); r.source.assign(p,p+size);
        r.defines=defines; r.flags=flags; r.dllHash=dllHash; r.translator=translator;
        std::sort(r.defines.begin(),r.defines.end(),[](const Define& a,const Define& b){return a.name<b.name;});
        for(size_t i=0;i<r.defines.size();++i) if(r.defines[i].name.empty() || (i && r.defines[i-1].name==r.defines[i].name))
            return {Error::InvalidInput,"Empty or duplicate macro"};
        std::set<std::string> ancestors;
        std::function<void(const Bytes&,const void*,uint32_t)> visit;
        visit=[&](const Bytes& bytes,const void* parent,uint32_t id) {
            if(ancestors.size()>64 || r.includes.size()>8192) throw std::runtime_error("Include closure limit");
            auto hash=sha256(bytes); if(!ancestors.insert(hash).second) return;
            const auto list=directives(hash,bytes);
            for(const Directive& d:*list) {
                if(!d.error.empty()) throw std::runtime_error(d.error);
                Include inc; inc.parent=id;inc.kind=d.kind;inc.name=d.name;
                const void* data=nullptr; size_t n=0;
                bool opened=resolver && resolver->open(inc.kind,inc.name,parent,data,n);
                try {
                    if(opened) {
                        if((!data && n) || n>16*1024*1024) throw std::runtime_error("Invalid include buffer");
                        inc.found=1; if(n) inc.bytes.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+n);
                    }
                    r.includes.push_back(inc); auto child=uint32_t(r.includes.size());
                    if(opened) visit(inc.bytes,data,child);
                } catch(...) {if(opened) resolver->close(data);throw;}
                if(opened) resolver->close(data);
            }
            ancestors.erase(hash);
        };
        const uint8_t fx2[]={1,9,255,254};
        if(r.source.size()<4 || std::memcmp(r.source.data(),fx2,4))visit(r.source,nullptr,0);
        out=std::move(r); return {};
    } catch(const std::exception& e) {return {Error::InvalidInput,e.what()};}
}
std::string cacheKey(const Request& request) {
    // Source bytes are content-hashed; callback order, parent identity and virtual spelling matter.
    Request r=request; std::sort(r.defines.begin(),r.defines.end(),[](const Define& a,const Define& b){return a.name<b.name;});
    Writer w; w(std::string("BF2FX-key-literal-closure"),CacheVersion,sha256(r.source),r.defines,r.flags,r.dllHash,r.translator);
    w(uint32_t(r.includes.size())); for(const auto& i:r.includes) w(i.parent,i.kind,i.found,i.name,sha256(i.bytes));
    return sha256(w.bytes);
}
static Result exceptionResult(const std::exception& e) {
    std::string s=e.what(); return {s=="BF2FX_VERSION_MISMATCH"?Error::VersionMismatch:
        s=="BF2FX_HASH_MISMATCH"?Error::HashMismatch:Error::Io,s};
}
Result load(const std::string& cache,const Request& request,Entry& entry) {
    try {
        auto key=cacheKey(request); fs::path root(cache),path=root/"entries"/key/"entry.bin";
        const Bytes magic={1,9,255,254};
        if(request.source.size()>=4 && std::equal(magic.begin(),magic.end(),request.source.begin()))path=root/"binary"/(key+".bin");
        if(!fs::exists(path) && fs::exists(root/"refused"/(key+".bin"))) {
            Entry refused=unenvelope<Entry>(readFile((root/"refused"/(key+".bin")).string()),"entry");
            if(refused.key!=key || refused.kind!=RefusedEntry) return {Error::HashMismatch,"BF2FX_HASH_MISMATCH"};
            entry=std::move(refused); return {Error::Refused,"BF2FX_CACHE_REFUSED: "+key};
        }
        if(!fs::exists(path)) {
            atomicWrite(root/"requests"/(key+".request"),envelope(request,"request"));
            int fd=::open((root/"requests.txt").c_str(),O_WRONLY|O_CREAT|O_APPEND,0600);
            if(fd<0) throw std::runtime_error("Cannot append requests.txt");
            bool ok=flock(fd,LOCK_EX)==0; std::string line=key+"\n";
            if(ok) ok=::write(fd,line.data(),line.size())==ssize_t(line.size());
            if(ok) ok=fsync(fd)==0; flock(fd,LOCK_UN); ::close(fd);
            if(!ok) throw std::runtime_error("Cannot append requests.txt");
            return {Error::Miss,"BF2FX_CACHE_MISS: "+key};
        }
        Entry loaded=unenvelope<Entry>(readFile(path.string()),"entry");
        if(loaded.key!=key) return {Error::HashMismatch,"BF2FX_HASH_MISMATCH"};
        entry=std::move(loaded); return {};
    } catch(const std::exception& e) {return exceptionResult(e);}
}
Result publish(const std::string& cache,const Request& request,const Entry& entry) {
    try {
        if(entry.key!=cacheKey(request)) return {Error::HashMismatch,"BF2FX_HASH_MISMATCH"};
        if(entry.kind==RefusedEntry) {
            if(!entry.fx2.empty() || (entry.parseResult==0 && entry.compileResult==0))
                return {Error::InvalidInput,"Refusal record without a failing HRESULT"};
            atomicWrite(fs::path(cache)/"refused"/(entry.key+".bin"),envelope(entry,"entry"));
            return {};
        }
        if(entry.kind==ReflectionOnlyEntry && (!entry.fx2.empty() || entry.parseResult!=0 || entry.compileResult==0))
            return {Error::InvalidInput,"Reflection-only record needs a parsing compiler and a failing CompileEffect"};
        if(entry.kind>ReflectionOnlyEntry) return {Error::InvalidInput,"Unknown entry kind"};
        auto dir=fs::path(cache)/"entries"/entry.key; fs::create_directories(dir);
        atomicWrite(dir/"effect.fxo",entry.fx2);
        atomicWrite(dir/"description.bin",envelope(entry.description,"description"));
        for(const auto& s:entry.description.shaders) if(!s.msl.empty()) {
            atomicWrite(dir/("object_"+std::to_string(s.object)+".metal"),Bytes(s.msl.begin(),s.msl.end()));
            atomicWrite(dir/("object_"+std::to_string(s.object)+".reflection"),envelope(s,"shader"));
        }
        atomicWrite(dir/"entry.bin",envelope(entry,"entry"));
        if(entry.kind==ReflectionOnlyEntry) return {};
        // Ra creates its runtime effect from the compiled FX2 buffer with NULL macros/include.
        // Preserve both the source compile flags and its documented runtime |0x800 form.
        Request binary=request;binary.source=entry.fx2;binary.defines.clear();binary.includes.clear();
        for(uint32_t flags:{request.flags,request.flags|0x800u}) {
            binary.flags=flags;Entry alias=entry;alias.key=cacheKey(binary);
            atomicWrite(fs::path(cache)/"binary"/(alias.key+".bin"),envelope(alias,"entry"));
        }
        return {};
    } catch(const std::exception& e) {return exceptionResult(e);}
}
} // namespace bf2fx
#endif
