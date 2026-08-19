@echo off
rem ---------------------------------------------------------------------------
rem Post-build step for appdr2 / appmesh.
rem
rem Callers pass:  postbuild <exe path> <description> [<config>]
rem ...which this script has never read. It used to run:
rem
rem     rh.exe -script postbuild.txt
rem
rem with both names unqualified, so it only resolved when the working directory
rem happened to be tools\. MSBuild runs post-build steps from the *project*
rem directory, so it failed with "'rh.exe' is not recognized" and took the build
rem down with it (error MSB3073).
rem
rem Resolving the paths does not help. tools\postbuild.txt is not a build script
rem - it is Resource Hacker's own saved session state that got committed, as did
rem rh.ini. It has no [COMMANDS] section, so it edits no resources, and its
rem [FILENAMES] Open= line points at one developer's install path
rem ("C:/Games/Dark Regin 2", sic). Run it and rh.exe logs "Failed!" and returns
rem non-zero - the build breaks either way, just with a different message.
rem
rem So the step is dormant: both .vcxproj files now set
rem PostBuildEventUseInBuild=false. This script stays as a no-op so that
rem re-enabling the event cannot break the build again.
rem
rem Note this is *not* where version stamping happens - that is a PreLink event
rem calling tools\version.exe, which rewrites version.rc on every build.
rem ---------------------------------------------------------------------------
exit /b 0
