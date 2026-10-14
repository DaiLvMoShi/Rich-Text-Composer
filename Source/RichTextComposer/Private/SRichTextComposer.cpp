#include "SRichTextComposer.h"

#include "Components/RichTextBlock.h" // FRichTextStyleRow
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Framework/Text/TextLayout.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/DataTable.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/ConfigCacheIni.h"
#include "RichTextComposerVersionCompat.h" // multi-version support (5.2 - 5.8)
#include "CoreGlobals.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "RichTextComposer"

namespace
{
	/** Loads a DataTable from asset data (prefers the loaded object, otherwise loads by path). */
	UDataTable* LoadTableFromAssetData(const FAssetData& AssetData)
	{
		if (UObject* Asset = AssetData.GetAsset())
		{
			return Cast<UDataTable>(Asset);
		}
		return Cast<UDataTable>(AssetData.ToSoftObjectPath().TryLoad());
	}

	/** Expected value of the RowStructure asset registry tag. */
	FString GetWantedRowStructTagValue()
	{
		return FRichTextStyleRow::StaticStruct()->GetPathName();
	}
}

//////////////////////////////////////////////////////////////////////////
// Construction
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::Construct(const FArguments& InArgs)
{
	PanelBrush = MakeShareable(new FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.025f, 0.55f)));

	// Editor background: load the saved setting (near black by default, customizable)
	LoadEditorBackgroundColor();
	EditorBrush = MakeShareable(new FSlateColorBrush(EditorBackgroundColor));

	// Editor default style for untagged text (its color follows the background brightness so it stays readable)
	EditorTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
	EditorTextStyle.SetFontSize(14.0f);
	EditorTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.92f, 0.92f)));
	UpdateDefaultTextColorForBackground();

	OutputTextStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));
	OutputTextStyle.SetFontSize(11.0f);
	OutputTextStyle.SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.88f, 0.70f)));

	EditorStyleSet = MakeShareable(new FSlateStyleSet(TEXT("RichTextComposerEditor")));
	EditorStyleSet->SetParentStyleName(FAppStyle::GetAppStyleSetName());

	MarkupMarshaller = MakeShareable(new FStyleMarkupLayoutMarshaller());
	MarkupMarshaller->SetStyles(EditorStyleSet, EditorTextStyle);

	Engine.SetPlainText(FString());
	StatusMessage = LOCTEXT("InitialStatus", "Pick a style table -> edit text -> select text and click a style on the left");

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── Style table row ─────────────────────────────────
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(6.0f, 6.0f, 6.0f, 3.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TableLabel", "Style Table"))
				.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.45f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(TableComboBox, SComboBox<TSharedPtr<FAssetData>>)
				.OptionsSource(&TableItems)
				.OnGenerateWidget(this, &SRichTextComposer::MakeTableComboBoxEntry)
				.OnSelectionChanged(this, &SRichTextComposer::HandleTableSelected)
				.ToolTipText(LOCTEXT("TableComboTip", "Lists only DataTables whose Row Structure is RichTextStyleRow"))
				[
					SNew(STextBlock)
					.Text(this, &SRichTextComposer::GetSelectedTableText)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f))
			[
				SNew(SButton)
				.OnClicked(this, &SRichTextComposer::HandleRefreshTablesClicked)
				.ToolTipText(LOCTEXT("RefreshTip", "Rescan the project for RichTextStyleRow DataTables"))
				[
					SNew(STextBlock).Text(LOCTEXT("Refresh", "Refresh"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SAssignNew(StatusTextBlock, STextBlock)
				.Text(StatusMessage)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 9))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.72f, 0.82f)))
				.AutoWrapText(true)
			]
		]

		// ── Body ─────────────────────────────────────────
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(6.0f, 3.0f, 6.0f, 6.0f))
		[
			SNew(SHorizontalBox)

			// Left: style list
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
			[
				SNew(SBox)
				.WidthOverride(300.0f)
				[
					SNew(SBorder)
					.BorderImage(PanelBrush.Get())
					.Padding(4.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("StylesLabel", "Styles (click to apply to the selection)"))
							.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(StyleListView, SListView<TSharedPtr<FRichTextComposerStyleItem>>)
							.ListItemsSource(&StyleItems)
							.OnGenerateRow(this, &SRichTextComposer::OnGenerateStyleRow)
							.OnSelectionChanged(this, &SRichTextComposer::HandleStyleSelectionChanged)
							.SelectionMode(ESelectionMode::Single)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleClearStyleClicked)
								.ToolTipText(LOCTEXT("ClearStyleTip", "Clear the style of the selected text"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearStyle", "Clear Style"))
								]
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleClearAllStylesClicked)
								.ToolTipText(LOCTEXT("ClearAllStylesTip", "Clear all styles in the whole text"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearAllStyles", "Clear All Styles"))
								]
							]
						]
					]
				]
			]

			// Right: WYSIWYG editor + output
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				// Editor (editable + live style rendering)
				// The header and background controls stay on the panel color; the background only affects the text canvas below
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f, 2.0f, 2.0f, 0.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("EditorLabel", "Editor (WYSIWYG: edit directly, styles render live)"))
								.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
							[
								SAssignNew(CaretStyleBlock, STextBlock)
								.Text(CaretStyleText)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.65f, 0.8f, 0.65f)))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(STextBlock)
								.Text(this, &SRichTextComposer::GetTextStatsText)
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
							]
						]

						// Background: presets + custom color picker (written to the editor config so it persists)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f, 0.0f, 2.0f, 4.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.0f, 0.0f, 6.0f, 0.0f))
							[
								SNew(STextBlock)
								.Text(LOCTEXT("BackgroundLabel", "Background"))
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
								.ColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.60f, 0.60f)))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(1.0f, 0.0f))
							[
								SNew(SButton)
								.ContentPadding(1.0f)
								.OnClicked(FOnClicked::CreateSP(this, &SRichTextComposer::HandleBackgroundPresetClicked, FLinearColor(0.02f, 0.02f, 0.02f)))
								.ToolTipText(LOCTEXT("BgDarkBlack", "Near Black"))
								[
									SNew(SColorBlock).Color(FLinearColor(0.02f, 0.02f, 0.02f)).Size(FVector2D(16.0f, 12.0f)).UseSRGB(false)
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(1.0f, 0.0f))
							[
								SNew(SButton)
								.ContentPadding(1.0f)
								.OnClicked(FOnClicked::CreateSP(this, &SRichTextComposer::HandleBackgroundPresetClicked, FLinearColor(0.12f, 0.12f, 0.13f)))
								.ToolTipText(LOCTEXT("BgDarkGray", "Dark Gray"))
								[
									SNew(SColorBlock).Color(FLinearColor(0.12f, 0.12f, 0.13f)).Size(FVector2D(16.0f, 12.0f)).UseSRGB(false)
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(1.0f, 0.0f))
							[
								SNew(SButton)
								.ContentPadding(1.0f)
								.OnClicked(FOnClicked::CreateSP(this, &SRichTextComposer::HandleBackgroundPresetClicked, FLinearColor(0.35f, 0.35f, 0.36f)))
								.ToolTipText(LOCTEXT("BgMidGray", "Mid Gray"))
								[
									SNew(SColorBlock).Color(FLinearColor(0.35f, 0.35f, 0.36f)).Size(FVector2D(16.0f, 12.0f)).UseSRGB(false)
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(1.0f, 0.0f))
							[
								SNew(SButton)
								.ContentPadding(1.0f)
								.OnClicked(FOnClicked::CreateSP(this, &SRichTextComposer::HandleBackgroundPresetClicked, FLinearColor(0.96f, 0.96f, 0.96f)))
								.ToolTipText(LOCTEXT("BgLight", "Light"))
								[
									SNew(SColorBlock).Color(FLinearColor(0.96f, 0.96f, 0.96f)).Size(FVector2D(16.0f, 12.0f)).UseSRGB(false)
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleCustomBackgroundClicked)
								.ToolTipText(LOCTEXT("BgCustomTip", "Pick a custom editor background (remembered; light backgrounds switch the default text color to dark)"))
								[
									SNew(STextBlock).Text(LOCTEXT("BgCustom", "Custom..."))
								]
							]
						]

						// Text canvas: the background applies only here
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SBorder)
							.BorderImage(EditorBrush.Get())
							.Padding(6.0f)
							[
								SNew(SBox)
								.MinDesiredHeight(200.0f)
								[
									SAssignNew(TextEditor, SMultiLineEditableText)
									.TextStyle(&EditorTextStyle)
									.Marshaller(MarkupMarshaller)
									.HintText(LOCTEXT("TextHint", "Type here. Select text and click a style on the left; new characters inherit the style at the caret..."))
									.AutoWrapText(true)
									.AllowMultiLine(true)
									.ClearTextSelectionOnFocusLoss(false)
									.SelectAllTextWhenFocused(false)
									.OnTextChanged(this, &SRichTextComposer::HandleMarkupTextChanged)
									.OnCursorMoved(this, &SRichTextComposer::HandleCursorMoved)
								]
							]
						]
				]

				// Output
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
				[
					SNew(SBorder)
					.BorderImage(PanelBrush.Get())
					.Padding(4.0f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("OutputLabel", "Output (read-only, select all to copy)"))
								.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Dirty", "● Changes not output yet"))
								.Visibility(TAttribute<EVisibility>::CreateSP(this, &SRichTextComposer::GetOutputDirtyVisibility))
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.72f, 0.25f)))
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SBox)
							.MinDesiredHeight(56.0f)
							.MaxDesiredHeight(120.0f)
							[
								SAssignNew(OutputBox, SMultiLineEditableText)
								.TextStyle(&OutputTextStyle)
								.IsReadOnly(true)
								.AutoWrapText(true)
								.AllowMultiLine(true)
							]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(2.0f, 4.0f, 2.0f, 2.0f))
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleOutputClicked)
								.ToolTipText(LOCTEXT("OutputTip", "Generate markup text usable by the engine RichTextBlock from the current editor content"))
								[
									SNew(STextBlock).Text(LOCTEXT("OutputButton", "Output Markup"))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleImportClicked)
								.ToolTipText(LOCTEXT("ImportTip", "Read markup from the clipboard and parse it back into the editor"))
								[
									SNew(STextBlock).Text(LOCTEXT("ImportButton", "Import Markup"))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleClearTextClicked)
								.ToolTipText(LOCTEXT("ClearTextTip", "Clear the editor text and styles"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearText", "Clear Text"))
								]
							]
						]
					]
				]
			]
		]
	];

	// Init: scan style tables -> initialize the editor from markup
	RefreshTableList();
	SetEditorMarkup(CurrentMarkup);
	UpdateCaretStyleText();
}

