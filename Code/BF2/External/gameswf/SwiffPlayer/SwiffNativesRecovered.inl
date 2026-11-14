// ===================================================================================================
// c-swiff-natives: the retail natives the port had not registered.
// SwiffPlayer.dll 1.50 (base 0x10000000); every body below was read instruction by instruction with
// objdump. Calling convention of the shipped callbacks: ecx = the result as_value, [ebp+8] = the
// argument vector, [ebp+0x10] = first_arg; argument n is element first_arg - n (fn.arg(n) here); the
// int3 after the bounds test is the debug assert and has no counterpart. Result writes build a
// temporary as_value (type 1 string, 2 number, 3 Boolean) and assign it through sub_10009920.
// ===================================================================================================

// ---- dice.bf2.Profile --------------------------------------------------------------------------

// sub_1005D050 "getMinimumAge": the shipped body is a bare `ret 0Ch`.
// The AS result is left untouched (undefined).
void as_profile_getMinimumAge(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getMinimumAge");
}

// sub_1005D080 "clearComparePlayer": player host +0x218 setPendingRecord(0, "") with a constructed
// empty string (0x100D7835). No AS result.
void as_profile_clearComparePlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_clearComparePlayer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string empty("");
    h->setPendingRecord(0, empty);                                                          // +0x218
}

// sub_1005D110 "setNamePrefix": GlobalSettings by name first, then arg 0 to_string (sub_1008D0F0)
// into a std::string, copied by value into GlobalSettings +0x2C setNamePrefix. No AS result.
void as_profile_setNamePrefix(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setNamePrefix");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GlobalSettings"));
    const std::string prefix(fn.arg(0).to_string());
    static_cast<IGlobalSettings*>(gs)->setNamePrefix(prefix);                               // +0x2C
}

// sub_1005D190 "setSelectedAge": arg 0 to_number (sub_1008B570) and the value popped (fstp st0).
// Nothing is stored and no AS result is written.
void as_profile_setSelectedAge(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setSelectedAge");
    (void) fn.arg(0).to_number();
}

// sub_1005D220 "setUseAdvancedServerBrowser": GeneralSettings by name, then arg 0 to_bool
// (sub_1008B6B0), GeneralSettings +0xA4. No AS result.
void as_profile_setUseAdvancedServerBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setUseAdvancedServerBrowser");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const bool value = fn.arg(0).to_bool();
    static_cast<IGeneralSettings*>(gs)->setUseAdvancedServerBrowser(value);                 // +0xA4
}

// sub_1005D350 "setComparePlayer": the host first, then ProfileManager +0x44 isLoggedIn. Logged out:
// host +0x244 handleProfileTransition("connection to profile server lost") and return. Otherwise
// id = (int) arg 0 (fistp, truncating control word); a zero id does nothing. A non-zero id takes
// arg 1 to_string, host +0x218 setPendingRecord(id, name), host +0x220 setSelectedPlayerId(id).
void as_profile_setComparePlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setComparePlayer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* pm = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    if (!static_cast<IProfileManager*>(pm)->isLoggedIn())                                    // +0x44
    {
        h->handleProfileTransition(std::string("connection to profile server lost"));        // +0x244
        return;
    }
    const int id = bf2x87::toIntTruncate(fn.arg(0).to_number());
    if (id == 0)
        return;
    const std::string name(fn.arg(1).to_string());
    h->setPendingRecord(id, name);                                                          // +0x218
    h->setSelectedPlayerId(id);                                                             // +0x220
}

// sub_1005D5B0 "getSelectedAge": the constant 13.0 (0x100DDEC8) as a number.
void as_profile_getSelectedAge(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getSelectedAge");
    fn.result->set_double(13.0);
}

// sub_1005D800 "deleteCurrentProfile": inside the registry SwiffHost's busy cursor (sub_10013520);
// player host, LocalProfileManager, ProfileManager. Logged out (+0x44): host +0x244 with "connection
// to profile server lost" and Boolean false. Logged in: ProfileManager +0x30 getProfileId(out) (its
// Boolean ignored) is compared with host +0x21C getSelectedPlayerId; a mismatch posts message
// {type 1, code 0, "you can only delete your active profile"} through host +0x1E4 and answers false.
// A match calls ProfileManager +0x38 deleteProfile: false answers false; true reads +0x34
// getUniqueNick(out) into an empty string, LocalProfileManager +0x68 setGamespyNick(it), +0x40
// saveCurrentProfile, and answers true.
void as_profile_deleteCurrentProfile(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_deleteCurrentProfile");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    IBase* pmBase = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    IProfileManager* pm = static_cast<IProfileManager*>(pmBase);
    if (!pm->isLoggedIn())                                                                  // +0x44
    {
        h->handleProfileTransition(std::string("connection to profile server lost"));        // +0x244
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    int profileId;                                     // uninitialised in the shipped frame as well
    (void) pm->getProfileId(profileId);                                                     // +0x30
    if (h->getSelectedPlayerId() != profileId)                                              // +0x21C
    {
        const std::string text("you can only delete your active profile");
        SwiffHostMessage message;
        message.type = 1;
        message.code = 0;
        message.text = text;
        (void) h->postMessage(message);                                                     // +0x1E4
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    if (!pm->deleteProfile())                                                               // +0x38
    {
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    std::string nick;
    (void) pm->getUniqueNick(nick);                                                         // +0x34
    static_cast<ILocalProfileManager*>(lpm)->setGamespyNick(nick);                          // +0x68
    (void) static_cast<ILocalProfileManager*>(lpm)->saveCurrentProfile();                   // +0x40
    fn.result->set_bool(true);
    busyClose(busy);
}

// sub_1005DA70 "newGamespyProfile": inside the busy cursor; player host, LocalProfileManager,
// ProfileManager. Logged out: host +0x244 "connection to profile server lost", Boolean false.
// Logged in: nick = arg 0, country = arg 1 (both to_string); ProfileManager +0x20
// createProfile(nick, country). Refused: +0x4C requestNickSuggestions(nick), Boolean false.
// Created: LocalProfileManager +0x68 setGamespyNick(nick), +0x40 saveCurrentProfile, Boolean true.
void as_profile_newGamespyProfile(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_newGamespyProfile");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    IProfileManager* pm = static_cast<IProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("ProfileManager")));
    if (!pm->isLoggedIn())                                                                  // +0x44
    {
        h->handleProfileTransition(std::string("connection to profile server lost"));        // +0x244
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    const std::string nick(fn.arg(0).to_string());
    const std::string country(fn.arg(1).to_string());
    if (!pm->createProfile(nick, country))                                                  // +0x20
    {
        (void) pm->requestNickSuggestions(nick);                                            // +0x4C
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    static_cast<ILocalProfileManager*>(lpm)->setGamespyNick(nick);                          // +0x68
    (void) static_cast<ILocalProfileManager*>(lpm)->saveCurrentProfile();                   // +0x40
    fn.result->set_bool(true);
    busyClose(busy);
}

// sub_1005DCE0 "deleteLocalProfile": inside the busy cursor; the player host is fetched and
// discarded; LocalProfileManager; index = (int) arg 0; Boolean of +0x28 deleteProfile(index).
void as_profile_deleteLocalProfile(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_deleteLocalProfile");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    const bool deleted = static_cast<ILocalProfileManager*>(lpm)->deleteProfile(index);    // +0x28
    fn.result->set_bool(deleted);
    busyClose(busy);
}

// sub_1005DDD0 "getUseAdvancedServerBrowser": Boolean of GeneralSettings +0xA0.
void as_profile_getUseAdvancedServerBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getUseAdvancedServerBrowser");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    fn.result->set_bool(static_cast<IGeneralSettings*>(gs)->getUseAdvancedServerBrowser()); // +0xA0
}

// sub_1005E100 "getStatPlayer": host +0x21C getSelectedPlayerId as a number.
void as_profile_getStatPlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getStatPlayer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(static_cast<double>(h->getSelectedPlayerId()));                  // +0x21C
}

// sub_1005E2A0 "searchPlayer" / sub_10060D80 "searchUniquePlayer": inside the busy cursor; the
// player host, arg 0 to_string. An empty name or one whose first byte is ' ' fails. Otherwise host
// +0x3C getSearchForPlayer() +0x10 search(name, unique, false). searchPlayer answers that Boolean
// (false on the early refusal). searchUniquePlayer answers a number: -1.0 (0x100D8868) on a refusal
// or an empty result, else the first row's profile id from a second getSearchForPlayer() +0x18
// getPlayers(rows) (its Boolean ignored).
static bool searchNameUsable(const std::string& name)
{
    return !name.empty() && name.c_str()[0] != ' ';
}
void as_profile_searchPlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_searchPlayer");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    if (searchNameUsable(name)
        && static_cast<ISearchForPlayer*>(h->getSearchForPlayer())->search(name, false, false)) // +0x3C, +0x10
        fn.result->set_bool(true);
    else
        fn.result->set_bool(false);
    busyClose(busy);
}
void as_profile_searchUniquePlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_searchUniquePlayer");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    if (searchNameUsable(name)
        && static_cast<ISearchForPlayer*>(h->getSearchForPlayer())->search(name, true, false))  // +0x3C, +0x10
    {
        std::vector<ISearchForPlayer::Player> rows;
        (void) static_cast<ISearchForPlayer*>(h->getSearchForPlayer())->getPlayers(rows);    // +0x3C, +0x18
        if (rows.empty())
            fn.result->set_double(-1.0);
        else
            fn.result->set_double(static_cast<double>(rows[0].profileId));
    }
    else
        fn.result->set_double(-1.0);
    busyClose(busy);
}

// sub_1005F4A0 "getSprint": team = (int) arg 0, kit = (int) arg 1 (both converted before the lookup),
// MenuTeamManager by name, +0x30 getTeam(team), kits[kit] (no bounds test, 108-byte rows, +0x58):
// the number 1.0f (0x100D7EA0) - sprintStaminaDissipationFactor, evaluated on the x87 stack.
void as_profile_getSprint(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getSprint");
    const int team = bf2x87::toIntTruncate(fn.arg(0).to_number());
    const int kit  = bf2x87::toIntTruncate(fn.arg(1).to_number());
    IMenuTeamManager* tm = static_cast<IMenuTeamManager*>(
        classManager->getSingleton(renddx9abi::HostString("MenuTeamManager")));
    MenuTeam* row = tm->getTeam(static_cast<unsigned int>(team));                          // +0x30
    const double factor = static_cast<double>(row->kits[kit].sprintStaminaDissipationFactor);
    fn.result->set_double(1.0 - factor);
}

// sub_1005FD90 "getNumOnlineProfiles" / sub_1005FEE0 "getNumOfflineProfiles": LocalProfileManager by
// name; absent: the number 0. Present: +0x48 getLocalProfiles(rows, true) (Boolean ignored) and the
// count of rows whose gamespyNick (+0x54) is != "" (online, operator!=) or == "" (offline, operator==).
static void countLocalProfiles(const gameswf::fn_call& fn, bool online)
{
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    if (!lpm)
    {
        fn.result->set_double(0.0);
        return;
    }
    std::vector<LocalProfile> rows;
    (void) static_cast<ILocalProfileManager*>(lpm)->getLocalProfiles(rows, true);           // +0x48
    int count = 0;
    for (unsigned int i = 0; i < rows.size(); ++i)
        if (online ? (rows[i].gamespyNick != "") : (rows[i].gamespyNick == ""))
            ++count;
    fn.result->set_double(static_cast<double>(count));
}
void as_profile_getNumOnlineProfiles(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNumOnlineProfiles");
    countLocalProfiles(fn, true);
}
void as_profile_getNumOfflineProfiles(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNumOfflineProfiles");
    countLocalProfiles(fn, false);
}

// ---- dice.bf2.Multiplay ------------------------------------------------------------------------

// The player host's current browser list: host +0x134 getServerListKind() (a temporary that dies
// after the call) handed to +0x7C getServerList. Every Multiplay row below that touches the
// browser starts this way.
static IServerBrowser* currentServerList(ISwiffHost* h)
{
    return h->getServerList(h->getServerListKind());                                        // +0x134, +0x7C
}

// sub_10050B70 "refreshBrowser": the current list; when present, +0x2C requeueRecords (Boolean
// ignored). No AS result.
void as_multiplay_refreshBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_refreshBrowser");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list = currentServerList(h);
    if (list)
        (void) list->requeueRecords();                                                      // +0x2C
}

// sub_10050C10 "stopDownloadDemo": host +0x44 getDemoLibrary() +0x3C cancelDownload (Boolean
// ignored). No AS result.
void as_multiplay_stopDownloadDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_stopDownloadDemo");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) static_cast<IDemoLibrary*>(h->getDemoLibrary())->cancelDownload();              // +0x44, +0x3C
}

// sub_10050C80 "clearHistory": inside the busy cursor, the player host (discarded) and
// GeneralSettings +0x154 clearServerHistory. No AS result.
void as_multiplay_clearHistory(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_clearHistory");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    static_cast<IGeneralSettings*>(gs)->clearServerHistory();                              // +0x154
    busyClose(busy);
}

// sub_10051480 "setHighestPing": the current list; when present, (int) arg 0 into +0x5C setMaxPing.
// An absent list leaves the argument unread. No AS result.
void as_multiplay_setHighestPing(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setHighestPing");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list = currentServerList(h);
    if (list)
        list->setMaxPing(bf2x87::toIntTruncate(fn.arg(0).to_number()));                     // +0x5C
}

// sub_100519C0 "setFilter": the player host is fetched and discarded; arg 0 to_string is assigned to
// the DLL-global free-text clause (unk_100F8810). No AS result.
void as_multiplay_setFilter(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setFilter");
    (void) g_swiffPlayerInstance->getSwiffHost();
    const std::string text(fn.arg(0).to_string());
    s_textFilter = text;
}

// sub_10051A20 "sort": inside the busy cursor; the player host; key = arg 0 to_string, ascending =
// arg 1 to_bool, mode = (int) arg 2, all converted before the list is fetched; then the current
// list's +0x38 sort(key, ascending, mode), no null test. No AS result.
void as_multiplay_sort(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_sort");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string key(fn.arg(0).to_string());
    const bool ascending = fn.arg(1).to_bool();
    const int mode = bf2x87::toIntTruncate(fn.arg(2).to_number());
    currentServerList(h)->sort(key, ascending, mode);                                       // +0x38
    busyClose(busy);
}

// sub_10051F00 "deleteDemo": the player host first, then (int) arg 0, host +0x44 getDemoLibrary()
// +0x20 removeEntry(index) (Boolean ignored). No AS result.
void as_multiplay_deleteDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_deleteDemo");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    (void) static_cast<IDemoLibrary*>(h->getDemoLibrary())->removeEntry(static_cast<unsigned int>(index)); // +0x20
}

// sub_100523E0 "hasDemoDownloadFailed": Boolean of host +0x44 getDemoLibrary() +0x4C.
void as_multiplay_hasDemoDownloadFailed(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_hasDemoDownloadFailed");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(static_cast<IDemoLibrary*>(h->getDemoLibrary())->hasDownloadFailed()); // +0x4C
}

// sub_10052450 "getScrollFadeSpeed": host +0x314 (the credits scroller's fade speed) as a number.
void as_multiplay_getScrollFadeSpeed(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getScrollFadeSpeed");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(static_cast<double>(h->getIntF08()));                             // +0x314
}

// sub_100526A0 "isServerBrowserBusy": Boolean of the current list's +0x1C isBrowsing, no null test.
void as_multiplay_isServerBrowserBusy(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_isServerBrowserBusy");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const bool busy = currentServerList(h)->isBrowsing();                                   // +0x1C
    fn.result->set_bool(busy);
}

// sub_100528A0 "validateIp": arg 0 to_string, Boolean of host +0x250 isDottedQuad.
void as_multiplay_validateIp(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_validateIp");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string address(fn.arg(0).to_string());
    fn.result->set_bool(h->isDottedQuad(address));                                          // +0x250
}

// sub_10052AF0 "playNow": the player host; both in-game server-info keys cleared (sub_100136B0:
// unk_100F7040 = "", dword_100F3240 = 0; sub_100626D0: unk_100F8B14 = "", dword_100F88CC = 0); host
// +0xB8 isGameRunning(&running) with running pre-set false. A running game returns with the AS
// result untouched. Otherwise, inside the busy cursor, the Boolean of host +0x16C playNow.
void as_multiplay_playNow(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_playNow");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    s_ingameAddressA = ""; s_ingamePortA = 0;
    s_ingameAddressB = ""; s_ingamePortB = 0;
    bool running = false;
    (void) h->isGameRunning(&running);                                                      // +0xB8
    if (running)
        return;
    BusyHost busy = busyOpen();
    fn.result->set_bool(h->playNow());                                                      // +0x16C
    busyClose(busy);
}

// "%s %d" (0x100DD398) of a GeneralSettings row's address and query port, the key the favourites
// and history rows are matched by, case-insensitively (sub_10003870: _stricmp(row, key) == 0).
static bool serverEntryMatches(const GeneralServerEntry* e, const std::string& key)
{
    const std::string rowKey = swiffFormatString("%s %d", e->address.c_str(), e->queryPort);
    return ::_stricmp(rowKey.c_str(), key.c_str()) == 0;
}
// The message the favourites rows post: type 1, code 0, the text assigned into a default record.
static void postMenuMessage(ISwiffHost* h, const char* literal)
{
    const std::string text(literal);
    SwiffHostMessage message;
    message.type = 1;
    message.code = 0;
    message.text = text;
    (void) h->postMessage(message);                                                         // +0x1E4
}

// sub_10052C80 "addFavourite": the player host, GeneralSettings, then the busy cursor; name = arg 0,
// address = arg 1 (to_string), port = (int) arg 2; key = "%s %d"(address, port). Host +0x250
// isDottedQuad(address) false posts "Not a valid ip" and answers false. Otherwise a favourite whose
// key matches posts "Server already in favorites" and answers false; none: GeneralSettings +0x11C
// addFavouriteServer(address, port, name) and true.
void as_multiplay_addFavourite(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_addFavourite");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IGeneralSettings* gs = static_cast<IGeneralSettings*>(
        classManager->getSingleton(renddx9abi::HostString("GeneralSettings")));
    BusyHost busy = busyOpen();
    const std::string name(fn.arg(0).to_string());
    const std::string address(fn.arg(1).to_string());
    const int port = bf2x87::toIntTruncate(fn.arg(2).to_number());
    const std::string key = swiffFormatString("%s %d", address.c_str(), port);
    if (!h->isDottedQuad(address))                                                          // +0x250
    {
        postMenuMessage(h, "Not a valid ip");
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    for (unsigned int i = 0; i < static_cast<unsigned int>(gs->getNumFavouriteServers()); ++i) // +0x120
    {
        if (serverEntryMatches(gs->getFavouriteServer(i), key))                            // +0x124
        {
            postMenuMessage(h, "Server already in favorites");
            fn.result->set_bool(false);
            busyClose(busy);
            return;
        }
    }
    gs->addFavouriteServer(address, port, name);                                            // +0x11C
    fn.result->set_bool(true);
    busyClose(busy);
}

// sub_10053060 "removeFavourite": the player host (discarded), GeneralSettings, then the busy cursor;
// key = arg 0. The first favourite whose "%s %d" key matches is removed through +0x12C
// removeFavouriteServerEntry(index) (Boolean ignored) and the answer is true; no match answers false.
void as_multiplay_removeFavourite(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_removeFavourite");
    (void) g_swiffPlayerInstance->getSwiffHost();
    IGeneralSettings* gs = static_cast<IGeneralSettings*>(
        classManager->getSingleton(renddx9abi::HostString("GeneralSettings")));
    BusyHost busy = busyOpen();
    const std::string key(fn.arg(0).to_string());
    for (unsigned int i = 0; i < static_cast<unsigned int>(gs->getNumFavouriteServers()); ++i) // +0x120
    {
        if (serverEntryMatches(gs->getFavouriteServer(i), key))                            // +0x124
        {
            (void) gs->removeFavouriteServerEntry(i);                                       // +0x12C
            fn.result->set_bool(true);
            busyClose(busy);
            return;
        }
    }
    fn.result->set_bool(false);
    busyClose(busy);
}

// sub_10053230 "addHistoryToFavourites": the busy cursor, the player host, GeneralSettings; key =
// arg 0. No history row (+0x13C count, +0x140 row) with that key answers false. For the first match,
// a favourite with the same key posts "Server allready in favourites" [sic] and answers false;
// otherwise +0x11C addFavouriteServer(row address, row query port, row name) and true.
void as_multiplay_addHistoryToFavourites(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_addHistoryToFavourites");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IGeneralSettings* gs = static_cast<IGeneralSettings*>(
        classManager->getSingleton(renddx9abi::HostString("GeneralSettings")));
    const std::string key(fn.arg(0).to_string());
    GeneralServerEntry* row = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(gs->getServerHistoryCount()); ++i) // +0x13C
    {
        GeneralServerEntry* e = gs->getServerHistory(i);                                    // +0x140
        if (serverEntryMatches(e, key)) { row = e; break; }
    }
    if (!row)
    {
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    for (unsigned int i = 0; i < static_cast<unsigned int>(gs->getNumFavouriteServers()); ++i) // +0x120
    {
        if (serverEntryMatches(gs->getFavouriteServer(i), key))                            // +0x124
        {
            postMenuMessage(h, "Server allready in favourites");
            fn.result->set_bool(false);
            busyClose(busy);
            return;
        }
    }
    gs->addFavouriteServer(row->address, row->queryPort, row->name);                        // +0x11C
    fn.result->set_bool(true);
    busyClose(busy);
}

// sub_10053540 "removeHistory": the busy cursor, the player host (discarded), GeneralSettings; key =
// arg 0. The first history row whose key matches is removed through +0x148 removeServerHistoryEntry
// (Boolean ignored), answering true; no match answers false.
void as_multiplay_removeHistory(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_removeHistory");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    IGeneralSettings* gs = static_cast<IGeneralSettings*>(
        classManager->getSingleton(renddx9abi::HostString("GeneralSettings")));
    const std::string key(fn.arg(0).to_string());
    for (unsigned int i = 0; i < static_cast<unsigned int>(gs->getServerHistoryCount()); ++i) // +0x13C
    {
        if (serverEntryMatches(gs->getServerHistory(i), key))                              // +0x140
        {
            (void) gs->removeServerHistoryEntry(i);                                         // +0x148
            fn.result->set_bool(true);
            busyClose(busy);
            return;
        }
    }
    fn.result->set_bool(false);
    busyClose(busy);
}

// sub_10053E50 "getNumDemoBookmarks": LocalProfileManager +0xA8 as a number (fild, signed).
void as_multiplay_getNumDemoBookmarks(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getNumDemoBookmarks");
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int count = static_cast<int>(static_cast<ILocalProfileManager*>(lpm)->getNumDemoBookmarks()); // +0xA8
    fn.result->set_double(static_cast<double>(count));
}

// sub_10053EE0 "isDownloadingDemo": Boolean of host +0x44 getDemoLibrary() +0x28 isDownloading.
void as_multiplay_isDownloadingDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_isDownloadingDemo");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(static_cast<IDemoLibrary*>(h->getDemoLibrary())->isDownloading());  // +0x28
}

// sub_10053F50 "getDownloadingHostname" / sub_10053FE0 "getDownloadingUrl" / sub_10054070
// "getDownloadingDate": host +0x44 getDemoLibrary() +0x2C / +0x30 / +0x34 (const std::string&),
// copied, published as a string.
void as_multiplay_getDownloadingHostname(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getDownloadingHostname");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value(static_cast<IDemoLibrary*>(h->getDemoLibrary())->getDownloadName()); // +0x2C
    fn.result->set_tu_string(tu_string(value.c_str()));
}
void as_multiplay_getDownloadingUrl(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getDownloadingUrl");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value(static_cast<IDemoLibrary*>(h->getDemoLibrary())->getDownloadUrl()); // +0x30
    fn.result->set_tu_string(tu_string(value.c_str()));
}
void as_multiplay_getDownloadingDate(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getDownloadingDate");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value(static_cast<IDemoLibrary*>(h->getDemoLibrary())->getDownloadDescription()); // +0x34
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// sub_10054100 "getDownloadingProgress": host +0x44 getDemoLibrary() +0x38 (float) as a number.
void as_multiplay_getDownloadingProgress(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getDownloadingProgress");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(static_cast<double>(
        static_cast<IDemoLibrary*>(h->getDemoLibrary())->getDownloadProgress()));          // +0x38
}

// sub_10054160 "downloadDemo": the busy cursor, the player host, LocalProfileManager; index =
// (int) arg 0; +0xAC getDemoBookmark(index). None answers false. Otherwise host +0x44
// getDemoLibrary() +0x48 setPendingBookmark(index), then (fetched again) +0x14 download(bookmark
// +0x00, +0x38, +0x54) with its Boolean ignored, and the answer is true.
void as_multiplay_downloadDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_downloadDemo");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    LocalDemoBookmark* bookmark =
        static_cast<ILocalProfileManager*>(lpm)->getDemoBookmark(static_cast<unsigned int>(index)); // +0xAC
    if (!bookmark)
    {
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    static_cast<IDemoLibrary*>(h->getDemoLibrary())->setPendingBookmark(index);             // +0x48
    (void) static_cast<IDemoLibrary*>(h->getDemoLibrary())->download(
        bookmark->field00, bookmark->field38, bookmark->field54);                           // +0x14
    fn.result->set_bool(true);
    busyClose(busy);
}

