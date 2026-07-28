# 05 — Rendering and Audio Audit Notebook

## Scope

Task 7: rendering, transitions, fonts, surfaces/textures, audio, and repeated SDL
resource lifecycle, on production baseline `09d6c7bf` (v2.4.27).

Files reviewed in full: `src/shaderstuff.cpp` (1777), `src/shaderstuff.h` (170),
`src/transitionmanager.cpp` (76), `src/transitionmanager.h` (51),
`src/ttftext.cpp` (82), `src/ttftext.h` (64), `src/audiomixer.cpp` (200),
`src/audiomixer.h` (58), `src/sdl3_compat.h` (12). Render-lifecycle consumers
reviewed for their Task 7 slice: `src/frozenbubble.cpp` (644, render/lifecycle
paths), `src/bubblegame_render.cpp` (1212), `src/mainmenu_panels.cpp` (579,
render/effect paths), plus the resource-creation sites they depend on in
`src/bubblegame.cpp` (constructor/NewGame/ReloadGame texture loads, `Penguin`
and `Shooter` sprite structs in `src/bubblegame.h`), `src/mainmenu.cpp`
(constructor, `InitCandy`/`RefreshCandy`), `src/menubutton.cpp` (icon texture
ownership), and `src/highscoremanager.cpp` (`CreateLevelImages`, the sole
`shrink_` caller). Cross-owner allocation handover from Task 3: `server/log.c`
and `server/stats.c` raw-allocation policy (IMP-010).

Dynamic evidence uses `/tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp`,
which links the unchanged production objects `shaderstuff.cpp.o`,
`transitionmanager.cpp.o`, `audiomixer.cpp.o`, `ttftext.cpp.o`,
`gamesettings.cpp.o`, and `platform.cpp.o` from `build-audit-werror` (strict)
and `build-audit-sanitize` (ASan+UBSan). All stateful runs used
`CFFIXED_USER_HOME` isolation proven before any file was opened, dummy
video/audio drivers, and real read-only `share/` assets.

## Trust boundaries and invariants

- **Effect-surface contract.** Every `shaderstuff.cpp` pixel routine assumes a
  32-bit surface whose pitch equals `w * 4`; the `SDL_GetRGBA`-style routines
  index `((Uint32*)pixels)[x + y*w]` and several transition routines address the
  destination with the *source's* pitch (`copy_line`, `circle_effect`,
  `plasma_effect`). Transitions operate only on the two 640×480
  `SDL_PIXELFORMAT_ARGB8888` surfaces `TransitionManager` creates, so pitch
  equality holds by construction. The menu candy/overlook effects operate on
  `IMG_Load` results; the harness `formats` run measured **seven** of the eight
  probed files as 4-byte-per-pixel tight-pitch ABGR8888 — and those seven are
  exactly the inputs that reach a pixel routine (`fblogo.png`,
  `fblogo-mask.png`, and the five `txt_*_text.png` `overlook_` sources) — so the
  `[x + y*w]` indexing and the fixed `Rdec/Gdec/Bdec/Adec = 0/1/2/3` byte-offset
  macros are valid for them. The eighth probed file, `back_one_player.png`, is
  **`SDL_PIXELFORMAT_RGB24`, 3 bytes per pixel**. It is not an effect input: it
  loads into `HighscoreManager::backgroundSfc` (`highscoremanager.cpp:189`) and
  its only consumer is the format-converting `SDL_BlitSurface` at
  `highscoremanager.cpp:291` into a freshly created ARGB8888 `bigOne`; `shrink_`
  then reads `bigOne`, never `backgroundSfc` (whole-tree grep: `backgroundSfc`
  appears only at `highscoremanager.h:63`, `.cpp:189`, `:290` (a log), and
  `:291`). The pixel-format safety argument therefore rests on *which* surfaces
  reach the routines, not on a uniform 4-bpp asset set. The 4-bpp assumption is
  an asset invariant, not an enforced one: a 24-bit PNG dropped into the
  candy/overlook input set would break it silently — and `back_one_player.png`
  proves 3-bpp PNGs already ship in this tree (recorded under IMP-013).
- **`get_pixel`/`set_pixel` bounds are caller-owned.** `set_pixel` has no bounds
  check; `get_pixel` clamps, but its upper bounds are `s->w`/`s->h` — one past
  the last valid index (see IMP-013). Every reachable caller was traced to
  stay within `w-2`/`h-2` guards or measured-contained data (points mask).
- **Ownership boundary.** All textures belong to the renderer; SDL3 destroys
  them with it. The shutdown order in `~FrozenBubble` destroys the renderer
  first and no code touches textures afterwards (`HighscoreManager::Dispose`
  saves files and closes only its font), so no shutdown use-after-free exists.
- **Audio guards.** Every public `AudioMixer` entry point is safe when
  construction failed: `PlayMusic`/`PlaySFX` test `mixerEnabled`,
  `PauseMusic` tests `musicTrack`, `MuteAll` tests `mixer`. The unguarded
  precondition is object *existence*: `FrozenBubble`'s early-return
  constructor paths leave `audMixer` indeterminate and `RunForEver` line 228
  dereferences it — that is BUG-034 (Task 6, reproduced), whose audio-side
  lifetime question closes here: a *constructed* mixer survives device
  failure, missing files, and unknown track ids (sanitized run below).
- **Text ownership.** `TTFText` owns `outTexture` always and `textFont` only
  when `ownsFont` (path-loaded); external fonts are never closed by it.
  `UpdateText` destroys the previous texture before rendering. `curText`
  retains the caller's pointer, but `Text()` has no callers (IMP-007).
- **Snapshot flow.** `DoSnipIn` captures the pre-switch frame into `snapIn`;
  `TakeSnipOut` captures the first post-switch frame into `snapOut` and runs
  `effect()`, which progressively copies `snapOut` into `snapIn` and presents
  each step. Both are skipped for `gfxLevel() > 2` and on WASM.
- All inputs to this subsystem are local assets and local state; no untrusted
  network data reaches the pixel or audio paths directly. Team-color indexing
  from peer options (SEC-007) surfaces in `bubblegame_render.cpp` lines 87,
  451, 482 (`kTeamColors[team - 1]`) — consumption sites of the Task 6 finding,
  no new ID.

## Static review

### Step 1 — SDL resource ownership table

One row per owner/family. "Repl." = behavior when re-created; "Destroy" = the
path that releases it. Leak rows are cross-referenced to findings.