//////////////////////////////////////////////////////////////////////////
// FGCObject
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectedTable != nullptr)
	{
#if RTC_ENGINE_OLDER_THAN(5, 7)
		// Before 5.7 only the raw pointer overload exists (5.7+ requires TObjectPtr; raw pointers are unsafe with incremental GC)
		UDataTable* TablePtr = SelectedTable;
		Collector.AddReferencedObject(TablePtr);
#else
		Collector.AddReferencedObject(SelectedTable);
#endif
	}
}

FString SRichTextComposer::GetReferencerName() const
{
	return TEXT("SRichTextComposer");
}

//////////////////////////////////////////////////////////////////////////
// Style Table
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::RefreshTableList()
{
	TableItems.Reset();

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("DataTable")));

	TArray<FAssetData> FoundAssets;
	AssetRegistry.GetAssets(Filter, FoundAssets);

	const FString WantedRowStruct = GetWantedRowStructTagValue();
	for (const FAssetData& Asset : FoundAssets)
	{
		FString RowStructValue;
		if (Asset.GetTagValue(TEXT("RowStructure"), RowStructValue) && RowStructValue == WantedRowStruct)
		{
			TableItems.Add(MakeShared<FAssetData>(Asset));
		}
	}

	TableItems.Sort([](const TSharedPtr<FAssetData>& A, const TSharedPtr<FAssetData>& B)
	{
		return A->AssetName.LexicalLess(B->AssetName);
	});

	if (TableComboBox.IsValid())
	{
		TableComboBox->RefreshOptions();
	}

	// Keep the current selection if possible, otherwise select the first table
	TSharedPtr<FAssetData> ToSelect;
	if (SelectedTable != nullptr)
	{
		const FSoftObjectPath SelectedPath(SelectedTable);
		for (const TSharedPtr<FAssetData>& Item : TableItems)
		{
			if (Item.IsValid() && Item->ToSoftObjectPath() == SelectedPath)
			{
				ToSelect = Item;
				break;
			}
		}
	}
	if (!ToSelect.IsValid() && TableItems.Num() > 0)
	{
		ToSelect = TableItems[0];
	}

	if (ToSelect.IsValid())
	{
		if (TableComboBox.IsValid())
		{
			TableComboBox->SetSelectedItem(ToSelect);
		}
		HandleTableSelected(ToSelect, ESelectInfo::Direct);
	}
	else
	{
		SelectedTable = nullptr;
		RebuildStyleEntries();
		SetStatus(LOCTEXT("NoTables", "No DataTable with Row Structure = RichTextStyleRow was found. Create one: Content Browser -> right click -> Miscellaneous -> Data Table -> choose RichTextStyleRow"));
	}
}