// sub_100542C0 "deleteDemoBookmark": the busy cursor, the player host (discarded),
// LocalProfileManager; Boolean of +0xB0 removeDemoBookmark((int) arg 0).
void as_multiplay_deleteDemoBookmark(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_deleteDemoBookmark");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    fn.result->set_bool(static_cast<ILocalProfileManager*>(lpm)->removeDemoBookmark(
        static_cast<unsigned int>(index)));                                                 // +0xB0
    busyClose(busy);
}

// sub_10054940 "addFavouriteServer": the busy cursor, GeneralSettings, the player host; the row id
// "%s %d %d" (0x100D99BC: address, query port, host port; the same unbounded %s as joinServer,
// bounded here the same way [CHOSEN, not recovered]). A favourite already at (address, query port)
// (+0x134 findFavouriteServer) answers false. Otherwise the current list's +0x3C getRecord(address,
// queryPort, false, false): none answers false; a record is added as +0x11C addFavouriteServer(
// entry.address, entry.queryPort, entry.hostName) and the answer is true.
void as_multiplay_addFavouriteServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_addFavouriteServer");
    BusyHost busy = busyOpen();
    IGeneralSettings* gs = static_cast<IGeneralSettings*>(
        classManager->getSingleton(renddx9abi::HostString("GeneralSettings")));
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string id(fn.arg(0).to_string());
    std::string address;
    char buf[260] = {0};
    int queryPort = 0, hostPort = 0;
    ::sscanf(id.c_str(), "%259s %d %d", buf, &queryPort, &hostPort);
    address = buf;
    if (gs->findFavouriteServer(address, queryPort))                                        // +0x134
    {
        fn.result->set_bool(false);
        busyClose(busy);
        return;
    }
    ServerBrowserRecord* rec = currentServerList(h)->getRecord(address, queryPort, false, false); // +0x3C
    if (!rec)
        fn.result->set_bool(false);
    else
    {
        gs->addFavouriteServer(rec->entry.address, rec->entry.queryPort, rec->entry.hostName); // +0x11C
        fn.result->set_bool(true);
    }
    busyClose(busy);
}

// sub_10054B20 "removeFavouriteServer": the busy cursor, the player host and GeneralSettings are
// fetched, and the answer is false. The shipped body removes nothing.
void as_multiplay_removeFavouriteServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_removeFavouriteServer");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    (void) classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    fn.result->set_bool(false);
    busyClose(busy);
}

// sub_10055350 "getSelectedServerFilterGameMode" / sub_100555E0 "getSelectedFilterGameModeId": host
// +0x120 getModInfo() +0x1C getCurrentMod(); an empty gameModes vector (Mod +0x10C) answers "". The
// Id row publishes gameModes[0]; the other publishes ModInfo +0x4C getNameFromGameMode(gameModes[0]).
void as_multiplay_getSelectedServerFilterGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSelectedServerFilterGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();     // +0x120, +0x1C
    if (mod->gameModes.empty())
    {
        fn.result->set_tu_string(tu_string(""));
        return;
    }
    const std::string first(mod->gameModes[0]);
    const std::string name = static_cast<IModInfo*>(h->getModInfo())->getNameFromGameMode(first); // +0x4C
    fn.result->set_tu_string(tu_string(name.c_str()));
}
void as_multiplay_getSelectedFilterGameModeId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSelectedFilterGameModeId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();     // +0x120, +0x1C
    if (mod->gameModes.empty())
    {
        fn.result->set_tu_string(tu_string(""));
        return;
    }
    const std::string first(mod->gameModes[0]);
    fn.result->set_tu_string(tu_string(first.c_str()));
}

// sub_100563B0 "getMapPathFromName": name = arg 0; every mod of host +0x120 getModInfo() +0x14
// getModList() (re-fetched each step) and every map of it is searched for map.name == name
// (operator==). No match publishes the name itself. A match builds a string as_value from the map's
// path and destroys it without assigning it (100564D6..100564F2: sub_10007680, then sub_1008A2F0,
// no sub_10009920): the AS result stays untouched. Observed shipped defect, preserved.
void as_multiplay_getMapPathFromName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getMapPathFromName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    for (unsigned int m = 0; m < static_cast<IModInfo*>(h->getModInfo())->getModList()->size(); ++m) // +0x14
    {
        const IModInfo::Mod& mod = (*static_cast<IModInfo*>(h->getModInfo())->getModList())[m];
        for (unsigned int i = 0; i < mod.maps.size(); ++i)
        {
            if (mod.maps[i].name == name)
            {
                const tu_string discarded(mod.maps[i].path.c_str());     // built, never assigned
                (void) discarded;
                return;
            }
        }
    }
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// sub_10057310 "playDemo": the busy cursor, the player host, (int) arg 0; host +0x44
// getDemoLibrary() +0x1C getEntry(index). None: nothing. An entry queues menu event 12 with the
// entry's path (+0x20) as its only argument through host +0x2C pushEvent. No AS result.
void as_multiplay_playDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_playDemo");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    DemoLibraryEntry* entry =
        static_cast<IDemoLibrary*>(h->getDemoLibrary())->getEntry(static_cast<unsigned int>(index)); // +0x1C
    if (entry)
    {
        const std::string path(entry->path);
        MenuEvent event;
        event.type = 12;
        event.args.push_back(path);
        (void) h->pushEvent(event);                                                          // +0x2C
    }
    busyClose(busy);
}

// sub_10057450 "getMapName": index = (int) arg 0; host +0xFC getServerEntries(rows). False answers
// the number 0. Otherwise the row at index publishes its map (+0); an index past the end answers "".
void as_multiplay_getMapName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getMapName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = bf2x87::toIntTruncate(fn.arg(0).to_number());
    std::vector<SwiffServerEntry> rows;
    if (!h->getServerEntries(rows))                                                         // +0xFC
    {
        fn.result->set_double(0.0);
        return;
    }
    for (unsigned int i = 0; i < rows.size(); ++i)
    {
        if (i == static_cast<unsigned int>(index))
        {
            fn.result->set_tu_string(tu_string(rows[i].map.c_str()));
            return;
        }
    }
    fn.result->set_tu_string(tu_string(""));
}

// ---- dice.bf2.Sound (the VoIP rows) -----------------------------------------------------------

// byte_100F8B4C: "VoIP initialised". Only voipInitialize's client arm (0x1006913F) raises it.
static bool s_voipInitialized = false;
// flt_100EC92C (s_voipThreshold, SwiffPlayer.cpp): the menu's capture threshold, kept while a voice
// test overrides the client's.
// dword_100F8B50: the profile name voipInitialize last pushed the AudioSettings voice values for.
static std::string s_voipInitializedProfile;

// Every VoIP row reads the player (dword_100F2F20) and its +0x8C getBf2VoipClient, the BF2.exe voice
// client SwiffPlayer reached through getSingleton("Bf2VoipClient") (Game/VoipClient/BF2VoipClient.h;
// virtual slots only, plus VoipClient's header-inline setLocalLoopback, as the DLL links its copy).
static BF2VoipClient* voipPlayerClient(SwiffPlayer* player)
{
    return player ? static_cast<BF2VoipClient*>(player->getBf2VoipClient()) : 0;            // player +0x8C
}
static IAudioSettings* voipAudioSettings()
{
    return static_cast<IAudioSettings*>(classManager->getSingleton(renddx9abi::HostString("AudioSettings")));
}

// sub_10069010 "voipInitialize": needs the client and the LocalProfileManager. A client that is neither
// in a session nor testing starts the local test (quality 3, mixer); failing that answers false. Then
// the player's VoIP tick is armed and the row is initialised; the first call for a profile pushes the
// AudioSettings voice values (playback, capture, boost, threshold) into the client. True.
void as_sound_voipInitialize(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipInitialize");
    SwiffPlayer* player = g_swiffPlayerInstance;
    BF2VoipClient* client = voipPlayerClient(player);
    if (!client)
    {
        fn.result->set_bool(false);                                                         // 0x1006925B
        return;
    }
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    if (!lpm)
    {
        fn.result->set_bool(false);                                                         // 0x1006906A
        return;
    }
    const std::string profile = static_cast<ILocalProfileManager*>(lpm)->getName();       // +0x7C
    if (client->isActive() || client->isTesting())                                          // +0x38 +0x34
    {
        player->setVoipUpdateFlag(1);                                                       // +0x90
        s_voipInitialized = true;
        if (profile == s_voipInitializedProfile)
        {
            fn.result->set_bool(true);
            return;
        }
    }
    else if (!client->startTest(3, true))                                                   // +0x30
    {
        fn.result->set_bool(false);
        return;
    }
    IAudioSettings* as = voipAudioSettings();
    if (!as)
    {
        fn.result->set_bool(false);
        return;
    }
    s_voipInitializedProfile = profile;
    client->setPlaybackVolume(as->getVoipPlaybackVolume());                                // +0x1C, as +0x40
    client->setCaptureVolume(as->getVoipCaptureVolume());                                  // +0x20, as +0x48
    client->mixerSetBoost(as->getVoipBoostEnabled());                                      // +0xCC, as +0x58
    s_voipThreshold = as->getVoipCaptureThreshold();                                       // as +0x50
    client->setCaptureThreshold(s_voipThreshold);                                          // +0x14
    player->setVoipUpdateFlag(1);
    s_voipInitialized = true;
    fn.result->set_bool(true);
}

// sub_100692B0 "voipSetEnable": stores the AudioSettings flag and enables or disables the client's user
// switch (+0x98 takes "disabled"). True. No player, AudioSettings, initialisation or client: no result.
void as_sound_voipSetEnable(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipSetEnable");
    SwiffPlayer* player = g_swiffPlayerInstance;
    IAudioSettings* as = voipAudioSettings();
    if (!player || !as || !s_voipInitialized || !voipPlayerClient(player))
        return;
    if (fn.arg(0).to_bool())
    {
        as->setVoipEnabled(true);                                                           // +0x3C
        voipPlayerClient(player)->setUserDisabled(false);                                   // +0x98
    }
    else
    {
        as->setVoipEnabled(false);
        voipPlayerClient(player)->setUserDisabled(true);
    }
    fn.result->set_bool(true);
}

// sub_100693C0 "voipGetEnable": VoIP not initialised answers false; otherwise AudioSettings by name,
// absent answers false, present answers the Boolean of +0x38 getVoipEnabled. No client involved.
void as_sound_voipGetEnable(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetEnable");
    if (!s_voipInitialized)
    {
        fn.result->set_bool(false);
        return;
    }
    IBase* as = classManager->getSingleton(renddx9abi::HostString("AudioSettings"));
    if (!as)
    {
        fn.result->set_bool(false);
        return;
    }
    fn.result->set_bool(audioSettingsOf(as)->getVoipEnabled());                             // +0x38
}

// The rows that read the player, the client and AudioSettings: an absent AudioSettings answers false;
// the remaining null exits leave the result untouched.
#define VOIP_ROW_PROLOGUE(row)                                                             \
    SWF_AS_TRACE("as_sound_" #row);                                                       \
    SwiffPlayer* player = g_swiffPlayerInstance;                                          \
    BF2VoipClient* client = voipPlayerClient(player);                                     \
    if (!client)                                                                          \
        return;                                                                           \
    IAudioSettings* as = voipAudioSettings();                                             \
    if (!as)                                                                              \
    {                                                                                     \
        fn.result->set_bool(false);                                                       \
        return;                                                                           \
    }

// sub_100694E0 "voipStartTest": only with a session or test running. Marks the test (byte_100F8B4D),
// mutes sound groups 0..3 through the host (+0x18C), loops capture back to playback and opens the gate
// (threshold 0). No result; a client with neither answers false.
void as_sound_voipStartTest(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipStartTest)
    (void) as;
    if (!client->isTesting() && !client->isActive())
    {
        fn.result->set_bool(false);
        return;
    }
    s_voipTestRunning = true;
    player->getSwiffHost()->slot18C(1);                                                     // +0x5C, +0x18C
    client->setLocalLoopback(true);                                                         // 0x100BC8B0
    client->setCaptureThreshold(0.0f);                                                      // +0x14
}

// sub_10069750 / sub_100698C0 "voipSetTransmitVolume" / "voipSetRecieveVolume": once initialised, the
// number goes to the client and to AudioSettings (capture +0x20/+0x4C, playback +0x1C/+0x44).
void as_sound_voipSetTransmitVolume(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipSetTransmitVolume)
    if (!s_voipInitialized)
        return;
    const float volume = static_cast<float>(fn.arg(0).to_number());
    client->setCaptureVolume(volume);
    as->setVoipCaptureVolume(volume);
}
void as_sound_voipSetRecieveVolume(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipSetRecieveVolume)
    if (!s_voipInitialized)
        return;
    const float volume = static_cast<float>(fn.arg(0).to_number());
    client->setPlaybackVolume(volume);
    as->setVoipPlaybackVolume(volume);
}

// sub_10069A30 "voipSetThresholdVolume": the menu value is always kept; outside a voice test it also
// reaches the client and AudioSettings (+0x14, +0x54).
void as_sound_voipSetThresholdVolume(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipSetThresholdVolume)
    if (!s_voipInitialized)
        return;
    const float threshold = static_cast<float>(fn.arg(0).to_number());
    s_voipThreshold = threshold;
    if (s_voipTestRunning)
        return;
    client->setCaptureThreshold(threshold);
    as->setVoipCaptureThreshold(threshold);
}

// sub_10069BA0 "voipSetBoost": AudioSettings follows only when the mixer accepted the boost (+0xCC).
void as_sound_voipSetBoost(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipSetBoost)
    if (!s_voipInitialized)
        return;
    const bool boost = fn.arg(0).to_bool();
    if (client->mixerSetBoost(boost))
        as->setVoipBoostEnabled(boost);                                                     // +0x5C
}

// sub_10069E10 "voipSetUsePushToTalk": not initialised answers false; otherwise the client and
// AudioSettings take the flag (+0xAC, +0x64), with no result.
void as_sound_voipSetUsePushToTalk(const gameswf::fn_call& fn)
{
    VOIP_ROW_PROLOGUE(voipSetUsePushToTalk)
    if (!s_voipInitialized)
    {
        fn.result->set_bool(false);
        return;
    }
    const bool enabled = fn.arg(0).to_bool();
    client->setPushToTalkEnabled(enabled);
    as->setVoipUsePushToTalk(enabled);
}
#undef VOIP_ROW_PROLOGUE

// The getters: player and client first, null exits leave the result untouched.
// sub_10069840 / sub_100699B0: once initialised, the client's capture / playback volume (+0x24/+0x28).
void as_sound_voipGetTransmitVolume(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetTransmitVolume");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (client && s_voipInitialized)
        fn.result->set_double(client->getCaptureVolume());
}
void as_sound_voipGetRecieveVolume(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetRecieveVolume");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (client && s_voipInitialized)
        fn.result->set_double(client->getPlaybackVolume());
}
// sub_10069B30: the kept menu threshold, as a number.
void as_sound_voipGetThresholdVolume(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetThresholdVolume");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (client && s_voipInitialized)
        fn.result->set_double(s_voipThreshold);
}
// sub_10069CA0: not initialised answers false; otherwise the mixer's boost (+0xD4).
void as_sound_voipGetBoost(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetBoost");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (!client)
        return;
    if (!s_voipInitialized)
    {
        fn.result->set_bool(false);
        return;
    }
    bool boost = false;
    client->mixerGetBoost(&boost);
    fn.result->set_bool(boost);
}
// sub_10069D80 / sub_10069F50: the mixer's boost support (+0xC8) / the push-to-talk flag (+0xA8),
// false while not initialised.
void as_sound_voipIsBoostEnabled(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipIsBoostEnabled");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (client)
        fn.result->set_bool(s_voipInitialized ? client->mixerHasBoost() : false);
}
void as_sound_voipGetUsePushToTalk(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetUsePushToTalk");
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (client)
        fn.result->set_bool(s_voipInitialized ? client->getPushToTalkEnabled() : false);
}

// sub_10069FE0 "voipSendingVoice": Boolean false first. Once initialised: the capture level against the
// menu threshold. At or above it the answer is true and loopback plays back; below it false and the
// loopback stays silent (client +0x95, written directly).
void as_sound_voipSendingVoice(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipSendingVoice");
    fn.result->set_bool(false);
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (!client)
        return;
    bool sending = false;
    if (s_voipInitialized)
    {
        double level = 0.0;
        client->getCaptureLevel(&level);                                                    // +0x3C
        if (static_cast<double>(s_voipThreshold) <= level)
        {
            client->m_loopbackSilent = false;
            sending = true;
        }
        else
        {
            client->m_loopbackSilent = true;
        }
    }
    fn.result->set_bool(sending);
}

// sub_1006A0F0 "voipGetSendingVoiceVolume": the number 0 first; once initialised, the capture level
// while the client reports capturing, else 0.
void as_sound_voipGetSendingVoiceVolume(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipGetSendingVoiceVolume");
    fn.result->set_double(0.0);
    BF2VoipClient* client = voipPlayerClient(g_swiffPlayerInstance);
    if (!client || !s_voipInitialized)
        return;
    double level = 0.0;
    fn.result->set_double(client->getCaptureLevel(&level) ? level : 0.0);
}

// ---- dice.bf2.Mod ------------------------------------------------------------------------------

// dword_100EC604: the mod row the Mod page selected, -1 at load (.data initialiser), written only by
// Mod.selectMod.
static int s_selectedModIndex = -1;

// sub_1004F630 "getNumMods": host +0x190 getModCount(&n), n pre-set 0; false answers 0, true n.
void as_mod_getNumMods(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_getNumMods");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    int count = 0;
    if (!h->getModCount(&count))                                                            // +0x190
        fn.result->set_double(0.0);
    else
        fn.result->set_double(static_cast<double>(count));
}

// sub_1004F6E0 "browseHomepage" / sub_1004F750 "activateMod": with a selection, a default Mod record
// (sub_100224E0 / sub_10022580) filled by host +0x194 getModAt(selection, &mod); a filled record is
// handed to +0x19C requestBrowseModHomepage / +0x198 requestActivateMod (Booleans ignored). No AS
// result. requestActivateMod's parameter is the record itself, whose first member is the path.
void as_mod_browseHomepage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_browseHomepage");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = s_selectedModIndex;
    if (index == -1)
        return;
    IModInfo::Mod mod;
    if (h->getModAt(index, &mod))                                                           // +0x194
        (void) h->requestBrowseModHomepage(&mod);                                           // +0x19C
}
void as_mod_activateMod(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_activateMod");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = s_selectedModIndex;
    if (index == -1)
        return;
    IModInfo::Mod mod;
    if (h->getModAt(index, &mod))                                                           // +0x194
        (void) h->requestActivateMod(&mod.path);                                            // +0x198
}

// sub_1004F800 "selectMod": (int) arg 0 into dword_100EC604 (fistp straight to the global). No AS
// result and no player access.
void as_mod_selectMod(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_selectMod");
    s_selectedModIndex = bf2x87::toIntTruncate(fn.arg(0).to_number());
}

// sub_1004F840 "activateModAndJoinServer": the player host; mod = "mods/" + arg 0, address = arg 1,
// port = (int) arg 2; host +0x368(mod, address, port), Boolean ignored (BF2.exe 0x626200 is `mov al,
// 1; ret 0Ch`). No AS result.
void as_mod_activateModAndJoinServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_activateModAndJoinServer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string mod(fn.arg(0).to_string());
    mod = std::string("mods/") + mod;
    const std::string address(fn.arg(1).to_string());
    const int port = bf2x87::toIntTruncate(fn.arg(2).to_number());
    (void) h->slot368(mod, address, port);                                                  // +0x368
}

// sub_1004FA00 "getModTitle" / sub_1004FB60 "getModDesc": the player host, then a dice String
// object (0x64 bytes, sub_1009BB10). No selection, or getModAt refusing: narrow "" with the wide side
// cleared. A record: the title (+0x38) / desc (+0x54) key localised through host +0xA8
// (sub_1004F5E0) into the wide side, the narrow side "".
static void publishModLocalised(const gameswf::fn_call& fn, std::string IModInfo::Mod::* field)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    const int index = s_selectedModIndex;
    if (index == -1)
    {
        gameswf::assign_dice_string_object_narrow(text, "");
        fn.result->set_as_object_interface(text);
        return;
    }
    IModInfo::Mod mod;
    if (!h->getModAt(index, &mod))                                                          // +0x194
        gameswf::assign_dice_string_object_narrow(text, "");
    else
    {
        std::wstring wide;
        (void) g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(wide, mod.*field);  // +0xA8
        gameswf::assign_dice_string_object_wide(text, BF2_DICE_UTF16(wide.c_str()));
    }
    fn.result->set_as_object_interface(text);
}
void as_mod_getModTitle(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_getModTitle");
    publishModLocalised(fn, &IModInfo::Mod::title);
}
void as_mod_getModDesc(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_getModDesc");
    publishModLocalised(fn, &IModInfo::Mod::desc);
}

