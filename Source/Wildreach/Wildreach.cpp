#include "Wildreach.h"
#include "WRStore.h"
#include "WRNet.h"
#include "Engine/ReplicationDriver.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogWildreach);

/** The game module: on shutdown, the world store commits what's pending and takes its shutdown backup (WRStore). */
class FWildreachModule : public FDefaultGameModuleImpl
{
public:
	/** A server's game net driver gets the Wildreach Replication Graph when net.graph.mode asks (WRNet; 100 players). */
	virtual void StartupModule() override
	{
		WRNet::InstallGraphFactory();
	}

	virtual void ShutdownModule() override
	{
		UReplicationDriver::CreateReplicationDriverDelegate().Unbind();
		WRStore::Close();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FWildreachModule, Wildreach, "Wildreach");
