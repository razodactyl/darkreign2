# Changelog

All notable changes to this project are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.459]

### Added

- **Command line build and run workflow, independent of the Visual Studio IDE.**
  The solution could only be built and launched from inside the IDE, which also
  meant the run configuration — working directory and command line arguments —
  lived in per-user `.vcxproj.user` files that are not shared.

  `scripts/dr2.ps1` drives MSBuild directly. It locates the toolchain through
  `vswhere -prerelease -latest`, requiring both MSBuild and the C++ tools, so it
  resolves to a Visual Studio install that actually carries the `v145` platform
  toolset every project here requests. That distinction matters: a machine with
  both VS 18 (2026) Insiders and the 2022 Build Tools installed must use the
  former, and the naive "latest MSBuild" lookup can pick the latter, which has no
  v145 and fails at project load.

  Builds are pinned to `Win32`. The solution declares `x64` configurations, but
  no project defines one, so an x64 build silently does nothing.

  For launching, the script reads `OutDir` and `TargetName` out of
  `appdr2/appdr2.vcxproj` rather than hardcoding a path, resolving to
  `C:\Games\Dark Reign 2\dr2_<Config>.exe`. It starts the game with that folder
  as the working directory, which is required — the game resolves its data packs
  relative to the working directory, not to the executable. The default argument
  string `--borderless -s` matches the existing debug configuration.

  Actions are `build`, `rebuild`, `clean`, `run` (build then launch) and `launch`
  (launch only), with `-Config Debug|Release`, `-Target <project>` to build a
  single project instead of the whole solution, `-GameArgs` and `-Quiet`.

  `Makefile` is a thin front end over the script for the common cases:

  | Goal | Effect |
  | --- | --- |
  | `make build` | build Release |
  | `make build-debug` | build Debug |
  | `make run` | build and launch Debug |
  | `make debug` | build and launch Debug |
  | `make release` | build and launch Release |
  | `make rebuild` / `clean` / `launch` | as named, Debug |

  `CONFIG=` and `TARGET=` override the per-goal defaults on any goal, since a
  command line variable takes precedence over a target-specific one in GNU make.
  PowerShell is invoked with `-NoProfile -ExecutionPolicy Bypass` so the goals
  behave the same regardless of the user's profile or execution policy.

  A full `Makefile` replacing MSBuild was considered and rejected. The twenty
  `.vcxproj` files already encode include paths, the forced `std.h` precompiled
  header, per-project link settings and resource compilation; restating all of
  that in make would duplicate it and drift the moment anything changed in the
  IDE. MSBuild remains the build system and these are front ends to it, so the
  IDE and the command line cannot disagree about how the game is built.

  `Makefile`, `scripts/dr2.ps1`

- **OpenGL 3.3 rendering backend, selected with `-ogl`.** The DirectX 7 renderer
  is now one implementation behind an interface rather than the only thing there
  is. DirectX remains the default and is unchanged in behaviour.

  `graphics/vid_backend.h` declares a table of function pointers covering the
  frame, render state, texture stages and storage, transforms, materials, lights
  and geometry. The public `Vid::SetXState` functions keep their `renderState`
  bookkeeping and return values; only the parts that talked to the device moved.
  No caller outside `graphics/` changed.

  On the OpenGL path there is no DirectDraw or Direct3D device at all. The
  context is created on the game's existing Win32 window through WGL, so the
  window, the message pump and DirectInput are untouched — the 2020 attempt at
  this replaced them with GLFW and broke multiplayer and game processing in the
  process. DirectDraw could not simply be left running alongside: with a GL
  context on the same window, `DirectDrawEnumerateEx` takes around eleven seconds
  and frequently never returns. `InitOGLDrivers` and `InitOGLDevice` stand in for
  `InitDD` and `InitD3D` + `InitSurfaces`, taking 239ms.

  Only one vertex format needed implementing. The game builds with
  `DODXLEANANDGRUMPY` — "do only TLVERTS" — so everything, terrain and meshes
  included, is software-transformed into pre-transformed `FVF_TLVERTEX` before it
  reaches the device. Instrumenting the draw path confirmed it: around 95 draw
  calls a frame, none of any other format.

  Textures work because the engine already handles bitmaps with no DirectDraw
  surface — that is what `bitmapNORMAL` is — so a texture here is a system-memory
  bitmap plus a texture object, and the readers, `CopyBits`, font glyph
  rendering, `pixelscale.cpp` and Bink are untouched.

  `3rdparty/glad/`, `graphics/vid_backend*.{h,cpp}`, `graphics/vid_dx.h`,
  `graphics/vid.cpp`, `graphics/bitmap.cpp`, `main/maininit.cpp`

