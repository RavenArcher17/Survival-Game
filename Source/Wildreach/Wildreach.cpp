#include "Wildreach.h"
#include "WRStore.h"
#include "WRNet.h"
#include "Engine/ReplicationDriver.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "WRMainMenu.h"

DEFINE_LOG_CATEGORY(LogWildreach);

/** The game module: on shutdown, the world store commits what's pending and takes its shutdown backup (WRStore). */
class FWildreachModule : public FDefaultGameModuleImpl
{
public:
	/** A server's game net driver gets the Wildreach Replication Graph when net.graph.mode asks (WRNet; 100 players). */
	virtual void StartupModule() override
	{
		WRNet::InstallGraphFactory();
		// Slate widgets held in statics go while the engine is still up, not in the C runtime's exit (SWRMainMenu::ReleaseAtExit).
		FCoreDelegates::OnEnginePreExit.AddStatic(&SWRMainMenu::ReleaseAtExit);
	}

	virtual void ShutdownModule() override
	{
		UReplicationDriver::CreateReplicationDriverDelegate().Unbind();
		WRStore::Close();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FWildreachModule, Wildreach, "Wildreach");
