#ifndef PLAYERACCOUNT_H
#define PLAYERACCOUNT_H

#include <string>

// This device's player account (protocol 1.6 -- see server/account.h for the
// sign-in exchange). An account is a random 16-character recovery code; the
// signing key is derived from it, so the code alone moves an account to
// another device. The code is created on first use and saved next to
// settings.ini (localStorage on the web build). Nothing here talks to the
// network; NetworkClient asks for the public key and a signature.
namespace playeraccount {

// The saved code, creating and saving one first if there is none. 16 chars
// of Crockford base32 (0-9, A-Z without I L O U): 80 random bits.
std::string Code();

// Hex EdDSA public key for AUTH, and a hex signature over the server's
// challenge for AUTHSIG. Empty if the challenge isn't 64 hex digits.
std::string PublicKeyHex();
std::string SignChallenge(const std::string& nonceHex);

// Uppercases, drops spaces and dashes, and reads O as 0 and I/L as 1, the
// usual Crockford leniency for a code someone types from a screen. Returns
// "" unless the result is exactly 16 valid characters.
std::string NormalizeCode(const std::string& typed);

// "ABCD-EFGH-JKMN-PQRS", for showing to the player.
std::string FormatCode(const std::string& code);

// Replace this device's account with the one `typed` names (after
// NormalizeCode). False, and nothing changed, if it isn't a valid code.
// save=false keeps it for this run only (tests).
bool UseCode(const std::string& typed, bool save = true);

}  // namespace playeraccount

#endif
