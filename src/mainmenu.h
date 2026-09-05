/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 dchau360
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef MAINMENU_H
#define MAINMENU_H

#include <SDL3/SDL.h>
#include <vector>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <memory>
#ifdef FROZEN_BUBBLE_TEST_ACCESS
#include <functional>
#endif

#include "menubutton.h"
#include "networkclient.h"
#include "shaderstuff.h"
#include "bubblegame.h"
#include "netbot.h"
#include "ttftext.h"

#pragma region "banner_defines"
#define BANNER_START 1000
#define BANNER_SPACING 80
#define BANNER_MINX 304
#define BANNER_MAXX 596
#define BANNER_Y 243
#define BANNER_SLOWDOWN 1
#pragma endregion

#define BLINK_FRAMES 5
#define BLINK_SLOWDOWN 30

#define SP_OPT 5

class MainMenu final
{
public:
    MainMenu(const SDL_Renderer *renderer);
    MainMenu(const MainMenu&) = delete;
    ~MainMenu();
    void Render(void);
    void RefreshCandy();
    void HandleInput(SDL_Event *e);
    void SelectAndPressButton(int idx);
    // Handles a tap/click at logical canvas coords while a panel is open.
    // Returns true when consumed. First tap on a row highlights it, a second tap
    // on the same row activates it.
    //
    // verticalDrift is how far the finger moved vertically before release
    // (0 for a mouse click, which has no such ambiguity). A touch that
    // travels a real distance but falls short of ClassifyMenuSwipe's own
    // Up/Down threshold is an undershot swipe attempt, not a stationary
    // tap -- and since most rows are shorter than that travel, it easily
    // lands right back on the row it started from. Activating an
    // already-selected row on release is still consumed here (the tap
    // landed on a real row) but does nothing further when the drift is
    // past tap-jitter range, instead of mutating that row's value.
    bool HandlePanelTap(float lx, float ly, float verticalDrift = 0.f);
    // True when (lx, ly) sits on a row registered with splitAdjust -- a row
    // stepped by which half a second tap lands on (Game speed, Victories
    // limit, bot count, ...). A pure query, never mutating the selection: it
    // lets a caller ask "does HandlePanelTap need first look here?" before
    // applying a swipe gesture that would otherwise compete with it. See the
    // call site in FrozenBubble::HandleInput for why that competition is real.
    bool IsSteppedRowAt(float lx, float ly) const;
    bool IsAwaitingKeyBind() const { return showingKeysPanel && awaitKp; }
    bool IsTextEditActive() const { return networkFieldEditing; }
    bool HasAnyPanelOpen() const {
        return showingKeysPanel || showingSPPanel || showingOptPanel
            || showingNetPanel || showingLevelPanel || showingLocalMPPanel
            || showingNetSetupPanel || showingHelpPanel;
    }
    void SetupNewGame(int mode);
    void ShowPanel(int which);
    void ReturnToMenu();
    void ReturnToNetLobby();  // Return to network lobby after quitting a network game
private:
#ifdef FROZEN_BUBBLE_TEST_ACCESS
    friend struct MainMenuTestAccess;
    struct HeadlessTestTag {};
    MainMenu(const SDL_Renderer *renderer, HeadlessTestTag);
    bool headlessTestMode = false;
    std::function<void(const SetupSettings&)> testLocalGameStart;
#endif

    const SDL_Renderer *renderer;
    std::vector<MenuButton> buttons;
    SDL_Texture *background;

    //candy
    SDL_Rect fb_logo_rect, candy_fb_rect;
    SDL_Texture *fbLogo;
    TextureEx candyOrig, candyModif, logoMask;
    int candyIndex = 0;
    int candyMethod = -1;
    bool candyInit = false;
    void InitCandy();
    
