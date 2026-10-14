#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Docking/TabManager.h"

class SDockTab;

/**
 * Rich Text Composer: entry point of the editor tool plugin.
 * Registers the toolbar button, the Window menu entry and the dockable tool tab.
 */
class FRichTextComposerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Opens (or focuses) the tool tab. */
	static void OpenComposerTab();

private:
	void RegisterMenus();

	TSharedRef<SDockTab> SpawnComposerTab(const FSpawnTabArgs& Args);
};