| Resource (family, count) | Creator | Owner / aliases | Repl. behavior | Destruction path |
|---|---|---|---|---|
| `SDL_Window` | `FrozenBubble` ctor | `FrozenBubble::window` | never replaced | `~FrozenBubble` (explicitly invoked in `RunForEver`) |
| `SDL_Renderer` | `FrozenBubble` ctor | `FrozenBubble::renderer`; aliased by every subsystem | never replaced | `~FrozenBubble`, before window |
| Window icon surface | `SDL_LoadBMP` | ctor local | n/a | destroyed immediately after `SDL_SetWindowIcon` |
| `MainMenu` textures (~44: background, logo, 4 banners, 4 blink, 5 idle SP buttons, panel/button/void, 5+13 net spots, misc) | `MainMenu` ctor | `MainMenu` members | never replaced | `~MainMenu` destroys only `background`+`fbLogo`; `MainMenu` itself is never deleted → shutdown-scope only (IMP-007 family note) |
| `activeSPButtons[5]` **surfaces** | `IMG_Load` in `MainMenu` ctor (`mainmenu.cpp:124`, **result never null-checked**) | `MainMenu` | never | never destroyed; process-lifetime. Unchecked loads are dereferenced at `mainmenu_panels.cpp:198` (`activeSPButtons[0]->w`) and passed into production `overlook_` at `:213`, which reads `orig->format` at `shaderstuff.cpp:1485` — BUG-044 |
| `MenuButton` icons + 2 backgrounds (per button, 8 buttons) | `MenuButton` ctor | move-only `MenuButton` | n/a | `~MenuButton` destroys all (verified Task 6) |
| `candyOrig`/`candyModif`/`logoMask` (`TextureEx` surfaces + `candyModif.tex`) | `InitCandy` | `MainMenu` | `LoadTextureData`/`LoadEmptyAndApply` **reassign `sfc` without destroying the previous surface**; only the method-3/4/8 branches explicitly destroy `candyOrig.sfc` once; `OutputTexture` destroys the prior texture correctly | no destructor on `TextureEx` → leaks on every `RefreshCandy` (user-triggered LCTRL, `mainmenu_input.cpp:280`) — BUG-001 |
| Heap `SDL_Rect` pairs | `LoadEmptyAndApply` (`shaderstuff.h:67`) | none (owner-less `new`) | n/a | never freed — 2 per call; the three call sites (`mainmenu.cpp:208`, `:218`, `:231`) sit in the mutually exclusive `if (candyMethod == 3)` / `else if (== 4)` / `else if (== 5)` / `else if (== 8)` chain at `mainmenu.cpp:204/214/224/228`, so **at most one call — 2 leaked rects — runs per `InitCandy`** — BUG-001 (clang-analyzer NewDeleteLeaks concurs) |
| `overlookSfc` surface | `SPPanelRender` first use | `MainMenu` | re-inited in place by `restartOverlook` | never destroyed; process-lifetime |
| `miniOverlook` texture | `SPPanelRender` per frame | frame-local | n/a | destroyed same frame (correct) |
| `TransitionManager::snapIn`/`snapOut` (640×480 ARGB8888) | `TransitionManager` ctor | singleton | never | `~TransitionManager` via `Dispose()` — **never called by anyone**; process-lifetime |
| `SDL_RenderReadPixels` snapshot surfaces | `DoSnipIn`/`TakeSnipOut`/`RenderPaused` | call-local | n/a | destroyed same call (correct; null-safe) |
| Per-frame transition texture | `synchro_after` (`shaderstuff.cpp:71`) | **none** — created into a by-value parameter and dropped; `transitionTexture` is initialized `nullptr` and never assigned (whole-tree grep) | one per animation frame | **never** — BUG-041, runtime-reproduced 1.2 MB/frame |
| `BubbleGame` constructor textures (4×8 bubble sets, 2×8 stick, 35 pause penguin, frozen/prelight ×4, 3 shooters, 2 compressor, 2 on-top, 4+4 attack, 5 left overlays, 2 dots, 2+2 panels, pause background) | `BubbleGame` ctor | `BubbleGame` members | loaded once | `~BubbleGame` destroys only background, pauseBackground, and the 4×8 bubble sets; the object is never deleted → shutdown-scope only |
| `background` | each `NewGame` | `BubbleGame` | destroyed before replacement (`bubblegame.cpp:353`) | correct |
| `bubbleArrays[i].hurryTexture` (one live texture per active player, 1–20 per match; **17 load sites** in `bubblegame.cpp`, lines 441–849 — 18 occurrences of the identifier, one of which is the comment at line 304) | each `NewGame` case | `BubbleArray` | **reloaded per `NewGame` with no destroy of the prior texture** | never — BUG-042 |
| Penguin animation frames (71+97+68+158 = 394 per player) | `Penguin::LoadPenguin`, called for every player in every `NewGame` case and `ApplyMiniSlotGeometry` | `Penguin` arrays | **unconditionally re-`IMG_LoadTexture`s over prior pointers** | never — BUG-042 |
| `prePauseBackground` | `RenderPaused` on pause entry | `BubbleGame` | destroyed before replacement (`bubblegame_render.cpp:1188`) | correct while running; last one process-lifetime |
| `TTFText` instances — **exhaustive, 23 total**: `BubbleGame` 13 scalars (`inGameText`, `winsP1Text`, `winsP2Text`, `scoreText`, `comboText`, `finalScoreText`, `mpTrainText`, `clearWinText`, `targetingText`, `statsText`, `malusAlertText`, `chatLineText`, `chatInputText`; `bubblegame.h:532-545`) **+ `playerNameWinText[MAX_NET_PLAYERS]` = 5 more instances (`bubblegame.h:534`)**; `MainMenu` 2 (`panelText`, `networkText`); **`FrozenBubble::menuText` (`frozenbubble.h:96`)**; `HighscoreManager` 2 (`panelText`, `nameInput`) plus the per-score `layoutText` | ctors | owning class | `UpdateText` destroys prior texture; `LoadFont(path)` closes prior owned font | `~TTFText` closes owned font + texture (correct); sanitized 200-iteration stress clean. `LoadFont` sweep (18 call sites, whole tree): every instance is font-loaded **except two** — `targetingText`, which is still driven through `UpdateText`/`Coords()`/`Texture()` at `bubblegame_render.cpp:946-957` (BUG-043), and `FrozenBubble::menuText`, which has zero references outside `frozenbubble.h:96` (dead member — IMP-012 extension). `playerNameWinText[i]` is correctly loaded in the `bubblegame.cpp:153` loop |
| `HighscoreManager` surfaces (`backgroundSfc`, 8 `useBubbles`) and textures (6 + `smallBG[10]`) | ctor / `CreateLevelImages` | singleton | `smallBG[slot]` destroyed before replacement (correct) | `Dispose` saves files and closes only the font; rest process-lifetime |
| `highscoreFont` (TTF) | ctor | singleton | never | `~HighscoreManager` via `Dispose` (before `TTF_Quit` — order verified) |
| `shaderstuff` static buffers: `circle_steps` (1.2 MB), `plasma`/`plasma2`/`plasma3` (300 KB ea), `points` (200), `flakes` (200), `precalc_cos/sin` (200 ea) | `init_effects` / first effect use | translation-unit statics | allocated once | never freed; process-lifetime by design; all `fb__out_of_memory`-guarded **except `precalc_cos`/`precalc_sin`** (IMP-010) |
| `MIX_Mixer`, music `MIX_Track` | `AudioMixer` ctor | singleton | never | `Dispose` (called from `~FrozenBubble`) — correct order, verified |
| `curMusicAudio` | `PlayMusic` | singleton | stopped (500 ms fade + 600 ms blocking delay on native) then destroyed before replacement | `Dispose` |
| SFX `MIX_Audio` cache (`sfxFiles` map) | `GetSFX` on demand | singleton | failed loads cached as `nullptr` forever (retried never; re-logs stale `SDL_GetError`) | `Dispose` destroys non-null entries |
| SFX `MIX_Track` pool (`sfxTracks`) | `PlaySFX` when all busy | singleton | reused when idle; grows to peak concurrency | `Dispose` stops and destroys all |
| Server `log.c` message allocations / `stats.c` hash+strings (IMP-010 handover) | `log_message`, stats put | caller / hash table | n/a | Task 3 ownership review; policy inconsistency dispositioned under IMP-010 below |

Because Apple ASan cannot detect leaks, this table plus the static create/destroy
pairing carries the resource-leak burden; the two leak defects it exposes
(BUG-041, BUG-042) are backed respectively by runtime RSS measurement and by a
complete grep-verified absence of any destroy site.

### Step 2 — surface and pixel operations

- **memcpy transitions** (`copy_line`, `copy_column`, `bars_effect`,
  `fillrect`/`squares_effect`, `circle_effect`, `plasma_effect`): all offsets
  are products of loop indices bounded by `XRES`/`YRES` (640/480) and
  `img->pitch`/`bpp`; destination is addressed with the source's pitch, valid
  because both operands are always `snapIn`/`snapOut` (640×480 ARGB8888,
  pitch 2560). `bars_effect` covers `16 × 40` bar segments of `(640/16)*bpp`
  bytes; `fillrect` rejects `i >= 640/32 || j >= 480/32` before writing.
  Verified in-bounds; six sanitized full transition animations ran with no
  ASan/UBSan diagnostic.