TSharedRef<SWidget> SRichTextComposer::MakeTableComboBoxEntry(TSharedPtr<FAssetData> InItem)
{
	return SNew(STextBlock)
		.Text(InItem.IsValid() ? FText::FromName(InItem->AssetName) : FText::GetEmpty())
		.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10));
}

FText SRichTextComposer::GetSelectedTableText() const
{
	if (SelectedTable != nullptr)
	{
		return FText::FromString(SelectedTable->GetName());
	}
	return (TableItems.Num() > 0)
		? LOCTEXT("SelectTable", "Select a DataTable...")
		: LOCTEXT("NoTableFound", "No RichTextStyleRow DataTable found");
}

void SRichTextComposer::HandleTableSelected(TSharedPtr<FAssetData> InItem, ESelectInfo::Type SelectInfo)
{
	if (!InItem.IsValid())
	{
		SelectedTable = nullptr;
		RebuildStyleEntries();
		return;
	}

	UDataTable* Table = LoadTableFromAssetData(*InItem);
	if (Table == nullptr)
	{
		SetStatus(LOCTEXT("TableLoadFailed", "Failed to load the DataTable"));
		return;
	}

	if (Table->GetRowStruct() != FRichTextStyleRow::StaticStruct())
	{
		SelectedTable = nullptr;
		RebuildStyleEntries();
		SetStatus(FText::Format(LOCTEXT("TableWrongStruct", "\"{0}\" is not a RichTextStyleRow DataTable and cannot be used"), FText::FromString(Table->GetName())));
		return;
	}

	SelectedTable = Table;
	RebuildStyleEntries();
}

