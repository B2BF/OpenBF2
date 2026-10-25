// [TEMP] Runtime checks for VOIP::CVoipRouterInstance (Phase 2).
//
// A mock transport supplies Send/Receive so the routing core runs without sockets. Each assertion
// mirrors a branch read from fh2 (CVoipRouterInstance 0x847e50-0x848770); frames are laid out the
// way the shipped code consumes them, from the tail in: [payload][channel/count][sessionId:2][id].

#include <cstdio>
#include <vector>

#include "../BF2/VoipServer/VoipRouter.h"

static int g_checks = 0;
static int g_failures = 0;

static void expect(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

namespace {

typedef std::vector<unsigned char> Bytes;

class TestRouter : public VOIP::CVoipRouterInstance
{
public:
    struct Deliver
    {
        unsigned short from;
        unsigned short to;
        bool loopback;
        std::vector<char> data;
    };
    struct Packet
    {
        unsigned int connId;
        unsigned short clientId;
        std::vector<char> bytes;
    };

    std::vector<Deliver> m_delivered;
    std::vector<Packet> m_incoming;

    char* Buffer() { return m_packetBuffer; }

    virtual VOIP::VRESULT Send(bool loopback, const VOIP::ClientData* from, const VOIP::ClientData* to, char* data, int length)
    {
        Deliver d;
        d.from = from ? from->m_sessionId : 0;
        d.to = to ? to->m_sessionId : 0;
        d.loopback = loopback;
        d.data.assign(data, data + length);
        m_delivered.push_back(d);
        return VOIP::VRESULT_SUCCESS;
    }

    virtual VOIP::VRESULT Receive(unsigned& connId, unsigned short& clientId, char* buffer, int bufferSize, int& length)
    {
        if (m_incoming.empty())
            return VOIP::VRESULT_FAILURE;
        Packet p = m_incoming.front();
        m_incoming.erase(m_incoming.begin());
        connId = p.connId;
        clientId = p.clientId;
        length = static_cast<int>(p.bytes.size());
        if (length > bufferSize)
            length = bufferSize;
        for (int i = 0; i < length; ++i)
            buffer[i] = p.bytes[i];
        return VOIP::VRESULT_SUCCESS;
    }
};

VOIP::ClientData makeClient(unsigned short sessionId, unsigned char channel, unsigned short playerId,
                            unsigned int connId, unsigned short clientId, unsigned char setChannel)
{
    VOIP::ClientData d;
    d.m_sessionId = sessionId;
    d.m_connectionId = connId;
    d.m_clientId = clientId;
    d.m_playerId = playerId;
    d.m_channel = channel;
    Bytes set;
    set.push_back(setChannel);
    d.m_outgoingSets.push_back(set);
    d.m_singleChannel = 0;
    d.m_multi = 0;
    return d;
}

std::vector<char> frame(const Bytes& bytes)
{
    std::vector<char> out;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        out.push_back(static_cast<char>(bytes[i]));
    return out;
}

void push(TestRouter& r, unsigned int connId, unsigned short clientId, const Bytes& bytes)
{
    TestRouter::Packet p;
    p.connId = connId;
    p.clientId = clientId;
    p.bytes = frame(bytes);
    r.m_incoming.push_back(p);
}

} // namespace

int main()
{
    using namespace VOIP;

    TestRouter router;
    router.SetClientData(makeClient(10, 5, 0xAAAA, 1, 1, 5));   // A listens on channel 5
    router.SetClientData(makeClient(20, 5, 0x1234, 2, 2, 5));   // B listens on channel 5 and speaks
    router.SetClientData(makeClient(30, 5, 0xBBBB, 3, 3, 5));   // C listens on channel 5
    expect(router.Start() == VRESULT_SUCCESS, "Start");

    // ---- extract helpers ----
    {
        char buf[] = {'a', 'b', 0x34, 0x12, 7};
        int len = 5;
        unsigned char id = 0;
        expect(router.ExtractPacketId(buf, len, id) == VRESULT_SUCCESS && id == 7 && len == 4,
               "ExtractPacketId pops the last byte");
        unsigned short sid = 0;
        expect(router.ExtractClientSessionId(buf, len, sid) == VRESULT_SUCCESS && sid == 0x1234 && len == 2,
               "ExtractClientSessionId pops the last two bytes");
        len = 1;
        expect(router.ExtractClientSessionId(buf, len, sid) == VRESULT_FAILURE, "ExtractClientSessionId guard");
        len = 0;
        expect(router.ExtractPacketId(buf, len, id) == VRESULT_FAILURE, "ExtractPacketId guard");
    }

    // ---- default channel set: B -> {A, C}, player id appended, sender excluded ----
    {
        push(router, 2, 2, Bytes{'A', 'B', 20, 0, 1});
        router.Update();
        expect(router.m_delivered.size() == 2, "default: delivered to both listeners");
        if (router.m_delivered.size() == 2)
        {
            expect(router.m_delivered[0].from == 20, "default: from is the speaker");
            expect(router.m_delivered[0].to == 10 && router.m_delivered[1].to == 30,
                   "default: destinations are A and C, never B");
            expect(router.m_delivered[0].loopback, "default: loopback flag set");
            const std::vector<char>& d = router.m_delivered[0].data;
            expect(d.size() == 4 && d[0] == 'A' && d[1] == 'B'
                   && static_cast<unsigned char>(d[2]) == 0x34 && static_cast<unsigned char>(d[3]) == 0x12,
                   "default: payload then speaker player id");
        }
        router.m_delivered.clear();
    }

    // ---- alternate channel set: trailing set index not routed ----
    {
        push(router, 2, 2, Bytes{'C', 0, 20, 0, 2});
        router.Update();
        expect(router.m_delivered.size() == 2, "alternate: delivered to both listeners");
        if (!router.m_delivered.empty())
        {
            const std::vector<char>& d = router.m_delivered[0].data;
            expect(d.size() == 3 && d[0] == 'C'
                   && static_cast<unsigned char>(d[1]) == 0x34 && static_cast<unsigned char>(d[2]) == 0x12,
                   "alternate: set index dropped from the payload");
        }
        router.m_delivered.clear();
    }

    // ---- multi channel set: count + set indices dropped ----
    {
        push(router, 2, 2, Bytes{'D', 0, 1, 20, 0, 3});
        router.Update();
        expect(router.m_delivered.size() == 2, "multi: delivered to both listeners");
        if (!router.m_delivered.empty())
        {
            const std::vector<char>& d = router.m_delivered[0].data;
            expect(d.size() == 3 && d[0] == 'D'
                   && static_cast<unsigned char>(d[1]) == 0x34 && static_cast<unsigned char>(d[2]) == 0x12,
                   "multi: count and set index dropped from the payload");
        }
        router.m_delivered.clear();
    }

    // ---- keep-alive: acknowledged, nothing routed ----
    {
        push(router, 2, 2, Bytes{20, 0, 6});
        router.Update();
        expect(router.m_delivered.empty(), "keep-alive routes nothing");
    }

    // ---- unknown speaker: nothing routed ----
    {
        push(router, 9, 9, Bytes{'X', 99, 0, 1});
        router.Update();
        expect(router.m_delivered.empty(), "unknown speaker routes nothing");
    }

    // ---- direct dispatch result codes ----
    {
        char* buf = router.Buffer();
        buf[0] = static_cast<char>(99);
        buf[1] = 0;
        expect(router.HandleIncomingPacket(1, 1, 1, buf, 2) == VRESULT_AUTH_FAILED,
               "HandleIncomingPacket reports an unknown speaker as AUTH_FAILED");
        buf[0] = 20;
        buf[1] = 0;
        expect(router.HandleIncomingPacket(6, 2, 2, buf, 2) == VRESULT_SUCCESS,
               "HandleIncomingPacket acknowledges keep-alive");
    }

    // ---- removal ----
    {
        expect(router.RemoveClient(10) == VRESULT_SUCCESS, "RemoveClient existing");
        expect(router.RemoveClient(10) == VRESULT_FAILURE, "RemoveClient missing");
        push(router, 2, 2, Bytes{'E', 20, 0, 1});
        router.Update();
        expect(router.m_delivered.size() == 1 && router.m_delivered[0].to == 30,
               "after removing A only C receives");
        router.m_delivered.clear();

        expect(router.RemoveAllClients() == VRESULT_SUCCESS, "RemoveAllClients");
        push(router, 2, 2, Bytes{'F', 20, 0, 1});
        router.Update();
        expect(router.m_delivered.empty(), "after RemoveAllClients nothing is routed");
    }

    std::printf("voip router: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