- **`plasma_init`**: exact `fread` of 640×480 bytes with `exit(1)` on shortfall
  (hard-exit load policy — IMP-010); normalization loops bounded; `plasma3`
  filled by `plasma_effect` type 3 before use.
- **Bilinear/bicubic samplers** (`rotate_bilinear_`, `stretch_`, `tilt_`,
  `waterize_`, `flipflop_`, `overlook_`): every sampled neighbor is guarded by
  `x_ < 0 || x_ > w-2 || y_ < 0 || y_ > h-2` (or `w-4/h-4` for bicubic) before
  the 2×2/4×4 fetch, so `+1`/`+3` neighbors stay inside. `snow_`'s flake spawn
  and collision probes stay within `w-1` (spawn cap `w-4-wideness`, probe
  `x_+3`); vertical probes are removed at `y >= h-4`.
- **Off-by-one clamp bounds around the pixel helpers.** `set_pixel`
  (`shaderstuff.cpp:41-45`) has **no clamp of its own** — it indexes
  `[x + y*s->w]` directly; the off-by-one clamps that feed it live at its call
  sites, `shaderstuff.cpp:488` (`shrink_`) and `:1155` (`points_`), both
  `CLAMP(…, 0, dest->w)` / `CLAMP(…, 0, dest->h)`. `get_pixel`
  (`shaderstuff.cpp:47-50`) applies the same wrong bounds internally:
  `CLAMP(x,0,s->w)` admits `x == w` (index `w + y*w`) and `CLAMP(y,0,s->h)`
  admits `y == h` (index `x + h*w`, past the buffer). ASan-demonstrated on the
  production object (dynamic evidence below). Reachability with shipped data
  was ruled out: guarded samplers stay ≤ `w-2`; `shrink_`'s only caller
  (`CreateLevelImages`) computes dest writes 0–63 × 0–84 inside a 64×85
  surface and source reads ≤ (447,387) inside 640×480; `points_`'s random walk
  can only leave the mask's white region if a white pixel touches the border,
  and the measured `fblogo-mask.png` has **zero** white border pixels.
  Recorded as IMP-013, not a defect.
- **Format assumptions**: the harness `formats` run probed eight shipped files.
  **Seven** load as 4-byte-per-pixel tight-pitch `SDL_PIXELFORMAT_ABGR8888`
  (`fblogo.png`, `fblogo-mask.png`, and the five `txt_*_text.png` `overlook_`
  sources) — and those seven are precisely the surfaces that reach a pixel
  routine, so the `[x + y*w]` indexing and RGBA byte-offset macros hold for
  every actual effect input. The eighth, `back_one_player.png`, is
  `SDL_PIXELFORMAT_RGB24` at **3 bpp**; it is *not* an effect input — it is
  `HighscoreManager::backgroundSfc`, whose sole consumer is the
  format-converting `SDL_BlitSurface` into the ARGB8888 `bigOne` at
  `highscoremanager.cpp:291` (grep: `backgroundSfc` occurs only at
  `highscoremanager.h:63`, `.cpp:189`, `:290`, `:291`). `shrink_` then reads
  `bigOne`, so no 3-bpp surface is ever indexed as 32-bit. The guards are
  uneven: only `overlook_init_`/`overlook_` (`shaderstuff.cpp:1460`, `:1485`,
  `:1490`), `waterize_` (`:1206`, `:1212`), and `:620` test `!= 4` and
  `abort()`; the remaining samplers (`:549/554`, `:716/721`, `:819`,
  `:888/894`, `:944/950`, `:1018/1024`, `:1105/1110/1115`, `:1569/1574`) only
  reject `== 1`, so a 2- or 3-byte candy input would silently misindex there.
  The contract is undocumented and unasserted at the `IMG_Load` sites (IMP-013
  documentation item), and `back_one_player.png` demonstrates that 3-bpp PNGs
  already ship in `share/`.
- **`myLockSurface`** retries forever on persistent `SDL_LockSurface` failure
  (10 ms backoff). All surfaces passed are plain malloc-backed surfaces whose
  lock cannot fail persistently; noted under IMP-013 as a robustness item.
- **Failed allocations**: all effect `malloc`s call `fb__out_of_memory`
  (abort) except `precalc_cos`/`precalc_sin` in `waterize_`
  (`shaderstuff.cpp:1221-1226`), which are dereferenced unchecked — the exact
  IMP-010 render-side instance (cppcheck `nullPointerOutOfMemory` concurs).
- **Failed loads reaching pixel routines (fix round).** A second instance of
  BUG-001's missing-asset crash class exists outside `TextureEx`:
  `MainMenu`'s constructor stores five `IMG_Load` results into
  `activeSPButtons[i]` with **no null check** (`mainmenu.cpp:124`, inside the
  `SP_OPT` loop; note the sibling `idleSPButtons[i] = IMG_LoadTexture(...)` on
  the previous line is equally unchecked but only ever reaches SDL entry points
  that validate null). `activeSPButtons` is then dereferenced twice in
  `SPPanelRender`: unconditionally at `mainmenu_panels.cpp:198`
  (`SDL_CreateSurface(activeSPButtons[0]->w, activeSPButtons[0]->h, …)`) and at
  `:213`, which passes `activeSPButtons[i]` into production `overlook_`, whose
  first statement reads `orig->format` (`shaderstuff.cpp:1485`). Registered as
  BUG-044; the `:213` half is reproduced under UBSan/ASan below. Whole-tree
  grep confirms `activeSPButtons` has exactly these four occurrences
  (`mainmenu.h:128`, `mainmenu.cpp:124`, `mainmenu_panels.cpp:198`, `:213`),
  so no null check exists anywhere on the path.
- **`rotate_nearest_`, `rotate_bicubic_`, `autopseudocrop`, `draw_line_`,
  `blacken_`, `alphaize_`, `pixelize_`** have no callers anywhere in `src/`
  (grep). `rotate_bicubic_`'s misuse of dest dimensions to clamp source reads
  (`shaderstuff.cpp:753`) and `draw_line_`'s degenerate 0/0 NaN slope are
  therefore unreachable dead code — recorded under the already-confirmed
  IMP-009 dead-code family, not promoted.

### Step 3 — transition and text lifecycle

- Snapshot capture: `DoSnipIn`/`TakeSnipOut` read the full output surface and
  scale-blit into the fixed 640×480 canvases; null read results are tolerated
  by SDL's argument validation. Logical letterbox presentation means captures
  include letterbox bars scaled into 640×480 — cosmetic only.
- Transition triggers — **exactly two**, established by enumerating every
  producer and consumer (`grep -rn 'DoSnipIn\|TakeSnipOut' src/`): the only
  `DoSnipIn` call sites are `mainmenu.cpp:497` (the first statement of
  `MainMenu::SetupNewGame`, i.e. **game start** — every start path, including
  the network one at `mainmenu_netpanel.cpp:163`, funnels through
  `SetupNewGame`) and `bubblegame.cpp:1012` (**round reload**, `ReloadGame`).
  The only `TakeSnipOut` call site is `bubblegame_render.cpp:1173`, gated on
  `!firstRenderDone`. **Menu return is not a trigger**: `QuitToTitle`
  (`bubblegame.cpp:1363`) clears `firstRenderDone` but calls no `DoSnipIn`, and
  no `BubbleGame::Render` runs again until the next `SetupNewGame` — which
  supplies its own `DoSnipIn` — so the cleared flag is consumed by the
  game-start animation rather than producing an extra one. The earlier
  three-trigger wording ("game start, round reload, and menu return")
  double-counted `mainmenu.cpp:497`, attributing the game-start producer to
  menu return; it is corrected here and in BUG-041. Each `TakeSnipOut` runs one `effect()`
  animation of ~31–41 frames; each frame creates a 640×480 texture that is
  never destroyed (BUG-041): `synchro_after` receives `tex` by value, destroys
  only the caller's stale pointer (always null, since
  `TransitionManager::transitionTexture` is never assigned) and drops the
  replacement. `~TransitionManager` frees the surfaces but no transition
  texture; `Dispose` has no caller.
