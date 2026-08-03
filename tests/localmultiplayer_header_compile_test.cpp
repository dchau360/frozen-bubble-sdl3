#include "localmultiplayer_settings.h"

int main() {
    LocalMultiplayerOptions options;
    SetupSettings settings = BuildLocalMultiplayerSettings(options);
    return settings.playerCount == 2 ? 0 : 1;
}
