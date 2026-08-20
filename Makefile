# Thin front end over the scripts/ -- MSBuild does the real work.
#
#   make build          build Release          make run       build+launch Debug
#   make build-debug    build Debug            make debug     build+launch Debug
#   make rebuild        rebuild Debug          make release   build+launch Release
#   make clean          clean Debug            make launch    launch Debug, no build
#
#   make manifest       stamp the built installer's size and SHA-256 into
#                       updates.cfg -- run after compiling the NSIS installer,
#                       before uploading. See docs/update-system.md.
#
# Override either default on any goal: make build CONFIG=Debug, make run TARGET=util
# Override the release being stamped: make manifest VERSION=146000
CONFIG ?= Debug
TARGET ?=
VERSION ?= 145900
DR2 = powershell -NoProfile -ExecutionPolicy Bypass -File scripts/dr2.ps1
REL = powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release.ps1

# Goal name -> script action, where the two differ.
ACTION = $@
build-debug:     ACTION = build
debug release:   ACTION = run

# Per-goal config defaults. A CONFIG= on the command line still wins over these.
build release:   CONFIG = Release

.PHONY: build build-debug rebuild clean run debug release launch manifest
build build-debug rebuild clean run debug release launch:
	$(DR2) $(ACTION) -Config $(CONFIG) -Target "$(TARGET)"

manifest:
	$(REL) -Version $(VERSION)