- Renderer reset: none exists — the renderer is created once; no device-lost
  handling is needed on this SDL3 usage pattern.
- Text refresh: `UpdateText` destroys the previous texture first, renders
  foreground and background layers, blits with a −1,−1 shadow offset, and
  tolerates both TTF failures (null-return early-outs) and
  `SDL_CreateTextureFromSurface` failure (render sites pass null textures to
  SDL, which validates). Empty and 10 KB strings exercised dynamically.
  Failure leaves `coords` stale — cosmetic. Font replacement closes the prior
  owned font; adopting an external font clears ownership; the destructor
  closes only owned fonts (external-font survival verified dynamically).
- **Font-load sweep (fix round).** The 18 `LoadFont` call sites in `src/` were
  matched against the 23 `TTFText` instances in the ownership table. Two
  instances are never font-loaded: `FrozenBubble::menuText` (harmless — zero
  references outside `frozenbubble.h:96`, a dead member recorded under IMP-012)
  and `BubbleGame::targetingText`, which **is** used. `targetingText` has no
  initializer for `coords` (`ttftext.h:57`) and the default constructor
  (`ttftext.cpp:22-24`) writes nothing, so on a heap-allocated `BubbleGame`
  (`frozenbubble.cpp:173`, `new BubbleGame(renderer)`) `coords` starts
  indeterminate. `UpdateText` (`bubblegame_render.cpp:946`) then hits the
  `if (!textFont || !txt) return;` early-out at `ttftext.cpp:48` — before
  `coords.w`/`coords.h` are assigned and before `outTexture` is created — so
  `UpdatePosition` writes only `coords.x/.y` and the render at
  `bubblegame_render.cpp:957` reads an indeterminate `w`/`h` into an
  `SDL_FRect` and passes a null texture to `SDL_RenderTexture`. The multiplayer
  targeting indicator therefore can never draw. Registered as BUG-043 and
  reproduced against the production `ttftext.cpp` object below; this
  **reopens and corrects** the earlier IMP-005 render-slice closure, which had
  concluded there was no reachable use-before-initialization.
- Per-frame text churn: `Update2PText`, `UpdateScoreText`, `RenderRoundStats`
  (~30 `cell()` calls), `RenderRoyaleHud`, chat overlay, and
  `UpdatePlayerNameWinText` re-render TTF surfaces and re-create textures
  every frame. Correct (no leak — each `UpdateText` frees the prior texture)
  but wasteful; recorded under the confirmed IMP-009/IMP-006 families.
- Resize/logical scaling: `SDL_SetRenderLogicalPresentation(640,480,LETTERBOX)`
  set once; mouse/touch coordinates converted through
  `SDL_RenderCoordinatesFromWindow` (Task 6 reviewed the input side). F12
  toggles fullscreen through settings + `SDL_SetWindowFullscreen`. Real
  fullscreen/resize toggling was not executed on this host (limitation).
- Construction-failure rendering: if window/renderer/TTF init fails,
  `IsGameQuit` is set but the constructor **continues**, loading every menu and
  game asset against a null renderer. SDL validates null arguments so no crash
  path was found beyond the already-confirmed BUG-034 asset-directory case;
  the wasted full construction is noted under IMP-010's failure-policy theme.

### Step 4 — audio initialization and failure handling

- Construction: `MIX_Init` failure and `MIX_CreateMixerDevice` failure both
  set `mixerEnabled = false` and return; `musicTrack` creation failure leaves
  it null. All later entry points are guarded (see invariants). `GetSFX` is
  only reachable through `PlaySFX`'s `mixerEnabled` guard, so its unguarded
  `MIX_LoadAudio(mixer, …)` never sees a null mixer.
- Playback/replacement: `PlayMusic` stops the current track (immediate on
  WASM; 500 ms fade plus **600 ms blocking `SDL_Delay` on native**, freezing
  the main loop on every music change — improvement note under IMP-009),
  destroys the previous `MIX_Audio` only after the fade window, loads the new
  file, and starts an infinite-loop track. Unknown track ids produce an empty
  path whose load fails and is handled. Missing SFX files log and return;
  the failure is cached as a permanent null entry that re-logs a stale
  `SDL_GetError` (observed dynamically) — minor, noted with IMP-010.
- Pause/resume (`PauseMusic`), mute (`MuteAll` stops all tracks), and the
  SFX track pool (reuse-idle-else-create) were exercised under ASan+UBSan
  with no diagnostic and no RSS growth trend.
- Shutdown: `Dispose` stops/destroys SFX tracks, cached audio, music track,
  music audio, mixer, then `MIX_Quit`, called from `~FrozenBubble` before
  `SDL_Quit` — order correct. `Dispose` ends with `this->~AudioMixer()` while
  `ptrInstance` stays non-null — the same explicit-destructor singleton
  pattern as `TransitionManager`, `HighscoreManager`, and `GameSettings`. No
  production call occurs after `Dispose` (call-order trace), so this is
  confirmed improvement-grade (IMP-007 extension), not a defect.
- Callback/stream lifetime: no user callbacks are registered; all SDL3_mixer
  objects are destroyed before `MIX_Quit`, and the only asynchronous consumer
  (the mixer device) is destroyed in `Dispose`. The BUG-034 audio-side
  question — whether `audMixer` can be used before it exists — is a
  `FrozenBubble` construction defect already confirmed and reproduced by
  Task 6 (`frozenbubble.cpp:228`); no separate mixer-internal lifetime defect
  exists.

### Analyzer triage (scoped files)

229 cppcheck and 248 clang-tidy unique records touch the scoped files.
Promoted on independent evidence: `shaderstuff.h:55` nullPointerRedundantCheck
and `shaderstuff.h:68` NewDeleteLeaks (both inside BUG-001, reproduced);
`shaderstuff.cpp:1225-1226` nullPointerOutOfMemory (IMP-010);
`ttftext.h:53` operatorEqVarError / bugprone-unhandled-self-assignment
(IMP-007); `TextureEx`/`TTFText` uninitMember families (IMP-005 render slice —
**the `TTFText::coords` member of that family was re-triaged in the fix round
and promoted to BUG-043**, see below).
The dominant remaining families — 86 narrowing, 60 implicit-widening, 37
integer-division, ~150 cast/reserved-identifier/style records, insecureAPI
rand/bzero — were reviewed at their sites as deliberate Perl-port arithmetic
and style-level items; they stay under the confirmed IMP-006/IMP-008/IMP-009
umbrellas with no defect promotion. Individually dismissed records are in
"Dismissed candidates".

## Dynamic evidence

Harness: `/tmp/fb-sdl3-audit/task7/task7_render_audio_harness.cpp`, built
warnings-strict against `build-audit-werror` objects
(`/tmp/fb-sdl3-audit/task7/task7_harness`) and ASan+UBSan against
`build-audit-sanitize` objects (`task7_harness_sanitize`), exit 0 both
compiles. The user's three real preference files were SHA-256 hashed before any
run and verified byte-identical afterwards
(`/tmp/fb-sdl3-audit/task7/real-prefs-baseline.txt`, `shasum -c` all OK).