// sub_1004FCC0 "getModPath", sub_1004FFE0 "getModUrl", sub_10050120 "getModLogo", sub_10050260
// "getModVersion": no selection or getModAt refusing answers ""; a record publishes its path (+0x00),
// url (+0x70), logo (+0xA8) or version (+0xE0) as a plain string.
static void publishModField(const gameswf::fn_call& fn, std::string IModInfo::Mod::* field)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    if (s_selectedModIndex == -1)
    {
        fn.result->set_tu_string(tu_string(""));
        return;
    }
    IModInfo::Mod mod;
    if (!h->getModAt(s_selectedModIndex, &mod))                                             // +0x194
        fn.result->set_tu_string(tu_string(""));
    else
        fn.result->set_tu_string(tu_string((mod.*field).c_str()));
}
void as_mod_getModPath(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_mod_getModPath");    publishModField(fn, &IModInfo::Mod::path); }
void as_mod_getModUrl(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_mod_getModUrl");     publishModField(fn, &IModInfo::Mod::url); }
void as_mod_getModLogo(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_mod_getModLogo");    publishModField(fn, &IModInfo::Mod::logo); }
void as_mod_getModVersion(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_mod_getModVersion"); publishModField(fn, &IModInfo::Mod::version); }

// sub_1004FE00 "getModPathShort": as getModPath, but only the characters after the first '/' or '\'
// of the path, appended one at a time (operator+= char) to an empty string.
void as_mod_getModPathShort(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_getModPathShort");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    if (s_selectedModIndex == -1)
    {
        fn.result->set_tu_string(tu_string(""));
        return;
    }
    IModInfo::Mod mod;
    if (!h->getModAt(s_selectedModIndex, &mod))                                             // +0x194
    {
        fn.result->set_tu_string(tu_string(""));
        return;
    }
    std::string tail("");
    bool past = false;
    for (unsigned int i = 0; i < mod.path.size(); ++i)
    {
        if (!past)
        {
            if (mod.path[i] == '/' || mod.path[i] == '\\')
                past = true;
        }
        else
            tail += mod.path[i];
    }
    fn.result->set_tu_string(tu_string(tail.c_str()));
}

// ---- dice.bf2.Logic ----------------------------------------------------------------------------

// sub_1004C950 "cacheCredits": the player host (discarded), player +0x28 getRender() +0xF4
// preloadCredits (answer ignored). No AS result.
void as_logic_cacheCredits(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_cacheCredits");
    (void) g_swiffPlayerInstance->getSwiffHost();
    (void) g_swiffPlayerInstance->getRender()->preloadCredits();                            // +0x28, +0xF4
}

// sub_1004CA70 "quit": inside the busy cursor; the player host, LocalProfileManager; with a current
// profile (+0x44) +0x40 saveCurrentProfile, otherwise +0xC4 saveGlobalSettings (Booleans ignored);
// then host +0xB0 requestQuit. No AS result.
void as_logic_quit(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_quit");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    if (lpm->getCurrentProfile())                                                           // +0x44
        (void) lpm->saveCurrentProfile();                                                   // +0x40
    else
        (void) lpm->saveGlobalSettings();                                                   // +0xC4
    (void) h->requestQuit();                                                                // +0xB0
    busyClose(busy);
}

// sub_1004CB60 "assert": the shipped body is a bare `ret 0Ch`.
void as_logic_assert(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_assert");
}

// sub_1004CB70 "setActivateReason": host +0xC0 setMessageId((int) arg 0). No AS result.
void as_logic_setActivateReason(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setActivateReason");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setMessageId(bf2x87::toIntTruncate(fn.arg(0).to_number()));                          // +0xC0
}

// sub_1004CF80 "setScrollSpeed": host +0x32C with arg 0 as a float (fstp dword). No AS result.
void as_logic_setScrollSpeed(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setScrollSpeed");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setFloatF0C(static_cast<float>(fn.arg(0).to_number()));                              // +0x32C
}

// sub_1004CFD0 "setCommandLine": host +0x2C0 with (int) arg 0 (the port names the slot setPage).
void as_logic_setCommandLine(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setCommandLine");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setPage(bf2x87::toIntTruncate(fn.arg(0).to_number()));                               // +0x2C0
}

// sub_1004D350 "getRandomCreditsTexture": the player host (discarded), player +0x28 getRender()
// +0x100 getRandomCreditsName, published as a string.
void as_logic_getRandomCreditsTexture(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getRandomCreditsTexture");
    (void) g_swiffPlayerInstance->getSwiffHost();
    const std::string name = g_swiffPlayerInstance->getRender()->getRandomCreditsName();    // +0x28, +0x100
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// sub_1004D810 "getCommandlineServer" / sub_1004D890 "getCommandlineServerPassword": host +0xDC
// getJoinAddressSetting / +0xE0 getPasswordSetting (GSJoinAddress / GSPassword), as strings.
void as_logic_getCommandlineServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getCommandlineServer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value = h->getJoinAddressSetting();                                   // +0xDC
    fn.result->set_tu_string(tu_string(value.c_str()));
}
void as_logic_getCommandlineServerPassword(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getCommandlineServerPassword");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value = h->getPasswordSetting();                                      // +0xE0
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// sub_1004DAA0 "getAbsoluteFileName": host +0x234 resolveMenuPath(arg 0), as a string.
void as_logic_getAbsoluteFileName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getAbsoluteFileName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string path(fn.arg(0).to_string());
    const std::string value = h->resolveMenuPath(path);                                     // +0x234
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// sub_1004DE30 "restartExe": menu event 18 (0x12) with no arguments through host +0x2C pushEvent.
void as_logic_restartExe(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_restartExe");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    MenuEvent event;
    event.type = 0x12;
    (void) h->pushEvent(event);                                                              // +0x2C
}

// ---- dice.bf2.PlayerStats ----------------------------------------------------------------------

// sub_10059E00 "getPreviousRankLevel", sub_10059EE0 "getNextRankLevel", sub_1005A1E0
// "getRankNotificationLevel": the player host, ProfileManager (+0x30 getProfileId when present, the
// id pre-set 0), a rank record {time(), 0, 0, 0, 0} and host +0x284 getRankInfo(id, &record). False
// answers 0. True answers record.rank through host +0x1B8 getRankB / +0x1BC getRankC, or record.rank
// itself.
static bool currentRankRecord(ISwiffHost* h, IPlayerRankInfo::RankInfo& record)
{
    IBase* pm = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    int id = 0;
    if (pm)
        (void) static_cast<IProfileManager*>(pm)->getProfileId(id);                         // +0x30
    record.timestamp = static_cast<std::int32_t>(::time(0));
    record.profileId = 0; record.rank = 0; record.change = 0; record.decrease = 0;
    return h->getRankInfo(id, &record);                                                     // +0x284
}
void as_playerstats_getPreviousRankLevel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getPreviousRankLevel");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IPlayerRankInfo::RankInfo record;
    int value = 0;
    if (currentRankRecord(h, record))
        value = h->getRankB(record.rank);                                                   // +0x1B8
    fn.result->set_double(static_cast<double>(value));
}
void as_playerstats_getNextRankLevel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getNextRankLevel");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IPlayerRankInfo::RankInfo record;
    int value = 0;
    if (currentRankRecord(h, record))
        value = h->getRankC(record.rank);                                                   // +0x1BC
    fn.result->set_double(static_cast<double>(value));
}
void as_playerstats_getRankNotificationLevel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getRankNotificationLevel");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IPlayerRankInfo::RankInfo record;
    int value = 0;
    if (currentRankRecord(h, record))
        value = record.rank;
    fn.result->set_double(static_cast<double>(value));
}

// sub_1005A3B0 "refreshUnlocks" / sub_1005A490 "refreshStats": Boolean of host +0x1C8 fetchUnlocks /
// +0x1AC fetchPlayerStats((int) arg 0).
void as_playerstats_refreshUnlocks(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_refreshUnlocks");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(h->fetchUnlocks(bf2x87::toIntTruncate(fn.arg(0).to_number())));     // +0x1C8
}
void as_playerstats_refreshStats(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_refreshStats");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(h->fetchPlayerStats(bf2x87::toIntTruncate(fn.arg(0).to_number())));  // +0x1AC
}

// sub_1005A9B0 "getUnlocks": id = (int) arg 0; an unlock cache {time(), 0, 0, 0, {}} and the Boolean
// of host +0x1C4 getUnlocks(id, &cache).
void as_playerstats_getUnlocks(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getUnlocks");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int id = bf2x87::toIntTruncate(fn.arg(0).to_number());
    IPlayerUnlocks::UnlockCache cache;
    cache.field0 = static_cast<int>(::time(0));
    cache.field4 = 0; cache.available = 0; cache.selected = 0;
    fn.result->set_bool(h->getUnlocks(id, &cache));                                         // +0x1C4
}

// sub_1005BBA0 "getStats": id = (int) arg 0; a default stats record (sub_10020A80) and the Boolean
// of host +0x1B0 getPlayerStatsRecord(id, record).
void as_playerstats_getStats(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getStats");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int id = bf2x87::toIntTruncate(fn.arg(0).to_number());
    dice::hfe::SwiffPlayerStatsRecord record;
    fn.result->set_bool(h->getPlayerStatsRecord(id, record));                               // +0x1B0
}

// sub_1005B0E0 "getOpponentKills", sub_1005B1A0 "getVictimKills", sub_1005BAE0 "getRoadKills": a
// default stats record, host +0x1B0 getPlayerStatsRecord(+0x21C getSelectedPlayerId, record); false
// answers 0, true the record's vmks (+520), mvks (+484) or vrk (+2672) as a number.
static void publishStatsKills(const gameswf::fn_call& fn, int dice::hfe::SwiffPlayerStatsRecord::* field)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    dice::hfe::SwiffPlayerStatsRecord record;
    if (!h->getPlayerStatsRecord(h->getSelectedPlayerId(), record))                         // +0x21C, +0x1B0
        fn.result->set_double(0.0);
    else
        fn.result->set_double(static_cast<double>(record.*field));
}
void as_playerstats_getOpponentKills(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_playerstats_getOpponentKills"); publishStatsKills(fn, &dice::hfe::SwiffPlayerStatsRecord::vmks); }
void as_playerstats_getVictimKills(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_playerstats_getVictimKills");   publishStatsKills(fn, &dice::hfe::SwiffPlayerStatsRecord::mvks); }
void as_playerstats_getRoadKills(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_playerstats_getRoadKills");     publishStatsKills(fn, &dice::hfe::SwiffPlayerStatsRecord::vrk); }

// ---- dice.bf2.ServerInfo -----------------------------------------------------------------------

// sub_10062940 "getSponsorText": a default record through the in-game fetch (sub_100627F0), the
// sponsor text (details +160) with every "|" replaced by "\n" (sub_100037D0), as a string.
void as_serverinfo_getSponsorText(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getSponsorText");
    CachedRecord record;
    fetchInGameServerRecord(record);
    std::string text(record.details.sponsorText);
    for (std::string::size_type at = text.find("|"); at != std::string::npos; at = text.find("|", at + 1))
        text.replace(at, 1, "\n");
    fn.result->set_tu_string(tu_string(text.c_str()));
}

// sub_10062BA0 "getMapName": the in-game record's map name (entry +144), as a string.
void as_serverinfo_getMapName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getMapName");
    CachedRecord record;
    fetchInGameServerRecord(record);
    fn.result->set_tu_string(tu_string(record.entry.mapName.c_str()));
}

// sub_10062C50 "getMaxPlayers": the in-game record's max players (entry +100), as a number.
void as_serverinfo_getMaxPlayers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getMaxPlayers");
    CachedRecord record;
    fetchInGameServerRecord(record);
    fn.result->set_double(static_cast<double>(record.entry.maxPlayers));
}

// sub_10062CF0 "getGameModeTitle", sub_10062DA0 "getGameModeDescription", sub_10062E50
// "getBackgroundTitle", sub_10062F00 "getBackgroundDescription": all four publish the in-game
// record's game mode string (entry +108; each reads [ebp-1F0h] with the capacity at [ebp-1DCh]).
// The identical bodies are shipped behaviour.
static void publishInGameGameMode(const gameswf::fn_call& fn)
{
    CachedRecord record;
    fetchInGameServerRecord(record);
    fn.result->set_tu_string(tu_string(record.entry.gameMode.c_str()));
}
void as_serverinfo_getGameModeTitle(const gameswf::fn_call& fn)         { SWF_AS_TRACE("as_serverinfo_getGameModeTitle");         publishInGameGameMode(fn); }
void as_serverinfo_getGameModeDescription(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_serverinfo_getGameModeDescription");   publishInGameGameMode(fn); }
void as_serverinfo_getBackgroundTitle(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_serverinfo_getBackgroundTitle");       publishInGameGameMode(fn); }
void as_serverinfo_getBackgroundDescription(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_serverinfo_getBackgroundDescription"); publishInGameGameMode(fn); }

// ---- dice.bf2.Singleplay / Shader ---------------------------------------------------------------

// sub_10068BF0 "getBotSkills" / sub_10068C00 "setBotSkills": the player host is fetched and nothing
// else happens; no AS result.
void as_singleplay_getBotSkills(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_singleplay_getBotSkills");
    (void) g_swiffPlayerInstance->getSwiffHost();
}
void as_singleplay_setBotSkills(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_singleplay_setBotSkills");
    (void) g_swiffPlayerInstance->getSwiffHost();
}

// sub_10068C10 "chooseMission": id = (int) arg 0; host +0xEC getSinglePlayerLevels(rows). True: the
// first row whose index equals id goes to host +0xE8 requestHostServer (Boolean ignored). No AS
// result.
void as_singleplay_chooseMission(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_singleplay_chooseMission");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int id = bf2x87::toIntTruncate(fn.arg(0).to_number());
    std::vector<SwiffSinglePlayerLevel> rows;
    if (!h->getSinglePlayerLevels(rows))                                                    // +0xEC
        return;
    for (unsigned int i = 0; i < rows.size(); ++i)
    {
        if (rows[i].index == id)
        {
            (void) h->requestHostServer(&rows[i]);                                          // +0xE8
            return;
        }
    }
}

// sub_10068A10 "setShader": the player host (discarded); name = arg 0, technique = (int) arg 1;
// player +0x70 (renderSetPairState -> SwiffRender::setTextureTechnique). No AS result.
void as_shader_setShader(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_shader_setShader");
    (void) g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    const int technique = bf2x87::toIntTruncate(fn.arg(1).to_number());
#if defined(_MSC_VER)
    g_swiffPlayerInstance->renderSetPairState(reinterpret_cast<intptr_t>(&name), technique);       // +0x70
#else
    g_swiffPlayerInstance->renderSetPairState(name, technique);                             // +0x70
#endif
}

// ---- dice.bf2.ControlSettings.mapInput ---------------------------------------------------------

// sub_1000EDA0 "mapInput": the player host; ControlSettings +0x80 activateMouse; the edited map
// selected (sub_1000E250); input = (int) arg 0. Input -1: the selection restored (sub_1000E2E0),
// ControlSettings +0x84 deactivateMouse, Boolean true. Otherwise alternate = arg 1, second = arg 2
// (to_bool), arg 3 to_bool and dropped; gameInput = (unk_100F3138 == "_defaultGameControlMap");
// ControlSettings +0x30 mapNextInput(input, alternate, 0, second, gameInput). Answer 1: restore,
// deactivateMouse, post {3, 0, "dup" + getMappedInputName (+0x90)} through host +0x1E4, Boolean false.
// Answer 2: restore, deactivateMouse, false. Any other answer: restore, deactivateMouse, true.
// BF2.exe's +0x30 (0x4B2CA0) returns the manager's full EAX (+0x38), compared here with 1 and 2;
// both are declared int. The port's manager capture is a CHOSEN-STUB answering 0, so only the last
// arm is reachable until the capture routine (ELF 0x8497AC0) is recovered.
void as_controlsettings_mapInput(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_mapInput");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    controlSettingsOf(controlSettingsSingleton())->activateMouse();                         // +0x80
    selectEditedControlMap();
    const int input = bf2x87::toIntTruncate(fn.arg(0).to_number());
    if (input != -1)
    {
        const bool alternate = fn.arg(1).to_bool();
        const bool second = fn.arg(2).to_bool();
        (void) fn.arg(3).to_bool();
        const bool gameInput = (s_editedControlMap == "_defaultGameControlMap");
        const int answer = controlSettingsOf(controlSettingsSingleton())->mapNextInput(
            input, alternate, 0, second, gameInput);                                        // +0x30
        if (answer == 1)
        {
            restoreControlMapSelection();
            controlSettingsOf(controlSettingsSingleton())->deactivateMouse();               // +0x84
            const std::string mapped = controlSettingsOf(controlSettingsSingleton())->getMappedInputName(); // +0x90
            const std::string dup("dup");
            const std::string text = std::string(dup) + mapped;
            SwiffHostMessage message;
            message.type = 3;
            message.code = 0;
            message.text = text;
            (void) h->postMessage(message);                                                 // +0x1E4
            fn.result->set_bool(false);
            return;
        }
        if (answer == 2)
        {
            restoreControlMapSelection();
            controlSettingsOf(controlSettingsSingleton())->deactivateMouse();               // +0x84
            fn.result->set_bool(false);
            return;
        }
    }
    restoreControlMapSelection();
    controlSettingsOf(controlSettingsSingleton())->deactivateMouse();                       // +0x84
    fn.result->set_bool(true);
}

// The registrations, in the shipped registrars' order within each namespace (address of the
// name push; the callback address is the sub_ named in each body above).
static void registerRecoveredNatives(SwiffAsNamespace** ns, const char* const* names, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const char* n = names[i];
        if (::strcmp(n, "ControlSettings") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_mapInput>("mapInput");         // 1000F163 -> sub_1000EDA0
        }
        else if (::strcmp(n, "Logic") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_logic_quit>("quit");                           // 1004DF60 -> sub_1004CA70
            ns[i]->registerNativeCompatibleMethod<as_logic_assert>("assert");                       // 1004E0CB -> sub_1004CB60
            ns[i]->registerNativeCompatibleMethod<as_logic_setActivateReason>("setActivateReason"); // 1004E17B -> sub_1004CB70
            ns[i]->registerNativeCompatibleMethod<as_logic_getAbsoluteFileName>("getAbsoluteFileName"); // 1004E2DB -> sub_1004DAA0
            ns[i]->registerNativeCompatibleMethod<as_logic_getCommandlineServer>("getCommandlineServer"); // 1004E5F3 -> sub_1004D810
            ns[i]->registerNativeCompatibleMethod<as_logic_getCommandlineServerPassword>("getCommandlineServerPassword"); // 1004E64B -> sub_1004D890
            ns[i]->registerNativeCompatibleMethod<as_logic_setCommandLine>("setCommandLine");       // 1004E90B -> sub_1004CFD0
            ns[i]->registerNativeCompatibleMethod<as_logic_restartExe>("restartExe");               // 1004E9BB -> sub_1004DE30
            ns[i]->registerNativeCompatibleMethod<as_logic_setScrollSpeed>("setScrollSpeed");       // 1004EA6B -> sub_1004CF80
            ns[i]->registerNativeCompatibleMethod<as_logic_cacheCredits>("cacheCredits");           // 1004EB1B -> sub_1004C950
            ns[i]->registerNativeCompatibleMethod<as_logic_getRandomCreditsTexture>("getRandomCreditsTexture"); // 1004EB73 -> sub_1004D350
        }
        else if (::strcmp(n, "Mod") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_mod_getNumMods>("getNumMods");                 // 10050410 -> sub_1004F630
            ns[i]->registerNativeCompatibleMethod<as_mod_browseHomepage>("browseHomepage");         // 10050473 -> sub_1004F6E0
            ns[i]->registerNativeCompatibleMethod<as_mod_activateMod>("activateMod");               // 100504CB -> sub_1004F750
            ns[i]->registerNativeCompatibleMethod<as_mod_selectMod>("selectMod");                   // 10050523 -> sub_1004F800
            ns[i]->registerNativeCompatibleMethod<as_mod_getModTitle>("getModTitle");               // 1005057B -> sub_1004FA00
            ns[i]->registerNativeCompatibleMethod<as_mod_getModDesc>("getModDesc");                 // 100505D3 -> sub_1004FB60
            ns[i]->registerNativeCompatibleMethod<as_mod_getModPath>("getModPath");                 // 1005062B -> sub_1004FCC0
            ns[i]->registerNativeCompatibleMethod<as_mod_getModPathShort>("getModPathShort");       // 10050683 -> sub_1004FE00
            ns[i]->registerNativeCompatibleMethod<as_mod_getModUrl>("getModUrl");                   // 100506DB -> sub_1004FFE0
            ns[i]->registerNativeCompatibleMethod<as_mod_getModLogo>("getModLogo");                 // 10050733 -> sub_10050120
            ns[i]->registerNativeCompatibleMethod<as_mod_getModVersion>("getModVersion");           // 1005078B -> sub_10050260
            ns[i]->registerNativeCompatibleMethod<as_mod_activateModAndJoinServer>("activateModAndJoinServer"); // 1005083B -> sub_1004F840
        }
        else if (::strcmp(n, "Multiplay") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_multiplay_addFavouriteServer>("addFavouriteServer"); // 10057B7B -> sub_10054940
            ns[i]->registerNativeCompatibleMethod<as_multiplay_removeFavouriteServer>("removeFavouriteServer"); // 10057BD3 -> sub_10054B20
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getMapName>("getMapName");           // 10057C83 -> sub_10057450
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setFilter>("setFilter");             // 10057FF3 -> sub_100519C0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_sort>("sort");                       // 1005804B -> sub_10051A20
            ns[i]->registerNativeCompatibleMethod<as_multiplay_refreshBrowser>("refreshBrowser");   // 100580FB -> sub_10050B70
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setHighestPing>("setHighestPing");   // 10058203 -> sub_10051480
            ns[i]->registerNativeCompatibleMethod<as_multiplay_isDownloadingDemo>("isDownloadingDemo"); // 1005830B -> sub_10053EE0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getDownloadingHostname>("getDownloadingHostname"); // 10058363 -> sub_10053F50
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getDownloadingUrl>("getDownloadingUrl"); // 100583BB -> sub_10053FE0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getDownloadingDate>("getDownloadingDate"); // 10058413 -> sub_10054070
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getDownloadingProgress>("getDownloadingProgress"); // 1005846B -> sub_10054100
            ns[i]->registerNativeCompatibleMethod<as_multiplay_downloadDemo>("downloadDemo");       // 100584C3 -> sub_10054160
            ns[i]->registerNativeCompatibleMethod<as_multiplay_deleteDemoBookmark>("deleteDemoBookmark"); // 1005851B -> sub_100542C0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getNumDemoBookmarks>("getNumDemoBookmarks"); // 10058573 -> sub_10053E50
            ns[i]->registerNativeCompatibleMethod<as_multiplay_playDemo>("playDemo");               // 100585CB -> sub_10057310
            ns[i]->registerNativeCompatibleMethod<as_multiplay_clearHistory>("clearHistory");       // 10058BA3 -> sub_10050C80
            ns[i]->registerNativeCompatibleMethod<as_multiplay_addHistoryToFavourites>("addHistoryToFavourites"); // 10058BFB -> sub_10053230
            ns[i]->registerNativeCompatibleMethod<as_multiplay_removeHistory>("removeHistory");     // 10058C53 -> sub_10053540
            ns[i]->registerNativeCompatibleMethod<as_multiplay_removeFavourite>("removeFavourite"); // 10058CAB -> sub_10053060
            ns[i]->registerNativeCompatibleMethod<as_multiplay_addFavourite>("addFavourite");       // 10058D03 -> sub_10052C80
            ns[i]->registerNativeCompatibleMethod<as_multiplay_validateIp>("validateIp");           // 10058D5B -> sub_100528A0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_playNow>("playNow");                 // 10058E0B -> sub_10052AF0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSelectedFilterGameModeId>("getSelectedFilterGameModeId"); // 10059123 -> sub_100555E0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getMapPathFromName>("getMapPathFromName"); // 10059283 -> sub_100563B0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSelectedServerFilterGameMode>("getSelectedServerFilterGameMode"); // 100592DB -> sub_10055350
            ns[i]->registerNativeCompatibleMethod<as_multiplay_isServerBrowserBusy>("isServerBrowserBusy"); // 10059333 -> sub_100526A0
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getScrollFadeSpeed>("getScrollFadeSpeed"); // 10059543 -> sub_10052450
            ns[i]->registerNativeCompatibleMethod<as_multiplay_stopDownloadDemo>("stopDownloadDemo"); // 1005959B -> sub_10050C10
            ns[i]->registerNativeCompatibleMethod<as_multiplay_deleteDemo>("deleteDemo");           // 100595F3 -> sub_10051F00
            ns[i]->registerNativeCompatibleMethod<as_multiplay_hasDemoDownloadFailed>("hasDemoDownloadFailed"); // 1005964B -> sub_100523E0
        }
        else if (::strcmp(n, "PlayerStats") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getStats>("getStats");             // 1005C2C0 -> sub_1005BBA0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_refreshStats>("refreshStats");     // 1005C323 -> sub_1005A490
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getUnlocks>("getUnlocks");         // 1005C37B -> sub_1005A9B0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_refreshUnlocks>("refreshUnlocks"); // 1005C3D3 -> sub_1005A3B0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getRankNotificationLevel>("getRankNotificationLevel"); // 1005C743 -> sub_1005A1E0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getPreviousRankLevel>("getPreviousRankLevel"); // 1005C79B -> sub_10059E00
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getNextRankLevel>("getNextRankLevel"); // 1005C7F3 -> sub_10059EE0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getRoadKills>("getRoadKills");     // 1005C8FB -> sub_1005BAE0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getOpponentKills>("getOpponentKills"); // 1005CB0B -> sub_1005B0E0
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getVictimKills>("getVictimKills"); // 1005CC13 -> sub_1005B1A0
        }
        else if (::strcmp(n, "Profile") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_profile_searchUniquePlayer>("searchUniquePlayer"); // 1006158B -> sub_10060D80
            ns[i]->registerNativeCompatibleMethod<as_profile_searchPlayer>("searchPlayer");         // 100615E3 -> sub_1005E2A0
            ns[i]->registerNativeCompatibleMethod<as_profile_setComparePlayer>("setComparePlayer"); // 1006163B -> sub_1005D350
            ns[i]->registerNativeCompatibleMethod<as_profile_clearComparePlayer>("clearComparePlayer"); // 10061693 -> sub_1005D080
            ns[i]->registerNativeCompatibleMethod<as_profile_getStatPlayer>("getStatPlayer");       // 10061743 -> sub_1005E100
            ns[i]->registerNativeCompatibleMethod<as_profile_getUseAdvancedServerBrowser>("getUseAdvancedServerBrowser"); // 1006184B -> sub_1005DDD0
            ns[i]->registerNativeCompatibleMethod<as_profile_setUseAdvancedServerBrowser>("setUseAdvancedServerBrowser"); // 100618A3 -> sub_1005D220
            ns[i]->registerNativeCompatibleMethod<as_profile_deleteLocalProfile>("deleteLocalProfile"); // 10061AB3 -> sub_1005DCE0
            ns[i]->registerNativeCompatibleMethod<as_profile_getSprint>("getSprint");               // 10061C6B -> sub_1005F4A0
            ns[i]->registerNativeCompatibleMethod<as_profile_newGamespyProfile>("newGamespyProfile"); // 10061F83 -> sub_1005DA70
            ns[i]->registerNativeCompatibleMethod<as_profile_deleteCurrentProfile>("deleteCurrentProfile"); // 10061FDB -> sub_1005D800
            ns[i]->registerNativeCompatibleMethod<as_profile_getSelectedAge>("getSelectedAge");     // 100621EB -> sub_1005D5B0
            ns[i]->registerNativeCompatibleMethod<as_profile_setSelectedAge>("setSelectedAge");     // 10062243 -> sub_1005D190
            ns[i]->registerNativeCompatibleMethod<as_profile_getMinimumAge>("getMinimumAge");       // 1006229B -> sub_1005D050
            ns[i]->registerNativeCompatibleMethod<as_profile_getNumOnlineProfiles>("getNumOnlineProfiles"); // 1006234B -> sub_1005FD90
            ns[i]->registerNativeCompatibleMethod<as_profile_getNumOfflineProfiles>("getNumOfflineProfiles"); // 100623A3 -> sub_1005FEE0
            ns[i]->registerNativeCompatibleMethod<as_profile_setNamePrefix>("setNamePrefix");       // 10062453 -> sub_1005D110
        }
        else if (::strcmp(n, "ServerInfo") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getSponsorText>("getSponsorText");  // 100631E0 -> sub_10062940
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getMapName>("getMapName");          // 100632F3 -> sub_10062BA0
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getMaxPlayers>("getMaxPlayers");    // 1006334C -> sub_10062C50
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getGameModeTitle>("getGameModeTitle"); // 100633A5 -> sub_10062CF0
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getGameModeDescription>("getGameModeDescription"); // 100633FE -> sub_10062DA0
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getBackgroundTitle>("getBackgroundTitle"); // 10063457 -> sub_10062E50
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getBackgroundDescription>("getBackgroundDescription"); // 100634B0 -> sub_10062F00
        }
        else if (::strcmp(n, "Shader") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_shader_setShader>("setShader");                // 10068B16 -> sub_10068A10
        }
        else if (::strcmp(n, "Singleplay") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_singleplay_chooseMission>("chooseMission");    // 10068DDA -> sub_10068C10
            ns[i]->registerNativeCompatibleMethod<as_singleplay_getBotSkills>("getBotSkills");      // 10068E3F -> sub_10068BF0
            ns[i]->registerNativeCompatibleMethod<as_singleplay_setBotSkills>("setBotSkills");      // 10068E9C -> sub_10068C00
        }
        else if (::strcmp(n, "Sound") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_sound_voipInitialize>("voipInitialize");       // 1006A313 -> sub_10069010
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetEnable>("voipSetEnable");         // 1006A36B -> sub_100692B0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetEnable>("voipGetEnable");         // 1006A3C3 -> sub_100693C0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetBoost>("voipSetBoost");           // 1006A41B -> sub_10069BA0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetBoost>("voipGetBoost");           // 1006A473 -> sub_10069CA0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipStartTest>("voipStartTest");         // 1006A4CB -> sub_100694E0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetTransmitVolume>("voipSetTransmitVolume"); // 1006A57B -> sub_10069750
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetTransmitVolume>("voipGetTransmitVolume"); // 1006A5D3 -> sub_10069840
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetRecieveVolume>("voipSetRecieveVolume"); // 1006A62B -> sub_100698C0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetRecieveVolume>("voipGetRecieveVolume"); // 1006A683 -> sub_100699B0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetThresholdVolume>("voipSetThresholdVolume"); // 1006A6DB -> sub_10069A30
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetThresholdVolume>("voipGetThresholdVolume"); // 1006A733 -> sub_10069B30
            ns[i]->registerNativeCompatibleMethod<as_sound_voipIsBoostEnabled>("voipIsBoostEnabled"); // 1006A83B -> sub_10069D80
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSendingVoice>("voipSendingVoice");   // 1006A893 -> sub_10069FE0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetSendingVoiceVolume>("voipGetSendingVoiceVolume"); // 1006A8EB -> sub_1006A0F0
            ns[i]->registerNativeCompatibleMethod<as_sound_voipSetUsePushToTalk>("voipSetUsePushToTalk"); // 1006A943 -> sub_10069E10
            ns[i]->registerNativeCompatibleMethod<as_sound_voipGetUsePushToTalk>("voipGetUsePushToTalk"); // 1006A99B -> sub_10069F50
        }
    }
}

// ---- dice.bf2.HapticSettings (the Falcon force-feedback page) ------------------------------------
// 265 rows, registered by the HapticSettings registrar (0x10044590). The rows fall into 31 code
// shapes; each shape was read once in full and the per-row constants (vtable slots, components)
// extracted from every row by comparing the rows of a shape constant by constant.
// IHapticSettings' slot order (Game/HapticSettings.h, 505 slots, +0x10 first) matches every
// "+XX" annotation there; the slot each row calls is named in its body.

// dword_100F8530: the page the recoil rows read (0 infantry weapons, otherwise vehicle weapons).
static int s_hapticSettingsPage = 0;

static IHapticSettings* hapticSettings()
{
    return static_cast<IHapticSettings*>(classManager->getSingleton(renddx9abi::HostString("HapticSettings")));
}

// unk_100F3870: the three threshold names (static initialiser 0x100D5D40). The setters search the
// three (`cmp edi, 54h`); the getters' loop bound is 0x16C, thirteen 28-byte slots, so a name that is
// none of the three walks into the adjacent DLL globals (0x100F38C4..) in the shipped image.
// [CHOSEN, not recovered] The port searches the three for both and answers -1 for anything else,
// which is what the setters do.
static int hapticThresholdLevel(const std::string& key)
{
    static const char* const kNames[3] = {
        "BFHQ_physicsthresholdlow", "BFHQ_physicsthresholdmedium", "BFHQ_physicsthresholdhigh" };
    for (int i = 0; i < 3; ++i)
        if (key.compare(kNames[i]) == 0)
            return i;
    return -1;
}

// The two weapon-name tables (static initialiser 0x100D5D80 / 0x100D5E60): unk_100F6C70, the 13
// infantry names (0x16C bytes), and unk_100F3310, the 7 vehicle names (0xC4 bytes). The
// getWeaponInitial rows read single slots of them (0x100F6C70 [0], 0x100F3310 [0], 0x100F3348 [2],
// 0x100F3364 [3]).
static const char* const kHapticInfantryWeapons[13] = {
    "BFHQ_assaultrifles", "BFHQ_grenadelauncher", "BFHQ_carbines", "BFHQ_lightmachineguns",
    "BFHQ_sniperrifles", "BFHQ_pistols", "BFHQ_ataa", "BFHQ_submachineguns", "BFHQ_shotguns",
    "BFHQ_knife", "BFHQ_defibrillator", "BFHQ_explosives", "BFHQ_handgrenade" };
static const char* const kHapticVehicleWeapons[7] = {
    "BFHQ_v_lightmachineguns", "BFHQ_v_heavymachineguns", "BFHQ_v_helicopterguns",
    "BFHQ_v_airplaneguns", "BFHQ_v_missiles", "BFHQ_v_tankcannons", "BFHQ_v_tankmachineguns" };

// The recoil rows' weapon class: page 0 searches the 13 infantry names and answers the index;
// another page searches the 7 vehicle names and answers index + 100 (0x64). No match answers -1.
static int hapticWeaponClass(const std::string& key)
{
    if (s_hapticSettingsPage == 0)
    {
        for (int i = 0; i < 13; ++i)
            if (key.compare(kHapticInfantryWeapons[i]) == 0)
                return i;
        return -1;
    }
    for (int i = 0; i < 7; ++i)
        if (key.compare(kHapticVehicleWeapons[i]) == 0)
            return i + 100;
    return -1;
}

// The two list builders are declared with the other getListEntries branches inside SwiffPlayer.cpp's
// anonymous namespace; their definitions join it.
namespace {

// One selector row (both branches): the row keyed by the table name (sub_1001C770), its localised
// text as a dice String column (sub_10013600 + sub_1001CB80), and "selected" = 1 on the first row
// only (sub_10019970). The running count starts at 0 (edi, stored over the third argument at
// 0x1002289C) and is the published totalCount (sub_10019B00).
static void pushHapticSelectRow(ListResult& r, ListHost h, const char* name, int& count)
{
    r.beginRowStr(name);
    pushLocalised(r, h, name);
    if (count == 0)
        r.setInt("selected", 1);
    ++count;
}

// General.getListEntries "weaponselect" (0x10030F66..0x1003136F): the rows follow the page
// (dword_100F8530, jump table 0x100316B0). 0: the 13 infantry names. 1 (land): the vehicle names
// except [2] helicopter and [3] airplane guns. 2 (air): [3] airplane guns and [4] missiles.
// 3 (heli): [2] helicopter guns and [4] missiles. 4 (sea): [0] alone. Any other page: no row.
static void listHapticWeaponSelect(const gameswf::fn_call& fn, ListResult& r, ListHost h)
{
    r.begin();
    int count = 0;
    switch (s_hapticSettingsPage)
    {
    case 0:
        for (int i = 0; i < 13; ++i)
            pushHapticSelectRow(r, h, kHapticInfantryWeapons[i], count);
        break;
    case 1:
        for (int i = 0; i < 7; ++i)
            if (i != 2 && i != 3)
                pushHapticSelectRow(r, h, kHapticVehicleWeapons[i], count);
        break;
    case 2:
        for (int i = 0; i < 7; ++i)
            if (i == 3 || i == 4)
                pushHapticSelectRow(r, h, kHapticVehicleWeapons[i], count);
        break;
    case 3:
        for (int i = 0; i < 7; ++i)
            if (i == 2 || i == 4)
                pushHapticSelectRow(r, h, kHapticVehicleWeapons[i], count);
        break;
    case 4:
        pushHapticSelectRow(r, h, kHapticVehicleWeapons[0], count);
        break;
    default:
        break;
    }
    r.setTotalCount(count);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// General.getListEntries "thresholdselect" (0x10031374..0x1003147B): the three threshold names
// (unk_100F3870), the first selected; every page.
static void listHapticThresholdSelect(const gameswf::fn_call& fn, ListResult& r, ListHost h)
{
    static const char* const kNames[3] = {
        "BFHQ_physicsthresholdlow", "BFHQ_physicsthresholdmedium", "BFHQ_physicsthresholdhigh" };
    r.begin();
    int count = 0;
    for (int i = 0; i < 3; ++i)
        pushHapticSelectRow(r, h, kNames[i], count);
    r.setTotalCount(count);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

} // namespace

// apply / reset tail (0x10035176..0x100351B4): LocalProfileManager by name; present with a current
// profile (+0x44): +0x40 saveCurrentProfile.
static void hapticSaveCurrentProfile()
{
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    if (lpm && static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
        (void) static_cast<ILocalProfileManager*>(lpm)->saveCurrentProfile();
}

// The selection page state: two DLL-global std::strings, default-constructed empty by the static
// initialisers 0x100D6250 / 0x100D6270 and touched by no other code. setWeaponSelect* stores the
// name into the first (0x100F8550, text at +4 = unk_100F8554); setThresholdSelect* into the second
// (0x100F8534, text unk_100F8538). All five categories share each string.
static std::string s_hapticWeaponSelect;      // 0x100F8550
static std::string s_hapticThresholdSelect;   // 0x100F8534

// getWeaponInitial* (sub_10036D10 and its four twins): arg 0 to_number, fistp under a chop control
// word; 1 publishes the table name as a plain string (as_value type 1), anything else the name's
// dice String through sub_10036B70 - a third copy of the localised-String helper the VideoSettings
// rows use (sub_1006B310): the player's host first, an empty key's narrow String, otherwise the +0xA8
// localisation in a wide one.
static void publishHapticInitialName(const gameswf::fn_call& fn, const char* name)
{
    if (bf2x87::toIntTruncate(fn.arg(0).to_number()) == 1)
        fn.result->set_tu_string(tu_string(name));
    else
        fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString(name)));
}

// setThresholdSelect* (sub_10036DF0 and its four twins): arg 0 to_string; the result is the bool
// _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, published before the name is stored.
static void storeHapticThresholdSelect(const gameswf::fn_call& fn)
{
    const std::string name(fn.arg(0).to_string());
    fn.result->set_bool(::_stricmp(name.c_str(), "BFHQ_physicsthresholdhigh") == 0);
    s_hapticThresholdSelect = name;
}

typedef Vec3 (IHapticSettings::*HapticVecGetter)();
typedef void (IHapticSettings::*HapticVecSetter)(float x, float y, float z);

// One vector store, the block every setter runs once for its own slot and, on a match, once more
// for the generic slot: HapticSettings by name three times, the getter twice (a, b), then the setter
// with the new value in its component and the rest taken from a and b: X (v, b.y, a.z),
// Y (b.x, v, a.z), Z (b.x, a.y, v). The mirror passes of the vehicle rows reach the singleton through
// sub_10035100, the same lookup.
static void hapticStoreComponent(HapticVecGetter get, HapticVecSetter set, int component, float value)
{
    IHapticSettings* first = hapticSettings();
    IHapticSettings* second = hapticSettings();
    IHapticSettings* target = hapticSettings();
    const Vec3 a = (first->*get)();
    const Vec3 b = (second->*get)();
    if (component == 0)
        (target->*set)(value, b.y, a.z);
    else if (component == 1)
        (target->*set)(b.x, value, a.z);
    else
        (target->*set)(b.x, a.y, value);
}

// The local player's vehicle category the vehicle rows compare against (template +0x22C, the
// IPlayerControlObjectTemplate base, slot +0x24): LAND 0 (test eax, eax), SEA 1, AIR 2, HELI 3.
// SOLDIER rows test "on foot" instead.
enum HapticMirrorCategory { kHapticSoldier = -1, kHapticLand = 0, kHapticSea = 1, kHapticAir = 2, kHapticHeli = 3 };

// The 75 vector setters (sub_1003C5F0 .. sub_100436B0, one shape; constants per row checked against
// every row's disassembly): arg 0 to_number stored as float into the category slot, then
// playerManager +0x30 getLocalHumanPlayer.
//  * SOLDIER rows: no local player, or the player on foot (+0x44 getVehicle == +0xA0
//    getDefaultVehicle): the value is mirrored into the generic slot. In a vehicle: nothing more.
//  * Vehicle rows: no local player: nothing more. Otherwise the vehicle's template field (+0x14) is
//    read before the on-foot test, on foot ends the row, and a vehicle whose template is taken as
//    a PlayerControlObjectTemplate with no check (the shipped add 0x22C) and whose
//    getVehicleCategory equals the row's category gets the mirror (port guard: see the body).
static void hapticVectorSetter(const gameswf::fn_call& fn, HapticVecGetter get, HapticVecSetter set,
                               int component, int category,
                               HapticVecGetter mirrorGet, HapticVecSetter mirrorSet)
{
    const float value = static_cast<float>(fn.arg(0).to_number());
    hapticStoreComponent(get, set, component, value);
    world::IPlayer* player = reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer(); // +0x30
    if (category == kHapticSoldier)
    {
        if (player && player->getVehicle() != player->getDefaultVehicle())                  // +0x44 / +0xA0
            return;
        hapticStoreComponent(mirrorGet, mirrorSet, component, value);
        return;
    }
    if (!player)
        return;
    world::IObject* vehicle = player->getVehicle();                                          // +0x44
    // [CHOSEN, not recovered] Retail reads the template field (+0x14) here with no null test and,
    // after the on-foot test, takes template + 0x22C as the IPlayerControlObjectTemplate base with
    // no type test. A dead player's vehicle is the free camera (template MultiPlayerFreeCamera, a
    // FreeCameraTemplate of 0x188 bytes): retail's +0x22C read lands past that object in whatever
    // heap memory follows; the 64-bit port's layout puts a non-canonical pointer there (GP fault
    // in the owner's in-game APPLY while dead, 2026-11-02). Neither out-of-bounds value can be
    // reproduced, so the port asks the template for the interface (as SupplyObjectTemplate::
    // testVehicleType does) and treats a missing vehicle, a missing template or a template that is
    // not a PlayerControlObjectTemplate as "category does not match": no mirror. The MSVC x86
    // build keeps the shipped code (byte-identical goal); only non-x86 builds take the guard.
#if defined(_MSC_VER) && defined(_M_IX86)
    // Shipped x86 path, unchanged (no null or type test).
    const world::IObjectTemplate* tmpl = vehicle->getTemplate();                             // +0x14
    if (vehicle == player->getDefaultVehicle())                                              // +0xA0
        return;
    const world::IPlayerControlObjectTemplate* pco =
        static_cast<const world::PlayerControlObjectTemplate*>(tmpl);                        // +0x22C
    if (pco->getVehicleCategory() != category)                                               // +0x24
        return;
#else
    if (!vehicle)
        return;
    const world::IObjectTemplate* tmpl = vehicle->getTemplate();                             // +0x14
    if (vehicle == player->getDefaultVehicle())                                              // +0xA0
        return;
    const world::IPlayerControlObjectTemplate* pco = tmpl
        ? static_cast<const world::IPlayerControlObjectTemplate*>(
              tmpl->queryInterface(world::IID_IPlayerControlObjectTemplate))                 // retail: +0x22C
        : 0;
    if (!pco || pco->getVehicleCategory() != category)                                       // +0x24
        return;
#endif
    hapticStoreComponent(mirrorGet, mirrorSet, component, value);
}

// sub_10035140 "apply": HapticSettings +0x18, then LocalProfileManager by name: present with a current profile (+0x44), +0x40 saveCurrentProfile.
void as_hapticsettings_apply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_apply");
    (void) hapticSettings()->apply();                    // +0x18
    hapticSaveCurrentProfile();
}

// sub_100351C0 "reset": HapticSettings +0x20, then LocalProfileManager by name: present with a current profile (+0x44), +0x40 saveCurrentProfile.
void as_hapticsettings_reset(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_reset");
    hapticSettings()->setDefaultValues();                    // +0x20
    hapticSaveCurrentProfile();
}

// sub_10035240 "cancel": HapticSettings +0x24 loadCurrentProfileFile.
void as_hapticsettings_cancel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_cancel");
    hapticSettings()->loadCurrentProfileFile();                       // +0x24
}

// sub_10037AB0 "getControlBoxLengthSOLDIER_X": HapticSettings by name, +0x54 getControlBoxLengthSOLDIER() by value, component x as a number.
void as_hapticsettings_getControlBoxLengthSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSOLDIER_X");
    const Vec3 v = hapticSettings()->getControlBoxLengthSOLDIER();        // +0x54 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037B30 "getControlBoxStiffnessSOLDIER_X": HapticSettings by name, +0x5C getControlBoxStiffnessSOLDIER() by value, component x as a number.
void as_hapticsettings_getControlBoxStiffnessSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSOLDIER_X");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSOLDIER();        // +0x5C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037BB0 "getControlBoxLengthLAND_X": HapticSettings by name, +0x84 getControlBoxLengthLAND() by value, component x as a number.
void as_hapticsettings_getControlBoxLengthLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthLAND_X");
    const Vec3 v = hapticSettings()->getControlBoxLengthLAND();        // +0x84 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037C40 "getControlBoxStiffnessLAND_X": HapticSettings by name, +0x8C getControlBoxStiffnessLAND() by value, component x as a number.
void as_hapticsettings_getControlBoxStiffnessLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessLAND_X");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessLAND();        // +0x8C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037CD0 "getControlBoxLengthAIR_X": HapticSettings by name, +0x64 getControlBoxLengthAIR() by value, component x as a number.
void as_hapticsettings_getControlBoxLengthAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthAIR_X");
    const Vec3 v = hapticSettings()->getControlBoxLengthAIR();        // +0x64 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037D50 "getControlBoxStiffnessAIR_X": HapticSettings by name, +0x6C getControlBoxStiffnessAIR() by value, component x as a number.
void as_hapticsettings_getControlBoxStiffnessAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessAIR_X");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessAIR();        // +0x6C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037DD0 "getControlBoxLengthHELI_X": HapticSettings by name, +0x74 getControlBoxLengthHELI() by value, component x as a number.
void as_hapticsettings_getControlBoxLengthHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthHELI_X");
    const Vec3 v = hapticSettings()->getControlBoxLengthHELI();        // +0x74 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037E50 "getControlBoxStiffnessHELI_X": HapticSettings by name, +0x7C getControlBoxStiffnessHELI() by value, component x as a number.
void as_hapticsettings_getControlBoxStiffnessHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessHELI_X");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessHELI();        // +0x7C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037ED0 "getControlBoxLengthSEA_X": HapticSettings by name, +0x94 getControlBoxLengthSEA() by value, component x as a number.
void as_hapticsettings_getControlBoxLengthSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSEA_X");
    const Vec3 v = hapticSettings()->getControlBoxLengthSEA();        // +0x94 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037F60 "getControlBoxStiffnessSEA_X": HapticSettings by name, +0x9C getControlBoxStiffnessSEA() by value, component x as a number.
void as_hapticsettings_getControlBoxStiffnessSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSEA_X");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSEA();        // +0x9C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10037FF0 "getControlBoxLengthSOLDIER_Y": HapticSettings by name, +0x54 getControlBoxLengthSOLDIER() by value, component y as a number.
void as_hapticsettings_getControlBoxLengthSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSOLDIER_Y");
    const Vec3 v = hapticSettings()->getControlBoxLengthSOLDIER();        // +0x54 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038070 "getControlBoxStiffnessSOLDIER_Y": HapticSettings by name, +0x5C getControlBoxStiffnessSOLDIER() by value, component y as a number.
void as_hapticsettings_getControlBoxStiffnessSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSOLDIER_Y");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSOLDIER();        // +0x5C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100380F0 "getControlBoxLengthLAND_Y": HapticSettings by name, +0x84 getControlBoxLengthLAND() by value, component y as a number.
void as_hapticsettings_getControlBoxLengthLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthLAND_Y");
    const Vec3 v = hapticSettings()->getControlBoxLengthLAND();        // +0x84 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038180 "getControlBoxStiffnessLAND_Y": HapticSettings by name, +0x8C getControlBoxStiffnessLAND() by value, component y as a number.
void as_hapticsettings_getControlBoxStiffnessLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessLAND_Y");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessLAND();        // +0x8C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038210 "getControlBoxLengthAIR_Y": HapticSettings by name, +0x64 getControlBoxLengthAIR() by value, component y as a number.
void as_hapticsettings_getControlBoxLengthAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthAIR_Y");
    const Vec3 v = hapticSettings()->getControlBoxLengthAIR();        // +0x64 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038290 "getControlBoxStiffnessAIR_Y": HapticSettings by name, +0x6C getControlBoxStiffnessAIR() by value, component y as a number.
void as_hapticsettings_getControlBoxStiffnessAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessAIR_Y");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessAIR();        // +0x6C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038310 "getControlBoxLengthHELI_Y": HapticSettings by name, +0x74 getControlBoxLengthHELI() by value, component y as a number.
void as_hapticsettings_getControlBoxLengthHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthHELI_Y");
    const Vec3 v = hapticSettings()->getControlBoxLengthHELI();        // +0x74 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038390 "getControlBoxStiffnessHELI_Y": HapticSettings by name, +0x7C getControlBoxStiffnessHELI() by value, component y as a number.
void as_hapticsettings_getControlBoxStiffnessHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessHELI_Y");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessHELI();        // +0x7C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038410 "getControlBoxLengthSEA_Y": HapticSettings by name, +0x94 getControlBoxLengthSEA() by value, component y as a number.
void as_hapticsettings_getControlBoxLengthSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSEA_Y");
    const Vec3 v = hapticSettings()->getControlBoxLengthSEA();        // +0x94 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100384A0 "getControlBoxStiffnessSEA_Y": HapticSettings by name, +0x9C getControlBoxStiffnessSEA() by value, component y as a number.
void as_hapticsettings_getControlBoxStiffnessSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSEA_Y");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSEA();        // +0x9C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10038530 "getControlBoxLengthSOLDIER_Z": HapticSettings by name, +0x54 getControlBoxLengthSOLDIER() by value, component z as a number.
void as_hapticsettings_getControlBoxLengthSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSOLDIER_Z");
    const Vec3 v = hapticSettings()->getControlBoxLengthSOLDIER();        // +0x54 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_100385B0 "getControlBoxStiffnessSOLDIER_Z": HapticSettings by name, +0x5C getControlBoxStiffnessSOLDIER() by value, component z as a number.
void as_hapticsettings_getControlBoxStiffnessSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSOLDIER_Z");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSOLDIER();        // +0x5C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10038630 "getControlBoxLengthLAND_Z": HapticSettings by name, +0x84 getControlBoxLengthLAND() by value, component z as a number.
void as_hapticsettings_getControlBoxLengthLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthLAND_Z");
    const Vec3 v = hapticSettings()->getControlBoxLengthLAND();        // +0x84 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_100386C0 "getControlBoxStiffnessLAND_Z": HapticSettings by name, +0x8C getControlBoxStiffnessLAND() by value, component z as a number.
void as_hapticsettings_getControlBoxStiffnessLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessLAND_Z");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessLAND();        // +0x8C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10038750 "getControlBoxLengthAIR_Z": HapticSettings by name, +0x64 getControlBoxLengthAIR() by value, component z as a number.
void as_hapticsettings_getControlBoxLengthAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthAIR_Z");
    const Vec3 v = hapticSettings()->getControlBoxLengthAIR();        // +0x64 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_100387D0 "getControlBoxStiffnessAIR_Z": HapticSettings by name, +0x6C getControlBoxStiffnessAIR() by value, component z as a number.
void as_hapticsettings_getControlBoxStiffnessAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessAIR_Z");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessAIR();        // +0x6C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10038850 "getControlBoxLengthHELI_Z": HapticSettings by name, +0x74 getControlBoxLengthHELI() by value, component z as a number.
void as_hapticsettings_getControlBoxLengthHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthHELI_Z");
    const Vec3 v = hapticSettings()->getControlBoxLengthHELI();        // +0x74 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_100388D0 "getControlBoxStiffnessHELI_Z": HapticSettings by name, +0x7C getControlBoxStiffnessHELI() by value, component z as a number.
void as_hapticsettings_getControlBoxStiffnessHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessHELI_Z");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessHELI();        // +0x7C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10038950 "getControlBoxLengthSEA_Z": HapticSettings by name, +0x94 getControlBoxLengthSEA() by value, component z as a number.
void as_hapticsettings_getControlBoxLengthSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxLengthSEA_Z");
    const Vec3 v = hapticSettings()->getControlBoxLengthSEA();        // +0x94 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_100389E0 "getControlBoxStiffnessSEA_Z": HapticSettings by name, +0x9C getControlBoxStiffnessSEA() by value, component z as a number.
void as_hapticsettings_getControlBoxStiffnessSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getControlBoxStiffnessSEA_Z");
    const Vec3 v = hapticSettings()->getControlBoxStiffnessSEA();        // +0x9C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10038A70 "getAimSensitivitySOLDIER_X": HapticSettings by name, +0xBC getAimSensitivitySOLDIER() by value, component x as a number.
void as_hapticsettings_getAimSensitivitySOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySOLDIER_X");
    const Vec3 v = hapticSettings()->getAimSensitivitySOLDIER();        // +0xBC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038B00 "getAimSensitivityLengthSOLDIER_X": HapticSettings by name, +0xC4 getAimSensitivityLengthSOLDIER() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLengthSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSOLDIER_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSOLDIER();        // +0xC4 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038B90 "getTurnSensitivitySOLDIER_X": HapticSettings by name, +0xCC getTurnSensitivitySOLDIER() by value, component x as a number.
void as_hapticsettings_getTurnSensitivitySOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySOLDIER_X");
    const Vec3 v = hapticSettings()->getTurnSensitivitySOLDIER();        // +0xCC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038C20 "getAimSensitivityLAND_X": HapticSettings by name, +0x104 getAimSensitivityLAND() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLAND_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLAND();        // +0x104 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038CB0 "getAimSensitivityLengthLAND_X": HapticSettings by name, +0x10C getAimSensitivityLengthLAND() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLengthLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthLAND_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthLAND();        // +0x10C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038D40 "getTurnSensitivityLAND_X": HapticSettings by name, +0x114 getTurnSensitivityLAND() by value, component x as a number.
void as_hapticsettings_getTurnSensitivityLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityLAND_X");
    const Vec3 v = hapticSettings()->getTurnSensitivityLAND();        // +0x114 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038DD0 "getAimSensitivityAIR_X": HapticSettings by name, +0xD4 getAimSensitivityAIR() by value, component x as a number.
void as_hapticsettings_getAimSensitivityAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityAIR_X");
    const Vec3 v = hapticSettings()->getAimSensitivityAIR();        // +0xD4 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038E60 "getAimSensitivityLengthAIR_X": HapticSettings by name, +0xDC getAimSensitivityLengthAIR() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLengthAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthAIR_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthAIR();        // +0xDC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038EF0 "getTurnSensitivityAIR_X": HapticSettings by name, +0xE4 getTurnSensitivityAIR() by value, component x as a number.
void as_hapticsettings_getTurnSensitivityAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityAIR_X");
    const Vec3 v = hapticSettings()->getTurnSensitivityAIR();        // +0xE4 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10038F80 "getAimSensitivityHELI_X": HapticSettings by name, +0xEC getAimSensitivityHELI() by value, component x as a number.
void as_hapticsettings_getAimSensitivityHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityHELI_X");
    const Vec3 v = hapticSettings()->getAimSensitivityHELI();        // +0xEC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10039010 "getAimSensitivityLengthHELI_X": HapticSettings by name, +0xF4 getAimSensitivityLengthHELI() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLengthHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthHELI_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthHELI();        // +0xF4 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_100390A0 "getTurnSensitivityHELI_X": HapticSettings by name, +0xFC getTurnSensitivityHELI() by value, component x as a number.
void as_hapticsettings_getTurnSensitivityHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityHELI_X");
    const Vec3 v = hapticSettings()->getTurnSensitivityHELI();        // +0xFC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10039130 "getAimSensitivitySEA_X": HapticSettings by name, +0x11C getAimSensitivitySEA() by value, component x as a number.
void as_hapticsettings_getAimSensitivitySEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySEA_X");
    const Vec3 v = hapticSettings()->getAimSensitivitySEA();        // +0x11C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_100391C0 "getAimSensitivityLengthSEA_X": HapticSettings by name, +0x124 getAimSensitivityLengthSEA() by value, component x as a number.
void as_hapticsettings_getAimSensitivityLengthSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSEA_X");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSEA();        // +0x124 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_10039250 "getTurnSensitivitySEA_X": HapticSettings by name, +0x12C getTurnSensitivitySEA() by value, component x as a number.
void as_hapticsettings_getTurnSensitivitySEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySEA_X");
    const Vec3 v = hapticSettings()->getTurnSensitivitySEA();        // +0x12C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_100392E0 "getAimSensitivitySOLDIER_Y": HapticSettings by name, +0xBC getAimSensitivitySOLDIER() by value, component y as a number.
void as_hapticsettings_getAimSensitivitySOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySOLDIER_Y");
    const Vec3 v = hapticSettings()->getAimSensitivitySOLDIER();        // +0xBC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039370 "getAimSensitivityLengthSOLDIER_Y": HapticSettings by name, +0xC4 getAimSensitivityLengthSOLDIER() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLengthSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSOLDIER_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSOLDIER();        // +0xC4 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039400 "getTurnSensitivitySOLDIER_Y": HapticSettings by name, +0xCC getTurnSensitivitySOLDIER() by value, component y as a number.
void as_hapticsettings_getTurnSensitivitySOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySOLDIER_Y");
    const Vec3 v = hapticSettings()->getTurnSensitivitySOLDIER();        // +0xCC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039490 "getAimSensitivityLAND_Y": HapticSettings by name, +0x104 getAimSensitivityLAND() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLAND_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLAND();        // +0x104 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039520 "getAimSensitivityLengthLAND_Y": HapticSettings by name, +0x10C getAimSensitivityLengthLAND() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLengthLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthLAND_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthLAND();        // +0x10C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100395B0 "getTurnSensitivityLAND_Y": HapticSettings by name, +0x114 getTurnSensitivityLAND() by value, component y as a number.
void as_hapticsettings_getTurnSensitivityLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityLAND_Y");
    const Vec3 v = hapticSettings()->getTurnSensitivityLAND();        // +0x114 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039640 "getAimSensitivityAIR_Y": HapticSettings by name, +0xD4 getAimSensitivityAIR() by value, component y as a number.
void as_hapticsettings_getAimSensitivityAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityAIR_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityAIR();        // +0xD4 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100396D0 "getAimSensitivityLengthAIR_Y": HapticSettings by name, +0xDC getAimSensitivityLengthAIR() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLengthAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthAIR_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthAIR();        // +0xDC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039760 "getTurnSensitivityAIR_Y": HapticSettings by name, +0xE4 getTurnSensitivityAIR() by value, component y as a number.
void as_hapticsettings_getTurnSensitivityAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityAIR_Y");
    const Vec3 v = hapticSettings()->getTurnSensitivityAIR();        // +0xE4 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100397F0 "getAimSensitivityHELI_Y": HapticSettings by name, +0xEC getAimSensitivityHELI() by value, component y as a number.
void as_hapticsettings_getAimSensitivityHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityHELI_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityHELI();        // +0xEC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039880 "getAimSensitivityLengthHELI_Y": HapticSettings by name, +0xF4 getAimSensitivityLengthHELI() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLengthHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthHELI_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthHELI();        // +0xF4 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039910 "getTurnSensitivityHELI_Y": HapticSettings by name, +0xFC getTurnSensitivityHELI() by value, component y as a number.
void as_hapticsettings_getTurnSensitivityHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityHELI_Y");
    const Vec3 v = hapticSettings()->getTurnSensitivityHELI();        // +0xFC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_100399A0 "getAimSensitivitySEA_Y": HapticSettings by name, +0x11C getAimSensitivitySEA() by value, component y as a number.
void as_hapticsettings_getAimSensitivitySEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySEA_Y");
    const Vec3 v = hapticSettings()->getAimSensitivitySEA();        // +0x11C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039A30 "getAimSensitivityLengthSEA_Y": HapticSettings by name, +0x124 getAimSensitivityLengthSEA() by value, component y as a number.
void as_hapticsettings_getAimSensitivityLengthSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSEA_Y");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSEA();        // +0x124 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039AC0 "getTurnSensitivitySEA_Y": HapticSettings by name, +0x12C getTurnSensitivitySEA() by value, component y as a number.
void as_hapticsettings_getTurnSensitivitySEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySEA_Y");
    const Vec3 v = hapticSettings()->getTurnSensitivitySEA();        // +0x12C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_10039B50 "getAimSensitivitySOLDIER_Z": HapticSettings by name, +0xBC getAimSensitivitySOLDIER() by value, component z as a number.
void as_hapticsettings_getAimSensitivitySOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySOLDIER_Z");
    const Vec3 v = hapticSettings()->getAimSensitivitySOLDIER();        // +0xBC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039BE0 "getAimSensitivityLengthSOLDIER_Z": HapticSettings by name, +0xC4 getAimSensitivityLengthSOLDIER() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLengthSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSOLDIER_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSOLDIER();        // +0xC4 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039C70 "getTurnSensitivitySOLDIER_Z": HapticSettings by name, +0xCC getTurnSensitivitySOLDIER() by value, component z as a number.
void as_hapticsettings_getTurnSensitivitySOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySOLDIER_Z");
    const Vec3 v = hapticSettings()->getTurnSensitivitySOLDIER();        // +0xCC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039D00 "getAimSensitivityLAND_Z": HapticSettings by name, +0x104 getAimSensitivityLAND() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLAND_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLAND();        // +0x104 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039D90 "getAimSensitivityLengthLAND_Z": HapticSettings by name, +0x10C getAimSensitivityLengthLAND() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLengthLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthLAND_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthLAND();        // +0x10C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039E20 "getTurnSensitivityLAND_Z": HapticSettings by name, +0x114 getTurnSensitivityLAND() by value, component z as a number.
void as_hapticsettings_getTurnSensitivityLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityLAND_Z");
    const Vec3 v = hapticSettings()->getTurnSensitivityLAND();        // +0x114 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039EB0 "getAimSensitivityAIR_Z": HapticSettings by name, +0xD4 getAimSensitivityAIR() by value, component z as a number.
void as_hapticsettings_getAimSensitivityAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityAIR_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityAIR();        // +0xD4 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039F40 "getAimSensitivityLengthAIR_Z": HapticSettings by name, +0xDC getAimSensitivityLengthAIR() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLengthAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthAIR_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthAIR();        // +0xDC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_10039FD0 "getTurnSensitivityAIR_Z": HapticSettings by name, +0xE4 getTurnSensitivityAIR() by value, component z as a number.
void as_hapticsettings_getTurnSensitivityAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityAIR_Z");
    const Vec3 v = hapticSettings()->getTurnSensitivityAIR();        // +0xE4 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A060 "getAimSensitivityHELI_Z": HapticSettings by name, +0xEC getAimSensitivityHELI() by value, component z as a number.
void as_hapticsettings_getAimSensitivityHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityHELI_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityHELI();        // +0xEC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A0F0 "getAimSensitivityLengthHELI_Z": HapticSettings by name, +0xF4 getAimSensitivityLengthHELI() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLengthHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthHELI_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthHELI();        // +0xF4 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A180 "getTurnSensitivityHELI_Z": HapticSettings by name, +0xFC getTurnSensitivityHELI() by value, component z as a number.
void as_hapticsettings_getTurnSensitivityHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivityHELI_Z");
    const Vec3 v = hapticSettings()->getTurnSensitivityHELI();        // +0xFC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A210 "getAimSensitivitySEA_Z": HapticSettings by name, +0x11C getAimSensitivitySEA() by value, component z as a number.
void as_hapticsettings_getAimSensitivitySEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivitySEA_Z");
    const Vec3 v = hapticSettings()->getAimSensitivitySEA();        // +0x11C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A2A0 "getAimSensitivityLengthSEA_Z": HapticSettings by name, +0x124 getAimSensitivityLengthSEA() by value, component z as a number.
void as_hapticsettings_getAimSensitivityLengthSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getAimSensitivityLengthSEA_Z");
    const Vec3 v = hapticSettings()->getAimSensitivityLengthSEA();        // +0x124 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A330 "getTurnSensitivitySEA_Z": HapticSettings by name, +0x12C getTurnSensitivitySEA() by value, component z as a number.
void as_hapticsettings_getTurnSensitivitySEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getTurnSensitivitySEA_Z");
    const Vec3 v = hapticSettings()->getTurnSensitivitySEA();        // +0x12C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A3C0 "getPhysicsSOLDIERScale_X": HapticSettings by name, +0x134 getPhysicsSOLDIERScale() by value, component x as a number.
void as_hapticsettings_getPhysicsSOLDIERScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERScale_X");
    const Vec3 v = hapticSettings()->getPhysicsSOLDIERScale();        // +0x134 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_1003A450 "getPhysicsSOLDIERScale_Y": HapticSettings by name, +0x134 getPhysicsSOLDIERScale() by value, component y as a number.
void as_hapticsettings_getPhysicsSOLDIERScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERScale_Y");
    const Vec3 v = hapticSettings()->getPhysicsSOLDIERScale();        // +0x134 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_1003A4E0 "getPhysicsSOLDIERScale_Z": HapticSettings by name, +0x134 getPhysicsSOLDIERScale() by value, component z as a number.
void as_hapticsettings_getPhysicsSOLDIERScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERScale_Z");
    const Vec3 v = hapticSettings()->getPhysicsSOLDIERScale();        // +0x134 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003A570 "getPhysicsSOLDIERTransition": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x184 getPhysicsSOLDIERTransitionValue(level) as a number.
void as_hapticsettings_getPhysicsSOLDIERTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERTransition");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSOLDIERTransitionValue(level)));    // +0x184
}

// sub_1003A670 "getPhysicsSOLDIERSmoothTime_X": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x19C getPhysicsSOLDIERSmoothTimeX(level) as a number.
void as_hapticsettings_getPhysicsSOLDIERSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSOLDIERSmoothTimeX(level)));    // +0x19C
}