bool SRichTextComposer::ValidateSelectedTable()
{
	if (SelectedTable == nullptr)
	{
		return false;
	}

	if (SelectedTable->GetRowStruct() != FRichTextStyleRow::StaticStruct())
	{
		SelectedTable = nullptr;
		return false;
	}

	return true;
}

void SRichTextComposer::RebuildStyleEntries()
{
	StyleItems.Reset();
	KnownStyles.Reset();

	if (ValidateSelectedTable())
	{
		TArray<FName> RowNames = SelectedTable->GetRowNames();
		RowNames.Sort([](const FName& A, const FName& B)
		{
			return A.LexicalLess(B);
		});

		for (const FName& RowName : RowNames)
		{
			const FRichTextStyleRow* Row = SelectedTable->FindRow<FRichTextStyleRow>(RowName, TEXT("RichTextComposer"), false);
			if (Row == nullptr)
			{
				continue;
			}

			TSharedPtr<FRichTextComposerStyleItem> Item = MakeShared<FRichTextComposerStyleItem>();
			Item->RowName = RowName;
			Item->Style = Row->TextStyle;
			Item->bValidName = FRichTextMarkupRules::IsValidStyleName(RowName);

			StyleItems.Add(Item);
			KnownStyles.Add(RowName);
		}
	}

	if (StyleListView.IsValid())
	{
		StyleListView->RequestListRefresh();
	}

	RebuildEditorStyleSet();
	RecomputeUnknownStyles();
	UpdateCaretStyleText();

	if (SelectedTable != nullptr)
	{
		int32 InvalidCount = 0;
		for (const TSharedPtr<FRichTextComposerStyleItem>& Item : StyleItems)
		{
			if (Item.IsValid() && !Item->bValidName)
			{
				++InvalidCount;
			}
		}

		SetStatus(InvalidCount > 0
			? FText::Format(LOCTEXT("LoadedWithInvalid", "Loaded {0} styles; {1} row name(s) are invalid and cannot be used as markup"), FText::AsNumber(StyleItems.Num()), FText::AsNumber(InvalidCount))
			: FText::Format(LOCTEXT("Loaded", "Loaded {0} styles"), FText::AsNumber(StyleItems.Num())));
	}
	else
	{
		SetStatus(LOCTEXT("NoTable", "Select a RichTextStyleRow DataTable"));
	}
}

