#include "RichTextComposerModule.h"

#include "SRichTextComposer.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "RichTextComposer"

static const FName RichTextComposerTabName(TEXT("RichTextComposer"));

void FRichTextComposerModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		RichTextComposerTabName,
		FOnSpawnTab::CreateRaw(this, &FRichTextComposerModule::SpawnComposerTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Rich Text Composer"))
		.SetTooltipText(LOCTEXT("TabTooltip", "选择 RichTextStyleRow 数据表，为文本分配样式并输出富文本标记"))
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Color"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FRichTextComposerModule::RegisterMenus));

	UE_LOG(LogTemp, Display, TEXT("RichTextComposer: editor tool registered (toolbar button + Window menu + dockable tab)"));
}

void FRichTextComposerModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(RichTextComposerTabName);
}

void FRichTextComposerModule::OpenComposerTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(RichTextComposerTabName));
}

TSharedRef<SDockTab> FRichTextComposerModule::SpawnComposerTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SRichTextComposer)
		];
}

void FRichTextComposerModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	const FSlateIcon ComposerIcon(FAppStyle::GetAppStyleSetName(), "Icons.Color");

	// 主菜单：Window → Rich Text Composer
	if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))
	{
		FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("WindowLayout"));
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("OpenRichTextComposer"),
			LOCTEXT("WindowMenuEntry", "Rich Text Composer"),
			LOCTEXT("WindowMenuEntryTip", "打开富文本样式编排工具（输出 RichTextBlock 标记）"),
			ComposerIcon,
			FUIAction(FExecuteAction::CreateStatic(&FRichTextComposerModule::OpenComposerTab))));
	}

	// 关卡编辑器工具栏按钮
	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar")))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("RichTextComposerSection"));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			TEXT("RichTextComposerButton"),
			FUIAction(FExecuteAction::CreateStatic(&FRichTextComposerModule::OpenComposerTab)),
			LOCTEXT("ToolbarButton", "Rich Text"),
			LOCTEXT("ToolbarButtonTip", "Rich Text Composer：可视化分配富文本样式并输出标记"),
			ComposerIcon));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRichTextComposerModule, RichTextComposer)
