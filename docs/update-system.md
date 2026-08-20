# Dark Reign 2 update and release system

How a build gets its version number, how the running game discovers that a newer
build exists, and what it does about it.

There are two halves, joined by a single number:

- **Build time.** A PreBuild step stamps a build number into `dr2.exe`.
- **Run time.** The game fetches a manifest over TLS, compares that number
  against the manifest, and if it is behind, downloads an installer, verifies it
  against a hash, and runs it.

The same number also gates who can see and join whose games, so it is the one
value that has to be right in a release.

> **Reading this for the 1.459 release?** Start at section 10. There is a
> transition step that has to happen server-side, and a window in which 1.458
> clients cannot update at all without it.

---

## 1. The build number

### Where it comes from

Version stamping is a **PreBuild** event, one per configuration, in
`appdr2/appdr2.vcxproj` and `appmesh/appmesh.vcxproj`:

```
..\tools\version version.rc -roll=0 -include=dr2.rc -company="Pandemic Studios" -description="Dark Reign 2" -comments="RELEASE" -version=1,459
```

`tools/version.exe` (source: `apps/version/version.cpp`) regenerates
`version.rc` from scratch on every build. Nothing in `version.rc` is
hand-maintained - edits to it are overwritten on the next build.

**It has to be a PreBuild event, not a PreLink one.** MSBuild's order is
PreBuild → ClCompile/ResourceCompile → PreLink → Link, and `version.rc` is
compiled by `ResourceCompile Include="version.rc"` into `$(IntDir)version.res`.
Regenerating it in PreLink therefore lands *after* the resource has already been
compiled, so the binary carries the version file written by the **previous**
build. That was the arrangement until August 2026, and it went unnoticed for
years because the generated content never changed: with no counter file the
third field was always `0`, so last build's `1,458,0,0` and this build's
`1,458,0,0` were indistinguishable. It only surfaced the first time the version
was actually bumped, when a 1.459 build linked a 1.458 resource.

The old PreLink command also ran `rc version.rc` by hand. That wrote
`appdr2/version.res`, which is not a link input and which nothing ever consumed
- MSBuild compiles `version.rc` itself. It has been dropped.

| Project | Configuration | `-version` | `-comments` | Counter |
| --- | --- | --- | --- | --- |
| appdr2 | Release | `1,459` | `RELEASE` | `build.ver` |
| appdr2 | Debug | `1,460` | `DEVELOPMENT` | `build.dev.ver` |
| appdr2 | Debug - OpenGL | `1,460` | `DEVELOPMENT` | `build.dev.ver` |
| appmesh | all | `1,4` | `X` | `build.dev.ver` |

The build number itself is **not** on the command line. `version.exe` reads a
4-byte little-endian `U32` from a counter file, increments it (unless
`-roll=0`), writes it back, and emits it as:

- the third field of `FILEVERSION` / `PRODUCTVERSION` - `1,459,<build>,0`
- the `Build Number` string value
- the `b<build>` portion of `Build String`

### How the number is actually read back

This is the part that catches people, so it is worth being exact.

`Version::GetBuildNumber()` reads the `Build Number` string out of the running
executable's own `VERSIONINFO`. `version.exe` writes that string as the version
with its separators removed, followed by the remaining two `FILEVERSION` fields.
So the number the whole system compares is the version **flattened**, not a
counter:

| `-version` | build | `Build Number` | `GetBuildNumber()` |
| --- | --- | --- | --- |
| `1,454` | 0 | `145400` | 145400 |
| `1,458` | 0 | `145800` | 145800 |
| `1,459` | 0 | `145900` | 145900 |
| `1,4` (appmesh) | 0 | `1400` | 1400 |

That is confirmed against every shipped binary in `resources/DR2 Online/`: all
five are `1,45X,0,0`. It is also why the installers are named
"Patching to v1.458.0.0" and the manifest says `145800` - they are the same
thing written two ways.

**The counter must be zero.** It is appended to the version digits, so a
counter of 145899 makes the next build's `Build Number` read `14591459000`
rather than `145900`, and that client matches nothing and nobody. Every build
event passes `-roll=0` to hold it there, and bumping a release means changing
`-version` and nothing else.