void SRichTextComposer::RebuildEditorStyleSet()
{
	// Row name -> FTextBlockStyle: the editor renders with this, matching runtime RichTextBlock resolution
	EditorStyleSet = MakeShareable(new FSlateStyleSet(TEXT("RichTextComposerEditor")));
	EditorStyleSet->SetParentStyleName(FAppStyle::GetAppStyleSetName());

	for (const TSharedPtr<FRichTextComposerStyleItem>& Item : StyleItems)
	{
		if (Item.IsValid())
		{
			EditorStyleSet->Set(Item->RowName, Item->Style);
		}
	}

	if (MarkupMarshaller.IsValid())
	{
		MarkupMarshaller->SetStyles(EditorStyleSet, EditorTextStyle);

		// Mark dirty -> the engine re-marshals with the new styles on the next tick (keeping caret and selection)
		MarkupMarshaller->MakeDirty();
		RequestEditorTick();
	}
}

//////////////////////////////////////////////////////////////////////////
// Style list
//////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SRichTextComposer::OnGenerateStyleRow(TSharedPtr<FRichTextComposerStyleItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FSlateFontInfo SampleFont = (InItem->Style.Font.Size > 0.0f) ? InItem->Style.Font : EditorTextStyle.Font;
	const FSlateColor SampleColor = InItem->bValidName
		? InItem->Style.ColorAndOpacity
		: FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f));

	const FText ToolTip = InItem->bValidName
		? FText::Format(LOCTEXT("StyleRowTip", "Style row \"{0}\"\nClick to apply it to the selected text\nOutput markup: <{0}>content</>"), FText::FromName(InItem->RowName))
		: FText::Format(LOCTEXT("StyleRowInvalidTip", "Row name \"{0}\" cannot be used as markup: {1}"), FText::FromName(InItem->RowName), FRichTextMarkupRules::GetNameRequirementText());

	return SNew(STableRow<TSharedPtr<FRichTextComposerStyleItem>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem->RowName))
				.Font(SampleFont)
				.ColorAndOpacity(SampleColor)
				.ToolTipText(ToolTip)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(GetStyleSampleFontWarning(*InItem))
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 8))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f)))
			]
		];
}

void SRichTextComposer::HandleStyleSelectionChanged(TSharedPtr<FRichTextComposerStyleItem> InItem, ESelectInfo::Type SelectInfo)
{
	if (!InItem.IsValid())
	{
		return;
	}

	ApplyStyleToSelection(InItem->RowName);

	// Clear the list selection so the same style can be clicked again for a new selection
	if (StyleListView.IsValid())
	{
		StyleListView->ClearSelection();
	}
}

FText SRichTextComposer::GetStyleSampleFontWarning(const FRichTextComposerStyleItem& Item) const
{
	if (!Item.bValidName)
	{
		return LOCTEXT("InvalidRowName", "Invalid row name");
	}
	if (Item.Style.Font.Size <= 0.0f)
	{
		return LOCTEXT("NoFontSize", "No font size");
	}
	return FText::FromString(FString::Printf(TEXT("%.0fpt"), Item.Style.Font.Size));
}

