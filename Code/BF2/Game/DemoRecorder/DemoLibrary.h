// Lists profile demos and exposes download controls. [INFERRED] Names and placement.
// The HTTPRequest base sits at +4 (ctor 4C2510 calls 59C390 on [esi+4] and stores its table
// 8A90E8 there). update/isDownloading/cancelDownload and the three callbacks are recovered;
// download (4C2DF0) still stops: its target-name helper 4C26E0 is unrecovered.
#ifndef GAME_DEMORECORDER_DEMOLIBRARY_H
#define GAME_DEMORECORDER_DEMOLIBRARY_H
#include "../../Core/IBase.h"
#include "../Persistence/HTTPRequest.h"
#include <string>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {

// The 144-byte (PE) row refresh builds per demo file: the header's fields, the file's path in
// the demos directory and its name without extension. [INFERRED] names.
struct DemoLibraryEntry
{
    DemoLibraryEntry();
    int version;                                  // +00  DemoHeader::version
    std::string serverName;                       // +04  DemoHeader::serverName
    std::string path;                             // +20  "@HOME@/Profiles/Default/demos/<file>"; removeEntry deletes it
    std::string name;                             // +3C  the file name without its extension
    std::string dateTime;                         // +58  DemoHeader::dateTime
    std::string mapName;                          // +74  DemoHeader::mapName
};

// Every slot of the primary table 8A9108 except the destructor, which retail places in the
// HTTPRequest table. Slot names are [INFERRED] from the bodies and their callers; +40/+44's role is
// [UNRESOLVED]. String getters return references to the stored strings (retail returns the member's
// address).
class IDemoLibrary : public IBase {
public:
    virtual bool refresh() = 0;                                        //+10 4C3030: 4C2A60, then true; LocalProfileManager::login 4AA4CD
    virtual bool download(const std::string& name, const std::string& url, const std::string& description) = 0; //+14 4C2DF0 (name helper 4C26E0)
    virtual unsigned int getNumEntries() = 0;
    virtual DemoLibraryEntry* getEntry(unsigned int index) = 0;        //+1C 4C1FF0: null past the count (through the virtual count)
    virtual bool removeEntry(unsigned int index) = 0;                  //+20 4C2660: deletes the file, erases the row
    virtual int update() = 0;                                          //+24 4C1B70 -> HTTPRequest 59C420
    virtual bool isDownloading() = 0;                                  //+28 4C1B80 -> HTTPRequest 59C400
    virtual const std::string& getDownloadName() = 0;                  //+2C 4C1B90  +4C
    virtual const std::string& getDownloadUrl() = 0;
    virtual const std::string& getDownloadDescription() = 0;
    virtual float getDownloadProgress() = 0;                           //+38 4C1BB0  +A0
    virtual bool cancelDownload() = 0;                                 //+3C 4C1BD0
    virtual bool getFlagA4() = 0;                                      //+40 4C1BC0  +A4 [UNRESOLVED role]
    virtual void setFlagA4(bool value) = 0;                            //+44 4B3AB0 (ICF-shared setter)
    virtual void setPendingBookmark(int index) = 0;                    //+48 4B1C80  +A8; the download-complete callback hands it to LocalProfileManager +B0
    virtual bool hasDownloadFailed() = 0;                              //+4C 4C1C00  +AC, raised by the failure callback
};

class DemoLibrary final : public IDemoLibrary, public HTTPRequest {
public:
    DemoLibrary();                                                     // 4C2510, retail 176 bytes (24 of them the base at +4)
    ~DemoLibrary();                                                    // 4C25D0; its slot is the base table's 4C25C0 thunk
    static IBase* create(unsigned int iid, IBase* outer);
    void addRef() override;                                            // shared
    int getRef() override;                                             // shared
    int release() override;                                            // shared
    void* queryInterface(unsigned int iid) const override;
    bool refresh() override;
    bool download(const std::string& name, const std::string& url, const std::string& description) override;
    unsigned int getNumEntries() override;
    DemoLibraryEntry* getEntry(unsigned int index) override;
    bool removeEntry(unsigned int index) override;
    int update() override;
    bool isDownloading() override;
    const std::string& getDownloadName() override;
    const std::string& getDownloadUrl() override;
    const std::string& getDownloadDescription() override;
    float getDownloadProgress() override;
    bool cancelDownload() override;
    bool getFlagA4() override;
    void setFlagA4(bool value) override;
    void setPendingBookmark(int index) override;
    bool hasDownloadFailed() override;
    // The HTTPRequest table 8A90E8: +8 4C1EC0, +C 4C2D70, +18 4B3A90 (the rest are the base's).
    void onGetFailure(const std::string& message) override;
    void onGetFileComplete() override;
    void onProgress(float progress) override;

private:
    friend struct DemoLibraryFixture;
    bool refreshEntries();
    std::string nextFreeDemoName();                                    // 4C26E0 [INFERRED] name                                             // [INFERRED] name
    void reportError(const std::string& message);                      // [INFERRED] name: SwiffHost +1E4 record {1, message, 0}
    int m_refCount;                                                    // retail +1C
    std::vector<DemoLibraryEntry> m_entries;                           // retail +20
    std::string m_downloadPath;                                        // retail +30  the target file of a download
    std::string m_downloadName;                                        // retail +4C
    std::string m_downloadUrl;                                         // retail +68
    std::string m_downloadDescription;                                 // retail +84
    float m_downloadProgress;                                          // retail +A0
    bool m_flagA4;                                                     // retail +A4
    int m_pendingBookmark;                                             // retail +A8, -1
    bool m_downloadFailed;                                             // retail +AC
};

} }
#endif
#endif