> **This used to be read a different way.** Until August 2026 `GetBuildNumber()`
> ignored the string and rebuilt the number by pasting the four binary
> `FILEVERSION` fields together. Those fields are 16 bits each, so a non-zero
> counter was silently truncated - 145900 became 14828, and the client reported
> `1459148280`. The string is authoritative now. It carries the same value for
> every version shipped so far, so no client's identity changed, but it can no
> longer be truncated. `FILEVERSION` remains `1,459,0,0` and is what Explorer
> shows; nothing reads it.

### The counter files

Both live at the **repository root** and both are gitignored.

- **`build.ver`** is the release counter. Only `appdr2 | Release` touches it.
- **`build.dev.ver`** is the development counter, used by the Debug
  configurations and by appmesh.

They are separate on purpose. They used to be one file, which meant any Debug or
appmesh build in between advanced the number the next release would be stamped
with - so a release came out as 145901 because somebody rebuilt Debug first.
With them split, `build.ver` moves only when a release is actually linked.

`version.exe` resolves the default as `..\build.ver` relative to its working
directory. MSBuild runs build events from the *project* directory (`appdr2\`,
`appmesh\`), both one level below the repository root, so it lands at
`<repo>\build.ver`. The dev configurations pass `-buildver=..\build.dev.ver` to
point somewhere else. Any path works:

```
..\tools\version version.rc -buildver="D:\release\build.ver" -version=1,459 ...
```

> **History.** The counter was hardcoded as `c:\build.ver` until August 2026,
> which made the build number a property of the machine rather than of the
> checkout - a fresh machine silently produced build `0`. It now travels with
> the working copy.

**Neither file is created automatically.** If one is missing, `version.exe`
falls back to build `0` and leaves it missing.

Both should read **0**, and every build event now passes `-roll=0` so they stay
there - see the section above for why anything else breaks the client's
identity. `build.ver` is committed-adjacent state you should rarely touch, but
if it ever drifts, put it back:

```bash
python -c "import struct;open('build.ver','wb').write(struct.pack('<I',0))"
```

Read one back:

```bash
python -c "import struct;print(struct.unpack('<I',open('build.ver','rb').read(4))[0])"
```

An absent file works just as well as a zeroed one. The reason the original
`c:\build.ver` arrangement produced correct release numbers for years is that
the file was usually missing, so `build` came out `0` by accident.

### Numbering convention

A release is identified by its version with the dots removed: v1.459 is
**145900**, v1.458 is **145800**. The trailing two digits are the third and
fourth `FILEVERSION` fields, which stay at zero, leaving room for hotfixes
between point releases if the scheme is ever extended.

So `-version=1,459` is the whole of it. It is not cosmetic - it *is* the number
the updater, the lobby and the server compare.

### Rebuilding the tool

`tools/version.exe` is a committed prebuilt binary. Editing
`apps/version/version.cpp` has **no effect** until it is rebuilt and the new
binary replaces the committed one:

```bash
msbuild apps\version\version.vcxproj -p:Configuration=Release -p:Platform=Win32
```

The project writes its output straight to `tools\version.exe`. It links against
`bin\Release\system.lib`, so build the `system` project first if that library is
stale. It is deliberately **not** a member of `dr2.sln`: `appdr2`'s PreBuild event
invokes `..\tools\version`, so relinking the tool inside a parallel solution
build would race against the projects that run it.

---

## 2. What the build number gates

| Location | Behaviour |
| --- | --- |
| `styxnet/styxnet_client.cpp:764` | client sends its build number in `UserLogin` |
| `styxnet/styxnet_server.cpp:449` | server rejects the login outright unless the number matches exactly |
| `styxnet/styxnet_explorer.cpp:191` | LAN discovery hides sessions whose `U16` version differs |
| `multiplayer/multiplayer_network.cpp:224` | `session.version = U16(GetBuildNumber())` when registering a game with MINT |
| `multiplayer/won_cmd.cpp:449` | lobby game list skips games with a different `U16` version |

Both client-side filters are wrapped in `#ifndef DEVELOPMENT`, so Debug builds
see every game regardless of version. The server-side check at
`styxnet_server.cpp:449` is **not** conditional - a mismatched client is refused
even by a development build.

One sharp edge remains: `StyxNet::Session::version` is `U16`, so 145900 is
stored as 14828. Consistent across clients, but build numbers 65536 apart
collide and would appear mutually compatible.

That server-side check is also the best signal for how many players are still on
an old build, which is what decides when the transition window in section 10 can
close.

---

## 3. Config files shipped with the game

Both live under `library\engine\` in the packed game data.

### `version.cfg`

```
Data(0);
```

Read into `versionData` - a data-only revision counter, independent of the code
build number, letting a data-only patch supersede a client without relinking.
If the file is absent, `versionData` is `0`. It is absent from this repository
and from `bin/Debug/library/engine`, so in practice `versionData` is always `0`
and the whole data-revision axis is inert.

### `download.cfg`

The bootstrap. A missing or unparseable file is `ERR_FATAL` - the game will not
start. The 1.459 version, staged at
`resources/DR2 Online/DR2 Patch 145900/library/engine/download.cfg`:

```
Source("dr2.mintsoft.dev", 443, "/motd/darkreign2/");
Fallback("dr2.bytebreeze.me", 443, "/motd/darkreign2/");
FileUpdates("downloads/updates.cfg");
FileMotd("downloads/motd.cfg");
```

- `Source(host, port, path)` - the primary. Port 443, because every download is
  TLS now; there is no plaintext path left in the transport, so a `Source` on
  port 80 fails rather than downgrading.
- `Fallback(host, port, path)` - optional. Tried once, after the primary fails
  to answer *or fails to resolve*, before the check is reported as failed. It
  exists so the update mechanism outlives the domain it was first published on:
  if `dr2.mintsoft.dev` is ever switched off, clients already in the field know
  where to look. `dr2.bytebreeze.me` is not serving yet.
- `FileUpdates(name)` / `FileMotd(name)` - appended to `path` to form the
  request, and reused verbatim as the **local** save path.

The 1.458 file, for comparison, has no `Fallback` and names port 80.

---

## 4. The transport

Downloads go through a backend table, `wonclient/http_backend.h`, in the same
shape as the graphics backend seam in `graphics/vid_backend.h`:
`HTTP::Backend` is a table of function pointers, `HTTP::backend` points at the
one in use, and `HTTP::SelectBackend` chooses it once at startup. Callers -
`MINTCLIENT::Directory::DownloadProcessor` - never name a library.

| Backend | File | Notes |
| --- | --- | --- |
| `backendWinHttp` | `wonclient/http_backend_winhttp.cpp` | the default, and currently the only one |

WinHTTP is the default because TLS, certificate chain validation and hostname
checking all come from the operating system: no CA bundle to ship with the game
and keep current as roots rotate, and no third-party crypto inside a 32-bit
`/MT` build. An OpenSSL backend over cpp-httplib can be added beside it later
without any caller changing, which is the point of the seam.

Whatever the backend, the contract is fixed:

- **TLS only.** The update mechanism executes what it downloads, so a backend
  that can be talked into plaintext is a backend that can be talked into running
  someone else's installer. The WinHTTP backend passes `WINHTTP_FLAG_SECURE`
  and never relaxes validation - no `WINHTTP_OPTION_SECURITY_FLAGS`, no ignoring
  unknown CAs or name mismatches.
- **Redirects are followed, but never off TLS.** `WINHTTP_OPTION_REDIRECT_POLICY`
  is set explicitly to `DISALLOW_HTTPS_TO_HTTP`.
- **Connect by name, not address.** The hostname is what the certificate is
  checked against and what is sent as SNI. Handing a backend a resolved address
  means handing it a certificate that cannot match.
- **A failed transfer leaves no file.** Non-200, a short body against a declared
  `Content-Length`, a write failure or a caller abort all delete the destination,
  so nothing downstream can find a partial file and assume it is complete.
- **The completion callback fires exactly once.** It disposes the caller's
  state, so a second call is a double free rather than a duplicate
  notification. The previous cpp-httplib implementation could call it twice, from
  both the status check and the progress callback.

Proxies come from `Settings::GetProxy()` when set, and from the machine's own
configuration otherwise.

---

## 5. The update check

All of this is `multiplayer/multiplayer_download.cpp`.

1. **Trigger.** The interface fires the console command
   `multiplayer.download.updates` (registered at
   `multiplayer/multiplayer_cmd.cpp:176`), which calls `Download::GetUpdates()`.
   That resets the patch and extra lists, points the update source back at the
   primary, and starts the fetch. The interface scripts that issue this, and
   that bind the target controls via `multiplayer.register.updates` and
   `multiplayer.register.download`, live in the packed game data - they are not
   in this repository.

2. **Request.** `Get()` fills the shared `downloadContext` and calls
   `Download()`, which resolves the host, creates the local directory with
   `Dir::MakeFull` (so `downloads\` is made on demand), joins the server path to
   the file name, and calls `WonIface::HTTPGet`.

3. **Transport.** `WonIface::HTTPGet` forwards to
   `MINTCLIENT::Directory::HTTPGet`, which runs one download to completion on
   its own thread through `HTTP::backend->Get` (section 4).

4. **Pump.** Progress and completion arrive as `HTTPProgressUpdate`,
   `HTTPCompleted` and `HTTPFailed`, which `multiplayer/won.cpp:175` routes into
   `Download::Message`. Progress figures land in the `Context` that
   `Controls::Download` draws: file name, transferred/total, progress bar.

5. **Parse.** On `HTTPCompleted` for a request tagged `"Updates"`, the file just
   written to `downloads/updates.cfg` is reopened as a `PTree` and its top-level
   functions dispatched: `DefaultSource`, `CurrentVersion`, `Patch`, `Extra`.

6. **Compare.**

   ```cpp
   if (versionCode < updateVersionCode || versionData < updateVersionData)
   ```

   If behind, scan `Patch` entries for one whose language matches and whose
   `versionCode` / `versionData` are at or below ours, and download it.

7. **Fall back.** If the manifest fetch fails - a transport error, or a host
   that no longer resolves - and `download.cfg` named a `Fallback`, the whole
   check is retried once against it before anything is reported as failed. A
   retry discards whatever the failed attempt managed to parse, and repoints the
   patch source, so patches come from the same host the manifest did.

### Notifications

Sent with `IFace::NOTIFY` to the control registered for each purpose.

| CRC | Name | Sent to | When |
| --- | --- | --- | --- |
| `0x96B48B0D` | `Update::PatchAvailable` | `updateCtrl` | behind, and a usable patch was found - download already started |
| `0xF9068335` | `Update::Unpatchable` | `updateCtrl` | behind, but no patch applies to this version or language |
| `0xCA4DB1B4` | `Update::NoPatch` | `updateCtrl` | already current |
| `0x7CA15267` | `Update::CheckFailed` | `updateCtrl` | manifest fetch failed, and the fallback failed too or was not configured |
| `0x37976FA8` | `Download::PatchCompleted` | `downloadCtrl` | patch downloaded **and verified** |
| `0xB2623264` | `Download::PatchFailed` | `downloadCtrl` | patch download failed, **or failed verification** |
| `0x7091B101` | `Download::Completed` | `downloadCtrl` | any other download finished |
| `0x161F1710` | `Download::Failed` | `downloadCtrl` | any other download failed |

A hash mismatch deliberately reuses `Download::PatchFailed` rather than
introducing a code of its own: the interface scripts that react to these live in
the packed game data, so a new notification would arrive somewhere with nothing
listening. The log distinguishes the two cases.

Failures are suppressed when the transfer was cancelled. The UI sends
`DownloadMsg::Halt` (`0xD70B1311`) to `Controls::Download`, which calls
`AbortDownload()` and sets `aborted`; `won.abort` additionally calls
`Download::Abort()` to cancel a pending DNS lookup.

---

## 6. The manifest

Fetched from `<Source path><FileUpdates>` - with the shipped config that is
`https://dr2.mintsoft.dev/motd/darkreign2/downloads/updates.cfg`.

Standard `PTree` syntax. Bare values are positional arguments to the function;
named entries are child functions.

```
// The newest build in existence.
CurrentVersion(145900, 0);          // <versionCode> <versionData>

// One entry per upgrade step. LIST THESE NEWEST FIRST - see below.
Patch(145800, 0, "English")         // <fromCode> <fromData> [<language>]
{
    Size(5242880);                  // informational only
    Hash("9f86d081...");            // SHA256 of the file, lowercase hex
    File("downloads/dr2_145900.exe");
}

// Optional extra content. Parsed and listed, but see section 9.
Extra("Community Map Pack")
{
    Author("Jonathan");
    Size(12345);
    File("downloads/maps.zip");
    Source("other.host", 443, "/files/");   // optional
}
```

**Field semantics**

- `Patch`'s first two arguments are the version the patch upgrades *from*, not
  the version it produces. What it produces is implied by `CurrentVersion`.
- The optional third argument is a language name, CRC-compared against
  `MultiLanguage::GetLanguage()`. It defaults to `"English"` (CRC
  `0xE493D172`). A patch whose language does not match the client is never
  offered.
- `Size` is not enforced; real progress comes from the HTTP response. Every live
  entry has always carried `Size(0)`.
- `Hash` is the SHA-256 of the file as lowercase hex. See section 7.
- `File` does double duty. It is appended to the update path to build the
  request **and** used verbatim as the local save path, so `downloads/x.exe` is
  fetched from `<updatePath>downloads/x.exe` and written to `downloads\x.exe`.

**`DefaultSource` must stay out of the manifest.** It overrides the host, port
and path a client downloads patches from, and the two generations of client
need different ports: 1.458 cannot speak TLS, 1.459 will not speak anything
else. One file is served to both (section 10), so pinning a port breaks one of
them. With no `DefaultSource`, each client uses whatever its own `download.cfg`
says, which is correct for both. The 1.458 manifest carried
`DefaultSource("dr2.mintsoft.dev", 80, "/motd/darkreign2/")`, which was the same
host and path those clients were already configured with, so removing it changes
nothing for them.

**Ordering is load-bearing.** The selection loop
(`multiplayer/multiplayer_download.cpp:381`) requires a challenger to beat the
incumbent on *both* `versionCode` **and** `versionData`:

```cpp
(!patch || ((*p)->versionCode > patch->versionCode && (*p)->versionData > patch->versionData))
```

Since every real entry has `versionData` of `0`, `0 > 0` is false and the first
eligible entry in file order wins permanently. The live manifest lists patches in
descending order, so the first eligible entry is also the newest applicable one
and the right thing happens by construction. **Add new entries at the top.**
Appending them hands clients the oldest applicable patch instead of the newest.

**No chaining.** One check yields at most one patch. A client on 145400 gets the
145400 to 145500 step, and has to check again after restarting to advance
further. This is why a full "Total Patch" installer exists as a shortcut.

---

## 7. Verifying and applying a patch

A downloaded patch is about to be executed with elevation, so it does not become
the next process until it hashes to what the manifest said it would.
`VerifyPatch` (`multiplayer/multiplayer_download.cpp`) hashes the file with
SHA-256 and compares it to the manifest's `Hash`.

It **fails closed**. An entry with no hash is refused rather than trusted, so
dropping the field - or an older manifest being served from somewhere - cannot
quietly turn verification off. On any failure the file is deleted, so a later
run cannot find it and assume it was checked, and `Download::PatchFailed` is
raised.

Hashing is `system/sha256.{h,cpp}`, a thin wrapper over the Windows CNG provider
(`bcrypt.lib`) rather than a hash of our own. `system/md5.{h,cpp}` still exists
and is used elsewhere; it is not suitable here, since MD5 collisions are
cheap enough to matter against someone who can substitute an installer.

Once verified, `Main::RegisterNextProcess(patch->file)` is called and
`Download::PatchCompleted` is sent. Nothing runs yet - the game keeps playing.
The launch happens in `Main::Done()` (`main/maininit.cpp:287`) during shutdown,
branching on the file extension:

- **`.rtp`** - sets the environment variable `patch=<file>` and
  `CreateProcess("library\patch\patch.exe")`. That is `apps/applypatch`, which
  reads the `patch` variable (`apps/applypatch/applypatch.cpp:1201`),
  `LoadLibrary("patchw32.dll")` and calls `RTPatchApply32@12` to apply a binary
  delta. This is the original scheme; `README.md` item 18 notes the patcher and
  its DLL were folded into the game data by the 1.1 patch. `library/patch` is
  empty in this checkout, so the route is not currently wired up.

- **anything else** - `ShellExecuteEx` with the verb `runas` on
  `downloads\<filename>`, i.e. run the installer with a UAC elevation prompt.
  A `TODO` at `main/maininit.cpp:342` notes that variable files should move to
  `C:\ProgramData` so the elevation would not be needed.

The live manifest points at `.exe` files, so **the mechanism in use today is the
NSIS installer route, not RTPatch.**

---

## 8. Message of the day

Same transport, separate context, so it can overlap a patch download.
`multiplayer.download.motd` calls `Download::GetMotd()`, which fetches
`FileMotd` into `downloads/motd.cfg` and parses it for `Text` entries, each
echoed to the console under the `MessageOfTheDay` type (`0x70F02901`):

```
Text("Dark Reign 2 is back!");
Text("Ensure TCP 26214 is unblocked in your router to host.");
```

A failed MOTD fetch clears the handle so the next attempt retries; unlike the
updater it raises no notification and does not use the fallback source.

---

## 9. Extras

`Extra` entries are parsed into a list reachable through
`Download::GetExtras()`, defaulting their source to the manifest's source.
**Nothing calls `GetExtras()`** - the list is populated and never read, so
extras are currently inert, and they carry no hash support. The commented-out
`hello.txt` example in the live manifest is the only known use.

`Download::CheckVersion()` is likewise a stub that unconditionally returns
`TRUE`, and has no callers.

---

## 10. Cutting the 1.459 release

### First: 1.458 clients cannot update at all right now

`dr2.mintsoft.dev` answers port 80 with `308 Permanent Redirect` to HTTPS
(Caddy). Shipped 1.458 clients cannot follow that:

- they request port 80, per their `download.cfg`;
- their download code treats any status other than 200 as a hard failure;
- `set_follow_location` is commented out in the version of
  `wonclient/Directory.cpp` they shipped with;
- and they have no TLS, so they could not follow it even if they tried.

So every 1.458 update check ends in `Update::CheckFailed`, and the MOTD fails
silently. This is the state today, before any 1.459 work - the in-game updater
is already dead in the field.

The transition therefore has a server-side step. `Caddyfile.transition`, staged
beside the release, serves the manifest, the MOTD and the 1.459 installer - and
nothing else - as plain HTTP 200 on port 80, while everything else on the host
keeps redirecting. That revives the 1.458 updater long enough for those clients
to pull 1.459.

The 1.458 to 1.459 hop is unauthenticated, and has to be: that binary has no way
to verify what it downloads and cannot be changed retroactively. From 1.459
onward every patch is TLS with a SHA-256 check, so this is a one-time exposure
that closes when the window does. Publish a manual download link as well, for
players who never launch the old build.

Close the window once the number of 1.458 logins levels off - the build number
is on every `UserLogin`, so the server can count them. Nothing breaks for 1.459
clients when it goes; they were never using port 80.

### The release itself

1. **Nothing to seed.** `appdr2 | Release` already passes `-version=1,459`, and
   `build.ver` is 0 with `-roll=0`, which is what makes the third field zero.

2. **Build Release** and confirm the number landed - `appdr2/version.rc` must
   read `FILEVERSION 1,459,0,0`. Anything else in the third field means the
   counter drifted, and the client will report a ten-digit number that matches
   nothing. The quick check on the linked binary:

   ```powershell
   $v = (Get-Item 'C:\Games\Dark Reign 2\dr2_Release.exe').VersionInfo
   '' + $v.FileMajorPart + $v.FileMinorPart + $v.FileBuildPart + $v.FilePrivatePart
   ```

   That string is exactly what `GetBuildNumber()` computes, and it must read
   `145900`.

3. **Stage the payload** under `resources/DR2 Online/DR2 Patch 145900/`. The
   config files are already there:
   - `library/engine/download.cfg` - TLS, plus the `dr2.bytebreeze.me` fallback
   - `dr2_145900.nsi` - the installer script
   - `updates.cfg` - the manifest to upload
   - `Caddyfile.transition` - the server-side window described above

   Add the built `dr2.exe` and whatever else changed under `library/`, `docs/`,
   `mods/`.

4. **Compile the installer** to `dr2_145900.exe` with NSIS.

5. **Fill in the manifest.** `updates.cfg` has `Size(0)` and `Hash("")` on the
   new entry, marked TODO. Both come from the compiled installer:

   ```powershell
   (Get-Item dr2_145900.exe).Length
   (Get-FileHash dr2_145900.exe -Algorithm SHA256).Hash.ToLower()
   ```

   An empty or wrong `Hash` fails closed - 1.459 clients refuse the download
   rather than run it - so this cannot ship as a placeholder.

6. **Upload** `dr2_145900.exe` and `updates.cfg` to
   `/motd/darkreign2/downloads/`, and apply `Caddyfile.transition`.

7. **Verify three paths.** From a 1.458 client: the lobby reports a patch,
   downloads it over plain HTTP, and runs the installer on exit. From a 1.459
   client: the check runs over TLS and reports no patch needed. And deliberately
   corrupt a byte of the uploaded installer to confirm a 1.459 client refuses it
   and logs the mismatch rather than executing it.

### Later releases

Same shape, minus the transition: bump `-version` in `appdr2.vcxproj` (Release,
and the Debug pair one ahead), leave the counters alone, build, stage, compile
the installer, **prepend** a `Patch` entry with its real `Size` and `Hash`, bump
`CurrentVersion`, upload.

### Staging layout

`resources/DR2 Online/` holds one directory per release - `DR2 Patch 145400`
through `145900`, plus `DR2 Total Patch 145800`, a complete install carrying
dgVoodoo, `mss32.dll`, `VC_redist.x86.exe` and the music tracks.

### The dormant post-build step

`tools/postbuild.bat` is a no-op and `PostBuildEventUseInBuild` is `false` on
every configuration. It is documented in place: the script it used to run,
`tools/postbuild.txt`, is not a build script at all but Resource Hacker session
state committed by accident, pointing at one developer's install path. Do not
re-enable it.

---

## 11. Known issues

| Issue | Location | Effect |
| --- | --- | --- |
| Patch selection needs `\|\|`, not `&&` | `multiplayer_download.cpp:381` | first eligible entry always wins; descending manifest order is load-bearing |
| `U16` session version | `multiplayer_network.cpp:224` | build numbers 65536 apart appear compatible |
| Uninitialised `build` on a short read | `apps/version/version.cpp:111` | an empty or truncated counter file leaves `build` uninitialised rather than falling back to 0, so a zero-byte `build.ver` stamps garbage. Only the absent case is handled |
| `-roll=0` leaves the counter file open | `apps/version/version.cpp:119` | `Close()` is inside the roll branch, so with rolling off the handle is leaked and the `File` object is reopened for `version.rc`. Harmless because the process exits immediately, but `File::Open` asserts on an already-open handle |
| Option without `=` crashes | `apps/version/version.cpp:74` | the `if (value)` guard tests the first `strtok`, not the second, so `-roll` rather than `-roll=1` passes NULL to `Utils::Strdup` |
| `delete` on a `new[]` allocation | `apps/version/version.cpp:77` | `Utils::Strdup` returns `new char[]`; the parse loop frees it with scalar `delete` |
| `versionData` unused | no shipped `version.cfg` | the data-revision axis is dead, which is also what breaks the patch-selection comparison above |
| Extras unhashed and unread | `multiplayer_download.cpp` | `GetExtras()` has no callers, and `Extra` has no `Hash` field, so the path would need both before it could be used |
| `CheckVersion()` | `multiplayer_download.cpp` | dead API |
| 1.458 updater unreachable | server config | see section 10 - needs the transition window, and closes for good once that ends |

Fixed in 1.459, listed here because older builds still show them:

| Was | Now |
| --- | --- |
| `break` instead of `continue` on a version mismatch (`won_cmd.cpp:449`) | one foreign-version game no longer truncates the rest of the lobby list |
| Plain HTTP, no verification, executed elevated | TLS only, SHA-256 checked, fails closed |
| `patches` / `extras` accumulated across checks and the chosen `patch` was never reset | both cleared at the start of every check |
| The completion callback could fire twice | fires exactly once |
| `downloadContext.handle` never cleared on failure | cleared, so a retry can start |
| `GetBuildNumber` rebuilt the number from the 16-bit `FILEVERSION` fields | it reads the `Build Number` string, which the tool now writes as the flattened identity. Same value for every version shipped so far, but no longer truncatable |
| `Sprintf` passed 80 as the size of a `char[20]` | that code is gone with the concatenation |
| No proxy configured meant the target's own address was passed as the proxy | the configured proxy is passed through as-is |

---

## 12. Not part of this system

`multiplayer/multiplayer_transfer.cpp` is peer-to-peer mission and map transfer
between players already in a session, carried over StyxNet. It shares the word
"download" in the UI but has nothing to do with versioning or patching.
`README.md` lists "Map transfers fail to initiate" as an open issue against it.