//////////////////////////////////////////////////////////////////////////
// WYSIWYG editor
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::HandleMarkupTextChanged(const FText& NewMarkup)
{
	// The editor serializes through the marshaller, so the reported text is markup
	const FString Markup = FRichTextMarkupRules::NormalizeLineEndings(NewMarkup.ToString());
	if (Markup == CurrentMarkup)
	{
		return;
	}

	CurrentMarkup = Markup;
	Engine.SetMarkupText(CurrentMarkup);
	RecomputeUnknownStyles();
	MarkOutputDirty();
	UpdateCaretStyleText();

	SetStatus(FText::Format(LOCTEXT("TextChanged", "Text updated ({0} characters); changes not output yet"), FText::AsNumber(Engine.GetPlainText().Len())));
}

void SRichTextComposer::HandleCursorMoved(const FTextLocation& InLocation)
{
	UpdateCaretStyleText();
}

void SRichTextComposer::SetEditorMarkup(const FString& InMarkup)
{
	CurrentMarkup = InMarkup;
	Engine.SetMarkupText(CurrentMarkup);
	RecomputeUnknownStyles();

	if (TextEditor.IsValid())
	{
		// SetText re-marshals on the next tick; the engine preserves the caret internally
		TextEditor->SetText(FText::FromString(CurrentMarkup));
		RequestEditorTick();
	}

	MarkOutputDirty();
	UpdateCaretStyleText();
}

void SRichTextComposer::RequestEditorTick()
{
	if (TextEditor.IsValid())
	{
		// Register a one-shot active timer so the editor processes text/style changes on the next tick
		TextEditor->RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SRichTextComposer::HandleEditorTick));
	}
}

EActiveTimerReturnType SRichTextComposer::HandleEditorTick(double InCurrentTime, float InDeltaTime)
{
	return EActiveTimerReturnType::Stop;
}

FString SRichTextComposer::GetEditorPlainText() const
{
	if (TextEditor.IsValid())
	{
		return FRichTextMarkupRules::NormalizeLineEndings(TextEditor->GetPlainText().ToString());
	}
	return Engine.GetPlainText();
}

bool SRichTextComposer::CursorLocationToPlainIndex(const FTextLocation& Location, int32& OutIndex) const
{
	OutIndex = 0;
	if (!Location.IsValid())
	{
		return false;
	}

	const FString Plain = GetEditorPlainText();
	TArray<FString> Lines;
	Plain.ParseIntoArrayLines(Lines, false);

	const int32 LineIndex = FMath::Clamp(Location.GetLineIndex(), 0, FMath::Max(Lines.Num() - 1, 0));

	int32 Index = 0;
	for (int32 i = 0; i < LineIndex; ++i)
	{
		Index += Lines[i].Len() + 1; // +1 for '\n'
	}

	const int32 LineLen = (Lines.Num() > 0) ? Lines[LineIndex].Len() : 0;
	OutIndex = Index + FMath::Clamp(Location.GetOffset(), 0, LineLen);
	return true;
}

bool SRichTextComposer::GetSelectionRange(int32& OutStart, int32& OutEnd) const
{
	OutStart = 0;
	OutEnd = 0;

	if (!TextEditor.IsValid())
	{
		return false;
	}

	const FTextSelection Selection = TextEditor->GetSelection();
	if (!CursorLocationToPlainIndex(Selection.GetBeginning(), OutStart)
		|| !CursorLocationToPlainIndex(Selection.GetEnd(), OutEnd))
	{
		return false;
	}

	return OutEnd > OutStart;
}

void SRichTextComposer::UpdateCaretStyleText()
{
	FText NewText = FText::GetEmpty();

	if (TextEditor.IsValid())
	{
		const FTextSelection Selection = TextEditor->GetSelection();
		const FTextLocation& CaretLocation = Selection.LocationB.IsValid() ? Selection.LocationB : Selection.LocationA;

		int32 Index = 0;
		if (CursorLocationToPlainIndex(CaretLocation, Index) && !Engine.GetPlainText().IsEmpty())
		{
			const FName StyleAtCaret = Engine.GetStyleAt(Index);
			NewText = StyleAtCaret.IsNone()
				? LOCTEXT("CaretDefaultStyle", "Caret: default style")
				: FText::Format(LOCTEXT("CaretStyle", "Caret style: {0}"), FText::FromName(StyleAtCaret));
		}
	}

	CaretStyleText = NewText;
	if (CaretStyleBlock.IsValid())
	{
		CaretStyleBlock->SetText(CaretStyleText);
	}
}

