# Dark Reign 2 — 1.459 release notes (build 145900)

Written August 2026, for whoever is looking at this years later.

This directory is the staging area for one published release. Everything a
client downloads for 1.459 either lives here or was built from what is here.

**Identity: `145900`.** That is the version with its separators removed —
1.459.0.0 becomes 145900 — and it is the number the lobby, the StyxNet server
and the update manifest all compare. It is not a build counter. See
`docs/update-system.md` section 1 in the source tree if that seems surprising,
because it has surprised people before.

---

## Read this first: 1.459 is not an ordinary release

**The shipped 1.458 client cannot update itself.** Not "does not" — cannot.
`dr2.mintsoft.dev` answers port 80 with a 308 redirect to HTTPS, and the 1.458
binary requests port 80, treats any status other than 200 as a hard failure, has
redirect-following compiled out, and has no TLS at all. Every 1.458 update check
ends in `Update::CheckFailed`. This was already true before 1.459 existed; the
in-game updater had been quietly dead in the field for some time.

So publishing 1.459 the normal way reaches nobody. It needs a server-side
transition step — `Caddyfile.transition` in this directory — which serves the
manifest, the MOTD and the 1.459 installer, and nothing else, as plain HTTP 200
on port 80. That revives the 1.458 updater just long enough for those clients to
pull 1.459.

**The 1.458 → 1.459 hop is unauthenticated, and cannot be otherwise.** That
binary has no way to verify what it downloads and cannot be changed
retroactively. From 1.459 onward every patch is fetched over TLS and checked
against a SHA-256 in the manifest, and verification fails closed. This is a
one-time exposure that ends when the transition window closes.

**Close the window when uptake levels off.** Every `UserLogin` carries the
client's build number, so the server can count how many 1.458 clients are still
connecting. When that flattens, remove the block. Nothing breaks for 1.459
clients — they never used port 80. If you are reading this and the block is
still deployed years later, it is overdue.

---

## What is in this directory

| File | Purpose |
| --- | --- |
| `dr2.exe` | The 1.459 game binary, the payload. Verify it reports build 145900 before trusting the filename. |
| `library/engine/download.cfg` | **Load-bearing.** Moves clients to port 443 and gives them the fallback host. |
| `dr2_145900.nsi` | NSIS script. Ships `library` and `dr2.exe` only — docs, mods and music are commented out. |
| `dr2_145900.exe` | The compiled installer. This is what clients download and execute, elevated. |
| `updates.cfg` | The manifest, uploaded to `/motd/darkreign2/downloads/updates.cfg`. |
| `Caddyfile.transition` | The server-side block described above. `root * /srv/dr2` is a placeholder — set your real docroot. |

### Why `library/engine/download.cfg` matters more than it looks

```
Source("dr2.mintsoft.dev", 443, "/motd/darkreign2/");
Fallback("dr2.bytebreeze.me", 443, "/motd/darkreign2/");
```

Because the `.nsi` was narrowed to `File /r library` plus `File "dr2.exe"`, this
one config file is the only data the installer delivers. If it fails to land,
players get the new binary still pointing at port 80 with no fallback — and
since 1.459 has no plaintext path in its transport at all, their updater is dead
on arrival and they are stranded exactly as 1.458 users were. Install the
package once against a copy of a real install and confirm the file is there.

`Fallback` is new in 1.459. It is tried once, after the primary fails to answer
*or fails to resolve*, which is the case that matters: if `dr2.mintsoft.dev` is
ever switched off, a name that no longer resolves is how clients find out.
`dr2.bytebreeze.me` was not serving when this release was staged — it is a
promise to clients in the field, so that the escape hatch exists on the day it
is needed. **If you are reading this because mintsoft is gone, that host is
where clients are already looking.** Stand up `updates.cfg` and `motd.cfg` under
`/motd/darkreign2/downloads/` there, with valid TLS, and they will find it.

---

## Publishing

