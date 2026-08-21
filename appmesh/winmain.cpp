///////////////////////////////////////////////////////////////////////////////
//
// Copyright 1997-1999 Pandemic Studios, Dark Reign II
//
// Vid test application startup
//
// 1-APR-1998
//

#include "vid_public.h"
#include "main.h"
#include "meshview.h"
#include "sound.h"
#include "iface.h"
#include "dxlib.h"
#include "ptree.h"
#include "gamegod.h"

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "advapi32.lib")

#define APPLICATION_CONFIGFILE    "library\\engine\\startup.cfg"

//----------------------------------------------------------------------------


namespace Main
{
    //
    // CreateMainWindow
    //
    // Window initialization
    //
    HWND CreateMainWindow()
    {
        return CreateGameWindow("Pandemic Mesh Viewer");
    }

    //----------------------------------------------------------------------------

    //
    // Executes the config file for this application
    //
    void ExecInitialConfig()
    {
        PTree pTree;

        // Attempt to open the file
        if (!pTree.AddFile(APPLICATION_CONFIGFILE))
        {
            ERR_FATAL(("Unable to execute initial config file '%s'", APPLICATION_CONFIGFILE));
        }

        // The config file groups its scopes under StartupConfig and GameConfig,
        // so they can't be fed to the handler wholesale - the handler only knows
        // the commands *inside* those groups. dr2 has GameGod to run GameConfig
        // later on; we have no such thing, so both get run here. Without
        // GameConfig there is no data stream and no language, which kills
        // IFace::Init with "No language configured".
        FScope* fScope = pTree.GetGlobalScope()->GetFunction("StartupConfig", FALSE);

        // Startup config is not required
        if (fScope)
        {
            ProcessCmdScope(fScope);
        }

        // Game config is - it sets up the data streams, the language, and the
        // mesh viewer's own search path and default model
        ProcessCmdScope(pTree.GetGlobalScope()->GetFunction("GameConfig"));
    }
}

//----------------------------------------------------------------------------

//
// Start the App
//
void CDECL Start()
{
    // Initialize generic core systems
    // force windowed mode
    Main::CoreSystemInit();

    Vid::InitResources();

    // Initialise interface
    IFace::Init();

    Vid::InitIFace();
    Mesh::Manager::InitIFace();

    // The viewer's interface configs pull in interface_standard.cfg, which is
    // shared with the game and derives control types from the Game:: classes.
    // We never create one, but they have to be registered or DefineControlType
    // dies on the missing base class
    GameGod::RegisterControlClasses();

    // Start the message pump
    Main::MessagePump();

    GameGod::UnregisterControlClasses();

    Mesh::Manager::DoneIFace();
    Vid::DoneIFace();

    // Shutdown interface
    IFace::Done();

    // Shutdown generic core systems
    Main::CoreSystemDone();
}

//----------------------------------------------------------------------------

///////////////////////////////////////////////////////////////////////////////
//
// WinMain
//
// The Big Bahoola!
//
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR cmdLine, int)
{
    Quaternion q(PI / 4.0f, Vector(1, 0, 0));
    Matrix m(q);

    // Initialize the main system
    Main::Init(hInst, cmdLine);

    // Register application specific run codes
    Main::runCodes.Register("MeshView", MeshView::Process, MeshView::Init, MeshView::Done);

    // Set the initial run code
    Main::runCodes.Set("MeshView");

    // Run the game
    Debug::Exception::Handler(Start);

    // Shutdown main system
    Main::Done();

    return 0;
}

//----------------------------------------------------------------------------
