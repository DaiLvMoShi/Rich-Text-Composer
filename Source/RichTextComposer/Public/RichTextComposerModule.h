#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Framework/Docking/TabManager.h"

class SDockTab;

/**
 * Rich Text Composer：编辑器工具插件入口。
 * 负责注册工具栏按钮、Window 主菜单条目与可停靠的独立工具页签。
 */
class FRichTextComposerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** 打开（或聚焦）工具页签。 */
	static void OpenComposerTab();

private:
	void RegisterMenus();

	TSharedRef<SDockTab> SpawnComposerTab(const FSpawnTabArgs& Args);
};
