// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: DEMO-LIBRARY-FIXTURE]
// Tests real DemoLibrary and compression/header code against in-memory demo files.
// Typed registry, profile, and file observers supply only the requested external operations.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <list>
#include "../BF2/Game/DemoRecorder/DemoLibrary.h"
#include "../BF2/Game/DemoRecorder/DemoHeader.h"
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "SwiffHostStandIn.h"
#include "../BF2/IO/Stream/ZStream.h"
#include "../BF2/IO/ZUtil.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
using namespace dice::hfe;
static unsigned checks, failures;
static void check(bool value,const char* label)
{ ++checks; if(!value){++failures;std::printf("FAIL %s\n",label);} }

namespace dice { namespace hfe {
// [TEMP] Globals this isolated executable does not link from their owning units.
ClassManager* classManager=nullptr;
EventManager* eventManager=nullptr;
namespace io { FileManager* fileManager=nullptr; }
unsigned seamCalls=0; std::string seamText; int seamType=-1, seamFlag=-1;
// [TEMP] The typed SwiffHost stand-in: postMessage records the {type, text, code} record
// DemoLibrary::reportError posts (ISwiffHost +0x1E4); every other slot aborts.
struct DemoSwiffHost : SwiffHostStandIn {
    bool postMessage(const SwiffHostMessage& m) override { ++seamCalls;seamType=m.type;seamText=m.text;seamFlag=m.code;return true; }
};
static DemoSwiffHost swiffHostObserver;
struct DemoLibraryFixture {
    static HTTPRequest* base(IBase* b){ return static_cast<DemoLibrary*>(b); }  // the +4 base, through its own table
    static void transfer(IBase* b,int handle,const std::string& path){
        DemoLibrary* d=static_cast<DemoLibrary*>(b); d->m_getPending=true; d->m_ghttpHandleA=handle; d->m_downloadPath=path;
    }
    static int refs(IBase* b){ return static_cast<DemoLibrary*>(b)->m_refCount; }
    static int pending(IBase* b){ return static_cast<DemoLibrary*>(b)->m_pendingBookmark; }
    static const std::string& downloadPath(IBase* b){ return static_cast<DemoLibrary*>(b)->m_downloadPath; }
    static void report(IBase* b,const std::string& m){ static_cast<DemoLibrary*>(b)->reportError(m); }
};
} }

// [TEMP] The GameSpy HTTP entry points the HTTPRequest base calls on these paths: counted, the
// request handles recorded. Declared with the SDK's C names and plain widths (GHTTPRequest and
// GHTTPBool are ints) so ghttp.h and its socket headers stay out of this windows.h unit.
static unsigned ghttpStartups=0, ghttpCleanups=0;
static std::vector<int> ghttpThinks, ghttpCancels;
extern "C" void ghttpStartup(void) { ++ghttpStartups; }
extern "C" void ghttpCleanup(void) { ++ghttpCleanups; }
extern "C" int ghttpRequestThink(int request) { ghttpThinks.push_back(request); return 1; }
extern "C" void ghttpCancelRequest(int request) { ghttpCancels.push_back(request); }

// [TEMP] A refcounted byte-vector stream: the sink the deflating ZStream writes into and the source
// each inflating ZStream reads from (the file manager hands out a fresh one per open).
struct ByteStream : io::IStream {
    std::vector<unsigned char> bytes; unsigned pos=0; int refs=1; unsigned reads=0; bool deleted=false;
    static unsigned destroyed;
    ByteStream() {}
    explicit ByteStream(const std::vector<unsigned char>& b) : bytes(b) {}
    void addRef() override { ++refs; }
    int getRef() override { return refs; }
    int release() override { const int r=--refs; if(!r){ ++destroyed; delete this; } return r; }
    void* queryInterface(unsigned int iid) const override { return iid==dice::hfe::IID_IBase||iid==dice::hfe::io::IID_IStream?const_cast<ByteStream*>(this):nullptr; } // qualified: windows.h declares a GUID IID_IStream
    unsigned int read(void* buffer,unsigned int size) override {
        ++reads; unsigned n=size; if(pos+n>bytes.size()) n=static_cast<unsigned>(bytes.size()-pos);
        if(n) std::memcpy(buffer,&bytes[pos],n); pos+=n; return n;
    }
    unsigned int write(const void* buffer,unsigned int size) override {
        const unsigned char* p=static_cast<const unsigned char*>(buffer); bytes.insert(bytes.end(),p,p+size); return size;
    }
    int skip(unsigned int n) override { pos+=n; return 1; }
    int getAvailableBytes() override { return static_cast<int>(bytes.size()-pos); }
    io::IStream* clone() override { return nullptr; }
    bool canRead() override { return true; }
    bool canWrite() override { return true; }
};
unsigned ByteStream::destroyed=0;