// sub_1003A770 "getPhysicsSOLDIERSmoothTime_Y": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x1A4 getPhysicsSOLDIERSmoothTimeY(level) as a number.
void as_hapticsettings_getPhysicsSOLDIERSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSOLDIERSmoothTimeY(level)));    // +0x1A4
}

// sub_1003A870 "getPhysicsSOLDIERSmoothTime_Z": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x1AC getPhysicsSOLDIERSmoothTimeZ(level) as a number.
void as_hapticsettings_getPhysicsSOLDIERSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSOLDIERSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSOLDIERSmoothTimeZ(level)));    // +0x1AC
}

// sub_1003A970 "getPhysicsLANDScale_X": HapticSettings by name, +0x1FC getPhysicsLANDScale() by value, component x as a number.
void as_hapticsettings_getPhysicsLANDScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDScale_X");
    const Vec3 v = hapticSettings()->getPhysicsLANDScale();        // +0x1FC (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_1003AA00 "getPhysicsLANDScale_Y": HapticSettings by name, +0x1FC getPhysicsLANDScale() by value, component y as a number.
void as_hapticsettings_getPhysicsLANDScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDScale_Y");
    const Vec3 v = hapticSettings()->getPhysicsLANDScale();        // +0x1FC (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_1003AA90 "getPhysicsLANDScale_Z": HapticSettings by name, +0x1FC getPhysicsLANDScale() by value, component z as a number.
