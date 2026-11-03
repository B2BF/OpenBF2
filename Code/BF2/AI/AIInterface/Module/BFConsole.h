// Console configuration and lifecycle of the engine-side AI adapters.

#ifndef AI_AIINTERFACE_MODULE_BFCONSOLE_H
#define AI_AIINTERFACE_MODULE_BFCONSOLE_H

#include <map>
#include <list>
#include <string>

namespace dice {
namespace hfe {
class ConsoleObject;
namespace ai {

// Recovered subset of this TU's generated console list, registered by AIInterfaceModule.
std::list<ConsoleObject*>& kitTemplateConsoleObjects();

class AIEventListener;

// Radio-message values are four-byte integers; the constructor maps names to values
// 0..4 and 6..17. [UNRESOLVED] Enumerator names are unknown, so the enum remains opaque.
enum AIRadioMessageType : int;

// Settings for an AI radio message; preserve the field order used by the courier.
struct AIRadioMessageSettings
{
    /*  0 */ unsigned char m_opaque0[4];    // [UNRESOLVED]
    /*  4 */ float m_modifier;              // AIRadioMessages.ai column 1
    /*  8 */ float m_maxSendPeriod;         // AIRadioMessages.ai column 2
    /* 12 */ float m_receiveTimeToLive;     // AIRadioMessages.ai column 3
};

class BFConsole
{
public:
    BFConsole();
    ~BFConsole();

    void createSingletons();
    void clearSingletons();
    void aiMainInit();
    void reset();
    void addAIRadioMessage(const std::string& name, float a, float b, float c);
    bool getAIMessageSettings(int, AIRadioMessageSettings&);
    bool getVoiceFromAiMessageType(AIRadioMessageType, int&);
    // Emits the retail error diagnostic, then processes a legacy single-player code.
    std::string code(const std::string& command);

    // Always returns null.
    static void* getSinglePlayerInstance();

    // AIInterfaceModule::reset writes the recovered byte at +0x01 directly before asking the
    // connection and console to reset, and clearSingletons is what consumes it. Kept as a setter so
    // that call site does not reach into the object.
    void setSkipBulkClear(bool value) { m_skipBulkClear = value; }

    static BFConsole* m_instance;           // ds:

private:
    /* +0x00 */ unsigned char m_unknown0;   // [UNRESOLVED] - the constructor never writes it
    /* +0x01 */ bool m_skipBulkClear;
    /* +0x02 */ bool m_singletonsCreated;
    /* +0x03 */ // padding

    // Owned listener created by createSingletons and destroyed and cleared by clearSingletons.
    /* +0x04 */ AIEventListener* m_aiEventListener;

    // [CHOSEN] Member names for the radio message, settings, and voice registries.
    /* +0x08 */ std::map<std::string, AIRadioMessageType>           m_radioMessageTypes;
    /* +0x14 */ std::map<unsigned char, AIRadioMessageSettings>     m_messageSettings;
    /* +0x20 */ std::map<AIRadioMessageType, unsigned char>         m_messageVoices;
};

// DLL-facing interface to BFConsole. Keep the destructor before the six forwarding
// methods to preserve virtual slot order across ports.
class IBFModuleCourier
{
public:
    IBFModuleCourier();
    virtual ~IBFModuleCourier();
    virtual void initSingletons() = 0;
    virtual void clearSingletons() = 0;
    virtual void aiMainInit() = 0;
    virtual void reset() = 0;
    virtual bool getAIMessageSettings(int, AIRadioMessageSettings&) = 0;
    virtual bool getVoiceFromAiMessageType(AIRadioMessageType, int&) = 0;
};

class BFModuleCourier : public IBFModuleCourier
{
public:
    BFModuleCourier();
    virtual ~BFModuleCourier();
    virtual void initSingletons();
    virtual void clearSingletons();
    virtual void aiMainInit();
    virtual void reset();
    virtual bool getAIMessageSettings(int, AIRadioMessageSettings&);
    virtual bool getVoiceFromAiMessageType(AIRadioMessageType, int&);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIINTERFACE_MODULE_BFCONSOLE_H