// [TEMP] The profile manager the refresh asks for its current profile and the download completion
// hands its bookmark to; every other slot aborts.
struct ProfileOwner : ILocalProfileManager {
    LocalProfile* current=nullptr; unsigned queries=0; std::vector<unsigned> removedBookmarks;
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool hasDefaultProfile() override { std::abort(); }
    bool createDefaultProfile() override { std::abort(); }
    bool init() override { std::abort(); }
    bool login(int,const std::string&,const std::string&) override { std::abort(); }
    void shutdown() override { std::abort(); }
    bool createProfile(const std::string&,const std::string&) override { std::abort(); }
    bool deleteProfile(int) override { std::abort(); }
    bool createOnlineAccount(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    void frontierPrimary30() override { std::abort(); }
    void frontierPrimary34() override { std::abort(); }
    bool hasProfile(const std::string&) override { std::abort(); }
    bool loadCurrentProfile() override { std::abort(); }
    bool saveCurrentProfile() override { std::abort(); }
    LocalProfile* getCurrentProfile() override { ++queries; return current; }
    bool getLocalProfiles(std::vector<LocalProfile>&,bool) override { std::abort(); }
    std::string getProfilesPath() override { std::abort(); }
    bool findProfileDirectory(const std::string&,std::string&) override { std::abort(); }
    bool loadProfileFile(const std::string&) override { std::abort(); }
    bool applySettings(bool) override { std::abort(); }
    bool loginOnline(const std::string&,const std::string&,bool) override { std::abort(); }
    bool loginOnlineAlternate(const std::string&,const std::string&,bool) override { std::abort(); }
    void setName(const std::string&) override { std::abort(); }
    void setGamespyNick(const std::string&) override { std::abort(); }
    void setEmail(const std::string&) override { std::abort(); }
    void setPassword(const std::string&,bool) override { std::abort(); }
    void setNick(const std::string&) override { std::abort(); }
    void setNumTimesLoggedIn(int) override { std::abort(); }
    std::string getName() override { std::abort(); }
    std::string getGamespyNick() override { std::abort(); }
    std::string getEmail() override { std::abort(); }
    std::string getPassword() override { std::abort(); }
    std::string getNick() override { std::abort(); }
    int getNumTimesLoggedIn() override { std::abort(); }
    bool hasDefaultVoiceSettings() override { std::abort(); }
    bool loadVoiceSettings() override { std::abort(); }
    bool saveVoiceSettings() override { std::abort(); }
    bool readVoiceSettings(float&,float&,float&,bool&) override { std::abort(); }
    void addDemoBookmark(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    unsigned int getNumDemoBookmarks() override { std::abort(); }
    LocalDemoBookmark* getDemoBookmark(unsigned int) override { std::abort(); }
    bool removeDemoBookmark(unsigned int index) override { removedBookmarks.push_back(index); return true; }
    float getTotalPlayedTime() override { std::abort(); }
    void setTotalPlayedTime(float) override { std::abort(); }
    bool validateProfileName(const std::string&,std::string&) override { std::abort(); }
    bool quickLogin(const std::string&,const std::string&) override { std::abort(); }
    bool saveGlobalSettings() override { std::abort(); }
    bool validateProfileFiles(const std::string&) override { std::abort(); }
};

struct Registry : ClassManager {
    ProfileOwner* owner=nullptr; bool swiff=false; unsigned queries=0;
    IBase* getSingleton(const std::string& name) override {
        ++queries;
        if(name=="LocalProfileManager") return owner;
        if(name=="SwiffHost") return swiff?static_cast<IBase*>(&swiffHostObserver):nullptr;
        check(false,"refresh looks up only the profile manager and the SwiffHost");
        return nullptr;
    }
};

// [TEMP] The file manager: a directory listing, one byte image per file name, and a record of
// every open (arguments included) and deletion.
struct Files : io::FileManager {
    std::list<io::FileInfo> listing; unsigned searches=0, opens=0;
    std::vector<std::string> openedPaths, deletedPaths; std::string lastDirectory; bool lastRecursive=true;
    std::vector<std::pair<std::string,std::vector<unsigned char> > > images;
    int findFiles(const std::string& path,bool recursive,std::list<io::FileInfo>& out) override {
        ++searches;lastDirectory=path;lastRecursive=recursive;out=listing;return 7;
    }
    io::IStream* open(const std::string& path,io::FileMode mode,io::FileAccess access,unsigned flags) override {
        ++opens;openedPaths.push_back(path);
        check(mode==2 && access==0 && flags==0,"refresh opens every demo with the retail read mode/access/flags");
        for(unsigned i=0;i<images.size();++i) if(path==images[i].first) return new ByteStream(images[i].second);
        return nullptr;
    }
    bool deleteFile(const std::string& path) override { deletedPaths.push_back(path); return true; }
    void entry(const char* name,bool directory) { io::FileInfo info{};info.name=name;info.isDirectory=directory;listing.push_back(info); }
};

static std::vector<unsigned char> demoImage(int version,const std::string& server,const char* date,const char* map,unsigned payload,unsigned truncateTo)
{
    ByteStream* sink=new ByteStream;
    {
        io::ZStream* writer=new io::ZStream(sink,io::ZStream::ZSTREAMMODE_WRITE);
        DemoHeader header;header.version=version;header.serverName=server;header.dateTime=date;header.mapName=map;
        header.writeTo(writer);
        std::vector<unsigned char> filler(payload,0x5A);
        if(payload) writer->write(&filler[0],payload);
        writer->release(); // closes the writer: the stream is finished into the sink
    }
    std::vector<unsigned char> bytes=sink->bytes;
    sink->release();
    if(truncateTo && bytes.size()>truncateTo) bytes.resize(truncateTo);
    return bytes;
}

static void death(const char* mode,const char* expected)
{
    char executable[MAX_PATH]; GetModuleFileNameA(nullptr,executable,MAX_PATH);
    std::string command=std::string("\"")+executable+"\" "+mode;
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE reader=nullptr,writer=nullptr;
    if(!CreatePipe(&reader,&writer,&security,0)) { check(false,"diagnostic pipe"); return; }
    SetHandleInformation(reader,HANDLE_FLAG_INHERIT,0);
    STARTUPINFOA startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
    startup.hStdOutput=startup.hStdError=writer; startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    bool started=CreateProcessA(nullptr,&command[0],nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE;
    CloseHandle(writer); check(started,"start diagnostic child");
    if(!started) { CloseHandle(reader); return; }
    DWORD wait=WaitForSingleObject(process.hProcess,10000); check(wait==WAIT_OBJECT_0,"diagnostic child stops promptly");
    if(wait!=WAIT_OBJECT_0) TerminateProcess(process.hProcess,99);
    DWORD result; GetExitCodeProcess(process.hProcess,&result);
    std::string output; char data[512]; DWORD count;
    while(ReadFile(reader,data,sizeof(data),&count,nullptr)&&count) output.append(data,count);
    CloseHandle(reader); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    check(result!=0 && result!=STILL_ACTIVE,"diagnostic cannot succeed");
    if(output.find(expected)!=0) std::printf("Diagnostic mismatch for %s: expected %s; actual %s\n",mode,expected,output.c_str());
    check(output.find(expected)==0,"qualified diagnostic is the first output");
}

int main(int argc,char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    Registry registry; classManager=&registry;
    Files& files=*new Files; io::fileManager=&files;
    if(argc==2) {
        IBase* object=DemoLibrary::create(IID_IDemoLibrary,nullptr);
        IDemoLibrary* api=static_cast<IDemoLibrary*>(object);
        if(!std::strcmp(argv[1],"download")) api->download("name","http://server/demo","description");
        std::printf("unexpected return\n");
        return 0;
    }

    // the zlib stream pair
    {
        io::ZReader reader; io::ZWriter writer;
        check(!reader.open(nullptr) && !writer.open(nullptr),"the helpers refuse a null stream");
        check(reader.eof(),"a reader that is not open reports its end");
        ByteStream* sink=new ByteStream;
        io::ZStream* deflating=new io::ZStream(sink,io::ZStream::ZSTREAMMODE_WRITE);
        check(sink->refs==3,"the stream and its helper each retain the source");
        check(deflating->canWrite() && !deflating->canRead() && !deflating->clone() && deflating->getAvailableBytes()==0,
            "a write-mode stream writes only, clones nothing and reports no readable bytes");
        std::vector<unsigned char> payload(20000); for(unsigned i=0;i<payload.size();++i) payload[i]=static_cast<unsigned char>((i*7)^(i>>5));
        check(deflating->write(&payload[0],5000)==5000 && deflating->write(&payload[5000],15000)==15000,"writes report their whole input");
        check(deflating->read(&payload[0],1)==0,"a write-mode stream reads nothing");
        deflating->addRef(); check(deflating->getRef()==2 && deflating->release()==1,"non-final release keeps the stream");
        check(deflating->release()==0 && sink->refs==1 && !sink->bytes.empty() && sink->bytes.size()<payload.size(),
            "the final release finishes the deflate into the sink and drops both references to it");
        ByteStream* source=new ByteStream(sink->bytes); sink->release();
        io::ZStream* inflating=new io::ZStream(source,io::ZStream::ZSTREAMMODE_READ);
        check(inflating->canRead() && !inflating->canWrite() && inflating->getAvailableBytes()==1,"a read-mode stream reads only and reports a byte available");
        check(inflating->write(&payload[0],1)==0,"a read-mode stream writes nothing");
        std::vector<unsigned char> back(payload.size());
        check(inflating->read(&back[0],3000)==3000 && std::memcmp(&back[0],&payload[0],3000)==0,"the first read inflates across the staging buffer");
        check(inflating->skip(700)==1,"skip consumes through the static buffer");
        check(inflating->read(&back[3700],payload.size()-3700)==payload.size()-3700 && std::memcmp(&back[3700],&payload[3700],payload.size()-3700)==0,
            "the remainder inflates exactly");
        check(inflating->read(&back[0],1)==0 && inflating->getAvailableBytes()==0 && inflating->skip(1)==0,"after the stream end nothing is read, available or skipped");
        check(inflating->skip(0)==1,"a zero skip succeeds at once");
        check(source->refs==3 && inflating->release()==0 && source->refs==1,"the reader's final release drops the stream's and the helper's references");
        const unsigned destroyed=ByteStream::destroyed;
        check(source->release()==0 && ByteStream::destroyed==destroyed+1,"the fixture's own reference was the last one");
    }
    // the demo header
    {
        DemoHeader header;
        check(header.version==0 && header.serverName.empty() && header.dateTime.empty() && header.mapName.empty() && !header.byte10,"a fresh header is empty");
        ByteStream* raw=new ByteStream;
        header.version=3153;header.serverName="[DICE] server";header.dateTime="2026-09-08 12:00";header.mapName="strike_at_karkand";
        header.writeTo(raw);
        check(raw->bytes.size()==4+4+13+4+16+4+17,"writeTo emits the int and three length-prefixed strings");
        raw->pos=0; DemoHeader back;
        check(back.readFrom(raw) && back.version==3153 && back.serverName=="[DICE] server" && back.dateTime=="2026-09-08 12:00" && back.mapName=="strike_at_karkand",
            "readFrom restores every serialized field");
        raw->bytes.resize(30); raw->pos=0; DemoHeader broken;
        check(!broken.readFrom(raw) && broken.serverName=="[DICE] server" && broken.dateTime.empty(),"a truncated string fails the read and clears only that string");
        raw->release();
    }
    // the library
    check(!DemoLibrary::create(IID_IDemoLibrary,reinterpret_cast<IBase*>(1)) && ghttpStartups==0,"aggregation is refused before allocation");
    check(!DemoLibrary::create(0xffffffff,nullptr) && ghttpStartups==1 && ghttpCleanups==1,
        "unsupported factory id releases the fresh owner, whose HTTPRequest base starts and cleans up the HTTP layer");
    IBase* object=DemoLibrary::create(IID_IDemoLibrary,nullptr);
    check(ghttpStartups==2,"the HTTPRequest base starts the HTTP layer");
    check(object!=nullptr,"factory returns the owner for its interface id");
    if(!object){ std::printf("DemoLibrary: %u/%u passed\n",checks-failures,checks); return 1; }
    IDemoLibrary* api=static_cast<IDemoLibrary*>(object);
    check(DemoLibraryFixture::refs(object)==1 && api->getRef()==1,"refcount starts at one and queryInterface does not retain");
    check(api->queryInterface(IID_IBase)==object && api->queryInterface(IID_IDemoLibrary)==object && !api->queryInterface(0xffffffff),
        "queryInterface answers IBase and IDemoLibrary only");
    check(api->getNumEntries()==0 && !api->getEntry(0) && api->getDownloadName().empty() && api->getDownloadUrl().empty() && api->getDownloadDescription().empty()
        && api->getDownloadProgress()==0.0f && !api->getFlagA4() && DemoLibraryFixture::pending(object)==-1 && !api->hasDownloadFailed() && DemoLibraryFixture::downloadPath(object).empty(),
        "the constructed library is empty with every download field cleared");
    api->setFlagA4(true); api->setPendingBookmark(4);
    check(api->getFlagA4() && DemoLibraryFixture::pending(object)==4,"the flag and pending-bookmark setters store their values");
    check(!api->removeEntry(0),"removing from an empty library refuses");

    ProfileOwner owner; // the record is never read by the refresh, so a presence-only sentinel stands in below
    check(api->refresh() && api->getNumEntries()==0 && files.searches==0,"refresh without a profile manager scans nothing and still succeeds");
    registry.owner=&owner;
    check(api->refresh() && api->getNumEntries()==0 && files.searches==0 && owner.queries==1,"refresh without a current profile scans nothing");
    owner.current=reinterpret_cast<LocalProfile*>(1); // only presence is inspected; LocalProfile's constructor lives in the unlinked manager unit
    files.entry("subdir.bf2demo",true); files.entry("notes.txt",false); files.entry("first.bf2demo",false); files.entry("second.bf2demo",false);
    files.entry("broken.bf2demo",false); files.entry("stub.bf2demo",false); files.entry("missing.bf2demo",false);
    // broken: an incompressible 3000-character server name cut a hundred compressed bytes in, so the
    // name's length decodes but its bytes do not - the string reader's failure arm. stub: twenty
    // compressed bytes that inflate to nothing - the shipped reader takes a missing length as an empty
    // string and the version read ignores its count, so retail lists such a file with zeroed fields.
    std::string noisy(3000,' '); for(unsigned i=0;i<noisy.size();++i) noisy[i]=static_cast<char>(33+((i*131+(i>>3)*17)%90));
    files.images.push_back(std::make_pair(std::string("@HOME@/Profiles/Default/demos/first.bf2demo"),demoImage(3153,"Alpha","2026-09-08 10:00","gulf_of_oman",9000,0)));
    files.images.push_back(std::make_pair(std::string("@HOME@/Profiles/Default/demos/second.bf2demo"),demoImage(3153,"Beta","2026-09-08 11:00","dalian_plant",0,0)));
    files.images.push_back(std::make_pair(std::string("@HOME@/Profiles/Default/demos/broken.bf2demo"),demoImage(3153,noisy,"2026-09-08 12:00","kubra_dam",0,100)));
    files.images.push_back(std::make_pair(std::string("@HOME@/Profiles/Default/demos/stub.bf2demo"),demoImage(3153,"Delta","2026-09-08 13:00","zatar_wetlands",12000,20)));
    const unsigned destroyedBefore=ByteStream::destroyed;
    check(api->refresh(),"refresh with a profile succeeds");
    check(files.searches==1 && files.lastDirectory=="@HOME@/Profiles/Default/demos/" && !files.lastRecursive,"the demos directory is listed once, not recursively");
    check(files.openedPaths==std::vector<std::string>({"@HOME@/Profiles/Default/demos/first.bf2demo","@HOME@/Profiles/Default/demos/second.bf2demo",
        "@HOME@/Profiles/Default/demos/broken.bf2demo","@HOME@/Profiles/Default/demos/stub.bf2demo","@HOME@/Profiles/Default/demos/missing.bf2demo"}),
        "only non-directory .bf2demo entries are opened, in listing order");
    check(ByteStream::destroyed==destroyedBefore+4,"every opened stream is released");
    check(api->getNumEntries()==3,"the two readable headers and the undecodable stub become entries; the cut string and the missing file do not");
    DemoLibraryEntry* first=api->getEntry(0); DemoLibraryEntry* second=api->getEntry(1);
    check(first && first->version==3153 && first->serverName=="Alpha" && first->dateTime=="2026-09-08 10:00" && first->mapName=="gulf_of_oman"
        && first->path=="@HOME@/Profiles/Default/demos/first.bf2demo" && first->name=="first","the first entry carries the header fields, the path and the bare name");
    check(second && second->serverName=="Beta" && second->mapName=="dalian_plant" && second->name=="second","the second entry follows");
    DemoLibraryEntry* stub=api->getEntry(2);
    check(stub && stub->version==0 && stub->serverName.empty() && stub->dateTime.empty() && stub->mapName.empty() && stub->name=="stub"
        && stub->path=="@HOME@/Profiles/Default/demos/stub.bf2demo","an undecodable header lists with zeroed fields, as shipped");
    check(!api->getEntry(3),"getEntry past the count is null");
    check(api->refresh() && api->getNumEntries()==3 && files.searches==2,"a second refresh rebuilds the same three entries");
    check(api->removeEntry(0) && files.deletedPaths==std::vector<std::string>({"@HOME@/Profiles/Default/demos/first.bf2demo"}) && api->getNumEntries()==2
        && api->getEntry(0)->name=="second","removeEntry deletes the row's file and erases the row");
    check(!api->removeEntry(2) && files.deletedPaths.size()==1,"removeEntry past the count deletes nothing");

    // the HTTP side over the HTTPRequest base
    api->update();
    check(ghttpThinks.empty(),"update without a transfer thinks no request");
    check(!api->isDownloading(),"no transfer is not downloading");
    check(!api->cancelDownload() && ghttpCancels.empty() && files.deletedPaths.size()==1,"cancel without a transfer refuses and deletes nothing");
    DemoLibraryFixture::transfer(object,7,"@HOME@/Profiles/Default/demos/partial.bf2demo");
    api->update();
    check(ghttpThinks==std::vector<int>({7}),"update thinks the pending get request");
    check(api->isDownloading(),"a pending get is downloading");
    check(api->cancelDownload() && ghttpCancels==std::vector<int>({7}) && !api->isDownloading()
        && files.deletedPaths.size()==2 && files.deletedPaths[1]=="@HOME@/Profiles/Default/demos/partial.bf2demo",
        "cancel stops the request and deletes the target file");
    DemoLibraryFixture::base(object)->onProgress(0.25f);
    check(api->getDownloadProgress()==0.25f,"the progress callback stores the fraction");
    const int revision=DemoLibraryFixture::base(object)->getRevision();
    DemoLibraryFixture::transfer(object,8,"@HOME@/Profiles/Default/demos/partial.bf2demo");
    DemoLibraryFixture::base(object)->onGetFailure("");
    check(api->hasDownloadFailed() && !api->isDownloading() && DemoLibraryFixture::base(object)->getRevision()==revision+1
        && seamCalls==0 && files.deletedPaths.size()==3,"a failure latches, runs the base bookkeeping and deletes the file; an empty message is not reported");
    registry.swiff=true;
    DemoLibraryFixture::base(object)->onGetFailure("Unable to download demo file");
    check(seamCalls==1 && seamType==1 && seamText=="Unable to download demo file" && files.deletedPaths.size()==4,"a failure message is reported");
    registry.swiff=false; seamCalls=0;
    const unsigned searchesBefore=files.searches;
    DemoLibraryFixture::base(object)->onGetFileComplete();
    check(!api->hasDownloadFailed() && owner.removedBookmarks==std::vector<unsigned>({4}) && DemoLibraryFixture::pending(object)==-1
        && files.searches==searchesBefore+1 && api->getNumEntries()==3,"completion clears the latch, hands the bookmark over, clears it and rebuilds the list");
    DemoLibraryFixture::base(object)->onGetFileComplete();
    check(owner.removedBookmarks.size()==1 && files.searches==searchesBefore+2,"without a pending bookmark completion only rebuilds the list");
    ghttpThinks.clear(); ghttpCancels.clear();
    check(seamCalls==0,"no error is reported along the refresh path");
    DemoLibraryFixture::report(object,"Unable to download demo file");
    check(seamCalls==0,"without a SwiffHost the reporter does nothing");
    registry.swiff=true; DemoLibraryFixture::report(object,"Unable to download demo file");
    check(seamCalls==1 && seamType==1 && seamFlag==0 && seamText=="Unable to download demo file","with a SwiffHost the reporter posts the {1, text, 0} record");
    api->addRef();
    check(api->getRef()==2 && api->release()==1,"non-final release keeps the owner");
    check(api->release()==0 && ghttpCleanups==2 && ghttpCancels==std::vector<int>({8}),
        "final release reports zero; the base cancels the handle the finished get left behind, then cleans up");
    death("download","[FRONTIER] dice::hfe::DemoLibrary::download:");
    std::printf("DemoLibrary: %u/%u passed; native owner=%zu (retail 176 with the HTTPRequest base) entry=%zu (retail 144) header=%zu (retail 92) zstream=%zu (retail 24)\n",
        checks-failures,checks,sizeof(DemoLibrary),sizeof(DemoLibraryEntry),sizeof(DemoHeader),sizeof(io::ZStream));
    return failures?1:0;
}