void SRichTextComposer::ApplyStyleToSelection(FName StyleName)
{
	int32 Start = 0;
	int32 End = 0;
	if (!GetSelectionRange(Start, End))
	{
		SetStatus(LOCTEXT("NeedSelection", "Select some text in the editor first"));
		return;
	}

	if (!StyleName.IsNone() && !FRichTextMarkupRules::IsValidStyleName(StyleName))
	{
		SetStatus(FText::Format(LOCTEXT("InvalidName", "Row name \"{0}\" cannot be used as markup: {1}"), FText::FromName(StyleName), FRichTextMarkupRules::GetNameRequirementText()));
		return;
	}

	// Rebuild the model from the editor current markup, then apply the style (styling only changes tags, never the text)
	Engine.SetMarkupText(CurrentMarkup);
	Engine.ApplyStyleToRange(Start, End, StyleName);
	SetEditorMarkup(Engine.GetMarkupText());

	SetStatus(StyleName.IsNone()
		? FText::Format(LOCTEXT("ClearedRange", "Cleared the style of characters {0}-{1}"), FText::AsNumber(Start), FText::AsNumber(End))
		: FText::Format(LOCTEXT("Applied", "Applied style \"{2}\" to characters {0}-{1}"), FText::AsNumber(Start), FText::AsNumber(End), FText::FromName(StyleName)));
}

void SRichTextComposer::RecomputeUnknownStyles()
{
	Engine.CollectUnknownStyles(KnownStyles, UnknownStyles);
}

void SRichTextComposer::MarkOutputDirty()
{
	bOutputDirty = true;
}

//////////////////////////////////////////////////////////////////////////
// Editor background
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::LoadEditorBackgroundColor()
{
	FString ColorString;
	if (GConfig != nullptr
		&& GConfig->GetString(TEXT("RichTextComposer"), TEXT("EditorBackgroundColor"), ColorString, GEditorPerProjectIni))
	{
		FLinearColor Parsed;
		if (Parsed.InitFromString(ColorString))
		{
			EditorBackgroundColor = Parsed;
		}
	}
}

void SRichTextComposer::UpdateDefaultTextColorForBackground()
{
	// Bright text on dark backgrounds, dark text on light ones, so untagged text stays readable
	const FLinearColor DefaultTextColor = (EditorBackgroundColor.GetLuminance() > 0.5f)
		? FLinearColor(0.08f, 0.08f, 0.09f)
		: FLinearColor(0.92f, 0.92f, 0.92f);

	EditorTextStyle.SetColorAndOpacity(FSlateColor(DefaultTextColor));

	if (MarkupMarshaller.IsValid())
	{
		MarkupMarshaller->SetStyles(EditorStyleSet, EditorTextStyle);
	}

	if (TextEditor.IsValid())
	{
		// The engine caches a copy of the style, so push it back once; it re-marshals and keeps the caret
		TextEditor->SetTextStyle(&EditorTextStyle);
		RequestEditorTick();
	}
}

