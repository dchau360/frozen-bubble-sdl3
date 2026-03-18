/*
 * Frozen-Bubble SDL2 C++ Port
 * Copyright (c) 2000-2012 The Frozen-Bubble Team
 * Copyright (c) 2026 Huy Chau
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

#include "networkclient.h"
#include <cstring>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>

NetworkClient* NetworkClient::ptrInstance = nullptr;

NetworkClient::NetworkClient()
    : sockfd(-1), state(DISCONNECTED), currentGame(nullptr), recvBufferLen(0), myPlayerId(0) {
    SDL_Log("NetworkClient constructor called");
}

NetworkClient::~NetworkClient() {
    Disconnect();
}

NetworkClient* NetworkClient::Instance(const char* host, int port) {
    if (ptrInstance == nullptr) {
        ptrInstance = new NetworkClient();
    }

    // If host and port are provided and we're not connected, connect
    if (host != nullptr && port > 0 && ptrInstance->state == DISCONNECTED) {
        ptrInstance->Connect(host, port);
    }

    return ptrInstance;
}

void NetworkClient::Dispose() {
    if (ptrInstance != nullptr) {
        delete ptrInstance;
        ptrInstance = nullptr;
    }
}

bool NetworkClient::Connect(const char* host, int port) {
    if (state != DISCONNECTED) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Already connected or connecting");
        return false;
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create socket: %s", strerror(errno));
        return false;
    }

    // Increase socket receive buffer to handle bursts (like level sync)
    // Default is often 8KB-64KB, we want more for the rapid message bursts
    int rcvbuf = 256 * 1024;  // 256 KB receive buffer
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to set SO_RCVBUF: %s", strerror(errno));
    } else {
        SDL_Log("Set socket receive buffer to %d bytes", rcvbuf);
    }

    // Keep socket in blocking mode for initial handshake
    // Will set non-blocking after connection established

    // Setup server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &serverAddr.sin_addr) <= 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid address: %s", host);
        close(sockfd);
        sockfd = -1;
        return false;
    }

    // Connect (blocking mode)
    state = CONNECTING;
    int result = connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to connect: %s", strerror(errno));
        close(sockfd);
        sockfd = -1;
        state = DISCONNECTED;
        return false;
    }

    // Wait for and consume all initial server messages
    char buffer[BUFFER_SIZE];
    bool gotServerReady = false;

    // Give server time to send all initial messages
    SDL_Delay(200);

    // Read all available data
    while (true) {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms

        int selectResult = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (selectResult <= 0) {
            break; // No more data
        }

        ssize_t received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (received <= 0) {
            break;
        }

        buffer[received] = '\0';

        // Process each line
        char* line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strstr(line, "SERVER_READY") != NULL) {
                gotServerReady = true;
            }
            line = strtok(NULL, "\n");
        }
    }

    if (!gotServerReady) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Did not receive SERVER_READY");
        close(sockfd);
        sockfd = -1;
        state = DISCONNECTED;
        return false;
    }

    state = CONNECTED;
    SDL_Log("Connected to server %s:%d", host, port);
    return true;
}

void NetworkClient::Disconnect() {
    SDL_Log("!!! DISCONNECT CALLED - State was: %d, sockfd: %d", state, sockfd);
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    state = DISCONNECTED;
    currentGame = nullptr;
    gameList.clear();
    messageQueue.clear();
    SDL_Log("Disconnected from server");
}

bool NetworkClient::SendCommand(const char* command) {
    if (state == DISCONNECTED || sockfd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not connected to server");
        return false;
    }

    char buffer[BUFFER_SIZE];
    snprintf(buffer, sizeof(buffer), "FB/%d.%d %s\n", PROTO_MAJOR, PROTO_MINOR, command);

    ssize_t sent = send(sockfd, buffer, strlen(buffer), MSG_NOSIGNAL);
    if (sent < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send command: %s", strerror(errno));
        Disconnect();
        return false;
    }

    SDL_Log("Sent: %s", command);

    // Wait a moment and read any immediate response to keep socket clean
    fd_set readfds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms

    if (select(sockfd + 1, &readfds, NULL, NULL, &timeout) > 0) {
        char response[BUFFER_SIZE];
        ssize_t received = recv(sockfd, response, sizeof(response) - 1, MSG_DONTWAIT);
        if (received > 0) {
            response[received] = '\0';
            // Process response lines through the normal message parsing
            char* line = strtok(response, "\n");
            while (line != NULL) {
                ParseMessage(line);  // Add to message queue for processing
                line = strtok(NULL, "\n");
            }
        }
    }

    return true;
}

bool NetworkClient::SendNick(const char* nickname) {
    playerNick = nickname;
    myNickname = nickname;  // Store for ID mapping
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "NICK %s", nickname);
    return SendCommand(cmd);
}

bool NetworkClient::SendGeoLoc(const char* location) {
    playerGeoloc = location;
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "GEOLOC %s", location);
    return SendCommand(cmd);
}

bool NetworkClient::CreateGame() {
    // CREATE requires a game name argument (uses player's nickname)
    // Implement retry with suffix if NICK_IN_USE (original lines 4768-4785)

    std::string originalNick = playerNick;
    std::string tryNick = playerNick;
    int suffix = 2; // Start with suffix 2 for first retry
    int maxRetries = 20;

    for (int retry = 0; retry < maxRetries; retry++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "CREATE %s", tryNick.c_str());
        SDL_Log("Attempting to create game with nick: %s (attempt %d)", tryNick.c_str(), retry + 1);

        lastErrorResponse.clear(); // Clear previous error

        if (!SendCommand(cmd)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send CREATE command");
            return false;
        }

        // Wait a bit for server response (SendCommand already waits 100ms)
        SDL_Delay(50);

        // Check if we got NICK_IN_USE error
        if (lastErrorResponse == "NICK_IN_USE") {
            SDL_Log("Nickname '%s' is in use, trying with suffix %d", tryNick.c_str(), suffix);
            // Generate new nickname with suffix
            char suffixStr[16];
            snprintf(suffixStr, sizeof(suffixStr), "%d", suffix);
            tryNick = originalNick.substr(0, std::min((size_t)9, originalNick.length())) + suffixStr;
            suffix++;
            continue; // Retry with new nickname
        }

        // No error or different error - assume success
        SDL_Log("CREATE command successful with nickname '%s', setting state to IN_LOBBY", tryNick.c_str());
        state = IN_LOBBY;

        // Update playerNick and myNickname to the one that worked (for ID mapping)
        playerNick = tryNick;
        myNickname = tryNick;

        // Set up currentGame structure
        if (!currentGame) {
            currentGame = new GameRoom();
        }
        currentGame->creator = playerNick;
        currentGame->started = false;

        // Add self as the first player
        NetworkPlayer self;
        self.nick = playerNick;
        self.ready = false;
        currentGame->players.clear();
        currentGame->players.push_back(self);

        SDL_Log("Created game, currentGame has %d players", (int)currentGame->players.size());

        return true;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create game after %d retries", maxRetries);
    return false;
}

bool NetworkClient::JoinGame(const char* creator) {
    // JOIN requires creator_nick and player_nick
    // Implement retry with suffix if NICK_IN_USE (original lines 4768-4785)

    std::string originalNick = playerNick;
    std::string tryNick = playerNick;
    int suffix = 2;
    int maxRetries = 20;

    for (int retry = 0; retry < maxRetries; retry++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "JOIN %s %s", creator, tryNick.c_str());
        SDL_Log("Attempting to join game created by %s with nick: %s (attempt %d)", creator, tryNick.c_str(), retry + 1);

        lastErrorResponse.clear();

        if (!SendCommand(cmd)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send JOIN command");
            return false;
        }

        // Wait for server response
        SDL_Delay(50);

        // Check for NICK_IN_USE error
        if (lastErrorResponse == "NICK_IN_USE") {
            SDL_Log("Nickname '%s' is in use, trying with suffix %d", tryNick.c_str(), suffix);
            char suffixStr[16];
            snprintf(suffixStr, sizeof(suffixStr), "%d", suffix);
            tryNick = originalNick.substr(0, std::min((size_t)9, originalNick.length())) + suffixStr;
            suffix++;
            continue;
        }

        // Success - set up currentGame
        SDL_Log("JOIN command successful with nickname '%s', setting state to IN_LOBBY", tryNick.c_str());
        state = IN_LOBBY;

        // Update playerNick and myNickname to the one that worked (for ID mapping)
        playerNick = tryNick;
        myNickname = tryNick;

        // Set up currentGame - find it from the game list
        for (const auto& game : gameList) {
            if (game.creator == creator) {
                if (!currentGame) {
                    currentGame = new GameRoom();
                }
                *currentGame = game;
                SDL_Log("Set currentGame to %s's game with %d players from gameList", creator, (int)currentGame->players.size());
                break;
            }
        }

        // IMPORTANT: Add ourselves to the player list!
        // The server sends JOINED messages only to OTHER players, not to the joiner themselves
        // (see server/game.c add_player function - it sends to i < players_number before incrementing)
        if (currentGame) {
            NetworkPlayer self;
            self.nick = playerNick;
            self.ready = false;
            currentGame->players.push_back(self);
            SDL_Log("Added self (%s) to currentGame, now has %d players", playerNick.c_str(), (int)currentGame->players.size());
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to find game in gameList!");
        }

        return true;
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to join game after %d retries", maxRetries);
    return false;
}

bool NetworkClient::StartGame() {
    // Send START command to server
    // Server will respond with GAME_CAN_START push message when ready
    // Don't change state here - wait for GAME_CAN_START
    if (SendCommand("START")) {
        SDL_Log("Sent START command to server, waiting for GAME_CAN_START...");
        return true;
    }
    return false;
}

bool NetworkClient::PartGame() {
    if (SendCommand("PART")) {
        state = CONNECTED;
        currentGame = nullptr;
        return true;
    }
    return false;
}

bool NetworkClient::SendTalk(const char* message) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "TALK %s", message);
    return SendCommand(cmd);
}

bool NetworkClient::SendOptions(bool chainReaction, bool continueWhenLeave, bool singleTarget, int victoriesLimit) {
    // Send game options using SETOPTIONS command (original line 4468-4474)
    // Format: SETOPTIONS CHAINREACTION:0/1,CONTINUEGAMEWHENPLAYERSLEAVE:0/1,SINGLEPLAYERTARGETTING:0/1,VICTORIESLIMIT:num
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "SETOPTIONS CHAINREACTION:%d,CONTINUEGAMEWHENPLAYERSLEAVE:%d,SINGLEPLAYERTARGETTING:%d,VICTORIESLIMIT:%d",
             chainReaction ? 1 : 0,
             continueWhenLeave ? 1 : 0,
             singleTarget ? 1 : 0,
             victoriesLimit);
    SDL_Log("Sending game options: %s", cmd);
    return SendCommand(cmd);
}

bool NetworkClient::SendGameData(const char* data) {
    // Game messages use binary protocol: {myid byte}{data}\n
    // NOT the FB/1.2 prefix format!
    if (state != IN_GAME || sockfd < 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Not in game or not connected");
        return false;
    }

    if (myPlayerId == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Player ID not set - cannot send game data");
        return false;
    }

    char buffer[BUFFER_SIZE];
    buffer[0] = (char)myPlayerId;  // First byte is player ID (binary)

    // Format the message part (text + newline)
    int msgLen = snprintf(buffer + 1, sizeof(buffer) - 1, "%s\n", data);
    if (msgLen < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to format game data");
        return false;
    }

    // Total length = 1 byte (player ID) + message length
    size_t len = 1 + msgLen;

    // Don't log ping messages to avoid spam (sent every second)
    bool isPing = (strcmp(data, "p") == 0);

    // Log the exact bytes being sent for debugging
    if (!isPing) {
        SDL_Log(">>> Sending game data: [ID=%d] %s (total %zu bytes: 1 byte ID + %d bytes msg)",
                (int)myPlayerId, data, len, msgLen);
    }

    ssize_t sent = send(sockfd, buffer, len, MSG_NOSIGNAL);
    if (sent < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to send game data: %s", strerror(errno));
        Disconnect();
        return false;
    }

    if (sent != (ssize_t)len) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Partial send: %zd of %zu bytes", sent, len);
    }

    if (!isPing) {
        SDL_Log(">>> Successfully sent game data: [ID=%d] %s", (int)myPlayerId, data);
    }
    return true;
}

bool NetworkClient::RequestList() {
    return SendCommand("LIST");
}

bool NetworkClient::IsLeader() {
    // We're the leader if we created the game (we are the creator)
    if (!currentGame) {
        SDL_Log("IsLeader: No current game");
        return false;
    }
    bool isLeader = (currentGame->creator == playerNick);
    SDL_Log("IsLeader: creator='%s', playerNick='%s', result=%s",
            currentGame->creator.c_str(), playerNick.c_str(), isLeader ? "true" : "false");
    return isLeader;
}

bool NetworkClient::SendBubble(int cx, int cy, int bubbleId) {
    // Leader sends bubble position: b|cx|cy{bubbleId}
    char msg[64];
    snprintf(msg, sizeof(msg), "b|%d|%d%d", cx, cy, bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::SendNextBubble(int bubbleId) {
    // Leader sends next bubble: N{bubbleId}
    char msg[32];
    snprintf(msg, sizeof(msg), "N%d", bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::SendTobeBubble(int bubbleId) {
    // Leader sends tobe bubble: T{bubbleId}
    char msg[32];
    snprintf(msg, sizeof(msg), "T%d", bubbleId);
    return SendGameData(msg);
}

bool NetworkClient::WaitForBubble(int& cx, int& cy, int& bubbleId) {
    // Joiner waits for bubble message: b|cx|cy{bubbleId}
    int timeout = 5000;  // 5 second timeout
    Uint32 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages; // Collect non-matching messages

    while (SDL_GetTicks() - startTime < timeout) {
        Update();  // Process incoming data

        if (HasMessage()) {
            std::string msg = GetNextMessage();
            SDL_Log("WaitForBubble: Got message: %s", msg.c_str());

            // Only process GAMEMSG format (ignore server protocol messages)
            if (msg.find("GAMEMSG:") == 0) {
                int senderId;
                char gameData[512];
                if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) == 2) {
                    SDL_Log("WaitForBubble: Parsed gameData: %s", gameData);
                    // Check if it's a bubble message
                    if (gameData[0] == 'b' && gameData[1] == '|') {
                        // Parse b|cx|cy{bubbleId}  format: "b|0|07" means cx=0, cy=0, id=7
                        char* data = gameData + 2;  // Skip "b|"
                        SDL_Log("WaitForBubble: Parsing data: %s", data);

                        // Parse cx and the cy+bubbleId string
                        char cyBubble[16];
                        if (sscanf(data, "%d|%15s", &cx, cyBubble) == 2) {
                            // cyBubble is like "07" where first digit is cy, rest is bubbleId
                            if (strlen(cyBubble) >= 2) {
                                cy = cyBubble[0] - '0';  // First digit is cy
                                bubbleId = atoi(cyBubble + 1);  // Rest is bubbleId
                                SDL_Log("Received bubble: cx=%d cy=%d id=%d", cx, cy, bubbleId);
                                // Put back deferred messages before returning
                                for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
                                    PutBackMessage(*it);
                                }
                                return true;
                            } else {
                                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: cyBubble string too short: %s", cyBubble);
                            }
                        } else {
                            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Failed to parse bubble data: %s", data);
                        }
                    } else {
                        SDL_Log("WaitForBubble: Not a bubble message, deferring");
                        deferredMessages.push_back(msg);  // Defer non-bubble GAMEMSG for later (N, T, etc)
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForBubble: Failed to parse GAMEMSG");
                }
            }
            // Ignore non-GAMEMSG messages (server protocol) - don't put them back
        } else {
            SDL_Delay(10);  // Small delay to avoid busy waiting only when no messages
        }
    }

    // Timeout - put back deferred messages
    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
        PutBackMessage(*it);
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for bubble message");
    return false;
}

bool NetworkClient::WaitForNextBubble(int& bubbleId) {
    // Joiner waits for next bubble: N{bubbleId}
    int timeout = 5000;
    Uint32 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages;

    while (SDL_GetTicks() - startTime < timeout) {
        Update();

        if (HasMessage()) {
            std::string msg = GetNextMessage();

            // Only process GAMEMSG format (ignore server protocol messages)
            if (msg.find("GAMEMSG:") == 0) {
                int senderId;
                char gameData[512];
                if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) == 2) {
                    if (gameData[0] == 'N') {
                        if (sscanf(gameData + 1, "%d", &bubbleId) == 1) {
                            SDL_Log("Received next bubble: id=%d", bubbleId);
                            // Put back deferred messages
                            for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
                                PutBackMessage(*it);
                            }
                            return true;
                        }
                    } else {
                        // Not an N message, defer it
                        deferredMessages.push_back(msg);
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForNextBubble: Failed to parse GAMEMSG");
                }
            }
            // Ignore non-GAMEMSG messages (server protocol) - don't put them back
        } else {
            SDL_Delay(10);
        }
    }

    // Timeout - put back deferred messages
    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
        PutBackMessage(*it);
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for next bubble");
    return false;
}

bool NetworkClient::WaitForTobeBubble(int& bubbleId) {
    // Joiner waits for tobe bubble: T{bubbleId}
    int timeout = 5000;
    Uint32 startTime = SDL_GetTicks();
    std::vector<std::string> deferredMessages;

    while (SDL_GetTicks() - startTime < timeout) {
        Update();

        if (HasMessage()) {
            std::string msg = GetNextMessage();

            // Only process GAMEMSG format (ignore server protocol messages)
            if (msg.find("GAMEMSG:") == 0) {
                int senderId;
                char gameData[512];
                if (sscanf(msg.c_str(), "GAMEMSG:%d:%511[^\n]", &senderId, gameData) == 2) {
                    if (gameData[0] == 'T') {
                        if (sscanf(gameData + 1, "%d", &bubbleId) == 1) {
                            SDL_Log("Received tobe bubble: id=%d", bubbleId);
                            // Put back deferred messages
                            for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
                                PutBackMessage(*it);
                            }
                            return true;
                        }
                    } else {
                        // Not a T message, defer it
                        deferredMessages.push_back(msg);
                    }
                } else {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "WaitForTobeBubble: Failed to parse GAMEMSG");
                }
            }
            // Ignore non-GAMEMSG messages (server protocol) - don't put them back
        } else {
            SDL_Delay(10);
        }
    }

    // Timeout - put back deferred messages
    for (auto it = deferredMessages.rbegin(); it != deferredMessages.rend(); ++it) {
        PutBackMessage(*it);
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Timeout waiting for tobe bubble");
    return false;
}

void NetworkClient::AddStatusMessage(const std::string& message) {
    ChatMessage statusMsg;
    statusMsg.nick = "Server";
    statusMsg.message = message;
    statusMsg.timestamp = SDL_GetTicks();
    chatMessages.push_back(statusMsg);

    // Keep only last 50 messages
    if (chatMessages.size() > 50) {
        chatMessages.erase(chatMessages.begin());
    }
}

void NetworkClient::Update() {
    if (state == DISCONNECTED || sockfd < 0) return;

    // Read all available data to prevent socket buffer from filling up
    // The server will disconnect us if it can't send (buffer full)
    // Keep reading until EWOULDBLOCK (no more data available)
    int readsThisFrame = 0;
    while (readsThisFrame < 100) {  // Safety limit
        if (!ProcessIncomingData()) {
            break;  // No more data available
        }
        readsThisFrame++;
    }
    if (readsThisFrame > 10) {
        SDL_Log("Warning: Read %d packets in one frame - network buffer was filling up", readsThisFrame);
    }
}

bool NetworkClient::ProcessIncomingData() {
    char tempBuffer[BUFFER_SIZE];
    ssize_t received = recv(sockfd, tempBuffer, sizeof(tempBuffer) - 1, MSG_DONTWAIT);

    if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Receive error: %s", strerror(errno));
            Disconnect();
        }
        return false;  // No data available
    }

    if (received == 0) {
        SDL_Log("Server closed connection");
        Disconnect();
        return false;
    }

    // Append to buffer (don't null-terminate yet for binary data)
    if (recvBufferLen + received < BUFFER_SIZE) {
        memcpy(recvBuffer + recvBufferLen, tempBuffer, received);
        recvBufferLen += received;

        if (state == IN_GAME) {
            // In-game: binary protocol {id byte}{msg}\n
            int processed = 0;
            while (processed < recvBufferLen) {
                // Look for newline
                int msgEnd = -1;
                for (int i = processed; i < recvBufferLen; i++) {
                    if (recvBuffer[i] == '\n') {
                        msgEnd = i;
                        break;
                    }
                }

                if (msgEnd == -1) {
                    // No complete message yet
                    break;
                }

                // Extract message: first byte is sender ID, rest is message
                if (msgEnd > processed) {
                    unsigned char senderId = (unsigned char)recvBuffer[processed];
                    int msgStart = processed + 1;
                    int msgLen = msgEnd - msgStart;

                    if (msgLen > 0) {
                        char gameMsg[BUFFER_SIZE];
                        memcpy(gameMsg, recvBuffer + msgStart, msgLen);
                        gameMsg[msgLen] = '\0';

                        SDL_Log("Game message from player %d: %s", (int)senderId, gameMsg);

                        // Add to message queue for game to process
                        char fullMsg[BUFFER_SIZE];
                        snprintf(fullMsg, sizeof(fullMsg), "GAMEMSG:%d:%s", (int)senderId, gameMsg);
                        messageQueue.push_back(std::string(fullMsg));
                    }
                }

                processed = msgEnd + 1;
            }

            // Move remaining data to start of buffer
            if (processed > 0) {
                int remaining = recvBufferLen - processed;
                if (remaining > 0) {
                    memmove(recvBuffer, recvBuffer + processed, remaining);
                }
                recvBufferLen = remaining;
            }
        } else {
            // Lobby: text protocol FB/1.2 format
            recvBuffer[recvBufferLen] = '\0';

            // Process complete lines
            char* lineStart = recvBuffer;
            char* lineEnd;

            while ((lineEnd = strchr(lineStart, '\n')) != nullptr) {
                *lineEnd = '\0';
                ParseMessage(lineStart);
                lineStart = lineEnd + 1;
            }

            // Move remaining data to start of buffer
            int remaining = recvBuffer + recvBufferLen - lineStart;
            if (remaining > 0) {
                memmove(recvBuffer, lineStart, remaining);
                recvBufferLen = remaining;
            } else {
                recvBufferLen = 0;
            }
        }
    }
    return true;  // Successfully read and processed data
}

void NetworkClient::ParseMessage(const char* message) {
    if (strlen(message) == 0) return;

    SDL_Log("Received: %s", message);
    // Don't add server protocol messages to queue - they're handled immediately
    // Only GAMEMSG messages from ProcessIncomingData() should be queued
    HandleServerResponse(std::string(message));
}

void NetworkClient::HandleServerResponse(const std::string& response) {
    // Check if it's a PUSH message
    if (response.find("PUSH:") != std::string::npos) {
        size_t pushPos = response.find("PUSH:") + 6; // Skip "PUSH: "
        std::string pushMsg = response.substr(pushPos);
        HandlePushMessage(pushMsg);
        return;
    }

    // Check if it's a LIST response
    if (response.find("LIST:") != std::string::npos) {
        size_t listPos = response.find("LIST:") + 6; // Skip "LIST: "
        const char* listData = response.c_str() + listPos;
        ParseListResponse(listData);
        return;
    }

    // Handle other responses
    if (response.find("OK") != std::string::npos) {
        SDL_Log("Command successful");
        lastErrorResponse.clear(); // Clear error on success
    } else if (response.find("PONG") != std::string::npos) {
        SDL_Log("Ping response");
    } else if (response.find("NICK_IN_USE") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NICK_IN_USE error received");
        lastErrorResponse = "NICK_IN_USE";
    } else if (response.find("NO_SUCH_GAME") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "NO_SUCH_GAME error received");
        lastErrorResponse = "NO_SUCH_GAME";
    } else if (response.find("ALREADY_IN_GAME") != std::string::npos) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "ALREADY_IN_GAME error received");
        lastErrorResponse = "ALREADY_IN_GAME";
    }
}

void NetworkClient::HandlePushMessage(const std::string& pushMsg) {
    SDL_Log("PUSH message: %s", pushMsg.c_str());

    if (pushMsg.find("SERVER_READY") == 0) {
        // Server ready, extract server name
        SDL_Log("Server ready");
    } else if (pushMsg.find("JOINED:") == 0) {
        // Player joined
        std::string nick = pushMsg.substr(8); // Skip "JOINED: "
        SDL_Log("Player %s joined the game", nick.c_str());

        // Add to current game if we're in one
        if (currentGame) {
            NetworkPlayer player;
            player.nick = nick;
            player.ready = false;
            currentGame->players.push_back(player);
            SDL_Log("Added %s to game, now has %d players", nick.c_str(), (int)currentGame->players.size());
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "JOINED message received but currentGame is NULL!");
        }
    } else if (pushMsg.find("PARTED:") == 0) {
        // Player left
        std::string nick = pushMsg.substr(8); // Skip "PARTED: "
        SDL_Log("Player %s left the game", nick.c_str());

        // Remove from current game
        if (currentGame) {
            auto it = std::remove_if(currentGame->players.begin(), currentGame->players.end(),
                [&nick](const NetworkPlayer& p) { return p.nick == nick; });
            currentGame->players.erase(it, currentGame->players.end());
        }
    } else if (pushMsg.find("TALK:") == 0) {
        // Chat message
        std::string message = pushMsg.substr(6); // Skip "TALK: "
        ChatMessage chat;

        // Parse format: "nick: message" or just "message"
        size_t colonPos = message.find(':');
        if (colonPos != std::string::npos && colonPos < 20) {
            chat.nick = message.substr(0, colonPos);
            chat.message = message.substr(colonPos + 2); // Skip ": "
        } else {
            chat.nick = "Server";
            chat.message = message;
        }
        chat.timestamp = SDL_GetTicks();

        chatMessages.push_back(chat);

        // Keep only last 50 messages
        if (chatMessages.size() > 50) {
            chatMessages.erase(chatMessages.begin());
        }

        SDL_Log("Chat: [%s] %s", chat.nick.c_str(), chat.message.c_str());
    } else if (pushMsg.find("KICKED:") == 0) {
        std::string nick = pushMsg.substr(8);
        SDL_Log("Kicked: %s", nick.c_str());
    } else if (pushMsg.find("GAME_CAN_START:") == 0) {
        // Game is ready to start - server sent player mappings
        // Format: {id byte}{nick},{id byte}{nick},...
        std::string mappings = pushMsg.substr(16); // Skip "GAME_CAN_START: "
        SDL_Log("Game can start! Parsing player mappings...");

        // Parse binary format to find our player ID
        const char* data = mappings.c_str();
        size_t len = mappings.length();
        size_t i = 0;

        // Clear old mappings
        playerIdToNick.clear();

        while (i < len) {
            // First byte is player ID
            unsigned char playerId = (unsigned char)data[i];
            i++;

            // Read nickname until comma or end
            std::string nick;
            while (i < len && data[i] != ',') {
                nick += data[i];
                i++;
            }
            i++; // Skip comma

            SDL_Log("Player mapping: ID=%d nick=%s", (int)playerId, nick.c_str());

            // Store mapping for later use
            playerIdToNick[(int)playerId] = nick;

            // Check if this is us
            SDL_Log("Comparing nick='%s' with myNickname='%s'", nick.c_str(), myNickname.c_str());
            if (nick == myNickname) {
                myPlayerId = playerId;
                SDL_Log("Found our player ID: %d (matched nickname '%s')", (int)myPlayerId, nick.c_str());
            }

            // Update currentGame->players with the authoritative player list from server
            // This ensures all players see correct nicknames in the game room
            if (currentGame) {
                // Check if player already exists in the list
                bool found = false;
                for (auto& p : currentGame->players) {
                    if (p.nick == nick) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // Add new player
                    NetworkPlayer newPlayer;
                    newPlayer.nick = nick;
                    newPlayer.ready = false;
                    currentGame->players.push_back(newPlayer);
                    SDL_Log("Added player %s to currentGame from GAME_CAN_START (now %d players)",
                           nick.c_str(), (int)currentGame->players.size());
                }
            }
        }

        // Original Perl: leader must poll LEADER_CHECK_GAME_START until all others send OK_GAME_START,
        // then leader sends OK_GAME_START last. Non-leaders just send OK_GAME_START immediately.
        // This ensures all players are in prio mode before the leader starts sending sync messages.
        if (IsLeader()) {
            SDL_Log("Leader: polling LEADER_CHECK_GAME_START until all joiners are ready...");
            int attempts = 0;
            while (attempts < 50) { // up to 5 seconds
                char buf[BUFFER_SIZE];
                snprintf(buf, sizeof(buf), "FB/%d.%d LEADER_CHECK_GAME_START\n", PROTO_MAJOR, PROTO_MINOR);
                send(sockfd, buf, strlen(buf), MSG_NOSIGNAL);

                // Wait for server response
                fd_set readfds; struct timeval tv;
                FD_ZERO(&readfds); FD_SET(sockfd, &readfds);
                tv.tv_sec = 0; tv.tv_usec = 200000;
                if (select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0) {
                    char resp[BUFFER_SIZE];
                    ssize_t n = recv(sockfd, resp, sizeof(resp) - 1, MSG_DONTWAIT);
                    if (n > 0) {
                        resp[n] = '\0';
                        if (strstr(resp, "OTHERS_NOT_READY") != NULL) {
                            SDL_Log("Leader: others not ready yet, waiting...");
                            SDL_Delay(100);
                            attempts++;
                            continue;
                        }
                        if (strstr(resp, "OK") != NULL) {
                            SDL_Log("Leader: all players ready!");
                            break;
                        }
                    }
                }
                attempts++;
            }
        }

        SDL_Log("Sending OK_GAME_START acknowledgement (state is still %d)...", state);
        bool sent = SendCommand("OK_GAME_START");
        SDL_Log("OK_GAME_START sent result: %s", sent ? "SUCCESS" : "FAILED");

        // NOW transition to IN_GAME state after OK response has been handled
        SDL_Log("Setting state to IN_GAME (myPlayerId=%d)", (int)myPlayerId);
        state = IN_GAME;
    }
}

void NetworkClient::ParseListResponse(const char* listData) {
    // Clear current lists
    gameList.clear();
    openPlayers.clear();

    SDL_Log("=== Parsing LIST response ===");
    SDL_Log("Raw data: %s", listData);

    // Format: <open-players>,<space>[<game1>][<game2>]...<space>free:<count> games:<count> playing:<count> at:<geolocs>

    std::string data(listData);
    size_t pos = 0;

    // Parse open players (not in any game)
    size_t spacePos = data.find(' ');
    if (spacePos != std::string::npos) {
        std::string openPlayersStr = data.substr(0, spacePos);

        // Split by comma
        size_t start = 0;
        size_t commaPos;
        while ((commaPos = openPlayersStr.find(',', start)) != std::string::npos) {
            std::string playerStr = openPlayersStr.substr(start, commaPos - start);
            if (!playerStr.empty()) {
                NetworkPlayer player;

                // Check for geoloc (format: NICK:GEOLOC)
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;
                openPlayers.push_back(player);
                SDL_Log("Open player: %s%s%s", player.nick.c_str(),
                       player.geoloc.empty() ? "" : " (",
                       player.geoloc.empty() ? "" : (player.geoloc + ")").c_str());
            }
            start = commaPos + 1;
        }

        data = data.substr(spacePos + 1);
    }

    // Parse games (enclosed in brackets)
    while ((pos = data.find('[')) != std::string::npos) {
        size_t endPos = data.find(']', pos);
        if (endPos == std::string::npos) break;

        std::string gameStr = data.substr(pos + 1, endPos - pos - 1);
        GameRoom game;
        game.started = false;

        // Parse players in game (comma-separated)
        size_t start = 0;
        size_t commaPos;
        bool first = true;
        while ((commaPos = gameStr.find(',', start)) != std::string::npos) {
            std::string playerStr = gameStr.substr(start, commaPos - start);
            if (!playerStr.empty()) {
                NetworkPlayer player;

                // Check for geoloc
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;

                // First player is the creator
                if (first) {
                    game.creator = player.nick;
                    first = false;
                }

                game.players.push_back(player);
            }
            start = commaPos + 1;
        }

        // Don't forget the last player
        if (start < gameStr.length()) {
            std::string playerStr = gameStr.substr(start);
            if (!playerStr.empty()) {
                NetworkPlayer player;
                size_t colonPos = playerStr.find(':');
                if (colonPos != std::string::npos) {
                    player.nick = playerStr.substr(0, colonPos);
                    player.geoloc = playerStr.substr(colonPos + 1);
                } else {
                    player.nick = playerStr;
                }
                player.ready = false;

                if (first) {
                    game.creator = player.nick;
                }

                game.players.push_back(player);
            }
        }

        if (!game.players.empty()) {
            gameList.push_back(game);
            SDL_Log("Game: %s (%d players)", game.creator.c_str(), (int)game.players.size());
        }

        data = data.substr(endPos + 1);
    }

    SDL_Log("Found %d games and %d open players", (int)gameList.size(), (int)openPlayers.size());

    // Update currentGame from gameList - this is needed for BOTH host and joiners:
    // - Host: doesn't receive JOINED messages (server only sends to other players)
    // - Joiners: only receive JOINED for players who join AFTER them, not players already in game
    // So LIST is the authoritative source for the complete player list
    if (currentGame && !gameList.empty()) {
        for (const auto& game : gameList) {
            if (game.creator == currentGame->creator) {
                // Found our game - sync player list from LIST
                SDL_Log("LIST update: Our game '%s' has %d players", game.creator.c_str(), (int)game.players.size());

                // First, mark all existing players as "not seen"
                std::vector<std::string> seenNicks;
                for (const auto& p : game.players) {
                    seenNicks.push_back(p.nick);
                }

                // Add any players from LIST that aren't in currentGame
                for (const auto& p : game.players) {
                    bool found = false;
                    for (const auto& existing : currentGame->players) {
                        if (existing.nick == p.nick) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        currentGame->players.push_back(p);
                        SDL_Log("LIST sync: Added player %s to currentGame (now %d players)",
                               p.nick.c_str(), (int)currentGame->players.size());
                    }
                }

                // Remove players that are no longer in the LIST (they left)
                currentGame->players.erase(
                    std::remove_if(currentGame->players.begin(), currentGame->players.end(),
                        [&seenNicks](const NetworkPlayer& p) {
                            return std::find(seenNicks.begin(), seenNicks.end(), p.nick) == seenNicks.end();
                        }),
                    currentGame->players.end()
                );

                break;
            }
        }
    }
}

bool NetworkClient::HasMessage() {
    return !messageQueue.empty();
}

std::string NetworkClient::GetNextMessage() {
    if (messageQueue.empty()) return "";
    std::string msg = messageQueue.front();
    messageQueue.pop_front();
    return msg;
}

void NetworkClient::PutBackMessage(const std::string& msg) {
    messageQueue.push_front(msg);
}

std::vector<ServerInfo> NetworkClient::DiscoverLANServers() {
    std::vector<ServerInfo> servers;

#ifdef __WASM_PORT__
    // WebAssembly cannot use UDP sockets for LAN discovery
    SDL_Log("LAN discovery not available in WebAssembly port");
    return servers;
#endif

    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) return servers;

    int broadcast = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1511);
    dest.sin_addr.s_addr = INADDR_BROADCAST;

    char probe[64];
    snprintf(probe, sizeof(probe), "FB/%d.%d SERVER PROBE", PROTO_MAJOR, PROTO_MINOR);
    sendto(udpSock, probe, strlen(probe), 0, (struct sockaddr*)&dest, sizeof(dest));

    Uint32 startTime = SDL_GetTicks();
    while (SDL_GetTicks() - startTime < 1000) {
        fd_set readfds;
        struct timeval tv = {0, 50000};  // 50ms
        FD_ZERO(&readfds);
        FD_SET(udpSock, &readfds);
        if (select(udpSock + 1, &readfds, nullptr, nullptr, &tv) > 0) {
            char buf[256];
            struct sockaddr_in from;
            socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(udpSock, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &fromLen);
            if (n > 0) {
                buf[n] = '\0';
                int port = 0;
                if (sscanf(buf, "FB/%*d.%*d SERVER HERE AT PORT %d", &port) == 1) {
                    ServerInfo si;
                    si.host = inet_ntoa(from.sin_addr);
                    si.port = port;
                    bool dup = false;
                    for (const auto& s : servers) {
                        if (s.host == si.host && s.port == si.port) { dup = true; break; }
                    }
                    if (!dup) servers.push_back(si);
                }
            }
        }
    }

    close(udpSock);

    // UDP broadcast doesn't reach loopback — also probe 127.0.0.1 directly
    // so a locally-hosted server appears in the LAN list
    {
        int port = 1511;
        bool alreadyFound = false;
        for (const auto& s : servers) {
            if (s.host == "127.0.0.1" && s.port == port) { alreadyFound = true; break; }
        }
        if (!alreadyFound && MeasureLatency("127.0.0.1", port, 300) >= 0) {
            ServerInfo si;
            si.host = "127.0.0.1";
            si.port = port;
            servers.insert(servers.begin(), si);  // Put localhost first
        }
    }

    return servers;
}

int NetworkClient::MeasureLatency(const char* host, int port, int timeoutMs) {
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);
    if (getaddrinfo(host, portStr, &hints, &res) != 0 || !res) return -1;

    int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s < 0) { freeaddrinfo(res); return -1; }

    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    connect(s, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
    struct timeval tv{ timeoutMs / 1000, (timeoutMs % 1000) * 1000 };
    bool ok = (select(s + 1, nullptr, &wfds, nullptr, &tv) > 0);
    close(s);

    if (!ok) return -1;

    clock_gettime(CLOCK_MONOTONIC, &t1);
    long ms = (t1.tv_sec - t0.tv_sec) * 1000 + (t1.tv_nsec - t0.tv_nsec) / 1000000;
    return (int)ms;
}

bool NetworkClient::IsReachable(const char* host, int port, int timeoutMs) {
    return MeasureLatency(host, port, timeoutMs) >= 0;
}

// Public server list hosted in this repo — same format as original frozen-bubble.org: "host port" per line
#define GITHUB_SERVER_LIST_URL \
    "https://raw.githubusercontent.com/dchau360/frozen-bubble-sdl2-vibecode/main/servers.txt"

// Original Frozen Bubble master server list URL (format: "host port" per line)
#define FB_MASTER_SERVER_URL \
    "http://www.frozen-bubble.org/servers/serverlist-" PROTO_MAJOR_STR

#define XSTR(s) STR(s)
#define STR(s) #s
#define PROTO_MAJOR_STR XSTR(PROTO_MAJOR)

static void curlFetch(const char* url, std::vector<ServerInfo>& out, bool originalFormat) {
#ifdef __ANDROID__
    SDL_Log("curlFetch: not supported on Android (TODO: use Java HTTP)");
    (void)url; (void)out; (void)originalFormat;
    return;
#endif
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s --connect-timeout 5 --max-time 8 '%s' 2>/dev/null", url);
    FILE* fp = popen(cmd, "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;

        char host[256] = "";
        int port = 1511;
        char name[256] = "";

        if (originalFormat) {
            // Original format: "host port"
            if (sscanf(line, "%255s %d", host, &port) < 2) continue;
        } else {
            // GitHub format: "host:port Name..."
            char hostport[256] = "";
            sscanf(line, "%255s %255[^\n]", hostport, name);
            char* colon = strrchr(hostport, ':');
            if (colon) {
                *colon = '\0';
                strncpy(host, hostport, sizeof(host) - 1);
                port = atoi(colon + 1);
            } else {
                strncpy(host, hostport, sizeof(host) - 1);
            }
        }
        if (host[0] == '\0') continue;

        // Deduplicate
        bool dup = false;
        for (const auto& s : out)
            if (s.host == host && s.port == port) { dup = true; break; }
        if (dup) continue;

        ServerInfo si;
        si.host = host;
        si.port = port;
        si.name = (name[0] != '\0') ? name : (std::string(host) + ":" + std::to_string(port));
        out.push_back(si);
    }
    pclose(fp);
}

std::vector<ServerInfo> NetworkClient::FetchPublicServers() {
    std::vector<ServerInfo> servers;

#ifdef __WASM_PORT__
    // WebAssembly uses emscripten_fetch for HTTP requests
    // TODO: Implement async fetch using emscripten_websocket or emscripten_fetch
    SDL_Log("Public server fetch not yet implemented in WebAssembly port (requires emscripten_fetch)");
    return servers;
#endif

    SDL_Log("Fetching server lists...");

    // 1. Original Frozen Bubble master server
    curlFetch(FB_MASTER_SERVER_URL, servers, true);
    SDL_Log("After master server: %d servers", (int)servers.size());

    // 2. Community server list (same format as original: "host port")
    curlFetch(GITHUB_SERVER_LIST_URL, servers, true);
    SDL_Log("After GitHub list: %d servers total", (int)servers.size());

    return servers;
}
