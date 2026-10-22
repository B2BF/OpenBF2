// Result code shared by the BF2VoipServer facade and the VOIP routing library.
//
// The module returns VOIP::VRESULT from nearly every entry point. Values below are attested from
// fh2 returns; the symbolic names are [INFERRED] because the retail enum itself is not recovered.
// Do not invent additional members: the facade uses only 1 and -4, ReleaseInstance and the
// Extract* helpers use -1, HandleIncomingPacket uses -7, and the router's Update/HandleIncomingPacket
// also use -4.

#ifndef BF2_VOIPSERVER_VOIPRESULT_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPRESULT_H

namespace VOIP {

enum VRESULT
{
    VRESULT_SUCCESS       =  1,   // completed
    VRESULT_FAILURE       = -1,   // generic failure (instance not found, empty extract buffer)
    VRESULT_INVALID_STATE = -4,   // illegal lifecycle call; the facade's debug messages
    VRESULT_AUTH_FAILED   = -7    // BF-server packet rejected during authentication
};

} // namespace VOIP

#endif // BF2_VOIPSERVER_VOIPRESULT_H
