// Path: attested by the Win32 __FILE__ at the URL-validation diagnostic.
// Contents: ELF 0x82BDED0..0x82BE8B0 / PE 0x560F00..0x5617D0, read in both ports.
#include "HTTPRequest.h"
#include "../../External/GameSpy/ghttp/ghttp.h"
#include "../../Core/Debug.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#endif
namespace dice { namespace hfe {

// . Preserve the shipped spelling and unknown-result fallback.
std::string getErrorString(GHTTPResult result)
{
    static const char* const messages[] = {
        "<Unknown error>", "<A memory allocation failed>",
        "<The user-supplied buffer was too small to hold the file>",
        "<There was an error parsing the URL>", "<Failed looking up the hostname>",
        "<Failed to create/initialize/read/write a socket>",
        "<Failed connecting to the http server>", "<Error understanding a response from the server>",
        "<The request has been rejected by the server>", "<Not authorized to get the file>",
        "<The server has refused to send the file>", "<Failed to find the file on the server>",
        "<The server has encountered an internal error>",
        "<An error occured writing to the local file (for ghttpSaveFile[Ex])>",
        "<There was an error reading from a local file (for posting files from disk)>",
        "<Download started but was interrupted.  Only reported if file size is known>",
        "<The file is to big to be downloaded (size exceeds range of interal data types)>"
    };
    return messages[result >= 1 && result <= 16 ? result : 0];
}
// Retail ignores bufferLen and uses the terminating NUL. True releases the SDK buffer.
// The modern vendored SDK supplies its own GHTTPByteCount width.
GHTTPBool cbGetCallback(GHTTPRequest, GHTTPResult result, char* body, GHTTPByteCount, void* data)
{
    if (HTTPRequest* request = static_cast<HTTPRequest*>(data)) {
        if (result != GHTTPSuccess) request->onGetFailure(getErrorString(result));
        else request->onGetComplete(body ? std::string(body) : std::string());
    }
    return GHTTPTrue;
}
GHTTPBool cbPostCallback(GHTTPRequest, GHTTPResult result, char* body, GHTTPByteCount, void* data)
{
    if (HTTPRequest* request = static_cast<HTTPRequest*>(data)) {
        if (result != GHTTPSuccess) request->onPostFailure(getErrorString(result));
        else request->onPostComplete(body ? std::string(body) : std::string());
    }
    return GHTTPTrue;
}
GHTTPBool cbGetFileCallback(GHTTPRequest, GHTTPResult result, char*, GHTTPByteCount, void* data)
{
    if (HTTPRequest* request = static_cast<HTTPRequest*>(data)) {
        if (result != GHTTPSuccess) request->onGetFailure(getErrorString(result));
        else request->onGetFileComplete();
    }
    return GHTTPTrue;
}
void cbProgressCallback(GHTTPRequest, GHTTPState, const char*, GHTTPByteCount,
                        GHTTPByteCount received, GHTTPByteCount total, void* data)
{
    if (HTTPRequest* request = static_cast<HTTPRequest*>(data))
        request->onProgress(static_cast<float>(received) / static_cast<float>(total));
}
HTTPRequest::HTTPRequest()
    : m_getPending(false), m_postPending(false), m_ghttpHandleA(-1), m_postData(0),
      m_ghttpHandleB(-1), m_revision(1)
{ ghttpStartup(); }
HTTPRequest::~HTTPRequest()
{
    if (m_ghttpHandleB >= 0) ghttpCancelRequest(m_ghttpHandleB);
    if (m_postData) m_postData = 0;
    if (m_ghttpHandleA >= 0) ghttpCancelRequest(m_ghttpHandleA);
    ghttpCleanup();
}
void HTTPRequest::setProxy(const std::string& proxy) { ghttpSetProxy(proxy.c_str()); }
bool HTTPRequest::isUpdating() { return m_postPending || m_getPending; }
void HTTPRequest::update()
{
    if (m_ghttpHandleB != -1) ghttpRequestThink(m_ghttpHandleB);
    if (m_ghttpHandleA != -1) ghttpRequestThink(m_ghttpHandleA);
}
bool HTTPRequest::cancel()
{
    m_revision = 1;
    if (m_ghttpHandleB >= 0) {
        ghttpCancelRequest(m_ghttpHandleB);
        m_ghttpHandleB = -1;
    }
    if (m_postData) m_postData = 0;
    // Retail does not clear m_postPending here.
    if (m_ghttpHandleA < 0) return false;
    ghttpCancelRequest(m_ghttpHandleA);
    m_ghttpHandleA = -1;
    m_getPending = false;
    return true;
}
bool HTTPRequest::get(const std::string& url, bool asynchronous)
{
    if (m_getPending) return false;
    m_getPending = true;
    m_revision = 1;
    m_ghttpHandleA = ghttpGetA(url.c_str(), asynchronous ? GHTTPFalse : GHTTPTrue, cbGetCallback, this);
    if (m_ghttpHandleA < 0) { m_getPending = false; return false; }
    if (!m_getPending) { ghttpCancelRequest(m_ghttpHandleA); m_ghttpHandleA = -1; }
    return true;
}
bool HTTPRequest::post(const std::string& url, const std::string& body, bool asynchronous)
{
    if (m_postPending) return false;
    m_postData = ghttpNewPost();
    if (!m_postData) return false;
    m_postPending = true;
    if (ghttpPostAddStringA(static_cast<GHTTPPost>(m_postData), "BF2.HTTPRequest.Post", body.c_str()) != GHTTPTrue) {
        m_postData = 0; m_postPending = false; return false;
    }
    m_ghttpHandleB = ghttpPostA(url.c_str(), static_cast<GHTTPPost>(m_postData),
        asynchronous ? GHTTPFalse : GHTTPTrue, cbPostCallback, this);
    if (m_ghttpHandleB < 0) { m_postData = 0; m_postPending = false; return false; }
    if (!m_postPending) {
        ghttpCancelRequest(m_ghttpHandleB);
        m_ghttpHandleB = -1;
        if (m_postData) m_postData = 0;
    }
    return true;
}
bool HTTPRequest::getFile(const std::string& url, const std::string& file, bool asynchronous)
{
    if (m_getPending) return false;
    m_getPending = true;
    m_revision = 1;
    m_ghttpHandleA = ghttpSaveExA(url.c_str(), file.c_str(), "", 0, GHTTPFalse,
        asynchronous ? GHTTPFalse : GHTTPTrue, cbProgressCallback, cbGetFileCallback, this);
    if (m_ghttpHandleA < 0) { m_getPending = false; return false; }
    if (!m_getPending) { ghttpCancelRequest(m_ghttpHandleA); m_ghttpHandleA = -1; }
    return true;
}
void HTTPRequest::onGetComplete(const std::string&) { ++m_revision; m_getPending = false; }
void HTTPRequest::onGetFailure(const std::string&) { ++m_revision; m_getPending = false; }
void HTTPRequest::onGetFileComplete() { ++m_revision; m_getPending = false; }
void HTTPRequest::onPostComplete(const std::string&) { m_postData = 0; m_postPending = false; }
void HTTPRequest::onPostFailure(const std::string&) { m_postData = 0; m_postPending = false; }
void HTTPRequest::onProgress(float) {} // Shipped empty: ELF 0x82BE7F0 / PE 0x5614F0.
int HTTPRequest::getRevision() { return m_revision; }
bool HTTPRequest::validateUrlNotSpoofed(std::string host) const
{
    // [PLATFORM] ELF 0x82BE8A0 returns true. PE 0x561500 scans the hosts file,
    // emits a type-7 diagnostic on a mismatch, and still returns true.
#if defined(_WIN32)
    char path[260] = {};
    GetSystemDirectoryA(path, 260);
    std::strcat(path, "\\drivers\\etc\\hosts");
    FILE* file = std::fopen(path, "r");
    if (file) {
        bool invalid = false;
        char line[256];
        while (std::fgets(line, 256, file))
            if (line[0] != '#') {
                const std::string text(line);
                if (text.find(host) != std::string::npos && text.find("212.77.171.") == std::string::npos)
                    invalid = true;
            }
        std::fclose(file);
        if (invalid)
            Debug(DEBUGTYPE_7, "Game/Persistence/HTTPRequest.cpp", 445, __FILE__)
                << "The request is invalid due to an invalid GameSpy URL.";
    }
#endif
    return true;
}
std::string HTTPRequest::getGameSpyUrl() const
{
    // [CHOSEN, not recovered] Cross-platform OpenSpy policy, see #21. The reference
    // images used BF2Web.gamespy.com (PE) and BF2Web.game.bf2.us (ELF), not openspy.net.
    std::string url("http://BF2Web.bf2hub.p.bf2.org/ASP/");
    validateUrlNotSpoofed("BF2Web.bf2hub.p.bf2.org");
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: local-stats-fixture]
    // BF2_STATS_URL overrides persistence URLs for local fixtures after validating the
    // original URL. Inactive when unset; announces activation once on stderr.
    if (const char* fixture = std::getenv("BF2_STATS_URL"))
    {
        if (*fixture)
        {
            static bool announced = false;
            if (!announced)
            {
                announced = true;
                std::fprintf(stderr, "[TEMP] BF2_STATS_URL: persistence requests go to %s\n", fixture);
                std::fflush(stderr);
            }
            url = fixture;
        }
    }
    return url;
}
} }
