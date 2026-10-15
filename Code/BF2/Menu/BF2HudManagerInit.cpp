// HUD initialization. [INFERRED] Source split from Menu/BF2HudManager.cpp.
#include "BF2HudManager.h"
#include "HudAwardCatalogue.h"
#if !DEDICATED
#include "HudVoip.h"
#include "Ref2System.h"
#include "Bf2HudBuilder.h"
#include "BfListBoxData.h"
#include "BfEditData.h"
#include "HudQuestionManager.h"
#include "HudScoreboard.h"
#include "HudVoteMap.h"
#include "HudInformationAdapter.h"
#include "HudRadio.h"
#include "HudKitSelection.h"
#include "HudAmmo.h"
#include "HudVehicleInformation.h"
#include "HudLoadingScreen.h"
#include "HudSquad.h"
#include "HudCommander.h"
#include "HudChat.h"
#include "HudMessages.h"
#include "HudColors.h"
#include "../Core/Debug.h"
#include "../Core/EventManager.h"
#include "../External/BFMeme/Data.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../IO/Console/Console.h"
#include "../IO/Input/Input.h"
#include "../IO/Input/FalconDevice.h"
#include <new>

namespace dice { namespace hfe { namespace io { extern IBase* inputManager; } } }
namespace dice { namespace hfe { namespace hud {

namespace {
void runHudScript(const char* path)
{
    io::mainConsole->run(path, "", "", "", "", "", "", "", "", io::Console::ignoredString_);
}
// PE initialized source palettes, distinct from their mutable global copies.
const meme::Color color97dd60 = {0.8156862854957581f,0.8156862854957581f,0.8156862854957581f,1};
const meme::Color color97dd70 = {0.4470588266849518f,0.8901960849761963f,0.3450980484485626f,1};
const meme::Color color97dd80 = {0.0117647061124444f,0.7215686440467834f,0.9490196108818054f,1};
const meme::Color color97dd90 = {0.9686274528503418f,0.8274509906768799f,0.2549019753932953f,1};
const meme::Color color97e1a0 = {1,1,1,1};
const meme::Color color97def0 = {0.0313725508749485f,0.46666666865348816f,0.007843137718737125f,1};
const meme::Color color97de60 = {0.7f,0.7f,0.7f,1};
}

bool BF2HudManager::init()
{
    void* storage=::operator new(sizeof(menu::Ref2System));
    ref2System = storage ? new(storage) menu::Ref2System(800.0f, 600.0f) : nullptr;
    if (!ref2System) {
        Debug(DEBUGTYPE_6, "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Menu\\BF2HudManager.cpp", 911, "") << "BF2HudManager init fails";
        return false;
    }
    if (!ref2System->init()) {
        Debug(DEBUGTYPE_6, "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Menu\\BF2HudManager.cpp", 918, "") << "BF2HudManager init fails";
        return false;
    }
    runHudScript("Menu/Atlas/AtlasList.con");
    if (m_field33c.empty()) addTextureAtlas("MemeAtlas.tai");

    storage=::operator new(sizeof(HudQuestionManager));
    m_field100 = storage ? new(storage) HudQuestionManager : nullptr;
    storage=::operator new(sizeof(Bf2HudBuilder));
    m_field0fc = storage ? new(storage) Bf2HudBuilder : nullptr;
    storage=::operator new(sizeof(HudScoreboard));
    m_field120 = storage ? new(storage) HudScoreboard(ref2System) : nullptr;
    storage=::operator new(sizeof(HudVoip));
    m_field124 = storage ? new(storage) HudVoip(ref2System) : nullptr; // 756FC2, ctor 7A5170
    storage=::operator new(sizeof(HudVoteMap));
    m_field128 = storage ? new(storage) HudVoteMap(nullptr) : nullptr;
    storage=::operator new(sizeof(HudInformationAdapter));
    m_field104 = storage ? new(storage) HudInformationAdapter : nullptr;
    storage=::operator new(sizeof(HudRadio));
    m_field108 = storage ? new(storage) HudRadio : nullptr;
    storage=::operator new(sizeof(HudRadio));
    m_field10c = storage ? new(storage) HudRadio : nullptr;
    storage=::operator new(sizeof(HudRadio));
    m_field114 = storage ? new(storage) HudRadio : nullptr;
    storage=::operator new(sizeof(HudRadio));
    m_field110 = storage ? new(storage) HudRadio : nullptr;
    storage=::operator new(sizeof(HudRadio));
    m_field118 = storage ? new(storage) HudRadio : nullptr;
    storage=::operator new(sizeof(HudKitSelection));
    m_field11c = storage ? new(storage) HudKitSelection : nullptr;
    storage=::operator new(sizeof(HudAmmo));
    hudAmmo = storage ? new(storage) HudAmmo(nullptr) : nullptr;
    storage=::operator new(sizeof(HudVehicleInformation));
    hudVehicleInformation = storage ? new(storage) HudVehicleInformation : nullptr;
    storage=::operator new(sizeof(HudLoadingScreen));
    m_field134 = storage ? new(storage) HudLoadingScreen(ref2System) : nullptr;
    storage=::operator new(sizeof(HudSquad));
    m_field138 = storage ? new(storage) HudSquad(ref2System) : nullptr;
    storage=::operator new(sizeof(HudCommander));
    m_field13c = storage ? new(storage) HudCommander(ref2System) : nullptr;
    storage=::operator new(sizeof(HudChat));
    m_field140 = storage ? new(storage) HudChat(ref2System) : nullptr;
    storage=::operator new(sizeof(HudChat));
    hudChat = storage ? new(storage) HudChat(ref2System) : nullptr;
    storage=::operator new(sizeof(HudMessages));
    m_field148 = storage ? new(storage) HudMessages : nullptr;
    storage=::operator new(sizeof(HudAwardCatalogue));
    m_field14c = storage ? new(storage) HudAwardCatalogue : nullptr;
    disablePointer();
    io::g_falconMouseMode = 1;
    static_cast<io::IInputManager*>(io::inputManager)->registerInputFilter(this);
    eventManager->registerEventHandler(static_cast<EventCategory>(2), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(1), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(0), this, 0);
    eventManager->registerEventHandler(static_cast<EventCategory>(10), this, 0);
    if (m_field0fc) m_field0fc->setSystem(ref2System);
    if (m_field148) m_field148->init();
    if (hudAmmo) hudAmmo->setSystem(ref2System);

    if (hudChat) {
        hudChat->init("ingameChat",16,500);
        hudChat->setCapacity(0,4);
        hudChat->setCapacity(1,5);
        hudChat->setCapacity(7,3);
        hudChat->setCapacity(8,2);
        hudChat->setSpacing(0,4);
        hudChat->setSpacing(7,12);
        hudChat->setSpacing(8,9);
        hudChat->rebuildRows();
        hudChat->setDuration(6,15.0f);
        hudChat->setDuration(0,15.0f);
        hudChat->setDuration(1,20.0f);
        hudChat->setDuration(2,15.0f);
        hudChat->setDuration(3,15.0f);
        hudChat->setDuration(4,15.0f);
        hudChat->setDuration(5,15.0f);
        hudChat->setDuration(7,15.0f);
        hudChat->setDuration(8,500.0f);
        // Each call reloads the owner and each mutable palette element.
        auto color = [this](int kind,int variant,meme::Color value) {
            hudChat->setColor(kind,variant,value.r,value.g,value.b,value.a);
        };
        for (int variant=0; variant!=3; ++variant) color(6,variant,color97dd70);
        for (int variant=0; variant!=3; ++variant) color(3,variant,categoryThreeChatColors[variant]);
        for (int variant=0; variant!=3; ++variant) color(0,variant,defaultChatColors[variant]);
        for (int variant=0; variant!=3; ++variant) color(4,variant,{0.95703125f,0.7109375f,0.046875f,1});
        for (int variant=0; variant!=3; ++variant) color(5,variant,{0.95703125f,0.7109375f,0.046875f,1});
        for (int variant=0; variant!=3; ++variant) color(7,variant,color97dd60);
        color(8,0,color97dd60);
        color(8,1,color97dd70);
        color(8,2,color97dd90);
        color(8,3,color97dd80);
        if (meme::BfListBoxData* data=hudChat->getData()) data->m_field021=false;
    }
    if (m_field140) {
        m_field140->init("commanderChat",2,220);
        m_field140->setHistoryOutput(false);
        m_field140->setCapacity(0,9);
        m_field140->setCapacity(1,5);
        m_field140->rebuildRows();
        for (int kind : {0,1,2,5,6,4,7,3,8}) m_field140->setDuration(kind,100.0f);
        auto color = [this](int kind,int variant,meme::Color value) {
            m_field140->setColor(kind,variant,value.r,value.g,value.b,value.a);
        };
        for (int kind : {6,3,0,4,5})
            for (int variant=0; variant!=3; ++variant)
                color(kind,variant,{0.549f,0.733f,0.8309f,1});
        for (int variant=0; variant!=3; ++variant) color(7,variant,{1,1,1,1});
        color(8,0,color97e1a0);
        color(8,1,color97def0);
        color(8,2,color97de60);
        if (meme::BfListBoxData* data=m_field140->getData()) data->m_field021=false;
    }
    if (m_field124) m_field124->setSystem(ref2System);
    if (m_field128) m_field128->setSystem(ref2System);
    if (m_field104) m_field104->setSystem(ref2System);
    if (m_field108) m_field108->configure("Radio",11);
    if (m_field10c) m_field10c->configure("RadioVehicle",11);
    if (m_field114) m_field114->configure("SquadLeader",11);
    if (m_field110) m_field110->configure("Squad",11);
    if (m_field118) m_field118->configure("Commander",11);
    if (hudVehicleInformation) hudVehicleInformation->setSystem(ref2System);
    if (m_field134) m_field134->registerData();
    registerHudFunctions();
    registerHudData();
    registerHudEvents();
    if (m_field0fc) {
        ref2System->loadMenu("Menu/InGame",nullptr);
        runHudScript("Menu/HUD/HudSetup/HudSetupMain.con");
        runHudScript("Menu/Nametag/nametags.con");
        const meme::TextureHandle texture=ref2System->loadTexture("Ingame/GeneralIcons/full.tga",true);
        ref2System->setDefaultTexture(texture);
        hudMap=m_field0fc->m_field008;
        // Retail _Tidy releases all storage, not just the logical elements.
        std::vector<HudPendingRequest>().swap(m_field32c);
    }
    return true;
}

bool BF2HudManager::registerHudFunctions()
{
    if (m_field104) m_field104->registerFunctions();
    if (m_field120) m_field120->registerFunctions();
    if (m_field124) m_field124->registerFunctions();
    if (m_field128) m_field128->registerFunctions();
    if (m_field138) m_field138->registerFunctions();
    if (m_field13c) m_field13c->registerFunctions();
    return true;
}

bool BF2HudManager::registerHudData()
{
    if (m_field104) m_field104->registerData();
    if (m_field120) m_field120->registerData();
    if (m_field124) m_field124->registerData();
    if (m_field128) m_field128->registerData();
    if (hudAmmo) hudAmmo->registerData();
    if (hudVehicleInformation) hudVehicleInformation->init();
    if (m_field138) m_field138->registerData();
    if (m_field13c) m_field13c->registerData();
    if (m_field148) m_field148->registerData();
    m_field08c="";
    void* storage=::operator new(sizeof(meme::StringData));
    m_field0a8[0]=storage ? new(storage) meme::StringData(m_field08c.c_str(),"ChatBoxString") : nullptr;
    storage=::operator new(sizeof(meme::StringData));
    m_field0a8[1]=storage ? new(storage) meme::StringData(m_field08c.c_str(),"CommanderChatBoxString") : nullptr;
    storage=::operator new(sizeof(meme::BfEditData));
    m_field0b4[0]=storage ? new(storage) meme::BfEditData("SayData") : nullptr;
    storage=::operator new(sizeof(meme::BfEditData));
    m_field0b4[1]=storage ? new(storage) meme::BfEditData("CommanderSayData") : nullptr;
    storage=::operator new(sizeof(meme::BfEditData));
    m_field0e0=storage ? new(storage) meme::BfEditData("SquadRenameData") : nullptr;
    m_field0c0="";
    // Retail clears0C0 but deliberately reads08C for this constructor.
    storage=::operator new(sizeof(meme::StringData));
    m_field0dc=storage ? new(storage) meme::StringData(m_field08c.c_str(),"SquadRenameString") : nullptr;
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerInt("HudState",&m_field250);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("DisconnectMessageActive",&m_field0bd);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("PauseMessageActive",&m_field0be);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("CommanderTracking",&m_field2b0);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SetSpawnPoint",&m_field228);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("CommanderRightNameColorRed",&m_field350);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("CommanderRightNameColorGreen",&m_field358);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerFloat("CommanderRightNameColorBlue",&m_field354);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("InvalidVehicleDropArea",&m_field35c);
    return true;
}
} } }
#endif