void as_hapticsettings_getPhysicsLANDScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDScale_Z");
    const Vec3 v = hapticSettings()->getPhysicsLANDScale();        // +0x1FC (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003AB20 "getPhysicsLANDTransition": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x24C getPhysicsLANDTransitionValue(level) as a number.
void as_hapticsettings_getPhysicsLANDTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDTransition");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsLANDTransitionValue(level)));    // +0x24C
}

// sub_1003AC20 "getPhysicsLANDSmoothTime_X": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x264 getPhysicsLANDSmoothTimeX(level) as a number.
void as_hapticsettings_getPhysicsLANDSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsLANDSmoothTimeX(level)));    // +0x264
}

// sub_1003AD20 "getPhysicsLANDSmoothTime_Y": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x26C getPhysicsLANDSmoothTimeY(level) as a number.
void as_hapticsettings_getPhysicsLANDSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsLANDSmoothTimeY(level)));    // +0x26C
}

// sub_1003AE20 "getPhysicsLANDSmoothTime_Z": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x274 getPhysicsLANDSmoothTimeZ(level) as a number.
void as_hapticsettings_getPhysicsLANDSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsLANDSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsLANDSmoothTimeZ(level)));    // +0x274
}

// sub_1003AF20 "getPhysicsAIRScale_X": HapticSettings by name, +0x2C4 getPhysicsAIRScale() by value, component x as a number.
void as_hapticsettings_getPhysicsAIRScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRScale_X");
    const Vec3 v = hapticSettings()->getPhysicsAIRScale();        // +0x2C4 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_1003AFB0 "getPhysicsAIRScale_Y": HapticSettings by name, +0x2C4 getPhysicsAIRScale() by value, component y as a number.
void as_hapticsettings_getPhysicsAIRScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRScale_Y");
    const Vec3 v = hapticSettings()->getPhysicsAIRScale();        // +0x2C4 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_1003B040 "getPhysicsAIRScale_Z": HapticSettings by name, +0x2C4 getPhysicsAIRScale() by value, component z as a number.
void as_hapticsettings_getPhysicsAIRScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRScale_Z");
    const Vec3 v = hapticSettings()->getPhysicsAIRScale();        // +0x2C4 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003B0D0 "getPhysicsAIRTransition": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x314 getPhysicsAIRTransitionValue(level) as a number.
void as_hapticsettings_getPhysicsAIRTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRTransition");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsAIRTransitionValue(level)));    // +0x314
}

// sub_1003B1D0 "getPhysicsAIRSmoothTime_X": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x32C getPhysicsAIRSmoothTimeX(level) as a number.
void as_hapticsettings_getPhysicsAIRSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsAIRSmoothTimeX(level)));    // +0x32C
}

// sub_1003B2D0 "getPhysicsAIRSmoothTime_Y": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x334 getPhysicsAIRSmoothTimeY(level) as a number.
void as_hapticsettings_getPhysicsAIRSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsAIRSmoothTimeY(level)));    // +0x334
}

// sub_1003B3D0 "getPhysicsAIRSmoothTime_Z": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x33C getPhysicsAIRSmoothTimeZ(level) as a number.
void as_hapticsettings_getPhysicsAIRSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsAIRSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsAIRSmoothTimeZ(level)));    // +0x33C
}

// sub_1003B4D0 "getPhysicsHELIScale_X": HapticSettings by name, +0x38C getPhysicsHELIScale() by value, component x as a number.
void as_hapticsettings_getPhysicsHELIScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELIScale_X");
    const Vec3 v = hapticSettings()->getPhysicsHELIScale();        // +0x38C (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_1003B560 "getPhysicsHELIScale_Y": HapticSettings by name, +0x38C getPhysicsHELIScale() by value, component y as a number.
void as_hapticsettings_getPhysicsHELIScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELIScale_Y");
    const Vec3 v = hapticSettings()->getPhysicsHELIScale();        // +0x38C (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_1003B5F0 "getPhysicsHELIScale_Z": HapticSettings by name, +0x38C getPhysicsHELIScale() by value, component z as a number.
void as_hapticsettings_getPhysicsHELIScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELIScale_Z");
    const Vec3 v = hapticSettings()->getPhysicsHELIScale();        // +0x38C (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003B680 "getPhysicsHELITransition": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x3DC getPhysicsHELITransitionValue(level) as a number.
void as_hapticsettings_getPhysicsHELITransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELITransition");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsHELITransitionValue(level)));    // +0x3DC
}

// sub_1003B780 "getPhysicsHELISmoothTime_X": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x3F4 getPhysicsHELISmoothTimeX(level) as a number.
void as_hapticsettings_getPhysicsHELISmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELISmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsHELISmoothTimeX(level)));    // +0x3F4
}

// sub_1003B880 "getPhysicsHELISmoothTime_Y": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x3FC getPhysicsHELISmoothTimeY(level) as a number.
void as_hapticsettings_getPhysicsHELISmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELISmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsHELISmoothTimeY(level)));    // +0x3FC
}

// sub_1003B980 "getPhysicsHELISmoothTime_Z": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x404 getPhysicsHELISmoothTimeZ(level) as a number.
void as_hapticsettings_getPhysicsHELISmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsHELISmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsHELISmoothTimeZ(level)));    // +0x404
}

// sub_1003BA80 "getPhysicsSEAScale_X": HapticSettings by name, +0x454 getPhysicsSEAScale() by value, component x as a number.
void as_hapticsettings_getPhysicsSEAScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEAScale_X");
    const Vec3 v = hapticSettings()->getPhysicsSEAScale();        // +0x454 (sret)
    fn.result->set_double(static_cast<double>(v.x));
}

// sub_1003BB10 "getPhysicsSEAScale_Y": HapticSettings by name, +0x454 getPhysicsSEAScale() by value, component y as a number.
void as_hapticsettings_getPhysicsSEAScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEAScale_Y");
    const Vec3 v = hapticSettings()->getPhysicsSEAScale();        // +0x454 (sret)
    fn.result->set_double(static_cast<double>(v.y));
}

// sub_1003BBA0 "getPhysicsSEAScale_Z": HapticSettings by name, +0x454 getPhysicsSEAScale() by value, component z as a number.
void as_hapticsettings_getPhysicsSEAScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEAScale_Z");
    const Vec3 v = hapticSettings()->getPhysicsSEAScale();        // +0x454 (sret)
    fn.result->set_double(static_cast<double>(v.z));
}

// sub_1003BC30 "getPhysicsSEATransition": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x4A4 getPhysicsSEATransitionValue(level) as a number.
void as_hapticsettings_getPhysicsSEATransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEATransition");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSEATransitionValue(level)));    // +0x4A4
}

// sub_1003BD30 "getPhysicsSEASmoothTime_X": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x4BC getPhysicsSEASmoothTimeX(level) as a number.
void as_hapticsettings_getPhysicsSEASmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEASmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSEASmoothTimeX(level)));    // +0x4BC
}

// sub_1003BE30 "getPhysicsSEASmoothTime_Y": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x4C4 getPhysicsSEASmoothTimeY(level) as a number.
void as_hapticsettings_getPhysicsSEASmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEASmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSEASmoothTimeY(level)));    // +0x4C4
}

// sub_1003BF30 "getPhysicsSEASmoothTime_Z": arg 0 to_string looked up in the threshold names (unk_100F3870), HapticSettings by name, +0x4CC getPhysicsSEASmoothTimeZ(level) as a number.
void as_hapticsettings_getPhysicsSEASmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getPhysicsSEASmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getPhysicsSEASmoothTimeZ(level)));    // +0x4CC
}

// sub_1003C030 "getDamageScale": HapticSettings by name, +0x51C as a number.
void as_hapticsettings_getDamageScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getDamageScale");
    fn.result->set_double(static_cast<double>(hapticSettings()->getDamageScale()));      // +0x51C
}

// sub_1003C0B0 "getRecoilPunchScale": arg 0 to_string mapped to a weapon class (the page selects the infantry or the vehicle names), HapticSettings by name, +0x524 getRecoilPunchScale(class) as a number.
void as_hapticsettings_getRecoilPunchScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getRecoilPunchScale");
    const std::string key(fn.arg(0).to_string());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getRecoilPunchScale(weaponClass)));    // +0x524
}

// sub_1003C200 "getRecoilPunchTime": arg 0 to_string mapped to a weapon class (the page selects the infantry or the vehicle names), HapticSettings by name, +0x52C getRecoilPunchTime(class) as a number.
void as_hapticsettings_getRecoilPunchTime(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getRecoilPunchTime");
    const std::string key(fn.arg(0).to_string());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getRecoilPunchTime(weaponClass)));    // +0x52C
}

// sub_1003C350 "getRecoilYawScale": arg 0 to_string mapped to a weapon class (the page selects the infantry or the vehicle names), HapticSettings by name, +0x534 getRecoilYawScale(class) as a number.
void as_hapticsettings_getRecoilYawScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getRecoilYawScale");
    const std::string key(fn.arg(0).to_string());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getRecoilYawScale(weaponClass)));    // +0x534
}

// sub_1003C4A0 "getRecoilPitchScale": arg 0 to_string mapped to a weapon class (the page selects the infantry or the vehicle names), HapticSettings by name, +0x53C getRecoilPitchScale(class) as a number.
void as_hapticsettings_getRecoilPitchScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getRecoilPitchScale");
    const std::string key(fn.arg(0).to_string());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    fn.result->set_double(static_cast<double>(hs->getRecoilPitchScale(weaponClass)));    // +0x53C
}

// sub_10043820 "setPhysicsSOLDIERScale_X": arg 0 as a float, three HapticSettings lookups, +0x134 twice, +0x138(value, second.y, first.z).
void as_hapticsettings_setPhysicsSOLDIERScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERScale_X");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSOLDIERScale();
    const Vec3 second = b->getPhysicsSOLDIERScale();
    c->setPhysicsSOLDIERScale(value, second.y, first.z);    // +0x138
}

// sub_10043900 "setPhysicsSOLDIERScale_Y": arg 0 as a float, three HapticSettings lookups, +0x134 twice, +0x138(second.x, value, first.z).
void as_hapticsettings_setPhysicsSOLDIERScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERScale_Y");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSOLDIERScale();
    const Vec3 second = b->getPhysicsSOLDIERScale();
    c->setPhysicsSOLDIERScale(second.x, value, first.z);    // +0x138
}

// sub_100439E0 "setPhysicsSOLDIERScale_Z": arg 0 as a float, three HapticSettings lookups, +0x134 twice, +0x138(second.x, first.y, value).
void as_hapticsettings_setPhysicsSOLDIERScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERScale_Z");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSOLDIERScale();
    const Vec3 second = b->getPhysicsSOLDIERScale();
    c->setPhysicsSOLDIERScale(second.x, first.y, value);    // +0x138
}

// sub_10035380 "setPhysicsSOLDIERTransition": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x188 setPhysicsSOLDIERTransitionValue(level, value).
void as_hapticsettings_setPhysicsSOLDIERTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERTransition");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSOLDIERTransitionValue(level, value);    // +0x188
}

// sub_10035460 "setPhysicsSOLDIERSmoothTime_X": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x1A0 setPhysicsSOLDIERSmoothTimeX(level, (int) value).
void as_hapticsettings_setPhysicsSOLDIERSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSOLDIERSmoothTimeX(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x1A0
}

// sub_10035550 "setPhysicsSOLDIERSmoothTime_Y": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x1A8 setPhysicsSOLDIERSmoothTimeY(level, (int) value).
void as_hapticsettings_setPhysicsSOLDIERSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSOLDIERSmoothTimeY(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x1A8
}