- **Isolation gate** (`probe`): with `CFFIXED_USER_HOME=/tmp/fb-sdl3-audit/task7/home7`,
  `SDL_GetPrefPath` resolved to
  `/tmp/fb-sdl3-audit/task7/home7/Library/Application Support/frozen-bubble/`;
  `ISOLATION=OK`, exit 0, before any preference-owning singleton existed.
  Every stateful subcommand re-runs the same gate first in code order (the
  interleaved stderr lines in logs are unbuffered-stderr ordering, not
  execution order).
- **`formats`** (exit 0, `formats.log`): seven of the eight probed images —
  every file that reaches a pixel routine — are ABGR8888, 4 bpp, with
  `pitch == w*4`. The eighth line of the log reads
  `/gfx/back_one_player.png w=640 h=480 fmt=SDL_PIXELFORMAT_RGB24 bpp=3
  pitch=1920 tight=yes`: it is blit-only (`highscoremanager.cpp:291`), never
  indexed. `fblogo-mask.png` has **0** white border pixels, containing the
  `points_` walk. (The original Task 7 write-up said "all eight … 4 bpp",
  contradicting this same log; corrected in the fix round.)
- **`oobdemo`** (sanitized, exit 134, `oobdemo.log`): production
  `get_pixel` at `shaderstuff.cpp:49` performed a heap-buffer-overflow READ of
  4 bytes at exactly "0 bytes after" a tightly-sized 8×8 ARGB8888 pixel
  buffer when passed `x == w` — direct sanitizer proof of the CLAMP
  upper-bound off-by-one (IMP-013). In-bounds control read succeeded first.
- **`texleak 100`** (strict build, exit 0, `texleak.log`): 100 production
  `synchro_before`/`synchro_after` frames against a software renderer grew RSS
  perfectly linearly 18 → 138 MB — **1.2 MB per frame**, the exact size of one
  dropped 640×480 ARGB8888 texture (BUG-041).
- **`transition 5`** (strict build, isolated home, exit 0, `transition.log`):
  the full production path (`GameSettings::ReadSettings` default `gfxLevel=1`,
  `init_effects`, `TransitionManager::DoSnipIn`/`TakeSnipOut`, random
  production `effect()` animations) grew RSS 21 → 149 MB over five
  transitions (83/145/179/124/149; the one dip is allocator page return —
  the isolated `texleak` measurement shows the underlying growth is strictly
  linear). BUG-041 is thereby reproduced in the real production call path.
- **`transition 6` sanitized** (exit 0, `transition-sanitize.log`): six full
  effect animations under ASan+UBSan produced **no diagnostic** in any
  memcpy/pixel loop, RSS 44 → 264 MB (leak visible under sanitizer too).
- **`audio 3` sanitized** (isolated home, exit 0, `audio-sanitize.log`):
  three cycles of music start/replace, six overlapping SFX plus two distinct
  SFX, a missing SFX file, pause/resume, mute/unmute, an unknown track id,
  then `Dispose` — no sanitizer diagnostic, RSS steady (32→44 MB plateau),
  `dispose_ok`. Missing-SFX first failure logged the real error; repeats
  logged an empty stale error, confirming the cached-null-retry note.
- **`ttftext 200` sanitized** (exit 0, `ttftext-sanitize.log`): 200 cycles of
  owned-font load, empty/normal/10 KB-wrapped `UpdateText`, style change,
  owned-font replacement, external-font adoption, destruction of loaded and
  never-loaded instances, and external-font survival — no sanitizer
  diagnostic; RSS plateaued at 339 MB after iteration 50 with no further
  growth (allocator steady state, no per-iteration leak trend).
- **`texfail` / `texfail2`** (sanitized, exit 134 both): production
  `TextureEx::LoadEmptyAndApply` with a missing asset died on
  `member access within null pointer of type 'SDL_Surface'` at
  `shaderstuff.h:67`, and `LoadFromSurface(nullptr, …)` at `shaderstuff.h:55`
  — both BUG-001 failure orderings reproduced against production code. The two
  orderings differ in production reachability: the `:67` ordering is the
  **asset-reachable** one (`LoadEmptyAndApply` is called with a real asset path
  at `mainmenu.cpp:208/218/231`, so a missing or corrupt `fblogo.png` reaches
  it directly), whereas the `:55` ordering was reproduced by a direct harness
  call with `nullptr` — the only production callers of `LoadFromSurface`
  (`mainmenu.cpp:210/220/233`) pass `candyModif.sfc`, so reaching it in
  production additionally requires the preceding `SDL_CreateSurface` inside
  `LoadEmptyAndApply` to fail. Both are real orderings; only `:67` is
  asset-reachable.

Fix-round additions (same harness, same object sets; the two new subcommands
are `nofont` and `overlooknull`):

- **`nofont`** (strict build exit 0, sanitized build exit 0,
  `nofont.log` / `nofont-sanitize.log`): a `TTFText` placement-constructed into
  0xCD-poisoned storage (mirroring the heap-allocated `BubbleGame` at
  `frozenbubble.cpp:173`) and driven through the exact production sequence of
  `bubblegame_render.cpp:946-957` — `UpdateText`, `UpdatePosition`, `Coords()`,
  `Texture()` — printed:
  `texture=0x0 coords_x=120 coords_y=300 coords_w=-842150451 coords_h=-842150451`
  then `render_texture_ok=0 err=Parameter 'texture' is invalid`.
  `-842150451` is `0xCDCDCDCD`: `coords.w`/`coords.h` were never written, and
  SDL rejected the null texture. Direct proof of BUG-043 — the targeting
  indicator draws nothing and its destination rect is indeterminate.
- **`overlooknull`** (sanitized, exit 134, `overlooknull-sanitize.log`):
  production `overlook_(dest, nullptr, 0, 149)` reported
  `shaderstuff.cpp:1485:41: runtime error: member access within null pointer of
  type 'SDL_Surface'`, then `AddressSanitizer: SEGV on unknown address
  0x000000000004 … #0 overlook_ shaderstuff.cpp:1485`. This is the
  `mainmenu_panels.cpp:213` half of BUG-044 reproduced in production code; the
  `mainmenu_panels.cpp:198` half (`activeSPButtons[0]->w`) executes first in the
  same function and could not be linked without constructing the whole
  `MainMenu`, so it rests on the static chain (an unconditional `->w` on a
  pointer the tree never null-checks).

No listener, server, socket, browser, process kill, or hostile input was
created; runs used only dummy drivers, read-only `share/` assets, and the
isolated preference home.

## Candidates

All Task 7 candidates are resolved; none remain open.

- **BUG-001 — resolved: confirmed** (see Confirmed findings).
- **IMP-007 — resolved: confirmed improvement, extended** (see below).
- **IMP-010 — resolved: confirmed improvement** (see below).
- **BUG-041 — new, confirmed** (transition texture leak).
- **BUG-042 — new, confirmed** (per-`NewGame` penguin/hurry texture reload leak).
- **IMP-013 — new, confirmed improvement** (pixel-helper bounds/pitch contract).

Fix round 1 (independent review of this gate) reopened one closure and opened
two new candidates; all three are resolved:

- **IMP-005 render slice — reopened, corrected.** The original closure claimed
  no reachable use-before-initialization among `TextureEx`/`TTFText` members.
  `TTFText::coords` is a reachable one, with a functional consequence:
  promoted to BUG-043. IMP-005 itself stands as a confirmed improvement; only
  the render-slice *disposition* changed.
- **BUG-043 — new, confirmed** (`targetingText` never receives a font, so the
  multiplayer targeting indicator can never render; reproduced).
- **BUG-044 — new, confirmed** (`activeSPButtons` `IMG_Load` results are never
  null-checked and are dereferenced twice in `SPPanelRender`; the second
  dereference reproduced under UBSan/ASan).

## Confirmed findings

