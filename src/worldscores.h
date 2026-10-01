#ifndef WORLDSCORES_H
#define WORLDSCORES_H

#include <string>

#include "worldboard.h"

// The world board for classic single-player runs: this device's best runs go
// to one server fixed at build time (kWorldScoresHost, platform.cpp) under the player's
// anonymous account (playeraccount.h), and the board comes back from it. See
// server/hiscores.h for what the server keeps.
//
// The game has no connection to that server during single-player play, so
// this opens its own short one -- AUTH, AUTHSIG, HISCORE / HISCORES, close --
// and never sends NICK, so it never appears in anyone's lobby. Everything runs
// on the main thread from Pump(): native builds poll a non-blocking socket
// (name lookup alone goes to a worker, as in NetworkClient::Connect), the web
// build uses a WebSocket.
//
// Sending is on by default and switched off by the "World highscores"
// setting (GameSettings::worldHighscoresEnabled()). Switched off, nothing is
// sent and the board can still be read, anonymously: no AUTH, so no "you" line.
namespace worldscores {

// True when this build has a world board at all (the host constant is set).
bool Available();
// Available() and the player hasn't switched sending off.
bool SendingEnabled();

// Tracks mirror the local tables: 0 keyboard/gamepad, 1 mouse/touch.
constexpr int kTracks = 2;
// Two rankings per track: furthest level (boards 0/1) and most points in one
// life (boards 2/3) -- the server's own numbering, see server/hiscores.h.
constexpr int kBoards = 2 * kTracks;
inline int BoardIndex(bool points, int track) { return (points ? kTracks : 0) + track; }

// A classic run from level 1 reached `level` (101 = cleared the set) in
// timeMs. Kept on disk until the server has it, so a run survives a crash or
// no network; only the best pending run per board is kept, since the server
// only keeps bests. Sent the next time the game is out of a game (Pump()).
void RecordRun(int track, int level, int timeMs);

// The same run's current life has scored `points` so far, on `level`, at
// timeMs into the run. A life's score only grows until it ends (the game
// zeroes it on death), so calling this at every level cleared and again at
// the death keeps the life's final total -- and a life cut short by quitting
// still counts up to its last cleared level.
void RecordLife(int track, int points, int level, int timeMs);

// Fetch every board (and send anything pending on the way).
void RequestBoards();

enum class Status { Idle, Loading, Ready, Failed };
Status BoardStatus();
const WorldBoard& Board(int board);
// Why the last attempt failed, for the screen; empty otherwise.
const std::string& LastError();

// The name runs are listed under: "nick#tag", nick being the saved network
// nickname cut to what the server accepts ("unnamed" if none). Just the nick
// while sending is off, so looking at the board never creates an account.
std::string ShownName();
std::string SubmitNick();

// The same board as a web page, for an "open in browser" button; empty when
// this build has none.
const char* WebUrl();

// "Delete account" (the account screen): signs in to the board's server as
// this device's account and asks it to drop everything it keeps for it --
// the world-board runs and the weekly line (DELETEACCOUNT, server/game.c).
// Only once the server says OK does this device forget its unsent runs and
// start a new account (playeraccount::StartNewAccount()), so a failure
// changes nothing. Works whether or not sending is switched on.
enum class DeleteStatus { Idle, Working, Done, Failed };
void RequestDeleteAccount();
DeleteStatus DeleteAccountStatus();
// Why the last delete failed; empty otherwise.
const std::string& DeleteAccountError();
// Back to Idle once the screen has shown the result.
void ClearDeleteAccountStatus();

// Call once per frame. inGame suppresses sending pending runs mid-run (they
// go once the player is back in a menu); an explicit RequestBoards() always
// runs.
void Pump(bool inGame);

// Tests: point at a local server and keep pending runs in memory only.
void SetServerForTest(const std::string& host, int port);
int PendingCountForTest();

}  // namespace worldscores

#endif