- **Command line switches documented.** Twenty-one switches across
  `main/maininit.cpp` and `multiplayer/multiplayer.cpp` — the latter registered
  through `RegisterCmdLineHandler`, so they do not appear in the main switch
  statement and are easy to miss.

  Two are worth knowing: `-s` selects a *software Direct3D driver* rather than
  anything to do with sound or skipping, and `-safevid` is accepted but does
  nothing at all. A value must also be attached to its switch —
  `-borderless:1024x768` works, `-borderless 1024x768` does not, because the
  parser skips to the next `-` or `/` and the detached value is silently ignored.

  `README.md`

### Changed

- **DirectX is contained behind the backend interface.** `graphics/vertex.h`
  holds the vocabulary the whole game speaks — `PT_*`, `FVF_*`, `DP_*`,
  `RS_SRC_*`, `RS_DST_*` — and every one of those was literally defined as a
  Direct3D constant, so the game-wide blend vocabulary *was* the D3D enum. They
  are now self-contained. The numeric values are unchanged, so the DirectX
  backend still passes them through untranslated, but that is now a checked
  optimisation rather than a silent assumption: 35 `static_assert`s tie each
  constant to the D3D one it was derived from.

  `vid_public.h` no longer publishes `dxError`, `LOG_DXERR` or the device
  handles; those moved to `vid_dx.h`, included only by the six files that
  genuinely talk to DirectX. `ClearFlags` lost its `D3DCLEAR_*` values,
  `ViewPortDescD3D` became `Vid::ViewPort`, and the misleading `*D3D` / `*DX`
  suffixes were dropped from functions that no longer touch DirectX.

  `graphics/vertex.h`, `graphics/vid_public.h`, `graphics/vid_dx.h`

- **Specular lighting enabled.** `DOSPECULAR` had been commented out since the
  DX6-to-DX7 era and the code behind it had rotted: both `lightverts` files
  tested `light->d3d.dwFlags & D3DLIGHT_NO_SPECULAR`, but `dwFlags` was a
  `D3DLIGHT2` member that `D3DLIGHT7` does not have, and `d3d` is private. D3D7
  has no per-light specular disable, so the test is gone.

  The device's specular render state is deliberately left permanently on. The
  game draws pre-transformed vertices, and D3D takes the fog factor from the
  specular alpha channel for those, so letting the now-live `vid.specular` toggle
  drive `D3DRENDERSTATE_SPECULARENABLE` would have turned fog off along with
  specular.

  `graphics/bitmap.h`, `graphics/lightvertscamera.cpp`,
  `graphics/lightvertsmodel.cpp`, `graphics/vid_cmd_dialog.cpp`

### Fixed

- **Building light-up texture animations skipping frames.** The light sequences on
  buildings under construction, upgrade, refining or restoration only played
  correctly while fast-forward (`numpad /`) was held.

  `PollActivateTexAnim()` is called once per sim cycle (10Hz) by the task state
  machines, but `MeshEnt::SimulateTex()` runs once per *rendered* frame. The poll
  was retired by whichever render frame happened to tick the texture over, so the
  next render frame before the sim re-polled saw no poll and reset the sequence
  back to frame 0 via `SetTexFrame(0)`. In practice the lights never got past the
  first frame. Holding fast-forward made every rendered frame a sim cycle, so the
  poll was always present and the sequence played through.

  The poll is now retired on a 0.2s (two sim cycle) timeout instead, so a display
  frame can no longer cancel it. Texture time also catches up whole intervals
  rather than discarding them with `fmodf`, so the animation keeps its rate when
  the frame time exceeds the mesh's `TexTimerF`.

  `graphics/meshent.cpp`, `graphics/meshent.h`