// sub_10035640 "setPhysicsSOLDIERSmoothTime_Z": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x1B0 setPhysicsSOLDIERSmoothTimeZ(level, (int) value).
void as_hapticsettings_setPhysicsSOLDIERSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSOLDIERSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSOLDIERSmoothTimeZ(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x1B0
}

// sub_10043AC0 "setPhysicsLANDScale_X": arg 0 as a float, three HapticSettings lookups, +0x1FC twice, +0x200(value, second.y, first.z).
void as_hapticsettings_setPhysicsLANDScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDScale_X");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsLANDScale();
    const Vec3 second = b->getPhysicsLANDScale();
    c->setPhysicsLANDScale(value, second.y, first.z);    // +0x200
}

// sub_10043BA0 "setPhysicsLANDScale_Y": arg 0 as a float, three HapticSettings lookups, +0x1FC twice, +0x200(second.x, value, first.z).
void as_hapticsettings_setPhysicsLANDScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDScale_Y");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsLANDScale();
    const Vec3 second = b->getPhysicsLANDScale();
    c->setPhysicsLANDScale(second.x, value, first.z);    // +0x200
}

// sub_10043C80 "setPhysicsLANDScale_Z": arg 0 as a float, three HapticSettings lookups, +0x1FC twice, +0x200(second.x, first.y, value).
void as_hapticsettings_setPhysicsLANDScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDScale_Z");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsLANDScale();
    const Vec3 second = b->getPhysicsLANDScale();
    c->setPhysicsLANDScale(second.x, first.y, value);    // +0x200
}

// sub_10035730 "setPhysicsLANDTransition": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x250 setPhysicsLANDTransitionValue(level, value).
void as_hapticsettings_setPhysicsLANDTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDTransition");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsLANDTransitionValue(level, value);    // +0x250
}

// sub_10035810 "setPhysicsLANDSmoothTime_X": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x268 setPhysicsLANDSmoothTimeX(level, (int) value).
void as_hapticsettings_setPhysicsLANDSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsLANDSmoothTimeX(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x268
}

// sub_10035900 "setPhysicsLANDSmoothTime_Y": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x270 setPhysicsLANDSmoothTimeY(level, (int) value).
void as_hapticsettings_setPhysicsLANDSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsLANDSmoothTimeY(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x270
}

// sub_100359F0 "setPhysicsLANDSmoothTime_Z": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x278 setPhysicsLANDSmoothTimeZ(level, (int) value).
void as_hapticsettings_setPhysicsLANDSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsLANDSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsLANDSmoothTimeZ(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x278
}

// sub_10043D60 "setPhysicsAIRScale_X": arg 0 as a float, three HapticSettings lookups, +0x2C4 twice, +0x2C8(value, second.y, first.z).
void as_hapticsettings_setPhysicsAIRScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRScale_X");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsAIRScale();
    const Vec3 second = b->getPhysicsAIRScale();
    c->setPhysicsAIRScale(value, second.y, first.z);    // +0x2C8
}

// sub_10043E40 "setPhysicsAIRScale_Y": arg 0 as a float, three HapticSettings lookups, +0x2C4 twice, +0x2C8(second.x, value, first.z).
void as_hapticsettings_setPhysicsAIRScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRScale_Y");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsAIRScale();
    const Vec3 second = b->getPhysicsAIRScale();
    c->setPhysicsAIRScale(second.x, value, first.z);    // +0x2C8
}

// sub_10043F20 "setPhysicsAIRScale_Z": arg 0 as a float, three HapticSettings lookups, +0x2C4 twice, +0x2C8(second.x, first.y, value).
void as_hapticsettings_setPhysicsAIRScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRScale_Z");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsAIRScale();
    const Vec3 second = b->getPhysicsAIRScale();
    c->setPhysicsAIRScale(second.x, first.y, value);    // +0x2C8
}

// sub_10035AE0 "setPhysicsAIRTransition": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x318 setPhysicsAIRTransitionValue(level, value).
void as_hapticsettings_setPhysicsAIRTransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRTransition");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsAIRTransitionValue(level, value);    // +0x318
}

// sub_10035BC0 "setPhysicsAIRSmoothTime_X": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x330 setPhysicsAIRSmoothTimeX(level, (int) value).
void as_hapticsettings_setPhysicsAIRSmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRSmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsAIRSmoothTimeX(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x330
}

// sub_10035CB0 "setPhysicsAIRSmoothTime_Y": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x338 setPhysicsAIRSmoothTimeY(level, (int) value).
void as_hapticsettings_setPhysicsAIRSmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRSmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsAIRSmoothTimeY(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x338
}

// sub_10035DA0 "setPhysicsAIRSmoothTime_Z": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x340 setPhysicsAIRSmoothTimeZ(level, (int) value).
void as_hapticsettings_setPhysicsAIRSmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsAIRSmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsAIRSmoothTimeZ(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x340
}

// sub_10044000 "setPhysicsHELIScale_X": arg 0 as a float, three HapticSettings lookups, +0x38C twice, +0x390(value, second.y, first.z).
void as_hapticsettings_setPhysicsHELIScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELIScale_X");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsHELIScale();
    const Vec3 second = b->getPhysicsHELIScale();
    c->setPhysicsHELIScale(value, second.y, first.z);    // +0x390
}

// sub_100440E0 "setPhysicsHELIScale_Y": arg 0 as a float, three HapticSettings lookups, +0x38C twice, +0x390(second.x, value, first.z).
void as_hapticsettings_setPhysicsHELIScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELIScale_Y");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsHELIScale();
    const Vec3 second = b->getPhysicsHELIScale();
    c->setPhysicsHELIScale(second.x, value, first.z);    // +0x390
}

// sub_100441C0 "setPhysicsHELIScale_Z": arg 0 as a float, three HapticSettings lookups, +0x38C twice, +0x390(second.x, first.y, value).
void as_hapticsettings_setPhysicsHELIScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELIScale_Z");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsHELIScale();
    const Vec3 second = b->getPhysicsHELIScale();
    c->setPhysicsHELIScale(second.x, first.y, value);    // +0x390
}

// sub_10035E90 "setPhysicsHELITransition": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x3E0 setPhysicsHELITransitionValue(level, value).
void as_hapticsettings_setPhysicsHELITransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELITransition");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsHELITransitionValue(level, value);    // +0x3E0
}

// sub_10035F70 "setPhysicsHELISmoothTime_X": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x3F8 setPhysicsHELISmoothTimeX(level, (int) value).
void as_hapticsettings_setPhysicsHELISmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELISmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsHELISmoothTimeX(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x3F8
}

// sub_10036060 "setPhysicsHELISmoothTime_Y": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x400 setPhysicsHELISmoothTimeY(level, (int) value).
void as_hapticsettings_setPhysicsHELISmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELISmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsHELISmoothTimeY(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x400
}

// sub_10036150 "setPhysicsHELISmoothTime_Z": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x408 setPhysicsHELISmoothTimeZ(level, (int) value).
void as_hapticsettings_setPhysicsHELISmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsHELISmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsHELISmoothTimeZ(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x408
}

// sub_100442A0 "setPhysicsSEAScale_X": arg 0 as a float, three HapticSettings lookups, +0x454 twice, +0x458(value, second.y, first.z).
void as_hapticsettings_setPhysicsSEAScale_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEAScale_X");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSEAScale();
    const Vec3 second = b->getPhysicsSEAScale();
    c->setPhysicsSEAScale(value, second.y, first.z);    // +0x458
}

// sub_10044380 "setPhysicsSEAScale_Y": arg 0 as a float, three HapticSettings lookups, +0x454 twice, +0x458(second.x, value, first.z).
void as_hapticsettings_setPhysicsSEAScale_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEAScale_Y");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSEAScale();
    const Vec3 second = b->getPhysicsSEAScale();
    c->setPhysicsSEAScale(second.x, value, first.z);    // +0x458
}

// sub_10044460 "setPhysicsSEAScale_Z": arg 0 as a float, three HapticSettings lookups, +0x454 twice, +0x458(second.x, first.y, value).
void as_hapticsettings_setPhysicsSEAScale_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEAScale_Z");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IHapticSettings* a = hapticSettings();
    IHapticSettings* b = hapticSettings();
    IHapticSettings* c = hapticSettings();
    const Vec3 first = a->getPhysicsSEAScale();
    const Vec3 second = b->getPhysicsSEAScale();
    c->setPhysicsSEAScale(second.x, first.y, value);    // +0x458
}

// sub_10036240 "setPhysicsSEATransition": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x4A8 setPhysicsSEATransitionValue(level, value).
void as_hapticsettings_setPhysicsSEATransition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEATransition");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSEATransitionValue(level, value);    // +0x4A8
}

// sub_10036320 "setPhysicsSEASmoothTime_X": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x4C0 setPhysicsSEASmoothTimeX(level, (int) value).
void as_hapticsettings_setPhysicsSEASmoothTime_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEASmoothTime_X");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSEASmoothTimeX(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x4C0
}

// sub_10036410 "setPhysicsSEASmoothTime_Y": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x4C8 setPhysicsSEASmoothTimeY(level, (int) value).
void as_hapticsettings_setPhysicsSEASmoothTime_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEASmoothTime_Y");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSEASmoothTimeY(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x4C8
}

// sub_10036500 "setPhysicsSEASmoothTime_Z": arg 0 to_string, arg 1 as a float, the threshold level (3-entry loop), HapticSettings by name, +0x4D0 setPhysicsSEASmoothTimeZ(level, (int) value).
void as_hapticsettings_setPhysicsSEASmoothTime_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setPhysicsSEASmoothTime_Z");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int level = hapticThresholdLevel(key);
    IHapticSettings* hs = hapticSettings();
    hs->setPhysicsSEASmoothTimeZ(level, bf2x87::toIntTruncate(static_cast<double>(value)));    // +0x4D0
}

// sub_100365F0 "setDamageScale": arg 0 as a float, then HapticSettings by name, +0x520.
void as_hapticsettings_setDamageScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setDamageScale");
    const float value = static_cast<float>(fn.arg(0).to_number());
    hapticSettings()->setDamageScale(value);                       // +0x520
}

// sub_10036660 "setRecoilPunchScale": arg 0 to_string, arg 1 as a float, the weapon class, HapticSettings by name, +0x528 setRecoilPunchScale(class, value).
void as_hapticsettings_setRecoilPunchScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setRecoilPunchScale");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    hs->setRecoilPunchScale(weaponClass, value);    // +0x528
}

// sub_10036790 "setRecoilPunchTime": arg 0 to_string, arg 1 as a float, the weapon class, HapticSettings by name, +0x530 setRecoilPunchTime(class, value).
void as_hapticsettings_setRecoilPunchTime(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setRecoilPunchTime");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    hs->setRecoilPunchTime(weaponClass, value);    // +0x530
}

// sub_100368C0 "setRecoilYawScale": arg 0 to_string, arg 1 as a float, the weapon class, HapticSettings by name, +0x538 setRecoilYawScale(class, value).
void as_hapticsettings_setRecoilYawScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setRecoilYawScale");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    hs->setRecoilYawScale(weaponClass, value);    // +0x538
}

// sub_100369F0 "setRecoilPitchScale": arg 0 to_string, arg 1 as a float, the weapon class, HapticSettings by name, +0x540 setRecoilPitchScale(class, value).
void as_hapticsettings_setRecoilPitchScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setRecoilPitchScale");
    const std::string key(fn.arg(0).to_string());
    const float value = static_cast<float>(fn.arg(1).to_number());
    const int weaponClass = hapticWeaponClass(key);
    IHapticSettings* hs = hapticSettings();
    hs->setRecoilPitchScale(weaponClass, value);    // +0x540
}

// sub_10036B20 "setHapticSettingsPage": (int) arg 0 into dword_100F8530.
void as_hapticsettings_setHapticSettingsPage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setHapticSettingsPage");
    s_hapticSettingsPage = bf2x87::toIntTruncate(fn.arg(0).to_number());
}

// sub_10044540 "getHapticSettingsPage": dword_100F8530 as a number.
void as_hapticsettings_getHapticSettingsPage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getHapticSettingsPage");
    fn.result->set_double(static_cast<double>(s_hapticSettingsPage));
}

// sub_1003C5F0 "setControlBoxLengthSOLDIER_X": vector setter, component x: +0x54 getControlBoxLengthSOLDIER / +0x58 setControlBoxLengthSOLDIER; mirror +0x54 getControlBoxLengthSOLDIER / +0x48 setControlBoxLength (on foot or no local player).
void as_hapticsettings_setControlBoxLengthSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSOLDIER_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLengthSOLDIER, 0, kHapticSoldier,
                       &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLength);
}

// sub_1003C7D0 "setControlBoxStiffnessSOLDIER_X": vector setter, component x: +0x5C getControlBoxStiffnessSOLDIER / +0x60 setControlBoxStiffnessSOLDIER; mirror +0x5C getControlBoxStiffnessSOLDIER / +0x50 setControlBoxStiffness (on foot or no local player).
void as_hapticsettings_setControlBoxStiffnessSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSOLDIER_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffnessSOLDIER, 0, kHapticSoldier,
                       &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003D130 "setControlBoxLengthLAND_X": vector setter, component x: +0x84 getControlBoxLengthLAND / +0x88 setControlBoxLengthLAND; mirror +0x84 getControlBoxLengthLAND / +0x48 setControlBoxLength (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxLengthLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthLAND_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLengthLAND, 0, kHapticLand,
                       &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLength);
}

// sub_1003D2A0 "setControlBoxStiffnessLAND_X": vector setter, component x: +0x8C getControlBoxStiffnessLAND / +0x90 setControlBoxStiffnessLAND; mirror +0x8C getControlBoxStiffnessLAND / +0x50 setControlBoxStiffness (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxStiffnessLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessLAND_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffnessLAND, 0, kHapticLand,
                       &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003D9D0 "setControlBoxLengthAIR_X": vector setter, component x: +0x64 getControlBoxLengthAIR / +0x68 setControlBoxLengthAIR; mirror +0x64 getControlBoxLengthAIR / +0x48 setControlBoxLength (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxLengthAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthAIR_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLengthAIR, 0, kHapticAir,
                       &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLength);
}

// sub_1003DB30 "setControlBoxStiffnessAIR_X": vector setter, component x: +0x6C getControlBoxStiffnessAIR / +0x70 setControlBoxStiffnessAIR; mirror +0x6C getControlBoxStiffnessAIR / +0x50 setControlBoxStiffness (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxStiffnessAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessAIR_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffnessAIR, 0, kHapticAir,
                       &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003E210 "setControlBoxLengthHELI_X": vector setter, component x: +0x74 getControlBoxLengthHELI / +0x78 setControlBoxLengthHELI; mirror +0x74 getControlBoxLengthHELI / +0x48 setControlBoxLength (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxLengthHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthHELI_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLengthHELI, 0, kHapticHeli,
                       &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLength);
}

// sub_1003E370 "setControlBoxStiffnessHELI_X": vector setter, component x: +0x7C getControlBoxStiffnessHELI / +0x80 setControlBoxStiffnessHELI; mirror +0x7C getControlBoxStiffnessHELI / +0x50 setControlBoxStiffness (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxStiffnessHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessHELI_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffnessHELI, 0, kHapticHeli,
                       &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003EA50 "setControlBoxLengthSEA_X": vector setter, component x: +0x94 getControlBoxLengthSEA / +0x98 setControlBoxLengthSEA; mirror +0x94 getControlBoxLengthSEA / +0x48 setControlBoxLength (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxLengthSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSEA_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLengthSEA, 0, kHapticSea,
                       &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLength);
}

// sub_1003EBC0 "setControlBoxStiffnessSEA_X": vector setter, component x: +0x9C getControlBoxStiffnessSEA / +0xA0 setControlBoxStiffnessSEA; mirror +0x9C getControlBoxStiffnessSEA / +0x50 setControlBoxStiffness (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxStiffnessSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSEA_X");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffnessSEA, 0, kHapticSea,
                       &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003C9B0 "setControlBoxLengthSOLDIER_Y": vector setter, component y: +0x54 getControlBoxLengthSOLDIER / +0x58 setControlBoxLengthSOLDIER; mirror +0x54 getControlBoxLengthSOLDIER / +0x48 setControlBoxLength (on foot or no local player).
void as_hapticsettings_setControlBoxLengthSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSOLDIER_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLengthSOLDIER, 1, kHapticSoldier,
                       &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLength);
}

// sub_1003CB90 "setControlBoxStiffnessSOLDIER_Y": vector setter, component y: +0x5C getControlBoxStiffnessSOLDIER / +0x60 setControlBoxStiffnessSOLDIER; mirror +0x5C getControlBoxStiffnessSOLDIER / +0x50 setControlBoxStiffness (on foot or no local player).
void as_hapticsettings_setControlBoxStiffnessSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSOLDIER_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffnessSOLDIER, 1, kHapticSoldier,
                       &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003D410 "setControlBoxLengthLAND_Y": vector setter, component y: +0x84 getControlBoxLengthLAND / +0x88 setControlBoxLengthLAND; mirror +0x84 getControlBoxLengthLAND / +0x48 setControlBoxLength (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxLengthLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthLAND_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLengthLAND, 1, kHapticLand,
                       &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLength);
}

// sub_1003D580 "setControlBoxStiffnessLAND_Y": vector setter, component y: +0x8C getControlBoxStiffnessLAND / +0x90 setControlBoxStiffnessLAND; mirror +0x8C getControlBoxStiffnessLAND / +0x50 setControlBoxStiffness (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxStiffnessLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessLAND_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffnessLAND, 1, kHapticLand,
                       &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003DC90 "setControlBoxLengthAIR_Y": vector setter, component y: +0x64 getControlBoxLengthAIR / +0x68 setControlBoxLengthAIR; mirror +0x64 getControlBoxLengthAIR / +0x48 setControlBoxLength (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxLengthAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthAIR_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLengthAIR, 1, kHapticAir,
                       &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLength);
}

// sub_1003DDF0 "setControlBoxStiffnessAIR_Y": vector setter, component y: +0x6C getControlBoxStiffnessAIR / +0x70 setControlBoxStiffnessAIR; mirror +0x6C getControlBoxStiffnessAIR / +0x50 setControlBoxStiffness (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxStiffnessAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessAIR_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffnessAIR, 1, kHapticAir,
                       &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003E4D0 "setControlBoxLengthHELI_Y": vector setter, component y: +0x74 getControlBoxLengthHELI / +0x78 setControlBoxLengthHELI; mirror +0x74 getControlBoxLengthHELI / +0x48 setControlBoxLength (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxLengthHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthHELI_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLengthHELI, 1, kHapticHeli,
                       &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLength);
}

// sub_1003E630 "setControlBoxStiffnessHELI_Y": vector setter, component y: +0x7C getControlBoxStiffnessHELI / +0x80 setControlBoxStiffnessHELI; mirror +0x7C getControlBoxStiffnessHELI / +0x50 setControlBoxStiffness (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxStiffnessHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessHELI_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffnessHELI, 1, kHapticHeli,
                       &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003ED30 "setControlBoxLengthSEA_Y": vector setter, component y: +0x94 getControlBoxLengthSEA / +0x98 setControlBoxLengthSEA; mirror +0x94 getControlBoxLengthSEA / +0x48 setControlBoxLength (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxLengthSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSEA_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLengthSEA, 1, kHapticSea,
                       &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLength);
}

// sub_1003EEA0 "setControlBoxStiffnessSEA_Y": vector setter, component y: +0x9C getControlBoxStiffnessSEA / +0xA0 setControlBoxStiffnessSEA; mirror +0x9C getControlBoxStiffnessSEA / +0x50 setControlBoxStiffness (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxStiffnessSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSEA_Y");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffnessSEA, 1, kHapticSea,
                       &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003CD70 "setControlBoxLengthSOLDIER_Z": vector setter, component z: +0x54 getControlBoxLengthSOLDIER / +0x58 setControlBoxLengthSOLDIER; mirror +0x54 getControlBoxLengthSOLDIER / +0x48 setControlBoxLength (on foot or no local player).
void as_hapticsettings_setControlBoxLengthSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSOLDIER_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLengthSOLDIER, 2, kHapticSoldier,
                       &IHapticSettings::getControlBoxLengthSOLDIER, &IHapticSettings::setControlBoxLength);
}

// sub_1003CF50 "setControlBoxStiffnessSOLDIER_Z": vector setter, component z: +0x5C getControlBoxStiffnessSOLDIER / +0x60 setControlBoxStiffnessSOLDIER; mirror +0x5C getControlBoxStiffnessSOLDIER / +0x50 setControlBoxStiffness (on foot or no local player).
void as_hapticsettings_setControlBoxStiffnessSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSOLDIER_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffnessSOLDIER, 2, kHapticSoldier,
                       &IHapticSettings::getControlBoxStiffnessSOLDIER, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003D6F0 "setControlBoxLengthLAND_Z": vector setter, component z: +0x84 getControlBoxLengthLAND / +0x88 setControlBoxLengthLAND; mirror +0x84 getControlBoxLengthLAND / +0x48 setControlBoxLength (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxLengthLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthLAND_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLengthLAND, 2, kHapticLand,
                       &IHapticSettings::getControlBoxLengthLAND, &IHapticSettings::setControlBoxLength);
}

// sub_1003D860 "setControlBoxStiffnessLAND_Z": vector setter, component z: +0x8C getControlBoxStiffnessLAND / +0x90 setControlBoxStiffnessLAND; mirror +0x8C getControlBoxStiffnessLAND / +0x50 setControlBoxStiffness (vehicle category kHapticLand).
void as_hapticsettings_setControlBoxStiffnessLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessLAND_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffnessLAND, 2, kHapticLand,
                       &IHapticSettings::getControlBoxStiffnessLAND, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003DF50 "setControlBoxLengthAIR_Z": vector setter, component z: +0x64 getControlBoxLengthAIR / +0x68 setControlBoxLengthAIR; mirror +0x64 getControlBoxLengthAIR / +0x48 setControlBoxLength (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxLengthAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthAIR_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLengthAIR, 2, kHapticAir,
                       &IHapticSettings::getControlBoxLengthAIR, &IHapticSettings::setControlBoxLength);
}

// sub_1003E0B0 "setControlBoxStiffnessAIR_Z": vector setter, component z: +0x6C getControlBoxStiffnessAIR / +0x70 setControlBoxStiffnessAIR; mirror +0x6C getControlBoxStiffnessAIR / +0x50 setControlBoxStiffness (vehicle category kHapticAir).
void as_hapticsettings_setControlBoxStiffnessAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessAIR_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffnessAIR, 2, kHapticAir,
                       &IHapticSettings::getControlBoxStiffnessAIR, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003E790 "setControlBoxLengthHELI_Z": vector setter, component z: +0x74 getControlBoxLengthHELI / +0x78 setControlBoxLengthHELI; mirror +0x74 getControlBoxLengthHELI / +0x48 setControlBoxLength (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxLengthHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthHELI_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLengthHELI, 2, kHapticHeli,
                       &IHapticSettings::getControlBoxLengthHELI, &IHapticSettings::setControlBoxLength);
}

// sub_1003E8F0 "setControlBoxStiffnessHELI_Z": vector setter, component z: +0x7C getControlBoxStiffnessHELI / +0x80 setControlBoxStiffnessHELI; mirror +0x7C getControlBoxStiffnessHELI / +0x50 setControlBoxStiffness (vehicle category kHapticHeli).
void as_hapticsettings_setControlBoxStiffnessHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessHELI_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffnessHELI, 2, kHapticHeli,
                       &IHapticSettings::getControlBoxStiffnessHELI, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003F010 "setControlBoxLengthSEA_Z": vector setter, component z: +0x94 getControlBoxLengthSEA / +0x98 setControlBoxLengthSEA; mirror +0x94 getControlBoxLengthSEA / +0x48 setControlBoxLength (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxLengthSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxLengthSEA_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLengthSEA, 2, kHapticSea,
                       &IHapticSettings::getControlBoxLengthSEA, &IHapticSettings::setControlBoxLength);
}

// sub_1003F180 "setControlBoxStiffnessSEA_Z": vector setter, component z: +0x9C getControlBoxStiffnessSEA / +0xA0 setControlBoxStiffnessSEA; mirror +0x9C getControlBoxStiffnessSEA / +0x50 setControlBoxStiffness (vehicle category kHapticSea).
void as_hapticsettings_setControlBoxStiffnessSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setControlBoxStiffnessSEA_Z");
    hapticVectorSetter(fn, &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffnessSEA, 2, kHapticSea,
                       &IHapticSettings::getControlBoxStiffnessSEA, &IHapticSettings::setControlBoxStiffness);
}

// sub_1003F2F0 "setAimSensitivitySOLDIER_X": vector setter, component x: +0xBC getAimSensitivitySOLDIER / +0xC0 setAimSensitivitySOLDIER; mirror +0xBC getAimSensitivitySOLDIER / +0xA8 setAimSensitivity (on foot or no local player).
void as_hapticsettings_setAimSensitivitySOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySOLDIER_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivitySOLDIER, 0, kHapticSoldier,
                       &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivity);
}

// sub_1003F4E0 "setAimSensitivityLengthSOLDIER_X": vector setter, component x: +0xC4 getAimSensitivityLengthSOLDIER / +0xC8 setAimSensitivityLengthSOLDIER; mirror +0xBC getAimSensitivitySOLDIER / +0xB0 setAimSensitivityLength (on foot or no local player). The mirror reads the other two components through +0xBC getAimSensitivitySOLDIER, not +0xC4 (retail; the Y and Z rows use +0xC4).
void as_hapticsettings_setAimSensitivityLengthSOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSOLDIER_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSOLDIER, &IHapticSettings::setAimSensitivityLengthSOLDIER, 0, kHapticSoldier,
                       &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivityLength);
}