void SRichTextComposer::SetEditorBackgroundColor(FLinearColor InColor, bool bSaveToConfig)
{
	EditorBackgroundColor = InColor;

	if (EditorBrush.IsValid())
	{
		EditorBrush->TintColor = EditorBackgroundColor;
	}

	UpdateDefaultTextColorForBackground();

	if (bSaveToConfig && GConfig != nullptr)
	{
		GConfig->SetString(TEXT("RichTextComposer"), TEXT("EditorBackgroundColor"), *EditorBackgroundColor.ToString(), GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}

	Invalidate(EInvalidateWidgetReason::Paint);

	SetStatus(FText::Format(LOCTEXT("BackgroundChanged", "Editor background updated and saved: {0}"), FText::FromString(EditorBackgroundColor.ToString())));
}

FReply SRichTextComposer::HandleBackgroundPresetClicked(FLinearColor InColor)
{
	SetEditorBackgroundColor(InColor, true);
	return FReply::Handled();
}

FReply SRichTextComposer::HandleCustomBackgroundClicked()
{
	TWeakPtr<SRichTextComposer> WeakSelf = StaticCastSharedRef<SRichTextComposer>(AsShared());

	FColorPickerArgs Args;
	Args.ParentWidget = AsShared();
	Args.bUseAlpha = false;
	Args.bOnlyRefreshOnOk = false;
	Args.bOnlyRefreshOnMouseUp = false;
	Args.InitialColor = EditorBackgroundColor;
	Args.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([WeakSelf](FLinearColor NewColor)
	{
		if (TSharedPtr<SRichTextComposer> Pinned = WeakSelf.Pin())
		{
			Pinned->SetEditorBackgroundColor(NewColor, true);
		}
	});

	OpenColorPicker(Args);
	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////
// Buttons
//////////////////////////////////////////////////////////////////////////

FReply SRichTextComposer::HandleRefreshTablesClicked()
{
	RefreshTableList();
	return FReply::Handled();
}

FReply SRichTextComposer::HandleClearStyleClicked()
{
	ApplyStyleToSelection(NAME_None);
	return FReply::Handled();
}

FReply SRichTextComposer::HandleClearAllStylesClicked()
{
	Engine.SetMarkupText(CurrentMarkup);
	Engine.ClearAllStyles();
	SetEditorMarkup(Engine.GetMarkupText());
	SetStatus(LOCTEXT("ClearedAll", "Cleared all styles"));
	return FReply::Handled();
}

FReply SRichTextComposer::HandleOutputClicked()
{
	const FString Markup = CurrentMarkup;
	if (OutputBox.IsValid())
	{
		OutputBox->SetText(FText::FromString(Markup));
	}

	bOutputDirty = false;
	SetStatus(FText::Format(LOCTEXT("OutputDone", "Generated markup ({0} characters); select it in the output box to copy"), FText::AsNumber(Markup.Len())));
	return FReply::Handled();
}

FReply SRichTextComposer::HandleClearTextClicked()
{
	Engine.SetPlainText(FString());
	SetEditorMarkup(Engine.GetMarkupText());
	SetStatus(LOCTEXT("TextCleared", "Editor cleared"));
	return FReply::Handled();
}

FReply SRichTextComposer::HandleImportClicked()
{
	FString Clipboard;
	FPlatformApplicationMisc::ClipboardPaste(Clipboard);
	if (Clipboard.TrimStartAndEnd().IsEmpty())
	{
		SetStatus(LOCTEXT("ClipboardEmpty", "Clipboard is empty. Copy some markup first, e.g. <Title>Title</>"));
		return FReply::Handled();
	}

	Engine.SetMarkupText(Clipboard);
	SetEditorMarkup(Engine.GetMarkupText());

	FString Note;
	if (Engine.HadDroppedAttributes())
	{
		Note += TEXT(" (markup attributes ignored)");
	}
	if (Engine.HadUnrecognizedMarkup())
	{
		Note += TEXT(" (unrecognized '<' fragments were kept as plain text)");
	}

	SetStatus(FText::Format(LOCTEXT("Imported", "Imported and parsed markup ({0} characters){1}"), FText::AsNumber(Engine.GetPlainText().Len()), FText::FromString(Note)));
	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////
// State
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::SetStatus(const FText& InStatus)
{
	StatusMessage = InStatus;
	if (StatusTextBlock.IsValid())
	{
		StatusTextBlock->SetText(StatusMessage);
	}
}

EVisibility SRichTextComposer::GetOutputDirtyVisibility() const
{
	return bOutputDirty ? EVisibility::Visible : EVisibility::Hidden;
}

FText SRichTextComposer::GetTextStatsText() const
{
	int32 StyledRunCount = 0;
	for (const FRichTextStyleRun& Run : Engine.GetRuns())
	{
		if (!Run.StyleName.IsNone())
		{
			++StyledRunCount;
		}
	}

	FText Stats = FText::Format(
		LOCTEXT("TextStats", "{0} chars · {1} styled runs"),
		FText::AsNumber(Engine.GetPlainText().Len()),
		FText::AsNumber(StyledRunCount));

	if (UnknownStyles.Num() > 0)
	{
		Stats = FText::Format(LOCTEXT("TextStatsUnknown", "{0} · {1} style name(s) not in the current table"), Stats, FText::AsNumber(UnknownStyles.Num()));
	}

	return Stats;
}

#undef LOCTEXT_NAMESPACE
