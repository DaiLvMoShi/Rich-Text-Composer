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
		.SetTooltipText(LOCTEXT("TabTooltip", "Pick a RichTextStyleRow DataTable, assign styles to text and output markup"))
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

	// Main menu: Window -> Rich Text Composer
	if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))
	{
		FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("WindowLayout"));
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			TEXT("OpenRichTextComposer"),
			LOCTEXT("WindowMenuEntry", "Rich Text Composer"),
			LOCTEXT("WindowMenuEntryTip", "Open the rich text style composer (outputs RichTextBlock markup)"),
			ComposerIcon,
			FUIAction(FExecuteAction::CreateStatic(&FRichTextComposerModule::OpenComposerTab))));
	}

	// Level editor toolbar button
	if (UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar")))
	{
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection(TEXT("RichTextComposerSection"));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			TEXT("RichTextComposerButton"),
			FUIAction(FExecuteAction::CreateStatic(&FRichTextComposerModule::OpenComposerTab)),
			LOCTEXT("ToolbarButton", "Rich Text"),
			LOCTEXT("ToolbarButtonTip", "Rich Text Composer: assign rich text styles visually and output markup"),
			ComposerIcon));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRichTextComposerModule, RichTextComposer)