1. Build Release. Confirm `appdr2/version.rc` reads `FILEVERSION 1,459,0,0`.
2. Stage `dr2.exe` here. It must report build 145900 — see below.
3. Compile `dr2_145900.nsi` with NSIS.
4. From the repository root: `make manifest`

   This stamps the installer's size and SHA-256 into `updates.cfg`, after
   checking the staged payload really is build 145900 and that `CurrentVersion`
   agrees. **Re-run it every time the installer is recompiled.** It caught a
   stale hash the very first time it was used — the installer had been rebuilt
   between the hash being taken by hand and the manifest being written, and the
   published result would have been refused by every client.
5. Upload `dr2_145900.exe` and `updates.cfg` to `/motd/darkreign2/downloads/`.
6. Apply `Caddyfile.transition`.
7. Test all three paths — see below.
8. Publish a manual download link for players who never launch the old build.
   They have no other route.

---

## Traps

**The manifest has no `DefaultSource`, deliberately.** The 1.458 file had one.
It pins the host *and port* that patches are downloaded from, and the two
generations need different ports — 1.458 cannot speak TLS, 1.459 will not speak
anything else — while both read the same file from the same path. Without it,
each client uses whatever its own `download.cfg` says, which is correct for
both. Do not add it back.

**Patch entries are listed newest first, and that ordering is load-bearing.**
The client takes the first entry that applies, not the best one — the selection
loop compares against both `versionCode` and `versionData`, and since every real
entry has `versionData` of 0 the first eligible entry wins permanently. Always
prepend. Appending hands clients the *oldest* applicable patch.

**A missing or wrong `Hash` fails closed.** 1.459 clients refuse to run a patch
they cannot verify, delete it, and report `Download::PatchFailed`. This is
intentional — it cannot be turned off by omitting the field — but it means a
manifest published with a placeholder hash silently blocks every update.

**There is no chaining.** One check yields at most one patch. A client on 145400
gets the 145400→145500 step and must check again after restarting. That is what
the full "Total Patch" installer is for.

**Verify the payload, do not trust the filename.** A stale `dr2.exe` sitting in
a staging directory looks identical to a fresh one. `make manifest` refuses to
proceed if it does not report the expected build, which is the check that
catches it. To look manually:

```powershell
$v = (Get-Item dr2.exe).VersionInfo
'' + $v.FileMajorPart + $v.FileMinorPart + $v.FileBuildPart + $v.FilePrivatePart
```

---

## Testing before you announce

- **A 1.458 client.** The lobby reports a patch, downloads it over plain HTTP,
  and the installer runs on exit. This only works once the Caddy block is live.
- **A 1.459 client.** The check runs over TLS and reports no patch needed.
- **A corrupted installer.** Change one byte of the uploaded file and confirm a
  1.459 client *refuses* it and logs the mismatch instead of executing it. This
  is the test people skip, and it is the only one that proves verification is
  actually wired up rather than passing by accident.

---

## Where the rest of the knowledge is

- **`docs/update-system.md`** — how the whole mechanism works: version stamping,
  the manifest grammar, the transport, verification, and the known issues that
  are still open. Read this before changing anything here.
- **`CHANGELOG.md`, section 1.459** — the code changes in this release.

  Note: that section was written before the update system work landed, so it
  does not yet describe the move to TLS-only downloads, SHA-256 verification,
  the fallback source, or the version-stamping fixes. Those are covered in
  `docs/update-system.md`.

### What changed in the update mechanism for 1.459

Summarised here because it is the part that explains why this release needed
special handling at all:

- Downloads go through a transport backend table (`wonclient/http_backend.h`),
  with a WinHTTP implementation. TLS, certificate validation and hostname
  checking come from the operating system. There is no plaintext path.
- Patches are verified against a SHA-256 from the manifest before they are
  registered to run, and verification fails closed.
- `download.cfg` gained an optional `Fallback` source.
- Version stamping moved from a PreLink to a PreBuild event. It had been running
  *after* the resource compile, so every binary carried the previous build's
  version resource — invisible for years because the content never changed, and
  it surfaced the moment the version was actually bumped.
- `GetBuildNumber()` reads the `Build Number` string instead of rebuilding the
  number from the 16-bit `FILEVERSION` fields, which truncate.
- A lobby bug where one game from a mismatched build truncated the rest of the
  visible game list.