    //banner
    SDL_Rect banner_rect;
    SDL_Texture *bannerArtwork, *bannerCPU, *bannerLevel, *bannerSound;
    SDL_Texture *blinkGreenL, *blinkGreenR, *blinkPurpleL, *blinkPurpleR;
    int bannerFU = BANNER_SLOWDOWN;
    int bannerFormulas[4];
    int bannerMax = 0, bannerCurpos = 0;

    //blink
    SDL_Rect blink_green_left, blink_green_right, blink_purple_left, blink_purple_right;
    int blinkGreen = 0, blinkPurple = 0, waitGreen = 0, waitPurple = 0;

    //rest
    uint8_t active_button_index;

    void press();
    void up();
    void down();

    // ---- Panel row tap targets -------------------------------------------
    // Whichever panel renders publishes the on-screen band of each selectable
    // row here, together with the selection variable those rows drive. Touch
    // input then hit-tests the bands instead of re-deriving each panel's
    // layout, which would have to be kept in step by hand for five different
    // panels -- and which is not even expressible for the lobby's room list or
    // the game room's player grid, whose row counts vary at runtime.
    struct PanelTapRow {
        SDL_Rect rect;
        int index;      // value to assign to the primary selection
        int subIndex;   // value for the secondary selection, or -1 for none
        // Rows whose value is stepped with LEFT/RIGHT rather than activated
        // with Return. A second tap on one of these sends LEFT or RIGHT
        // depending on which half was touched; sending Return would do
        // literally nothing, which is how the game-speed row ended up with no
        // way to change it on a phone at all.
        bool splitAdjust;
        // Overrides what a second tap sends, for a sub-target that sits inside
        // a row and does something different from the row itself -- the follow
        // star on a server entry, which must toggle following rather than
        // connect. 0 means "use the default", i.e. splitAdjust or Return.
        // A row registered with one of these must be added *before* the wider
        // row it sits inside, since the first band that contains the touch
        // wins.
        SDL_Keycode activateKey;
    };
    std::vector<PanelTapRow> panelTapRows;
    int* panelTapSelection = nullptr;
    // Second axis, for the game room's grid: a cell picks both a settings row
    // and a player column. Null for the panels that are a plain list.
    int* panelTapSubSelection = nullptr;

    // Called by a panel's render before it registers rows. Clearing here means
    // the last panel to render owns the rows, which is what the caller wants:
    // only one panel is interactive at a time.
    void BeginPanelTapRows(int* selection, int* subSelection = nullptr);
    void AddPanelTapRow(int index, const SDL_Rect& rect, int subIndex = -1,
                        bool splitAdjust = false, SDL_Keycode activateKey = 0);

    // Follow/unfollow a server for join notifications: updates the persisted
    // list and, when we happen to be connected to that same server, tells it
    // to start or stop notifying this device. Shared by the LAN and Net server
    // lists.
    void ToggleFollowServer(const ServerInfo& server);

    // Follow toggle for the online lobby's header row (kLobbyFollow): the
    // list-based ToggleFollowServer() above needs a ServerInfo, but the
    // lobby only knows "whatever we're connected to right now" -- this
    // builds one from NetworkClient's connection state and also gates on
    // GetNotifySupport() so it's a no-op (with a status message) against a
    // server that has already told us, or is still telling us, whether it
    // understands the follow protocol at all.
    void ToggleFollowCurrentServer();

    // Re-send this device's push token if the server we just entered is one the
    // player follows. Called on every lobby entry: push tokens rotate, and a
    // stale one on the server silently stops delivering, so refreshing costs
    // one line on the wire and removes a whole class of "notifications just
    // stopped working" failures. A no-op where there is no token.
    void RefreshFollowRegistration();

