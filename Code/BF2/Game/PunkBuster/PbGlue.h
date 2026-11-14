// PunkBuster integration: the game side of the Even Balance SDK boundary. [INFERRED] path and file split.
//
// The client glue (BF2.exe 0x435900..0x436434, PbClient.cpp) owns the pbcl.dll instance at ds:0x96E5A8:
// it loads the vendor DLL dynamically (LoadLibraryA / GetProcAddress "ca"/"cb"), hands it the game
// callbacks, forwards events and pumps it once a frame. The BF2-side PB_* functions (BF2.exe
// 0x4D94D0..0x4DA49B, PbGame.cpp) answer the DLL's queries. Without pbcl.dll (and on every platform but
// Windows, where no vendor DLL exists) the glue takes retail's not-installed branch.
//
// The server glue (Game/PunkBuster/PbServer.cpp, both images) owns the pbsv instance (BF2.exe
// ds:0x96E758, ELF ds:0x8AC82A0) the same way, with entry points "sa"/"sb". The BF2-side PB_* functions
// serve both glues; the dedicated image carries the server half only.
#ifndef GAME_PUNKBUSTER_PBGLUE_H
#define GAME_PUNKBUSTER_PBGLUE_H

// ---- client glue (Game/PunkBuster/PbClient.cpp; client image only) -------------------------------
void  PbClientInitialize(void* instance);              // 0x436060
void  PbClientConnect();                               // 0x436220 [INFERRED] name: event 0x75 on join
char* PbClAddEvent(int type, int length, char* data);  // 0x436000
void  PbClientTrapConsole(char* command, int length);  // 0x436020
void  PbClientProcessEvents();                         // 0x436150

// ---- server glue (Game/PunkBuster/PbServer.cpp; both images; BF2.exe / ELF addresses) -----------
void  PbServerInitialize();                            // 0x437160 / 0x84F6170
void  PbServerTrapConsole(char* command, int length);  // 0x436E70 / 0x84F6140
void  PbServerProcessEvents();                         // 0x436E90 / 0x84F61F0
void  PbServerForceProcess();                          // 0x436EA0 / 0x84F6210
char* PbSvAddEvent(int type, int clientIndex, int length, char* data);  // 0x436D60 / 0x84F6020
void  PbSvAddStatEvent(int clientIndex, char* first, char* second);     // 0x436D90 / 0x84F6060
void  PbServerCompleteCommand(char* text, int length); // 0x436EB0 / 0x84F6230
void  PbPassConnectString(char* first, char* second);  // 0x436A70 / 0x84F6270
char* PbAuthClient(char* address, int flag, char* keyHash);  // 0x436A90 / 0x84F62A0
void  PbCaptureConsoleOutput(char* text);              // 0x436AC0 / 0x84F63B0
extern "C" bool isPbSvEnabled();                       // 0x436ED0 / 0x84F62E0
void  EnablePbSv();                                    // 0x436F40 / 0x84F6330
void  DisablePbSv();                                   // 0x436FC0 / 0x84F6370
void* PbServerGlueObject();                            // &pbsv, which the client glue records

// ---- BF2-side game functions (Game/PunkBuster/PbGame.cpp; both images unless marked) ------------
// BF2.exe / ELF addresses.
char* PB_basepath();                                   // 0x4D94D0 / 0x84F7340
char* PB_homepath();                                   // 0x4D9500 / 0x84F7370
void  PB_set_cl_punkbuster(int value);                 // 0x4D9530 / 0x84F7490
void  PB_set_sv_punkbuster(int value);                 // 0x4D95B0 / 0x84F7560
int   PB_maxClients();                                 // 0x4D9610 / 0x84F75F0
int   PB_getPlayerInfo(int index, char* info);         // 0x4D9620 / 0x84F7610
void  PB_outputText(char* text);                       // 0x4D97E0 / 0x84F7830
void  PB_issueConsoleCommand(char* command);           // 0x4D9890 / 0x84F7950
void  PB_DropClient(int index, char* reason);          // 0x4D98F0 / 0x84F7A00
void  PB_sv_sendToClient(int index, char* data, int length);  // 0x4D9950 / 0x84F7AA0
void  PB_cl_sendToServer(char* data, int length);      // 0x4D9B30, client image only [INFERRED] name
void  PB_sendUDPPacket(char* address, unsigned short port, char* data, int length);  // 0x4D9CF0 / 0x84F7E00
char* PB_getVersion();                                 // 0x4D9D60 / 0x84F7EB0
int   PB_GlQuery(char*);                               // 0x4D9DD0 / 0x84F88A0 [INFERRED] identity: returns 0
void  PB_handlePacket(char* data, int length, const char* from);  // 0x4D9DE0 / 0x84F88B0
char* PB_examineSetting(char* name);                   // 0x4D9F80 / 0x84F7F50
bool  PB_get_sv_punkbusterInstalled();                 // 0x4D9F40 / 0x84F8A10

// The console pre-dispatch hook BF2Engine::init registers (BF2.exe 0x402FD0): PB_SV_* commands go to
// the server glue, other PB_* commands to the client glue; true for any PB_ command.
// [CHOSEN, not recovered] placement: the ELF has it in BF2Engine.cpp's anonymous namespace; it lives
// here so the routing test can link it without the engine.
bool HandleCommandHookPB(const char* command, int length);

#endif