- **Multiplayer lobby drifting out of sync the longer players wait.** Setup
  changes — mission selection, teams, sides, colours, start locations, ready and
  launch — take longer and longer to reach the other players, until the lobby is
  effectively unusable. Starting the mission clears it, because the pre-mission
  synchronisation catches everything up.

  Lobby state is not sent as free-form messages; it goes through the synchronous
  data path (`Data::Store` / `Data::Send(..., sync = TRUE)`) and comes back inside
  the server's `SessionSyncData` batches. The server emits one of those every
  `updateInterval` (500ms) from the moment the session is created, whether or not
  the game has started, and each arriving batch adds its interval to
  `MultiPlayer::Data`'s `lag` counter.

  The only thing that drained that queue was `Data::ProcessSyncData()`, called
  solely from `GameTime::Cycle()` — which does not run until the `Mission`
  runcode. The lobby runs under `Shell`, whose `Process()` was `Base::Process()`
  and nothing else, so for the entire time players sat in the lobby the queue
  only grew. Two minutes of waiting is 120,000ms of lag and 240 queued batches
  sitting in front of every subsequent update.

  `Game::RC::SimInit::Process()` — the "Synchronizing" screen — then spun
  `GameTime::Cycle(TRUE)` once per rendered frame until `GetLag()` reached zero,
  burning off the whole backlog at once and applying every deferred lobby change
  in one go. That masked the problem well enough that it was never chased.

  `Shell::Process()` now pumps the lobby via a new `MultiPlayer::ProcessLobby()`,
  which is inert until `Data::Online()` and otherwise runs the existing
  `MultiPlayer::Process()` plus a new `Data::FlushSyncData()`. The latter is a
  loop over the existing `ProcessSyncData()`, so the extraction, dispatch and lag
  bookkeeping stay in one place. Sync data is applied as it arrives, lag stays at
  zero, and the synchronizing screen reaches `SetReady()` immediately instead of
  after a multi-second catch-up. It still waits on every player's ready flag, so
  the guarantee that nobody starts simulating ahead of anyone else is unchanged.

  Note this was never hardware or latency dependent — the accumulation is driven
  by the server's wall-clock timer, so it behaved identically in 1999. Frame rate
  only affected how long the catch-up screen took to clear it, one queued batch
  per rendered frame.

  `game/gameruncodes.cpp`, `multiplayer/multiplayer.cpp`,
  `multiplayer/multiplayer.h`, `multiplayer/multiplayer_data.cpp`,
  `multiplayer/multiplayer_data.h`

- **Short-sighted units behaving as if bezerk.** Units with no order would
  wander off to attack allied units and neutral objects such as mines, with
  nothing the player did to provoke it.

  `Tactical::SearchTargetAttack()` gated the smart target search on the blind
  target timer having elapsed *and* a seeing range of more than one cell.
  Failing either condition dropped the unit into the same fallback: a random
  target found through `UnitObjIter::FilterDataUnit(subject)`. That constructor
  leaves the team `NULL` and lets `Relation` default-construct to `NEUTRAL`, and
  `Team::TestUnitRelation()` returns TRUE for a NULL team whenever the relation
  is `NEUTRAL` — so the filter matched every object on the map rather than
  restricting to enemies as every other search in the function does.

  That fallback is the Sprawler bezerker mojo (`BlindTargetTime` on
  `ExplosionObjType`), where attacking anything nearby is the whole point. It is
  not appropriate for a unit that simply cannot see far — one whose type has a
  small `SeeingRange`, whose `NightModifier` rounds it down, or whose sight has
  been reduced by an instance modifier. `UnitObjFinder::Random` applies no threat
  test either, unlike `MaxThreatMinDefense`, and `UnitObj::CanEverDamage()` is
  relation-blind, so the resulting target survived to become a `Tasks::UnitAttack`.

  The walking is what made it read as a pathfinding fault. `Weapon::Object::
  OfferTarget()` deliberately accepts a target it has no firing solution on when
  the unit is idle and able to move, on the assumption that it should close to
  range — so the unit set off across the map towards whatever it had picked.

  The two conditions are now separate. Blind targetting keeps the random
  any-relation search unchanged; a seeing range of one cell or less acquires
  nothing at all.

  `coregame_ai/tactical_process.cpp`

- **Spy renders as the wrong unit after loading a save.** Morph into a unit,
  order an infiltrate on a building, then save and load: the spy comes back
  rendered as that building.

  `SpyIdle::PostLoad()` rebuilt a morphed spy's disguise by calling
  `StateMorph()`, which takes its mesh from `target`. But `target` is only the
  subject of the current order — both `Infiltrate()` and `Morph()` reassign it
  while the spy stays morphed, so by the time the game is saved it rarely names
  the disguise any more. `SetMorphTarget()` then rewrote `morphType` and
  `morphTeam` to agree with whatever mesh had been applied, so the corruption
  was written back out and survived the next save.

  The disguise is rebuilt from `SpyObj::morphType` instead — the type actually
  morphed into, which was already being written to the save file and until now
  never read back. Doing the mesh swap directly rather than through
  `StateMorph()` also stops the state machine state that `inst.LoadState()` had
  just restored from being clobbered, and stops the `"Spy::Morph"` FX replaying
  on every load.

  If `morphType` no longer resolves, the disguise cannot be rebuilt at all, so
  the morph is dropped quietly. Leaving the spy flagged morphed and clandestine
  while rendering as a spy would be worse than losing the disguise, and a load
  is not a detection.

  `coregame_objects/spyobj.cpp`, `coregame_objects/spyobj.h`,
  `coregame_tasks/tasks_spyidle.cpp`, `coregame_tasks/tasks_spyidle.h`