    // HandleInput decomposition (mainmenu_input.cpp)
    void MenuTextInputEvent(SDL_Event *e);
    bool MenuEditingKey(SDL_Event *e);
    bool KeysPanelKey(SDL_Event *e);
    bool HelpPanelKey(SDL_Event *e);
    bool LobbyChatTypingKey(SDL_Event *e);
    bool LocalMPPanelKey(SDL_Event *e);
    void PlayMenuSFX(const char *name);
    void MenuUpKey();
    void MenuDownKey();
    void MenuLeftRightKey(SDL_Event *e);
    void SubmitLobbyChatInput(NetworkClient *netClient);
    void GameRoomHostReturn(NetworkClient *netClient, GameRoom *currentGame);
    void MenuReturnKey();
    void MenuEscapeKey();

    void BlinkRender();
    void BannerRender();
    void CandyRender();

    TTFText panelText;

    // HelpPanelRender renders up to ~24 lines per page, each one a fresh
    // UpdateText() call. Every other panel that shares panelText resizes it
    // only once or twice per render (a header title, a footer hint), but
    // cycling one TTF_Font through three sizes/styles (15 Bold / 14 Normal /
    // 13 Italic) tens of times per render, called repeatedly across a help
    // page's scroll range, is what tripped LeakSanitizer on
    // localmultiplayer-settings-test (CI run for v2.4.63: 66728 bytes leaked,
    // stack entirely inside SDL_ttf/FreeType with no frames of our own code
    // -- resizing a shared TTF_Font that many times doesn't fully release
    // its previous glyph cache in the SDL_ttf/FreeType build CI uses). Three
    // dedicated, fixed-size fonts loaded once sidestep the resize churn
    // entirely, matching how every other TTFText member in this class is
    // used (LoadFont once at construction, never resized afterward).
    TTFText helpHeadingText, helpBodyText, helpNoteText;

    // LocalMPPanelRender draws each row through panelText separately (one
    // per-row colour), so panelText.Text() only ever holds whatever the last
    // row drawn was by the time the function returns -- unlike every other
    // panel here, which still renders as one block. This is where the whole
    // row list ends up instead, purely so MainMenuTestAccess can inspect a
    // specific row without a real renderer walking pixels.
    std::string lastLocalMPPanelText;

    //singleplayer panel
    SDL_Texture *singlePanelBG;
    SDL_Texture *singleButtonAct, *singleButtonIdle;
    int activeSPIdx = 0;
    bool showingSPPanel = false;

    // Runtime-rendered replacements for this panel's old baked labels
    // (txt_<option>_{text,outlined_text}.png): a heavily stylized carved-wood
    // font that read poorly at the row's own 37px height, wood grain bleeding
    // straight through the letterforms -- and txt_local_multiplayer_*.png
    // turned out to be a byte-for-byte copy of txt_multiplayer_training_*.png,
    // so two of the five rows read "MULTIPLAYER TRAINING". Neither is a MENU
    // STYLE row -- this panel's background art doesn't change with the
    // player's chosen theme -- so these render once with RenderRingedText
    // (ttftext.h) using a fixed, deliberately plain font instead of going
    // through menutheme.h.
    SDL_Texture *spLabelIdle[SP_OPT] = {};
    SDL_Texture *spLabelActive[SP_OPT] = {};
    SDL_Point spLabelSize[SP_OPT] = {};
    bool spLabelsReady = false;
    void EnsureSPLabels();

    SDL_Rect voidPanelRct = {(640/2) - (341/2), (480/2) - (280/2), 341, 280};
    void SPPanelRender();

    //Options panel render
    bool showingOptPanel = false, awaitKp = false, runDelay = false;
    int delayTime;
    SDL_Keycode lastOptInput = SDLK_UNKNOWN;
    SDL_Texture *voidPanelBG;
    void OptPanelRender();

    // Pick start level panel
    bool showingLevelPanel = false;
    std::string levelInput;
    int pickedStartLevel = 1;
    void LevelPanelRender();