- **BUG-001 (Medium, confirmed, runtime-reproduced):** `TextureEx` failure
  ordering and ownership. `LoadFromSurface` reads `img->w`/`img->h` before its
  null check (UBSan repro, `shaderstuff.h:55`); `LoadEmptyAndApply` continues
  after a failed `IMG_Load` into `img->w` (UBSan repro, `shaderstuff.h:67`) —
  a missing/corrupt menu-logo asset crashes the client instead of degrading.
  `LoadEmptyAndApply` also leaks two owner-less `new SDL_Rect` per call, and
  `TextureEx` has no destructor while `LoadTextureData`/`LoadEmptyAndApply`
  reassign `sfc` without destroying the prior surface, so every
  `RefreshCandy` (LCTRL on the title screen) leaks the previous candy
  surfaces. **Quantity (corrected in fix round 1):** the three
  `LoadEmptyAndApply` call sites (`mainmenu.cpp:208`, `:218`, `:231`) are the
  bodies of the mutually exclusive `if (candyMethod == 3)` / `else if (== 4)` /
  `else if (== 5)` / `else if (== 8)` chain at `mainmenu.cpp:204/214/224/228`,
  so **at most one call runs per `InitCandy` — 2 leaked `SDL_Rect`, not 6**.
  The earlier "up to three calls / six leaked rects" wording (inherited from
  Task 2 and not challenged when Task 7 restated it) is wrong. The
  per-`RefreshCandy` surface leak is the dominant cost; the rect leak is
  2 × 16 bytes per candy init. **Reachability of the two orderings:** the
  `shaderstuff.h:67` ordering is asset-reachable — the production callers pass
  a real `fblogo.png` path, so a missing/corrupt asset lands on it directly.
  The `shaderstuff.h:55` ordering was reproduced by a direct harness call with
  `nullptr`; every production caller of `LoadFromSurface`
  (`mainmenu.cpp:210/220/233`) passes `candyModif.sfc`, so reaching it in
  production additionally requires the `SDL_CreateSurface` inside
  `LoadEmptyAndApply` to fail. Both orderings are real defects; only `:67` is
  reachable from a missing asset alone.
- **BUG-041 (Medium, confirmed, runtime-reproduced):** every transition
  animation frame leaks one 640×480 texture. `synchro_after`
  (`shaderstuff.cpp:66-73`) takes `tex` by value, destroys only the incoming
  stale pointer and drops the newly created texture;
  `TransitionManager::transitionTexture` is initialized null and never
  assigned, so the destroy branch never fires and no frame texture is ever
  released. **Trigger set (corrected in fix round 1): exactly two, not three.**
  The whole tree contains only two `DoSnipIn` producers —
  `mainmenu.cpp:497` (first statement of `MainMenu::SetupNewGame`, i.e. **game
  start**; every start path including the network one at
  `mainmenu_netpanel.cpp:163` funnels through `SetupNewGame`) and
  `bubblegame.cpp:1012` (**round reload**, `ReloadGame`) — and one consumer,
  `bubblegame_render.cpp:1173`. Menu return via `QuitToTitle`
  (`bubblegame.cpp:1363`) clears `firstRenderDone` with **no** `DoSnipIn` and
  produces no animation; the cleared flag is consumed by the next game start,
  which pairs with its own `DoSnipIn`. The earlier "per game start, round
  reload, and menu return" wording double-counted `mainmenu.cpp:497` by
  attributing the game-start producer to menu return. At default `gfxLevel`
  (1) the cost is ~31–41 textures ≈ 40–50 MB of render memory **per game start
  and per round reload**; an *N*-round match therefore leaks ≈ 40–50 MB × *N*
  (one animation at start plus one per reload), and returning to the menu adds
  nothing. Reproduced: exact 1.2 MB/frame linear RSS growth over 100 production
  frames, and 21 → 149 MB across five full production
  `DoSnipIn`/`TakeSnipOut` cycles.
- **BUG-042 (Medium, confirmed, complete static causal proof):**
  `Penguin::LoadPenguin` (`bubblegame.h:105-126`) unconditionally
  `IMG_LoadTexture`s 394 animation frames over the prior pointers, and every
  `NewGame` player-count case (plus `ApplyMiniSlotGeometry`) calls it for
  every active player; `bubbleArrays[i].hurryTexture` is likewise re-loaded
  per `NewGame`. No destroy site exists for either family anywhere in the
  tree (grep), so every match start leaks the previous per-player penguin set
  and hurry textures. Not runtime-reproduced (requires full-game
  construction); the causal chain is complete: load sites, overwrite, and
  absence of any release path are all exhibited in source.
- **BUG-043 (Medium, confirmed, runtime-reproduced; new in fix round 1):**
  the multiplayer targeting indicator can never render, because
  `BubbleGame::targetingText` never receives a font.
  *Evidence and causal path.* (1) `TTFText::coords` (`ttftext.h:57`) has no
  initializer and the default constructor (`ttftext.cpp:22-24`) has an empty
  body and no member-initializer list, so `coords` is default-initialized —
  indeterminate for a `BubbleGame` allocated with `new`
  (`frozenbubble.cpp:173`). Only the *copy* constructor (`ttftext.h:52`) value-
  initializes `coords{}`, and it is not used here. (2) `targetingText`
  (`bubblegame.h:535`) has exactly three other occurrences in the tree
  (`bubblegame_render.cpp:946`, `:956`, `:957`) and **no `LoadFont` call**; the
  whole-tree `LoadFont` sweep finds 18 call sites, none of them
  `targetingText`. (3) `TTFText::UpdateText` therefore returns at
  `ttftext.cpp:48` (`if (!textFont || !txt) return;`) — after destroying and
  nulling `outTexture` at `:47`, but **before** `coords.w`/`coords.h` are
  written at `:57-58` and before `outTexture` is recreated at `:56`.
  (4) `UpdatePosition` (`ttftext.cpp:80-82`) writes only `coords.x`/`coords.y`.
  (5) `bubblegame_render.cpp:957` copies the whole `*targetingText.Coords()`
  into an `SDL_FRect` and calls `SDL_RenderTexture` with
  `targetingText.Texture()` — a null texture and an indeterminate `w`/`h`.
  *Reachability.* The render block is gated on
  `currentSettings.singlePlayerTargetting || currentSettings.playerCount > 5`
  plus `playerTargeting[i] >= 0` (`bubblegame_render.cpp:940-941`).
  `singlePlayerTargetting` is a room option (`mainmenu.h:203`, default `true`,
  toggled at `mainmenu_input.cpp:995/1151`, serialized at
  `mainmenu_netpanel.cpp:915`, copied into `SetupSettings` at
  `mainmenu.cpp:543`), and `playerTargeting[]` is written to a non-negative
  index locally at `bubblegame_state.cpp:251` and from peer `r` messages at
  `bubblegame_net.cpp:631`. The path is therefore live in ordinary
  multiplayer, not dead code.
  *User impact.* Players never see who each opponent is targeting — a shipped
  multiplayer HUD feature is inert. Every affected frame also reads an
  indeterminate value (undefined behavior) and issues a rejected SDL call.
  *Reproduction.* Harness `nofont` against the unchanged production
  `ttftext.cpp` object printed
  `texture=0x0 coords_x=120 coords_y=300 coords_w=-842150451
  coords_h=-842150451` and `render_texture_ok=0 err=Parameter 'texture' is
  invalid`; `-842150451` is the `0xCDCDCDCD` poison, proving `coords.w/h` are
  never written. Identical output from the strict and the ASan+UBSan builds.
  *Proposed correction.* Call `targetingText.LoadFont(ASSET("/gfx/DroidSans.ttf"), …)`
  in the `BubbleGame` constructor alongside the other twelve scalar
  `TTFText`s (`bubblegame.cpp:118-167`), give `TTFText::coords` an in-class
  `{}` initializer, and have `UpdateText`'s early-outs zero `coords.w/h` so a
  failed render cannot leave a stale or indeterminate rect.
  *Verification strategy.* After the fix, re-run the `nofont` harness variant
  with a font loaded and assert a non-null texture and `coords.w/h > 0`; in a
  full client, start a >1-player network room with single-player targeting
  enabled, target an opponent, and confirm the `> Nick` label appears above the
  shooter.