// sub_1003F6D0 "setTurnSensitivitySOLDIER_X": vector setter, component x: +0xCC getTurnSensitivitySOLDIER / +0xD0 setTurnSensitivitySOLDIER; mirror +0xCC getTurnSensitivitySOLDIER / +0xB8 setTurnSensitivity (on foot or no local player).
void as_hapticsettings_setTurnSensitivitySOLDIER_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySOLDIER_X");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivitySOLDIER, 0, kHapticSoldier,
                       &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivity);
}

// sub_10040460 "setAimSensitivityLAND_X": vector setter, component x: +0x104 getAimSensitivityLAND / +0x108 setAimSensitivityLAND; mirror +0x104 getAimSensitivityLAND / +0xA8 setAimSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLAND_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivityLAND, 0, kHapticLand,
                       &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivity);
}

// sub_100405D0 "setAimSensitivityLengthLAND_X": vector setter, component x: +0x10C getAimSensitivityLengthLAND / +0x110 setAimSensitivityLengthLAND; mirror +0x10C getAimSensitivityLengthLAND / +0xB0 setAimSensitivityLength (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLengthLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthLAND_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLengthLAND, 0, kHapticLand,
                       &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLength);
}

// sub_10040740 "setTurnSensitivityLAND_X": vector setter, component x: +0x114 getTurnSensitivityLAND / +0x118 setTurnSensitivityLAND; mirror +0x114 getTurnSensitivityLAND / +0xB8 setTurnSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setTurnSensitivityLAND_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityLAND_X");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivityLAND, 0, kHapticLand,
                       &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivity);
}

// sub_10041150 "setAimSensitivityAIR_X": vector setter, component x: +0xD4 getAimSensitivityAIR / +0xD8 setAimSensitivityAIR; mirror +0xD4 getAimSensitivityAIR / +0xA8 setAimSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityAIR_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivityAIR, 0, kHapticAir,
                       &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivity);
}

// sub_100412C0 "setAimSensitivityLengthAIR_X": vector setter, component x: +0xDC getAimSensitivityLengthAIR / +0xE0 setAimSensitivityLengthAIR; mirror +0xDC getAimSensitivityLengthAIR / +0xB0 setAimSensitivityLength (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityLengthAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthAIR_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLengthAIR, 0, kHapticAir,
                       &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLength);
}

// sub_10041430 "setTurnSensitivityAIR_X": vector setter, component x: +0xE4 getTurnSensitivityAIR / +0xE8 setTurnSensitivityAIR; mirror +0xE4 getTurnSensitivityAIR / +0xB8 setTurnSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setTurnSensitivityAIR_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityAIR_X");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivityAIR, 0, kHapticAir,
                       &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivity);
}

// sub_10041E40 "setAimSensitivityHELI_X": vector setter, component x: +0xEC getAimSensitivityHELI / +0xF0 setAimSensitivityHELI; mirror +0xEC getAimSensitivityHELI / +0xA8 setAimSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityHELI_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivityHELI, 0, kHapticHeli,
                       &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivity);
}

// sub_10041FB0 "setAimSensitivityLengthHELI_X": vector setter, component x: +0xF4 getAimSensitivityLengthHELI / +0xF8 setAimSensitivityLengthHELI; mirror +0xF4 getAimSensitivityLengthHELI / +0xB0 setAimSensitivityLength (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityLengthHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthHELI_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLengthHELI, 0, kHapticHeli,
                       &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLength);
}

// sub_10042120 "setTurnSensitivityHELI_X": vector setter, component x: +0xFC getTurnSensitivityHELI / +0x100 setTurnSensitivityHELI; mirror +0xFC getTurnSensitivityHELI / +0xB8 setTurnSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setTurnSensitivityHELI_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityHELI_X");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivityHELI, 0, kHapticHeli,
                       &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivity);
}

// sub_10042B30 "setAimSensitivitySEA_X": vector setter, component x: +0x11C getAimSensitivitySEA / +0x120 setAimSensitivitySEA; mirror +0x11C getAimSensitivitySEA / +0xA8 setAimSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivitySEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySEA_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivitySEA, 0, kHapticSea,
                       &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivity);
}

// sub_10042CA0 "setAimSensitivityLengthSEA_X": vector setter, component x: +0x124 getAimSensitivityLengthSEA / +0x128 setAimSensitivityLengthSEA; mirror +0x124 getAimSensitivityLengthSEA / +0xB0 setAimSensitivityLength (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivityLengthSEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSEA_X");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLengthSEA, 0, kHapticSea,
                       &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLength);
}

// sub_10042E10 "setTurnSensitivitySEA_X": vector setter, component x: +0x12C getTurnSensitivitySEA / +0x130 setTurnSensitivitySEA; mirror +0x12C getTurnSensitivitySEA / +0xB8 setTurnSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setTurnSensitivitySEA_X(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySEA_X");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivitySEA, 0, kHapticSea,
                       &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivity);
}

// sub_1003F8C0 "setAimSensitivitySOLDIER_Y": vector setter, component y: +0xBC getAimSensitivitySOLDIER / +0xC0 setAimSensitivitySOLDIER; mirror +0xBC getAimSensitivitySOLDIER / +0xA8 setAimSensitivity (on foot or no local player).
void as_hapticsettings_setAimSensitivitySOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySOLDIER_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivitySOLDIER, 1, kHapticSoldier,
                       &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivity);
}

// sub_1003FAB0 "setAimSensitivityLengthSOLDIER_Y": vector setter, component y: +0xC4 getAimSensitivityLengthSOLDIER / +0xC8 setAimSensitivityLengthSOLDIER; mirror +0xC4 getAimSensitivityLengthSOLDIER / +0xB0 setAimSensitivityLength (on foot or no local player).
void as_hapticsettings_setAimSensitivityLengthSOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSOLDIER_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSOLDIER, &IHapticSettings::setAimSensitivityLengthSOLDIER, 1, kHapticSoldier,
                       &IHapticSettings::getAimSensitivityLengthSOLDIER, &IHapticSettings::setAimSensitivityLength);
}

// sub_1003FCA0 "setTurnSensitivitySOLDIER_Y": vector setter, component y: +0xCC getTurnSensitivitySOLDIER / +0xD0 setTurnSensitivitySOLDIER; mirror +0xCC getTurnSensitivitySOLDIER / +0xB8 setTurnSensitivity (on foot or no local player).
void as_hapticsettings_setTurnSensitivitySOLDIER_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySOLDIER_Y");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivitySOLDIER, 1, kHapticSoldier,
                       &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivity);
}

// sub_100408B0 "setAimSensitivityLAND_Y": vector setter, component y: +0x104 getAimSensitivityLAND / +0x108 setAimSensitivityLAND; mirror +0x104 getAimSensitivityLAND / +0xA8 setAimSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLAND_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivityLAND, 1, kHapticLand,
                       &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivity);
}

// sub_10040A20 "setAimSensitivityLengthLAND_Y": vector setter, component y: +0x10C getAimSensitivityLengthLAND / +0x110 setAimSensitivityLengthLAND; mirror +0x10C getAimSensitivityLengthLAND / +0xB0 setAimSensitivityLength (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLengthLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthLAND_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLengthLAND, 1, kHapticLand,
                       &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLength);
}

// sub_10040B90 "setTurnSensitivityLAND_Y": vector setter, component y: +0x114 getTurnSensitivityLAND / +0x118 setTurnSensitivityLAND; mirror +0x114 getTurnSensitivityLAND / +0xB8 setTurnSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setTurnSensitivityLAND_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityLAND_Y");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivityLAND, 1, kHapticLand,
                       &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivity);
}

// sub_100415A0 "setAimSensitivityAIR_Y": vector setter, component y: +0xD4 getAimSensitivityAIR / +0xD8 setAimSensitivityAIR; mirror +0xD4 getAimSensitivityAIR / +0xA8 setAimSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityAIR_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivityAIR, 1, kHapticAir,
                       &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivity);
}

// sub_10041710 "setAimSensitivityLengthAIR_Y": vector setter, component y: +0xDC getAimSensitivityLengthAIR / +0xE0 setAimSensitivityLengthAIR; mirror +0xDC getAimSensitivityLengthAIR / +0xB0 setAimSensitivityLength (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityLengthAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthAIR_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLengthAIR, 1, kHapticAir,
                       &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLength);
}

// sub_10041880 "setTurnSensitivityAIR_Y": vector setter, component y: +0xE4 getTurnSensitivityAIR / +0xE8 setTurnSensitivityAIR; mirror +0xE4 getTurnSensitivityAIR / +0xB8 setTurnSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setTurnSensitivityAIR_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityAIR_Y");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivityAIR, 1, kHapticAir,
                       &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivity);
}

// sub_10042290 "setAimSensitivityHELI_Y": vector setter, component y: +0xEC getAimSensitivityHELI / +0xF0 setAimSensitivityHELI; mirror +0xEC getAimSensitivityHELI / +0xA8 setAimSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityHELI_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivityHELI, 1, kHapticHeli,
                       &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivity);
}

// sub_10042400 "setAimSensitivityLengthHELI_Y": vector setter, component y: +0xF4 getAimSensitivityLengthHELI / +0xF8 setAimSensitivityLengthHELI; mirror +0xF4 getAimSensitivityLengthHELI / +0xB0 setAimSensitivityLength (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityLengthHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthHELI_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLengthHELI, 1, kHapticHeli,
                       &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLength);
}

// sub_10042570 "setTurnSensitivityHELI_Y": vector setter, component y: +0xFC getTurnSensitivityHELI / +0x100 setTurnSensitivityHELI; mirror +0xFC getTurnSensitivityHELI / +0xB8 setTurnSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setTurnSensitivityHELI_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityHELI_Y");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivityHELI, 1, kHapticHeli,
                       &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivity);
}

// sub_10042F80 "setAimSensitivitySEA_Y": vector setter, component y: +0x11C getAimSensitivitySEA / +0x120 setAimSensitivitySEA; mirror +0x11C getAimSensitivitySEA / +0xA8 setAimSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivitySEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySEA_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivitySEA, 1, kHapticSea,
                       &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivity);
}

// sub_100430F0 "setAimSensitivityLengthSEA_Y": vector setter, component y: +0x124 getAimSensitivityLengthSEA / +0x128 setAimSensitivityLengthSEA; mirror +0x124 getAimSensitivityLengthSEA / +0xB0 setAimSensitivityLength (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivityLengthSEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSEA_Y");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLengthSEA, 1, kHapticSea,
                       &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLength);
}

// sub_10043260 "setTurnSensitivitySEA_Y": vector setter, component y: +0x12C getTurnSensitivitySEA / +0x130 setTurnSensitivitySEA; mirror +0x12C getTurnSensitivitySEA / +0xB8 setTurnSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setTurnSensitivitySEA_Y(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySEA_Y");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivitySEA, 1, kHapticSea,
                       &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivity);
}

// sub_1003FE90 "setAimSensitivitySOLDIER_Z": vector setter, component z: +0xBC getAimSensitivitySOLDIER / +0xC0 setAimSensitivitySOLDIER; mirror +0xBC getAimSensitivitySOLDIER / +0xA8 setAimSensitivity (on foot or no local player).
void as_hapticsettings_setAimSensitivitySOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySOLDIER_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivitySOLDIER, 2, kHapticSoldier,
                       &IHapticSettings::getAimSensitivitySOLDIER, &IHapticSettings::setAimSensitivity);
}

// sub_10040080 "setAimSensitivityLengthSOLDIER_Z": vector setter, component z: +0xC4 getAimSensitivityLengthSOLDIER / +0xC8 setAimSensitivityLengthSOLDIER; mirror +0xC4 getAimSensitivityLengthSOLDIER / +0xB0 setAimSensitivityLength (on foot or no local player).
void as_hapticsettings_setAimSensitivityLengthSOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSOLDIER_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSOLDIER, &IHapticSettings::setAimSensitivityLengthSOLDIER, 2, kHapticSoldier,
                       &IHapticSettings::getAimSensitivityLengthSOLDIER, &IHapticSettings::setAimSensitivityLength);
}

// sub_10040270 "setTurnSensitivitySOLDIER_Z": vector setter, component z: +0xCC getTurnSensitivitySOLDIER / +0xD0 setTurnSensitivitySOLDIER; mirror +0xCC getTurnSensitivitySOLDIER / +0xB8 setTurnSensitivity (on foot or no local player).
void as_hapticsettings_setTurnSensitivitySOLDIER_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySOLDIER_Z");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivitySOLDIER, 2, kHapticSoldier,
                       &IHapticSettings::getTurnSensitivitySOLDIER, &IHapticSettings::setTurnSensitivity);
}

// sub_10040D00 "setAimSensitivityLAND_Z": vector setter, component z: +0x104 getAimSensitivityLAND / +0x108 setAimSensitivityLAND; mirror +0x104 getAimSensitivityLAND / +0xA8 setAimSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLAND_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivityLAND, 2, kHapticLand,
                       &IHapticSettings::getAimSensitivityLAND, &IHapticSettings::setAimSensitivity);
}

// sub_10040E70 "setAimSensitivityLengthLAND_Z": vector setter, component z: +0x10C getAimSensitivityLengthLAND / +0x110 setAimSensitivityLengthLAND; mirror +0x10C getAimSensitivityLengthLAND / +0xB0 setAimSensitivityLength (vehicle category kHapticLand).
void as_hapticsettings_setAimSensitivityLengthLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthLAND_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLengthLAND, 2, kHapticLand,
                       &IHapticSettings::getAimSensitivityLengthLAND, &IHapticSettings::setAimSensitivityLength);
}

// sub_10040FE0 "setTurnSensitivityLAND_Z": vector setter, component z: +0x114 getTurnSensitivityLAND / +0x118 setTurnSensitivityLAND; mirror +0x114 getTurnSensitivityLAND / +0xB8 setTurnSensitivity (vehicle category kHapticLand).
void as_hapticsettings_setTurnSensitivityLAND_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityLAND_Z");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivityLAND, 2, kHapticLand,
                       &IHapticSettings::getTurnSensitivityLAND, &IHapticSettings::setTurnSensitivity);
}

// sub_100419F0 "setAimSensitivityAIR_Z": vector setter, component z: +0xD4 getAimSensitivityAIR / +0xD8 setAimSensitivityAIR; mirror +0xD4 getAimSensitivityAIR / +0xA8 setAimSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityAIR_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivityAIR, 2, kHapticAir,
                       &IHapticSettings::getAimSensitivityAIR, &IHapticSettings::setAimSensitivity);
}

// sub_10041B60 "setAimSensitivityLengthAIR_Z": vector setter, component z: +0xDC getAimSensitivityLengthAIR / +0xE0 setAimSensitivityLengthAIR; mirror +0xDC getAimSensitivityLengthAIR / +0xB0 setAimSensitivityLength (vehicle category kHapticAir).
void as_hapticsettings_setAimSensitivityLengthAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthAIR_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLengthAIR, 2, kHapticAir,
                       &IHapticSettings::getAimSensitivityLengthAIR, &IHapticSettings::setAimSensitivityLength);
}

// sub_10041CD0 "setTurnSensitivityAIR_Z": vector setter, component z: +0xE4 getTurnSensitivityAIR / +0xE8 setTurnSensitivityAIR; mirror +0xE4 getTurnSensitivityAIR / +0xB8 setTurnSensitivity (vehicle category kHapticAir).
void as_hapticsettings_setTurnSensitivityAIR_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityAIR_Z");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivityAIR, 2, kHapticAir,
                       &IHapticSettings::getTurnSensitivityAIR, &IHapticSettings::setTurnSensitivity);
}

// sub_100426E0 "setAimSensitivityHELI_Z": vector setter, component z: +0xEC getAimSensitivityHELI / +0xF0 setAimSensitivityHELI; mirror +0xEC getAimSensitivityHELI / +0xA8 setAimSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityHELI_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivityHELI, 2, kHapticHeli,
                       &IHapticSettings::getAimSensitivityHELI, &IHapticSettings::setAimSensitivity);
}

// sub_10042850 "setAimSensitivityLengthHELI_Z": vector setter, component z: +0xF4 getAimSensitivityLengthHELI / +0xF8 setAimSensitivityLengthHELI; mirror +0xF4 getAimSensitivityLengthHELI / +0xB0 setAimSensitivityLength (vehicle category kHapticHeli).
void as_hapticsettings_setAimSensitivityLengthHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthHELI_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLengthHELI, 2, kHapticHeli,
                       &IHapticSettings::getAimSensitivityLengthHELI, &IHapticSettings::setAimSensitivityLength);
}

// sub_100429C0 "setTurnSensitivityHELI_Z": vector setter, component z: +0xFC getTurnSensitivityHELI / +0x100 setTurnSensitivityHELI; mirror +0xFC getTurnSensitivityHELI / +0xB8 setTurnSensitivity (vehicle category kHapticHeli).
void as_hapticsettings_setTurnSensitivityHELI_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivityHELI_Z");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivityHELI, 2, kHapticHeli,
                       &IHapticSettings::getTurnSensitivityHELI, &IHapticSettings::setTurnSensitivity);
}

// sub_100433D0 "setAimSensitivitySEA_Z": vector setter, component z: +0x11C getAimSensitivitySEA / +0x120 setAimSensitivitySEA; mirror +0x11C getAimSensitivitySEA / +0xA8 setAimSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivitySEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivitySEA_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivitySEA, 2, kHapticSea,
                       &IHapticSettings::getAimSensitivitySEA, &IHapticSettings::setAimSensitivity);
}

// sub_10043540 "setAimSensitivityLengthSEA_Z": vector setter, component z: +0x124 getAimSensitivityLengthSEA / +0x128 setAimSensitivityLengthSEA; mirror +0x124 getAimSensitivityLengthSEA / +0xB0 setAimSensitivityLength (vehicle category kHapticSea).
void as_hapticsettings_setAimSensitivityLengthSEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setAimSensitivityLengthSEA_Z");
    hapticVectorSetter(fn, &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLengthSEA, 2, kHapticSea,
                       &IHapticSettings::getAimSensitivityLengthSEA, &IHapticSettings::setAimSensitivityLength);
}

// sub_100436B0 "setTurnSensitivitySEA_Z": vector setter, component z: +0x12C getTurnSensitivitySEA / +0x130 setTurnSensitivitySEA; mirror +0x12C getTurnSensitivitySEA / +0xB8 setTurnSensitivity (vehicle category kHapticSea).
void as_hapticsettings_setTurnSensitivitySEA_Z(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setTurnSensitivitySEA_Z");
    hapticVectorSetter(fn, &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivitySEA, 2, kHapticSea,
                       &IHapticSettings::getTurnSensitivitySEA, &IHapticSettings::setTurnSensitivity);
}

// sub_10035290 "setWeaponSelectSOLDIER": arg 0 to_string stored into the shared weapon selection (0x100F8550).
void as_hapticsettings_setWeaponSelectSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setWeaponSelectSOLDIER");
    s_hapticWeaponSelect = fn.arg(0).to_string();
}

// sub_10036CA0 "getWeaponSelectSOLDIER": the shared weapon selection (unk_100F8554) as a plain string.
void as_hapticsettings_getWeaponSelectSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponSelectSOLDIER");
    fn.result->set_tu_string(tu_string(s_hapticWeaponSelect.c_str()));
}

// sub_10036D10 "getWeaponInitialSOLDIER": arg 0 == 1 -> the name at 0x100F6C70 as a plain string, else its localised dice String (sub_10036B70).
void as_hapticsettings_getWeaponInitialSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponInitialSOLDIER");
    publishHapticInitialName(fn, kHapticInfantryWeapons[0]);
}

// sub_10036DF0 "setThresholdSelectSOLDIER": arg 0 to_string; result _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, then the name stored into the shared threshold selection (0x100F8534).
void as_hapticsettings_setThresholdSelectSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setThresholdSelectSOLDIER");
    storeHapticThresholdSelect(fn);
}

// sub_10036EB0 "getThresholdSelectSOLDIER": the shared threshold selection (unk_100F8538) as a plain string.
void as_hapticsettings_getThresholdSelectSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdSelectSOLDIER");
    fn.result->set_tu_string(tu_string(s_hapticThresholdSelect.c_str()));
}

// sub_10036F20 "getThresholdInitialSOLDIER": the localised dice String (sub_10036B70) of unk_100F3870, "BFHQ_physicsthresholdlow"; no argument read.
void as_hapticsettings_getThresholdInitialSOLDIER(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdInitialSOLDIER");
    fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString("BFHQ_physicsthresholdlow")));
}

// sub_100352C0 "setWeaponSelectLAND": arg 0 to_string stored into the shared weapon selection (0x100F8550).
void as_hapticsettings_setWeaponSelectLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setWeaponSelectLAND");
    s_hapticWeaponSelect = fn.arg(0).to_string();
}

// sub_10036F70 "getWeaponSelectLAND": the shared weapon selection (unk_100F8554) as a plain string.
void as_hapticsettings_getWeaponSelectLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponSelectLAND");
    fn.result->set_tu_string(tu_string(s_hapticWeaponSelect.c_str()));
}

// sub_10036FE0 "getWeaponInitialLAND": arg 0 == 1 -> the name at 0x100F3310 as a plain string, else its localised dice String (sub_10036B70).
void as_hapticsettings_getWeaponInitialLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponInitialLAND");
    publishHapticInitialName(fn, kHapticVehicleWeapons[0]);
}

// sub_100370C0 "setThresholdSelectLAND": arg 0 to_string; result _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, then the name stored into the shared threshold selection (0x100F8534).
void as_hapticsettings_setThresholdSelectLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setThresholdSelectLAND");
    storeHapticThresholdSelect(fn);
}

// sub_10037180 "getThresholdSelectLAND": the shared threshold selection (unk_100F8538) as a plain string.
void as_hapticsettings_getThresholdSelectLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdSelectLAND");
    fn.result->set_tu_string(tu_string(s_hapticThresholdSelect.c_str()));
}

// sub_100371F0 "getThresholdInitialLAND": the localised dice String (sub_10036B70) of unk_100F3870, "BFHQ_physicsthresholdlow"; no argument read.
void as_hapticsettings_getThresholdInitialLAND(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdInitialLAND");
    fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString("BFHQ_physicsthresholdlow")));
}

// sub_100352F0 "setWeaponSelectAIR": arg 0 to_string stored into the shared weapon selection (0x100F8550).
void as_hapticsettings_setWeaponSelectAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setWeaponSelectAIR");
    s_hapticWeaponSelect = fn.arg(0).to_string();
}

// sub_10037240 "getWeaponSelectAIR": the shared weapon selection (unk_100F8554) as a plain string.
void as_hapticsettings_getWeaponSelectAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponSelectAIR");
    fn.result->set_tu_string(tu_string(s_hapticWeaponSelect.c_str()));
}

// sub_100372B0 "getWeaponInitialAIR": arg 0 == 1 -> the name at 0x100F3364 as a plain string, else its localised dice String (sub_10036B70).
void as_hapticsettings_getWeaponInitialAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponInitialAIR");
    publishHapticInitialName(fn, kHapticVehicleWeapons[3]);
}

// sub_10037390 "setThresholdSelectAIR": arg 0 to_string; result _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, then the name stored into the shared threshold selection (0x100F8534).
void as_hapticsettings_setThresholdSelectAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setThresholdSelectAIR");
    storeHapticThresholdSelect(fn);
}

// sub_10037450 "getThresholdSelectAIR": the shared threshold selection (unk_100F8538) as a plain string.
void as_hapticsettings_getThresholdSelectAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdSelectAIR");
    fn.result->set_tu_string(tu_string(s_hapticThresholdSelect.c_str()));
}

// sub_100374C0 "getThresholdInitialAIR": the localised dice String (sub_10036B70) of unk_100F3870, "BFHQ_physicsthresholdlow"; no argument read.
void as_hapticsettings_getThresholdInitialAIR(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdInitialAIR");
    fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString("BFHQ_physicsthresholdlow")));
}

// sub_10035320 "setWeaponSelectHELI": arg 0 to_string stored into the shared weapon selection (0x100F8550).
void as_hapticsettings_setWeaponSelectHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setWeaponSelectHELI");
    s_hapticWeaponSelect = fn.arg(0).to_string();
}

// sub_10037510 "getWeaponSelectHELI": the shared weapon selection (unk_100F8554) as a plain string.
void as_hapticsettings_getWeaponSelectHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponSelectHELI");
    fn.result->set_tu_string(tu_string(s_hapticWeaponSelect.c_str()));
}

// sub_10037580 "getWeaponInitialHELI": arg 0 == 1 -> the name at 0x100F3348 as a plain string, else its localised dice String (sub_10036B70).
void as_hapticsettings_getWeaponInitialHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponInitialHELI");
    publishHapticInitialName(fn, kHapticVehicleWeapons[2]);
}

// sub_10037660 "setThresholdSelectHELI": arg 0 to_string; result _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, then the name stored into the shared threshold selection (0x100F8534).
void as_hapticsettings_setThresholdSelectHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setThresholdSelectHELI");
    storeHapticThresholdSelect(fn);
}