    //Keys panel render
    bool showingKeysPanel = false;
    int keyConfigPlayer = 1; // 1 or 2
    int keyConfigIndex = 0; // row in the panel; see KeyConfigRow in mainmenu_internal.h
    // "Reset all settings" needs a second confirming press. It throws away every
    // key binding the player has set, and it sits one row below a plain toggle,
    // so a single stray Enter must not be able to wipe the lot. Cleared whenever
    // the selection moves or the panel closes.
    bool resetAllArmed = false;
    // Shown instead of immediately flipping the row when the player tries to
    // turn ON highscore-stat uploading: a bare "OFF -> ON" toggle can't say
    // what that starts sending, so a popup does. Turning it back OFF needs no
    // confirmation and skips this entirely. See KeysPanelKey/KeysPanelRender.
    bool showingStatsUploadConfirm = false;
    // On-screen bands for this popup's two buttons, recomputed each frame by
    // KeysPanelRender and hit-tested by HandlePanelTap. A dedicated pair
    // rather than folding these into panelTapRows: that list is shared with
    // (and sits underneath) the settings rows the popup is drawn over, so a
    // tap on either button would otherwise be caught by whichever hidden row
    // happens to occupy the same screen position first.
    SDL_Rect statsConfirmYesRect{}, statsConfirmNoRect{};
    void KeysPanelRender();

    // LAN server discovery
    bool isLANGame = false;
    std::vector<ServerInfo> discoveredServers;
    int selectedServerIndex = 0;
    int lanMenuIndex = 0; // 0 = "Host a server", 1+ = discovered servers

    // Net game public server list (mode 10)
    std::vector<ServerInfo> publicServers;
    int netMenuIndex = 0; // 0 = "Manual entry", 1+ = public servers
    std::atomic<bool> serverFetchInProgress{false}; // true while background fetch is running
    std::thread serverFetchThread;
    std::mutex serverFetchMutex;
    std::vector<ServerInfo> serverFetchResult; // written by bg thread, swapped in on completion

    //Network panel render
    bool showingNetPanel = false;
    bool showingNetSetupPanel = false; // For chain reaction prompt before network lobby
    bool networkInLobby = false;
    bool networkGameStarting = false; // Track if game start has been initiated
    bool netStartRequested = false; // We've sent START and are waiting on GAME_CAN_START (see mainmenu_input.cpp)
    Uint32 netStartRequestedMs = 0; // Timestamp of that request, for the timeout in NetPanelRender()
    Uint32 wasmSyncWaitStart = 0; // WASM joiner: timestamp when we started waiting for sync messages
    Uint32 wasmBotWaitStart = 0;  // WASM leader: timestamp when we started waiting for our bots to enter the game
    bool serverHosting = false;
    int serverPid = -1;
    char networkHost[256] = "127.0.0.1";
    int networkPort = 1511;
    int networkInputMode = 0; // 0 = lobby main, 1 = port input, 2 = host/join choice, 3 = join game, 4 = chat input, 5 = username input, 6 = create confirm, 11 = pre-lobby nickname
    bool networkFieldEditing = false; // True when keyboard is open for host/port field
    int networkManualFieldIndex = 0;  // 0=host, 1=port, 2=connect (for manual-entry form navigation)
    std::string connectErrorMsg;
    char networkJoinCreator[32] = "";
    char networkChatInput[256] = "";
    char networkUsername[32] = "";
    char networkPreNick[32] = "";   // Nickname set before connecting (on server selection screen)
    int networkPreNickReturnMode = 7; // Mode to return to after editing pre-lobby nickname
    bool pendingLobbyConnect = false; // WASM: true while waiting for async WebSocket to open so we can send NICK+GEOLOC
    int selectedGameIndex = -1; // For game list selection
    int selectedActionIndex = 0; // Currently selected action in lobby (0=chat, 1=create, 2+=games)
    Uint32 lastListRequest = 0; // Timestamp of last LIST request
    TTFText networkText;