- **BUG-044 (Medium, confirmed, runtime-reproduced; new in fix round 1;
  same missing-asset crash class as BUG-001, cross-linked not duplicated):**
  the single-player panel dereferences unchecked `IMG_Load` results.
  *Evidence and causal path.* `MainMenu`'s constructor loop stores
  `activeSPButtons[i] = IMG_Load(activePath.c_str());` (`mainmenu.cpp:124`)
  with no null check and no error log. `SPPanelRender` then dereferences the
  array twice: unconditionally at `mainmenu_panels.cpp:198`
  (`SDL_CreateSurface(activeSPButtons[0]->w, activeSPButtons[0]->h,
  SURF_FORMAT)`, executed the first time the SP panel is drawn) and at `:213`,
  which passes `activeSPButtons[i]` as `orig` into production `overlook_`,
  whose first statement is
  `SDL_GetPixelFormatDetails(orig->format)` (`shaderstuff.cpp:1485`).
  `activeSPButtons` occurs exactly four times in the tree (`mainmenu.h:128`,
  `mainmenu.cpp:124`, `mainmenu_panels.cpp:198`, `:213`), so no guard exists
  anywhere on the path.
  *Distinction from BUG-001.* BUG-001 is the `TextureEx` failure ordering in
  `shaderstuff.h`; this is a separate owner (`MainMenu`/`mainmenu_panels.cpp`)
  with its own unguarded load and its own two dereference sites. They share the
  remediation theme recorded under IMP-010.
  *User impact.* A missing, unreadable, or corrupt
  `share/gfx/menu/txt_*_text.png` crashes the client the first time the
  single-player panel is opened, instead of degrading to a text-only button.
  Because the loads happen in the constructor and the crash happens on panel
  open, the failure is also far from its cause.
  *Reproduction.* Harness `overlooknull` drove production
  `overlook_(dest, nullptr, 0, 149)`: UBSan reported
  `shaderstuff.cpp:1485:41: runtime error: member access within null pointer of
  type 'SDL_Surface'`, followed by `AddressSanitizer: SEGV on unknown address
  0x000000000004` with `#0 overlook_ shaderstuff.cpp:1485` (exit 134). The
  `:198` half executes first in production and was not linkable in isolation
  (it needs full `MainMenu` construction); it rests on the static chain, which
  is complete — an unconditional `->w` on a never-checked pointer.
  *Proposed correction.* Null-check each `IMG_Load`/`IMG_LoadTexture` in the
  `SP_OPT` loop, log the failing path, and either abort with a clear asset
  error or skip the overlook effect for that entry; guard
  `mainmenu_panels.cpp:198` on `activeSPButtons[0] != nullptr`.
  *Verification strategy.* Run the client with one `txt_*_text.png` renamed and
  confirm the SP panel opens with a degraded entry and a logged asset error
  instead of crashing.
- **IMP-007 (improvement, confirmed and extended):** `TTFText`'s copy
  assignment returns `*this` without copying or transferring any member, its
  copy constructor silently resets to empty, and `curText` retains a borrowed
  pointer with no user (`Text()` has zero callers). Extended to the shared
  ownership-cleanup family: the `Dispose()` methods of `AudioMixer`,
  `TransitionManager` (never invoked), `HighscoreManager`, and `GameSettings`
  end in `this->~T()` while leaving `ptrInstance` dangling, so any
  post-Dispose `Instance()` returns a destroyed object. Current call order
  never does so (verified), keeping this improvement-grade. 200 sanitized
  lifecycle iterations support the current-behavior safety.
- **IMP-010 (improvement, confirmed; cross-owner disposition complete):**
  allocation/load failure policy is inconsistent by design across owners —
  server: unchecked/`exit`-style mixes proven in Task 3 (`log.c`, `stats.c`);
  render: `fb__out_of_memory` abort for most effect buffers but unchecked
  `precalc_cos`/`precalc_sin` (`shaderstuff.cpp:1221-1226`), `exit(1)` for
  `plasma.raw`, warn-and-crash for `TextureEx` (the defect-grade instance is
  BUG-001), warn-and-continue for `IMG_LoadTexture` sites, silent-null for
  `TTFText`; audio: log-and-degrade with permanent negative caching.
  The Step 1 ownership table records each owner's policy; recommendation is a
  single explicit policy (abort with message, or degrade with placeholder).
- **IMP-013 (improvement, new, ASan-demonstrated; attribution corrected in fix
  round 1):** the clamps guarding the pixel helpers use upper bounds `s->w`/
  `s->h` instead of `w-1`/`h-1`. `get_pixel` applies them internally
  (`shaderstuff.cpp:49`); `set_pixel` (`shaderstuff.cpp:41-45`) has **no clamp
  at all** — the equivalent off-by-one clamps sit at its two call sites,
  `shaderstuff.cpp:488` and `:1155`. ASan proved the resulting one-past-the-end read
  on the production object. Unreachable with shipped assets (guards ≤ `w-2`;
  measured mask containment) but becomes an out-of-bounds *write* through
  `points_`/`set_pixel` if the mask asset ever gains a white border pixel.
  Same family: the tight-pitch/32-bpp indexing assumption should be asserted,
  and `myLockSurface`'s unbounded retry loop bounded.
- **IMP-012 (improvement, confirmed in Task 6; extended in fix round 1, no new
  ID):** `FrozenBubble::menuText` (`frozenbubble.h:96`) is a `TTFText` member
  with **zero references anywhere outside its declaration** — `grep -rn
  'menuText' src/` returns exactly one line. It is never font-loaded, never
  updated, and never rendered; it costs one default-constructed `TTFText` per
  process and, like the other IMP-012 items, misleads readers into believing a
  menu text overlay exists. IMP-012's existing scope is "remove or wire up the
  menu's unreachable code", and it already enumerates unused members
  (`selectedGameIndex`, `controllerInputs[5]`); `menuText` is the same class of
  item in the same UI layer, so it is recorded there rather than consuming a
  new ID. (`BubbleGame::targetingText` is *not* filed here — it is used, which
  is precisely why it is a defect and not dead code.)
- **BUG-034 (cross-link, no new ID):** the audio-side lifetime half assigned
  to Task 7 closes as: the defect is entirely in `FrozenBubble`'s
  early-return construction (Task 6 reproduction at `frozenbubble.cpp:228`);
  a constructed `AudioMixer` is failure-safe at every public entry point
  (static guard trace plus sanitized failure-mode stress).
- **SEC-007 / IMP-005 / IMP-006 / IMP-008 / IMP-009 (slices):** Task 7
  documents the render-side consumption sites of SEC-007
  (`bubblegame_render.cpp:87,451,482`), closes IMP-005's render slice
  (`TextureEx`/`TTFText` uninitialized members) **with the fix-round
  correction**: the original closure's claim of "no reachable
  use-before-initialization" was wrong — `TTFText::coords` is reachable and
  functionally consequential through `targetingText`, now promoted to BUG-043.
  The remaining members of that family (`TextureEx::sfc/tex/rects`,
  `TTFText::curText`/`forecolor`/`backcolor` on font-loaded instances) still
  have no reachable use-before-initialization, so IMP-005 stands as a confirmed
  improvement with one promoted instance. Task 7 also closes the render slices of
  IMP-006/IMP-008 (numeric/cast/style families reviewed, none promoted), and
  adds to confirmed IMP-009 the dead effect helpers, the per-frame text-churn
  and per-frame `UpdatePlayerNameWinText` cost, and `PlayMusic`'s 600 ms
  blocking delay.

