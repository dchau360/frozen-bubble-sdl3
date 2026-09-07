#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "bubblegame.h"
#include "mainmenu.h"
#include "platform.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

static int failures = 0;
#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expression); \
    ++failures; } } while (false)

struct MainMenuTestAccess {
    static std::unique_ptr<MainMenu> create(SDL_Renderer* renderer) {
        return std::unique_ptr<MainMenu>(new MainMenu(renderer, MainMenu::HeadlessTestTag{}));
    }
    static char* edit(MainMenu& menu, int mode) {
        menu.showingNetPanel = true;
        menu.networkInLobby = mode != 11;
        menu.networkInputMode = mode;
        menu.selectedActionIndex = 0;
        char* buffer = mode == 11 ? menu.networkPreNick
                     : mode == 5 ? menu.networkUsername : menu.networkChatInput;
        buffer[0] = '\0';
        return buffer;
    }
};

struct BubbleGameTestAccess {
    static const char* chat(BubbleGame& game) {
        game.chattingMode = true;
        return game.chatInputBuf;
    }
};

template<class Editor>
static void type(Editor& editor, const char* text) {
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = text;
    editor.HandleInput(&event);
}

template<class Editor>
static void backspace(Editor& editor) {
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_BACKSPACE;
    editor.HandleInput(&event);
}

int main() {
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true);
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    InitDataDir();
    SDL_Window* window = SDL_CreateWindow("text-input-test", 64, 64, SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer) return 1;
    {
        auto menu = MainMenuTestAccess::create(renderer);
        for (int mode : {11, 4, 5}) {
            char* buffer = MainMenuTestAccess::edit(*menu, mode);
            type(*menu, u8"Aé界🎮");
            backspace(*menu);
            CHECK(std::string(buffer) == u8"Aé界");
            backspace(*menu);
            CHECK(std::string(buffer) == u8"Aé");
            // Android IMEs can send a backspace inside a text event.
            type(*menu, "\bZ");
            CHECK(std::string(buffer) == "AZ");
            backspace(*menu);
            backspace(*menu);
            backspace(*menu);
            CHECK(std::string(buffer).empty());

            const size_t limit = mode == 11 ? 15 : mode == 5 ? 31 : 255;
            const std::string prefix(limit - 1, 'a');
            type(*menu, prefix.c_str());
            type(*menu, u8"é");
            CHECK(std::string(buffer) == prefix);
            type(*menu, "Z");
            CHECK(std::string(buffer) == prefix + "Z");
            // A full field must still accept deletion and subsequent input.
            type(*menu, "\bQ");
            CHECK(std::string(buffer) == prefix + "Q");
        }
        // The lobby dock also supports Backspace outside its expanded editor.
        char* dock = MainMenuTestAccess::edit(*menu, 0);
        std::strcpy(dock, u8"Aé");
        backspace(*menu);
        CHECK(std::string(dock) == "A");

        BubbleGame game(renderer);
        const char* chat = BubbleGameTestAccess::chat(game);
        type(game, u8"Aé界🎮");
        for (const char* expected : {u8"Aé界", u8"Aé", "A", "", ""}) {
            backspace(game);
            CHECK(std::string(chat) == expected);
        }
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return failures ? 1 : 0;
}