    // Game settings (when hosting)
    bool chainReactionEnabled = true;
    bool singlePlayerTargetting = true;
    int victoriesLimitIndex = 5; // 0=none, 1=1, 2=2, 3=3, 4=4, 5=5, etc.
    int playerColorCounts[5] = {8, 8, 8, 8, 8};  // Per-player color count (5-8)
    bool playerNoCompress[5] = {false, false, false, false, false};  // Per-player: disable row compression
    bool playerAimGuide[5] = {false, false, false, false, false};  // Per-player: show aim guide
    int currentPlayerCol = 0;  // Focused player column when navigating per-player grid settings
    bool netRoomMouseEnabled = false;  // Per-session mouse/touch for network games (defaults OFF)
    bool netClearMode = false;         // Clear Mode for network game
    AttackMode netAttackMode = AttackMode::On;  // Attack bubbles for network game (ON/OFF/Cancel)
    bool netTeamMode = false;          // Team Mode for network game
    // Bots the host has added to the current game room. They are real room
    // members with their own connections; only this client simulates them.
    int netRoomBotCount = 0;
    int netRoomBotSkill = 1;           // index into BubbleAI::Skill
    // Pushes the host's current room-rule settings (the block above --
    // chainReactionEnabled through netRoomSizeChoice) to the server, so
    // every joiner sees them, and persists them via GameSettings, so the
    // next room this device creates starts from what was last configured
    // here instead of resetting to hardcoded defaults. Wraps every call to
    // NetworkClient::SendOptions().
    void SyncRoomOptions();
    // Persists only, for the two settings above that never go through
    // SendOptions (bot skill and room size are local-only, decided before
    // or independently of the room's broadcast rules). SyncRoomOptions()
    // calls this too, so a room-rule change saves everything at once.
    void SaveHostDefaults();
    // Each bot carries the skill it was added at (NetBotConnection::SetSkill)
    // rather than all sharing netRoomBotSkill live, so a skill change only
    // affects bots added after it -- a room can mix "bot1-high", "bot2-low".
    std::vector<std::unique_ptr<NetBotConnection>> lobbyBots;
    // Bring the number of connected bots in line with netRoomBotCount.
    void SyncLobbyBots();
    // Read the bots' sockets so they answer the server while in the lobby.
    void PumpLobbyBots();
    // PART every bot and forget them (leaving a room, or losing the server).
    void DropLobbyBots();

    int netRoomSizeChoice = 2;         // Index into kRoomSizes for "Create Game Room" (0=5,1=10,2=20); default 20 (royale headline mode)
    int netPlayerTeams[MAX_NET_PLAYERS] = {1,2,3,4,5,1,2,3,4,5,1,2,3,4,5,1,2,3,4,5}; // Per-player team (<=5-cap grid path)
    size_t lastProcessedChatCount = 0; // Host: how many chat msgs we've scanned for !team: commands (<=5 path)
    // >5-cap Team Mode state:
    int netTeamCount = 5;              // fixed team count for >5-cap rooms (matches kTeamColors' 5 entries)
    std::map<std::string,int> netTeamOverrides;   // nick -> chosen team (absent = auto-balance default)
    size_t teamOverrideChatCount = 0;  // all clients: chat msgs scanned for !team: -> override map (>5 path)
    int netRosterCursor = 0;           // host override: selected roster row (0-based)
    bool netRosterEditMode = false;    // host override: true while navigating the roster to reassign teams
    // Snapshot of playerNoCompress/netAttackMode taken the moment Clear Mode is
    // switched on, restored when switching away from it (Clear Mode forces both
    // on; without this, leaving Clear Mode left them stuck on forever).
    bool netPreClearNoCompress[5] = {false, false, false, false, false};
    AttackMode netPreClearAttackMode = AttackMode::On;

    // Geolocation state
    float myGeoLat = 0.0f, myGeoLon = 0.0f;
    bool myGeoLocSet = false;
    int netSpotSelfFrame = 0, netSpotSelfFrameTimer = 0;