## Dismissed candidates

- Cppcheck's two dangling-temporary errors at
  `mainmenu_netpanel.cpp:1000,1081` are false positives: each conditional
  `std::string` temporary is bound to a block-scoped `const std::string&`,
  which extends the temporary's lifetime through the subsequent `snprintf`.
  (Retained from Task 2.)
- Clang's uninitialized-alpha report at `shaderstuff.cpp:764`
  (`CallAndMessage`) — final dismissal after the Task 7 independent bounds
  review: the immediately preceding 4×4 loop iterates exactly 16 times and
  `SDL_GetRGBA` fills every `a_[i]` before `transform_cubic` reads them.
  (The neighboring `CLAMP` misuse at line 753 clamps source reads with dest
  dimensions, but `rotate_bicubic_` has no caller — dead code, IMP-009.)
- The 20 `clang-analyzer-optin.core.EnumCastOutOfRange` records in scoped
  files: the render-file instances are `TTF_HorizontalAlignment` casts fed
  only by `TTF_HORIZONTAL_ALIGN_*` constants and the virtual-scancode casts
  already dispositioned by Task 6 (BUG-028/BUG-035/BUG-036); no new
  out-of-range source exists in render code.
- `get_pixel` out-of-bounds reachability through `points_`: dismissed for
  shipped assets — the measured `fblogo-mask.png` border contains zero white
  pixels, so the walk cannot reach `x == w`/`y == h`; the arithmetic defect
  itself is recorded as IMP-013 with the ASan demonstration.
- `draw_line_` degenerate 0/0 NaN slope and `blacken_`'s fixed-`XRES` row
  clears on arbitrary surfaces: both functions have no callers (grep);
  unreachable, recorded under IMP-009 dead code.
- Shutdown-order use-after-free suspicion (renderer destroyed before
  texture-owning subsystems): dismissed — `HighscoreManager::Dispose` touches
  only files and its font, no `SDL_DestroyTexture` executes after
  `SDL_DestroyRenderer`, and SDL3 frees renderer-owned textures with the
  renderer.
- `RenderPaused`'s `pausePenguin[pauseFrame]` and stick-animation
  `imgBubbleStick[stickAnimFrame]` indexing: bounded — `pauseFrame` wraps to
  12 upon reaching 34 against a 35-entry array; `stickAnimFrame` renders at
  most `BUBBLE_STICKFC` (7) against 8-entry arrays.

## Coverage

All Task 7-owned rows in [FILE_COVERAGE.md](../FILE_COVERAGE.md) now carry
final dispositions: `shaderstuff.{cpp,h}`, `transitionmanager.{cpp,h}`,
`ttftext.{cpp,h}`, `audiomixer.{cpp,h}`, `sdl3_compat.h` (reviewed: trivial
correct int→float rect conversion; no finding) — complete;
`bubblegame_render.cpp` render/resource slice complete;
`frozenbubble.cpp`/`frozenbubble.h` render slice complete (platform slice
remains Task 8, plus the fix-round IMP-012 `menuText` note);
`mainmenu_panels.cpp` render/effect slice complete **with defect** — fix
round 1 corrected its disposition, which had recorded the overlook lifecycle as
correct without challenging the unchecked `activeSPButtons` loads it
dereferences (BUG-044); `server/log.c` and `server/stats.c` Task 7 allocation
boundary closed under IMP-010. Supporting resource-creation sites in
`bubblegame.cpp`, `bubblegame.h`, `mainmenu.cpp`, `menubutton.cpp`,
`highscoremanager.cpp` were re-examined only for ownership; their existing
Task 4-6 dispositions stand, now annotated with BUG-042 where applicable and
with BUG-043/BUG-044 origin sites (`bubblegame.h:535`, `mainmenu.cpp:124`).

## Limitations

- Apple ASan cannot detect leaks on this host; leak conclusions rest on the
  Step 1 ownership table, grep-verified destroy-site absence, and RSS
  measurements — not on leak-checker output.
- All rendering ran on the dummy video driver's software renderer. No Metal/
  GPU renderer, real window, fullscreen toggle, or live resize was executed;
  fullscreen/resize behavior is reasoned from SDL3 logical-presentation
  semantics only.
- No full-client menu↔game navigation was driven: BUG-042 is a complete
  static causal proof, not a runtime reproduction, because exercising
  `NewGame` repeatedly requires the graphical `FrozenBubble`/`MainMenu`
  construction chain.
- BUG-044's first dereference (`mainmenu_panels.cpp:198`) could not be linked
  in isolation — it lives in a TU that needs full `MainMenu` construction — so
  only its second dereference (`mainmenu_panels.cpp:213` → production
  `overlook_`) was reproduced; the `:198` half rests on the static chain.
  Likewise BUG-043 was reproduced at the `TTFText` boundary with poisoned
  storage standing in for the heap-allocated `BubbleGame`; no full-client run
  of a multiplayer targeting scenario was performed.
- `effect()` selects its animation via unseeded `rand()`; the six sanitized
  transitions covered the sequence that selection produced, not provably all
  five effect families.
- RSS is an indirect leak instrument; one inter-transition dip (allocator page
  return) was observed and recorded. The isolated per-frame measurement
  (exact 1.2 MB/frame over 100 frames) carries the quantitative claim.
- WASM and Android render/audio paths (including the WASM early-returns in
  `DoSnipIn`/`TakeSnipOut`/`PlayMusic`) were reviewed statically only; they
  remain Task 8 scope.
- The audio stress ran against the dummy audio backend; real-device timing,
  underruns, and hot-unplug behavior were not tested. No security-specific
  runtime testing was performed anywhere in Task 7 (user scope restriction);
  omitted checks are limitations, not passes.

## Gate conclusion

Complete, as corrected by fix round 1. Every scoped file has a final
disposition; BUG-001 is confirmed with two production-code sanitizer
reproductions (quantity corrected to 2 leaked rects per `InitCandy`, and the
two orderings' differing production reachability now stated); the inherited
IMP-007 and IMP-010 are confirmed as improvements with their cross-owner
dispositions finished; IMP-012 is extended with the dead `FrozenBubble::menuText`
member; BUG-034's audio-side lifetime question is closed by cross-link; new
findings BUG-041 (runtime-reproduced transition texture leak, trigger set
corrected to game start and round reload only), BUG-042 (static-proven
per-match texture reload leak), BUG-043 (reproduced: the multiplayer targeting
indicator can never render), BUG-044 (reproduced: unchecked `activeSPButtons`
loads dereferenced in `SPPanelRender`), and IMP-013 (ASan-demonstrated
pixel-helper bounds defect, unreachable with shipped assets) are registered;
the IMP-005 render-slice closure was reopened, corrected, and re-closed with
one promoted instance. No candidate remains open. Repeated-lifecycle scenarios
and their costs are recorded for Task 10's integration matrix.

**Fix-round provenance.** Independent review of the original Task 7 commit
(`d9597304`) raised five substantive findings and four minor ones. All nine
were verified against production source and accepted; none were disputed. Four
were factual corrections to claims this gate had made (BUG-001 leak quantity,
BUG-041 trigger set, the pixel-format claim that contradicted this gate's own
`formats.log`, and the `hurryTexture` load-site count), two were closures that
had missed a real defect (IMP-005's render slice → BUG-043; the
`mainmenu_panels.cpp` coverage row → BUG-044), and three were completeness
items (the exhaustive ownership table's missing `TTFText` entries, the
`get_pixel`/`set_pixel` clamp attribution, and BUG-001's ordering
reachability). Two new harness subcommands (`nofont`, `overlooknull`) were
added and both new defects reproduced against unchanged production objects.

Next gate: Task 8 (platform integration).
