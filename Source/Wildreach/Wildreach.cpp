#include "Wildreach.h"
#include "WRStore.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogWildreach);

/** The game module: on shutdown, the world store commits what's pending and takes its shutdown backup (WRStore). */
class FWildreachModule : public FDefaultGameModuleImpl
{
public:
	virtual void ShutdownModule() override
	{
		WRStore::Close();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FWildreachModule, Wildreach, "Wildreach");