    // World map lobby graphics
    SDL_Texture *netGameBackground = nullptr;
    SDL_Texture *netSpotFree = nullptr;
    SDL_Texture *netSpotInGame = nullptr;
    SDL_Texture *netSpotPlaying = nullptr;
    SDL_Texture *netSpotSelf[13]; // Animated self spot
    SDL_Texture *highlightServer = nullptr;

    void NetPanelRender();
    // NetPanelRender decomposition (mainmenu_netpanel.cpp)
    void NetPanelWorldMapRender();       // World map background + geolocation player spots
    void NetPanelLobbyActionsRender();   // Action list + per-player settings grid
    // Persistent chat dock: input row plus the most recent messages. `expanded`
    // grows it over the whole panel while a message is being composed, so the
    // conversation being replied to stays readable instead of being replaced by
    // a bare input box -- which matters most on a phone, where composing raises
    // a keyboard over the bottom half of the screen.
    void NetPanelChatDockRender(bool expanded = false);
    void NetPanelConnectionScreensRender(); // Pre-login screens: LAN list, manual entry, public list
    // LAN discovery (mode 7) and the public Net list (mode 10) are the same
    // screen shape -- a scrolling server list plus a details sidebar -- with
    // only the first row's action and the discovery-in-progress state
    // differing. One function serves both rather than keeping two ~110-line
    // near-duplicates in sync by hand.
    void ServerListPanelRender(bool isLAN);
    void NetSetupPanelRender(); // Chain reaction prompt for network games
    void SavePreNick();         // Persist networkPreNick (localStorage on WASM, INI elsewhere)
    void StartLocalServer();
    void StopLocalServer();

    //game setup defines
    bool chainReaction = false;
    int selectedMode;
    void BeginGameTransition();
    void StartLocalGame(const SetupSettings& settings);

    // Local multiplayer setup panel
    bool showingLocalMPPanel = false;
    int localMPMenuIndex = 0;       // 0=players, 1=CR, 2=row collapse, 3=mode, 4=malus, 5=team mode, 6=victories, 7..7+N-1=aim guide per player, 7+N..7+2N-1=colors per player, 7+2N=start
    int localMPPlayerCount = 2;     // 2-4 players
    int localMPBotCount = 0;        // bots fill the last slots; player 1 stays human
    int localMPBotSkill = 1;        // 0 easy, 1 normal, 2 hard
    bool localMPCR = true;          // Chain reaction enabled
    bool localMPNoCompress = false;  // Disable row compression for all players
    bool localMPClearMode = false;   // Clear Mode (win by clearing board; defaults compression+malus off)
    AttackMode localMPAttackMode = AttackMode::On;  // Attack bubbles (ON/OFF/Cancel)
    bool localMPTeamMode = false;    // Team Mode: odd player slots vs even (see LocalMPTeamOf)
    int localMPVictoriesIndex = 5;   // Index into kVictoriesLimits
    // Snapshot taken when Clear Mode is switched on, restored when switching away
    // from it (see the matching netPreClear* fields above for why).
    bool localMPPreClearNoCompress = false;
    AttackMode localMPPreClearAttackMode = AttackMode::On;
    bool localMPAimGuide[5] = {false, false, false, false, false};  // Per-player aim guide
    void LocalMPPanelRender();

    // Settings guide (mainmenu_help.cpp), opened by either screen's HELP box.
    // One frame, two pages: which one is showing is helpTopic.
    bool showingHelpPanel = false;
    // 0 == HelpTopic::OnlineRoom; the enum lives in mainmenu_internal.h,
    // which includes this header, so it cannot be named here.
    int helpTopic = 0;
    int helpScroll = 0;        // first visible line
    // kHelpRowClose; the enum lives in mainmenu_internal.h, which includes
    // this header, so it cannot be named here.
    int helpMenuIndex = 0;
    void HelpPanelRender();
};

#endif // MAINMENU_H