// sub_10037720 "getThresholdSelectHELI": the shared threshold selection (unk_100F8538) as a plain string.
void as_hapticsettings_getThresholdSelectHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdSelectHELI");
    fn.result->set_tu_string(tu_string(s_hapticThresholdSelect.c_str()));
}

// sub_10037790 "getThresholdInitialHELI": the localised dice String (sub_10036B70) of unk_100F3870, "BFHQ_physicsthresholdlow"; no argument read.
void as_hapticsettings_getThresholdInitialHELI(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdInitialHELI");
    fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString("BFHQ_physicsthresholdlow")));
}

// sub_10035350 "setWeaponSelectSEA": arg 0 to_string stored into the shared weapon selection (0x100F8550).
void as_hapticsettings_setWeaponSelectSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setWeaponSelectSEA");
    s_hapticWeaponSelect = fn.arg(0).to_string();
}

// sub_100377E0 "getWeaponSelectSEA": the shared weapon selection (unk_100F8554) as a plain string.
void as_hapticsettings_getWeaponSelectSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponSelectSEA");
    fn.result->set_tu_string(tu_string(s_hapticWeaponSelect.c_str()));
}

// sub_10037850 "getWeaponInitialSEA": arg 0 == 1 -> the name at 0x100F3310 as a plain string, else its localised dice String (sub_10036B70).
void as_hapticsettings_getWeaponInitialSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getWeaponInitialSEA");
    publishHapticInitialName(fn, kHapticVehicleWeapons[0]);
}

// sub_10037930 "setThresholdSelectSEA": arg 0 to_string; result _stricmp(name, "BFHQ_physicsthresholdhigh") == 0, then the name stored into the shared threshold selection (0x100F8534).
void as_hapticsettings_setThresholdSelectSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_setThresholdSelectSEA");
    storeHapticThresholdSelect(fn);
}

// sub_100379F0 "getThresholdSelectSEA": the shared threshold selection (unk_100F8538) as a plain string.
void as_hapticsettings_getThresholdSelectSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdSelectSEA");
    fn.result->set_tu_string(tu_string(s_hapticThresholdSelect.c_str()));
}

// sub_10037A60 "getThresholdInitialSEA": the localised dice String (sub_10036B70) of unk_100F3870, "BFHQ_physicsthresholdlow"; no argument read.
void as_hapticsettings_getThresholdInitialSEA(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_hapticsettings_getThresholdInitialSEA");
    fn.result->set_as_object_interface(videoSettingsLocalisedString(renddx9abi::HostString("BFHQ_physicsthresholdlow")));
}


static void registerHapticNatives(SwiffAsNamespace** ns, const char* const* names, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (::strcmp(names[i], "HapticSettings") != 0)
            continue;
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_apply>("apply");                                   // 1004460F
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_reset>("reset");                                   // 10044676
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_cancel>("cancel");                                  // 100446CE
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSOLDIER_X>("getControlBoxLengthSOLDIER_X");            // 10044744
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSOLDIER_X>("getControlBoxStiffnessSOLDIER_X");         // 100447B4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthLAND_X>("getControlBoxLengthLAND_X");               // 10044830
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessLAND_X>("getControlBoxStiffnessLAND_X");            // 100448AC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthAIR_X>("getControlBoxLengthAIR_X");                // 10044928
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessAIR_X>("getControlBoxStiffnessAIR_X");             // 100449A4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthHELI_X>("getControlBoxLengthHELI_X");               // 10044A20
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessHELI_X>("getControlBoxStiffnessHELI_X");            // 10044A9C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSEA_X>("getControlBoxLengthSEA_X");                // 10044B18
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSEA_X>("getControlBoxStiffnessSEA_X");             // 10044B94
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSOLDIER_Y>("getControlBoxLengthSOLDIER_Y");            // 10044C10
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSOLDIER_Y>("getControlBoxStiffnessSOLDIER_Y");         // 10044C8C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthLAND_Y>("getControlBoxLengthLAND_Y");               // 10044D08
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessLAND_Y>("getControlBoxStiffnessLAND_Y");            // 10044D84
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthAIR_Y>("getControlBoxLengthAIR_Y");                // 10044E00
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessAIR_Y>("getControlBoxStiffnessAIR_Y");             // 10044E7C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthHELI_Y>("getControlBoxLengthHELI_Y");               // 10044EF8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessHELI_Y>("getControlBoxStiffnessHELI_Y");            // 10044F74
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSEA_Y>("getControlBoxLengthSEA_Y");                // 10044FF0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSEA_Y>("getControlBoxStiffnessSEA_Y");             // 1004506C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSOLDIER_Z>("getControlBoxLengthSOLDIER_Z");            // 100450E8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSOLDIER_Z>("getControlBoxStiffnessSOLDIER_Z");         // 10045164
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthLAND_Z>("getControlBoxLengthLAND_Z");               // 100451E0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessLAND_Z>("getControlBoxStiffnessLAND_Z");            // 1004525C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthAIR_Z>("getControlBoxLengthAIR_Z");                // 100452D8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessAIR_Z>("getControlBoxStiffnessAIR_Z");             // 10045354
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthHELI_Z>("getControlBoxLengthHELI_Z");               // 100453D0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessHELI_Z>("getControlBoxStiffnessHELI_Z");            // 1004544C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxLengthSEA_Z>("getControlBoxLengthSEA_Z");                // 100454C8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getControlBoxStiffnessSEA_Z>("getControlBoxStiffnessSEA_Z");             // 10045544
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySOLDIER_X>("getAimSensitivitySOLDIER_X");              // 100455C0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSOLDIER_X>("getAimSensitivityLengthSOLDIER_X");        // 1004563C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySOLDIER_X>("getTurnSensitivitySOLDIER_X");             // 100456B8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLAND_X>("getAimSensitivityLAND_X");                 // 10045734
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthLAND_X>("getAimSensitivityLengthLAND_X");           // 100457B0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityLAND_X>("getTurnSensitivityLAND_X");                // 1004582C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityAIR_X>("getAimSensitivityAIR_X");                  // 100458A8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthAIR_X>("getAimSensitivityLengthAIR_X");            // 10045924
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityAIR_X>("getTurnSensitivityAIR_X");                 // 100459A0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityHELI_X>("getAimSensitivityHELI_X");                 // 10045A1C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthHELI_X>("getAimSensitivityLengthHELI_X");           // 10045A98
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityHELI_X>("getTurnSensitivityHELI_X");                // 10045B14
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySEA_X>("getAimSensitivitySEA_X");                  // 10045B90
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSEA_X>("getAimSensitivityLengthSEA_X");            // 10045C0C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySEA_X>("getTurnSensitivitySEA_X");                 // 10045C88
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySOLDIER_Y>("getAimSensitivitySOLDIER_Y");              // 10045D04
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSOLDIER_Y>("getAimSensitivityLengthSOLDIER_Y");        // 10045D80
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySOLDIER_Y>("getTurnSensitivitySOLDIER_Y");             // 10045DFC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLAND_Y>("getAimSensitivityLAND_Y");                 // 10045E78
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthLAND_Y>("getAimSensitivityLengthLAND_Y");           // 10045EF4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityLAND_Y>("getTurnSensitivityLAND_Y");                // 10045F70
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityAIR_Y>("getAimSensitivityAIR_Y");                  // 10045FEC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthAIR_Y>("getAimSensitivityLengthAIR_Y");            // 10046068
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityAIR_Y>("getTurnSensitivityAIR_Y");                 // 100460E4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityHELI_Y>("getAimSensitivityHELI_Y");                 // 10046160
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthHELI_Y>("getAimSensitivityLengthHELI_Y");           // 100461DC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityHELI_Y>("getTurnSensitivityHELI_Y");                // 10046258
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySEA_Y>("getAimSensitivitySEA_Y");                  // 100462D4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSEA_Y>("getAimSensitivityLengthSEA_Y");            // 10046350
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySEA_Y>("getTurnSensitivitySEA_Y");                 // 100463CC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySOLDIER_Z>("getAimSensitivitySOLDIER_Z");              // 10046448
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSOLDIER_Z>("getAimSensitivityLengthSOLDIER_Z");        // 100464C4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySOLDIER_Z>("getTurnSensitivitySOLDIER_Z");             // 10046540
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLAND_Z>("getAimSensitivityLAND_Z");                 // 100465BC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthLAND_Z>("getAimSensitivityLengthLAND_Z");           // 10046638
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityLAND_Z>("getTurnSensitivityLAND_Z");                // 100466B4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityAIR_Z>("getAimSensitivityAIR_Z");                  // 10046730
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthAIR_Z>("getAimSensitivityLengthAIR_Z");            // 100467AC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityAIR_Z>("getTurnSensitivityAIR_Z");                 // 10046828
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityHELI_Z>("getAimSensitivityHELI_Z");                 // 100468A4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthHELI_Z>("getAimSensitivityLengthHELI_Z");           // 10046920
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivityHELI_Z>("getTurnSensitivityHELI_Z");                // 1004699C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivitySEA_Z>("getAimSensitivitySEA_Z");                  // 10046A18
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getAimSensitivityLengthSEA_Z>("getAimSensitivityLengthSEA_Z");            // 10046A94
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getTurnSensitivitySEA_Z>("getTurnSensitivitySEA_Z");                 // 10046B10
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERScale_X>("getPhysicsSOLDIERScale_X");                // 10046B8C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERScale_Y>("getPhysicsSOLDIERScale_Y");                // 10046C08
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERScale_Z>("getPhysicsSOLDIERScale_Z");                // 10046C84
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERTransition>("getPhysicsSOLDIERTransition");             // 10046D00
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERSmoothTime_X>("getPhysicsSOLDIERSmoothTime_X");           // 10046D7C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERSmoothTime_Y>("getPhysicsSOLDIERSmoothTime_Y");           // 10046DF8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSOLDIERSmoothTime_Z>("getPhysicsSOLDIERSmoothTime_Z");           // 10046E74
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDScale_X>("getPhysicsLANDScale_X");                   // 10046EF0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDScale_Y>("getPhysicsLANDScale_Y");                   // 10046F6C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDScale_Z>("getPhysicsLANDScale_Z");                   // 10046FE8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDTransition>("getPhysicsLANDTransition");                // 10047064
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDSmoothTime_X>("getPhysicsLANDSmoothTime_X");              // 100470E0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDSmoothTime_Y>("getPhysicsLANDSmoothTime_Y");              // 1004715C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsLANDSmoothTime_Z>("getPhysicsLANDSmoothTime_Z");              // 100471D8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRScale_X>("getPhysicsAIRScale_X");                    // 10047254
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRScale_Y>("getPhysicsAIRScale_Y");                    // 100472D0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRScale_Z>("getPhysicsAIRScale_Z");                    // 1004734C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRTransition>("getPhysicsAIRTransition");                 // 100473C8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRSmoothTime_X>("getPhysicsAIRSmoothTime_X");               // 10047444
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRSmoothTime_Y>("getPhysicsAIRSmoothTime_Y");               // 100474C0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsAIRSmoothTime_Z>("getPhysicsAIRSmoothTime_Z");               // 1004753C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELIScale_X>("getPhysicsHELIScale_X");                   // 100475B8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELIScale_Y>("getPhysicsHELIScale_Y");                   // 10047634
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELIScale_Z>("getPhysicsHELIScale_Z");                   // 100476B0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELITransition>("getPhysicsHELITransition");                // 1004772C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELISmoothTime_X>("getPhysicsHELISmoothTime_X");              // 100477A8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELISmoothTime_Y>("getPhysicsHELISmoothTime_Y");              // 10047824
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsHELISmoothTime_Z>("getPhysicsHELISmoothTime_Z");              // 100478A0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEAScale_X>("getPhysicsSEAScale_X");                    // 1004791C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEAScale_Y>("getPhysicsSEAScale_Y");                    // 10047998
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEAScale_Z>("getPhysicsSEAScale_Z");                    // 10047A14
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEATransition>("getPhysicsSEATransition");                 // 10047A90
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEASmoothTime_X>("getPhysicsSEASmoothTime_X");               // 10047B0C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEASmoothTime_Y>("getPhysicsSEASmoothTime_Y");               // 10047B88
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getPhysicsSEASmoothTime_Z>("getPhysicsSEASmoothTime_Z");               // 10047C04
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getDamageScale>("getDamageScale");                          // 10047C80
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getRecoilPunchScale>("getRecoilPunchScale");                     // 10047CFC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getRecoilPunchTime>("getRecoilPunchTime");                      // 10047D78
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getRecoilYawScale>("getRecoilYawScale");                       // 10047DF4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getRecoilPitchScale>("getRecoilPitchScale");                     // 10047E70
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSOLDIER_X>("setControlBoxLengthSOLDIER_X");            // 10047EEC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSOLDIER_X>("setControlBoxStiffnessSOLDIER_X");         // 10047F68
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthLAND_X>("setControlBoxLengthLAND_X");               // 10047FE4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessLAND_X>("setControlBoxStiffnessLAND_X");            // 10048060
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthAIR_X>("setControlBoxLengthAIR_X");                // 100480DC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessAIR_X>("setControlBoxStiffnessAIR_X");             // 10048158
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthHELI_X>("setControlBoxLengthHELI_X");               // 100481D4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessHELI_X>("setControlBoxStiffnessHELI_X");            // 10048250
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSEA_X>("setControlBoxLengthSEA_X");                // 100482CC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSEA_X>("setControlBoxStiffnessSEA_X");             // 10048348
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSOLDIER_Y>("setControlBoxLengthSOLDIER_Y");            // 100483C4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSOLDIER_Y>("setControlBoxStiffnessSOLDIER_Y");         // 10048440
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthLAND_Y>("setControlBoxLengthLAND_Y");               // 100484BC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessLAND_Y>("setControlBoxStiffnessLAND_Y");            // 10048538
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthAIR_Y>("setControlBoxLengthAIR_Y");                // 100485B4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessAIR_Y>("setControlBoxStiffnessAIR_Y");             // 10048630
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthHELI_Y>("setControlBoxLengthHELI_Y");               // 100486AC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessHELI_Y>("setControlBoxStiffnessHELI_Y");            // 10048728
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSEA_Y>("setControlBoxLengthSEA_Y");                // 100487A4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSEA_Y>("setControlBoxStiffnessSEA_Y");             // 10048820
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSOLDIER_Z>("setControlBoxLengthSOLDIER_Z");            // 1004889C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSOLDIER_Z>("setControlBoxStiffnessSOLDIER_Z");         // 10048918
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthLAND_Z>("setControlBoxLengthLAND_Z");               // 10048994
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessLAND_Z>("setControlBoxStiffnessLAND_Z");            // 10048A10
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthAIR_Z>("setControlBoxLengthAIR_Z");                // 10048A8C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessAIR_Z>("setControlBoxStiffnessAIR_Z");             // 10048B08
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthHELI_Z>("setControlBoxLengthHELI_Z");               // 10048B84
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessHELI_Z>("setControlBoxStiffnessHELI_Z");            // 10048C00
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxLengthSEA_Z>("setControlBoxLengthSEA_Z");                // 10048C7C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setControlBoxStiffnessSEA_Z>("setControlBoxStiffnessSEA_Z");             // 10048CF8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySOLDIER_X>("setAimSensitivitySOLDIER_X");              // 10048D74
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSOLDIER_X>("setAimSensitivityLengthSOLDIER_X");        // 10048DF0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySOLDIER_X>("setTurnSensitivitySOLDIER_X");             // 10048E6C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLAND_X>("setAimSensitivityLAND_X");                 // 10048EE8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthLAND_X>("setAimSensitivityLengthLAND_X");           // 10048F64
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityLAND_X>("setTurnSensitivityLAND_X");                // 10048FE0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityAIR_X>("setAimSensitivityAIR_X");                  // 1004905C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthAIR_X>("setAimSensitivityLengthAIR_X");            // 100490D8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityAIR_X>("setTurnSensitivityAIR_X");                 // 10049154
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityHELI_X>("setAimSensitivityHELI_X");                 // 100491D0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthHELI_X>("setAimSensitivityLengthHELI_X");           // 1004924C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityHELI_X>("setTurnSensitivityHELI_X");                // 100492C8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySEA_X>("setAimSensitivitySEA_X");                  // 10049344
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSEA_X>("setAimSensitivityLengthSEA_X");            // 100493C0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySEA_X>("setTurnSensitivitySEA_X");                 // 1004943C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySOLDIER_Y>("setAimSensitivitySOLDIER_Y");              // 100494B8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSOLDIER_Y>("setAimSensitivityLengthSOLDIER_Y");        // 10049534
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySOLDIER_Y>("setTurnSensitivitySOLDIER_Y");             // 100495B0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLAND_Y>("setAimSensitivityLAND_Y");                 // 1004962C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthLAND_Y>("setAimSensitivityLengthLAND_Y");           // 100496A8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityLAND_Y>("setTurnSensitivityLAND_Y");                // 10049724
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityAIR_Y>("setAimSensitivityAIR_Y");                  // 100497A0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthAIR_Y>("setAimSensitivityLengthAIR_Y");            // 1004981C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityAIR_Y>("setTurnSensitivityAIR_Y");                 // 10049898
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityHELI_Y>("setAimSensitivityHELI_Y");                 // 10049914
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthHELI_Y>("setAimSensitivityLengthHELI_Y");           // 10049990
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityHELI_Y>("setTurnSensitivityHELI_Y");                // 10049A0C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySEA_Y>("setAimSensitivitySEA_Y");                  // 10049A88
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSEA_Y>("setAimSensitivityLengthSEA_Y");            // 10049B04
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySEA_Y>("setTurnSensitivitySEA_Y");                 // 10049B80
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySOLDIER_Z>("setAimSensitivitySOLDIER_Z");              // 10049BFC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSOLDIER_Z>("setAimSensitivityLengthSOLDIER_Z");        // 10049C78
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySOLDIER_Z>("setTurnSensitivitySOLDIER_Z");             // 10049CF4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLAND_Z>("setAimSensitivityLAND_Z");                 // 10049D70
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthLAND_Z>("setAimSensitivityLengthLAND_Z");           // 10049DEC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityLAND_Z>("setTurnSensitivityLAND_Z");                // 10049E68
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityAIR_Z>("setAimSensitivityAIR_Z");                  // 10049EE4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthAIR_Z>("setAimSensitivityLengthAIR_Z");            // 10049F60
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityAIR_Z>("setTurnSensitivityAIR_Z");                 // 10049FDC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityHELI_Z>("setAimSensitivityHELI_Z");                 // 1004A058
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthHELI_Z>("setAimSensitivityLengthHELI_Z");           // 1004A0D4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivityHELI_Z>("setTurnSensitivityHELI_Z");                // 1004A150
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivitySEA_Z>("setAimSensitivitySEA_Z");                  // 1004A1CC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setAimSensitivityLengthSEA_Z>("setAimSensitivityLengthSEA_Z");            // 1004A248
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setTurnSensitivitySEA_Z>("setTurnSensitivitySEA_Z");                 // 1004A2C4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERScale_X>("setPhysicsSOLDIERScale_X");                // 1004A340
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERScale_Y>("setPhysicsSOLDIERScale_Y");                // 1004A3BC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERScale_Z>("setPhysicsSOLDIERScale_Z");                // 1004A438
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERTransition>("setPhysicsSOLDIERTransition");             // 1004A4B4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERSmoothTime_X>("setPhysicsSOLDIERSmoothTime_X");           // 1004A530
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERSmoothTime_Y>("setPhysicsSOLDIERSmoothTime_Y");           // 1004A5AC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSOLDIERSmoothTime_Z>("setPhysicsSOLDIERSmoothTime_Z");           // 1004A628
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDScale_X>("setPhysicsLANDScale_X");                   // 1004A6A4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDScale_Y>("setPhysicsLANDScale_Y");                   // 1004A720
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDScale_Z>("setPhysicsLANDScale_Z");                   // 1004A79C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDTransition>("setPhysicsLANDTransition");                // 1004A818
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDSmoothTime_X>("setPhysicsLANDSmoothTime_X");              // 1004A894
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDSmoothTime_Y>("setPhysicsLANDSmoothTime_Y");              // 1004A910
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsLANDSmoothTime_Z>("setPhysicsLANDSmoothTime_Z");              // 1004A98C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRScale_X>("setPhysicsAIRScale_X");                    // 1004AA08
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRScale_Y>("setPhysicsAIRScale_Y");                    // 1004AA84
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRScale_Z>("setPhysicsAIRScale_Z");                    // 1004AB00
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRTransition>("setPhysicsAIRTransition");                 // 1004AB7C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRSmoothTime_X>("setPhysicsAIRSmoothTime_X");               // 1004ABF8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRSmoothTime_Y>("setPhysicsAIRSmoothTime_Y");               // 1004AC74
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsAIRSmoothTime_Z>("setPhysicsAIRSmoothTime_Z");               // 1004ACF0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELIScale_X>("setPhysicsHELIScale_X");                   // 1004AD6C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELIScale_Y>("setPhysicsHELIScale_Y");                   // 1004ADE8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELIScale_Z>("setPhysicsHELIScale_Z");                   // 1004AE64
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELITransition>("setPhysicsHELITransition");                // 1004AEE0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELISmoothTime_X>("setPhysicsHELISmoothTime_X");              // 1004AF5C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELISmoothTime_Y>("setPhysicsHELISmoothTime_Y");              // 1004AFD8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsHELISmoothTime_Z>("setPhysicsHELISmoothTime_Z");              // 1004B054
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEAScale_X>("setPhysicsSEAScale_X");                    // 1004B0D0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEAScale_Y>("setPhysicsSEAScale_Y");                    // 1004B14C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEAScale_Z>("setPhysicsSEAScale_Z");                    // 1004B1C8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEATransition>("setPhysicsSEATransition");                 // 1004B244
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEASmoothTime_X>("setPhysicsSEASmoothTime_X");               // 1004B2C0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEASmoothTime_Y>("setPhysicsSEASmoothTime_Y");               // 1004B33C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setPhysicsSEASmoothTime_Z>("setPhysicsSEASmoothTime_Z");               // 1004B3B8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setDamageScale>("setDamageScale");                          // 1004B434
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setRecoilPunchScale>("setRecoilPunchScale");                     // 1004B4B0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setRecoilPunchTime>("setRecoilPunchTime");                      // 1004B52C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setRecoilYawScale>("setRecoilYawScale");                       // 1004B5A8
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setRecoilPitchScale>("setRecoilPitchScale");                     // 1004B624
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setWeaponSelectSOLDIER>("setWeaponSelectSOLDIER");                  // 1004B6A0
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponSelectSOLDIER>("getWeaponSelectSOLDIER");                  // 1004B71C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponInitialSOLDIER>("getWeaponInitialSOLDIER");                 // 1004B798
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setThresholdSelectSOLDIER>("setThresholdSelectSOLDIER");               // 1004B814
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdSelectSOLDIER>("getThresholdSelectSOLDIER");               // 1004B890
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdInitialSOLDIER>("getThresholdInitialSOLDIER");              // 1004B90C
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setWeaponSelectLAND>("setWeaponSelectLAND");                     // 1004B988
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponSelectLAND>("getWeaponSelectLAND");                     // 1004BA04
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponInitialLAND>("getWeaponInitialLAND");                    // 1004BA80
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setThresholdSelectLAND>("setThresholdSelectLAND");                  // 1004BAFC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdSelectLAND>("getThresholdSelectLAND");                  // 1004BB78
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdInitialLAND>("getThresholdInitialLAND");                 // 1004BBF4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setWeaponSelectAIR>("setWeaponSelectAIR");                      // 1004BC70
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponSelectAIR>("getWeaponSelectAIR");                      // 1004BCEC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponInitialAIR>("getWeaponInitialAIR");                     // 1004BD68
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setThresholdSelectAIR>("setThresholdSelectAIR");                   // 1004BDE4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdSelectAIR>("getThresholdSelectAIR");                   // 1004BE60
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdInitialAIR>("getThresholdInitialAIR");                  // 1004BEDC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setWeaponSelectHELI>("setWeaponSelectHELI");                     // 1004BF58
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponSelectHELI>("getWeaponSelectHELI");                     // 1004BFD4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponInitialHELI>("getWeaponInitialHELI");                    // 1004C050
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setThresholdSelectHELI>("setThresholdSelectHELI");                  // 1004C0CC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdSelectHELI>("getThresholdSelectHELI");                  // 1004C148
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdInitialHELI>("getThresholdInitialHELI");                 // 1004C1C4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setWeaponSelectSEA>("setWeaponSelectSEA");                      // 1004C240
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponSelectSEA>("getWeaponSelectSEA");                      // 1004C2BC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getWeaponInitialSEA>("getWeaponInitialSEA");                     // 1004C338
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setThresholdSelectSEA>("setThresholdSelectSEA");                   // 1004C3B4
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdSelectSEA>("getThresholdSelectSEA");                   // 1004C430
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getThresholdInitialSEA>("getThresholdInitialSEA");                  // 1004C4AC
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_setHapticSettingsPage>("setHapticSettingsPage");                   // 1004C528
        ns[i]->registerNativeCompatibleMethod<as_hapticsettings_getHapticSettingsPage>("getHapticSettingsPage");                   // 1004C5A4
    }
}