- **Upgrade slots shifting on load when a slot is empty.** A building would come
  back from a save believing it held upgrade types it does not have, which then
  feeds `MissingUpgrade()` and the prerequisite checks in `CanUpgradeNow()`.

  `UnitObj::SaveState()` wrote the upgrade array sparsely — `StdSave::TypeReaper`
  emits nothing at all for a dead reaper — while `LoadState()` read it back
  positionally, one entry per slot in order. Slots are keyed to a specific
  upgrade type through `UpgradeInfo::index`, so a hole in the middle (upgrade 0
  destroyed while upgrade 1 survives) shifted every later entry down a slot.

  The slot number is now recorded alongside each reaper and used on load. Saves
  written before this remain readable through a positional fallback, which is
  wrong in exactly the cases that were already wrong and correct everywhere else.

  `coregame_objects/unitobj.cpp`

- **Sky geometry breaking up when the "Mirror" water reflection is enabled.**
  Large dark polygons appeared across the sky, most visible on the main menu
  backdrop.

  After the reflection pass, `Vid::Mirror::Stop()` fills the screen with the fog
  colour to cover anything the pass drew outside the water. That quad was built
  with `rhw = 0`. These are pre-transformed vertices, where `rhw` is 1/w, so 0
  means w = infinity — the perspective divide and everything derived from it
  become meaningless, and the quad rasterises as arbitrary dark polygons. Every
  other screen-space quad in the engine passes `rhw = 1`, which is also the
  function's own default; the mirror was the only caller passing 0.

  Found by bisection after two incorrect hypotheses: disabling the sky inside the
  mirror pass did not help, nor did disabling everything drawn into it, which
  narrowed it to the pass's own machinery.

  The OpenGL backend never showed this, incidentally — its vertex shader already
  guards the degenerate case and treats `rhw = 0` as w = 1, which is what the
  DirectX path should have done.

  `graphics/vidmirror.cpp`

- **Video settings not persisting between runs.** `Settings::Save` runs from
  `Vid::Done` and was writing correctly, but the load side never ran on the
  OpenGL path: `Settings::Load` is called from inside `InitDD`, and the saved
  mode is restored by `PickVidMode` at the end of it — both of which that path
  bypasses.

  Separately, the two backends shared one `settings.cfg`. `Settings::Load`
  validates the file against the enumerated hardware, and the two necessarily
  differ — DirectX reports two dgVoodoo drivers with 114 modes, OpenGL reports
  one with 55 — so each run rejected whatever the other had written and overwrote
  it on exit. OpenGL now uses `settings-ogl.cfg`.

  Note this is separate from `DEVELOPMENT` builds forcing `VIDMODEWINDOW` in
  `SetMode`, which is unchanged and means a saved fullscreen resolution is not
  applied in a dev build on either backend.

  `graphics/vid.cpp`, `graphics/vid_settings.cpp`

- **Music playing at full volume until the audio options page was opened.** The
  user profile applied `Sound::Vorbis::SetVolume` but never
  `Sound::Redbook::SetVolume`, while the options page sets both together, so the
  redbook path kept whatever the driver started with and jumped to the saved
  level as soon as the page was shown.

  Simply adding the call would not have worked: `Redbook::SetVolume` does nothing
  unless the driver is already open, and the profile is loaded before
  `Redbook::Claim` runs. Redbook now remembers the requested volume and applies
  it in `Claim`. `Volume()` also reports the remembered value rather than 0 when
  there is no driver, which stops a zero being written back into the profile.

  `sound/sound_redbook.cpp`, `game/user.cpp`

- **Post-build step failing the solution build.** `tools/postbuild.bat` ran
  `rh.exe -script postbuild.txt` with both names unqualified. MSBuild runs
  post-build steps from the project directory, so it failed with "rh.exe is not
  recognized" and took the build down with error MSB3073 whenever appmesh
  relinked.

  Resolving the paths would not have helped. `tools/postbuild.txt` is not a build
  script — it is Resource Hacker's own saved session state, committed by
  accident, with no `[COMMANDS]` section and an `Open=` line pointing at one
  developer's install path. The step is now dormant, matching what appdr2 already
  did, and the batch file is a documented no-op so re-enabling the event cannot
  break the build again.

  `tools/postbuild.bat`, `tools/postdll.bat`, `appmesh/appmesh.vcxproj`
