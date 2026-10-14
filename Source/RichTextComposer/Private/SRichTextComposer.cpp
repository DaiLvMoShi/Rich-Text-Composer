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
#include "RichTextComposerVersionCompat.h" // 多版本兼容（5.2 ~ 5.8）
#include "CoreGlobals.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "RichTextComposer"

namespace
{
	/** 从资产数据加载数据表（优先已加载对象，否则按路径加载）。 */
	UDataTable* LoadTableFromAssetData(const FAssetData& AssetData)
	{
		if (UObject* Asset = AssetData.GetAsset())
		{
			return Cast<UDataTable>(Asset);
		}
		return Cast<UDataTable>(AssetData.ToSoftObjectPath().TryLoad());
	}

	/** 数据表资产注册表里 RowStructure 标签的目标值。 */
	FString GetWantedRowStructTagValue()
	{
		return FRichTextStyleRow::StaticStruct()->GetPathName();
	}
}

//////////////////////////////////////////////////////////////////////////
// 构造
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::Construct(const FArguments& InArgs)
{
	PanelBrush = MakeShareable(new FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.025f, 0.55f)));

	// 编辑区背景色：读取上次保存设置（默认近黑，可自定义）
	LoadEditorBackgroundColor();
	EditorBrush = MakeShareable(new FSlateColorBrush(EditorBackgroundColor));

	// 编辑区默认样式：无标记文字使用它（文字颜色按背景亮度自动取反，避免看不清）
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
	StatusMessage = LOCTEXT("InitialStatus", "选择样式表 → 直接编辑文字 → 选中文字后点击左侧样式");

	ChildSlot
	[
		SNew(SVerticalBox)

		// ── 样式表选择行 ─────────────────────────────────
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
				.Text(LOCTEXT("TableLabel", "样式表"))
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
				.ToolTipText(LOCTEXT("TableComboTip", "只列出 Row Structure = RichTextStyleRow 的数据表"))
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
				.ToolTipText(LOCTEXT("RefreshTip", "重新扫描项目里所有 RichTextStyleRow 数据表"))
				[
					SNew(STextBlock).Text(LOCTEXT("Refresh", "刷新"))
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

		// ── 主体 ─────────────────────────────────────────
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(FMargin(6.0f, 3.0f, 6.0f, 6.0f))
		[
			SNew(SHorizontalBox)

			// 左：样式列表
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
							.Text(LOCTEXT("StylesLabel", "样式（单击应用到选中文字）"))
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
								.ToolTipText(LOCTEXT("ClearStyleTip", "清除选中文字的样式"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearStyle", "清除选中样式"))
								]
							]

							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(FMargin(2.0f, 0.0f, 0.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleClearAllStylesClicked)
								.ToolTipText(LOCTEXT("ClearAllStylesTip", "清除全文所有样式"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearAllStyles", "清除全部样式"))
								]
							]
						]
					]
				]
			]

			// 右：所见即所得编辑区 + 输出
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)

				// 编辑区（可编辑 + 实时样式渲染）
				// 标题行与背景色控件留在面板底色上，背景色只作用于下方文字画布
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
								.Text(LOCTEXT("EditorLabel", "编辑区（所见即所得：直接编辑，样式实时呈现）"))
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

						// 背景色：预设色块 + 自定义取色器（写入编辑器配置，下次打开仍生效）
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
								.Text(LOCTEXT("BackgroundLabel", "背景色"))
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
								.ToolTipText(LOCTEXT("BgDarkBlack", "近黑"))
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
								.ToolTipText(LOCTEXT("BgDarkGray", "深灰"))
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
								.ToolTipText(LOCTEXT("BgMidGray", "中灰"))
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
								.ToolTipText(LOCTEXT("BgLight", "浅色"))
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
								.ToolTipText(LOCTEXT("BgCustomTip", "用取色器自定义编辑区背景色（自动记住；浅色背景会自动把默认文字变成深色）"))
								[
									SNew(STextBlock).Text(LOCTEXT("BgCustom", "自定义…"))
								]
							]
						]

						// 文字画布：背景色只作用于这里
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
									.HintText(LOCTEXT("TextHint", "在此直接输入文本：选中文字后点左侧样式即可套用，输入的新字会继承所在位置的样式…"))
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

				// 输出
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
								.Text(LOCTEXT("OutputLabel", "输出（只读，可全选复制）"))
								.Font(FAppStyle::GetFontStyle(TEXT("SmallFontBold")))
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("Dirty", "● 有改动未输出"))
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
								.ToolTipText(LOCTEXT("OutputTip", "根据当前编辑区内容生成引擎 RichTextBlock 可用的标记文本"))
								[
									SNew(STextBlock).Text(LOCTEXT("OutputButton", "输出 markup"))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleImportClicked)
								.ToolTipText(LOCTEXT("ImportTip", "读取剪贴板里的标记文本，反解析回编辑区继续修改"))
								[
									SNew(STextBlock).Text(LOCTEXT("ImportButton", "从剪贴板导入标记"))
								]
							]

							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								SNew(SButton)
								.OnClicked(this, &SRichTextComposer::HandleClearTextClicked)
								.ToolTipText(LOCTEXT("ClearTextTip", "清空编辑区文本与样式"))
								[
									SNew(STextBlock).Text(LOCTEXT("ClearText", "清空文本"))
								]
							]
						]
					]
				]
			]
		]
	];

	// 初始化：扫描样式表 → 用标记文本初始化编辑区
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
		// 5.7 之前只提供裸指针重载（5.7 起改为要求 TObjectPtr，否则增量 GC 下不安全）
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
// 样式表
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

	// 尽量保持已有选择，否则默认选中第一张表
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
		SetStatus(LOCTEXT("NoTables", "没有找到 Row Structure 为 RichTextStyleRow 的数据表：内容浏览器右键 → 其他 → 数据表 → 选 RichTextStyleRow"));
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
		? LOCTEXT("SelectTable", "请选择数据表…")
		: LOCTEXT("NoTableFound", "未找到 RichTextStyleRow 数据表");
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
		SetStatus(LOCTEXT("TableLoadFailed", "数据表加载失败"));
		return;
	}

	if (Table->GetRowStruct() != FRichTextStyleRow::StaticStruct())
	{
		SelectedTable = nullptr;
		RebuildStyleEntries();
		SetStatus(FText::Format(LOCTEXT("TableWrongStruct", "「{0}」不是 RichTextStyleRow 结构的数据表，无法使用"), FText::FromString(Table->GetName())));
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
			? FText::Format(LOCTEXT("LoadedWithInvalid", "已载入 {0} 个样式，其中 {1} 个行名非法（无法用于标记）"), FText::AsNumber(StyleItems.Num()), FText::AsNumber(InvalidCount))
			: FText::Format(LOCTEXT("Loaded", "已载入 {0} 个样式"), FText::AsNumber(StyleItems.Num())));
	}
	else
	{
		SetStatus(LOCTEXT("NoTable", "请选择一张 RichTextStyleRow 数据表"));
	}
}

void SRichTextComposer::RebuildEditorStyleSet()
{
	// 行名 → FTextBlockStyle：编辑区按此渲染，与运行时 RichTextBlock 的解析机制一致
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

		// 标记为脏 → 下一次 Tick 时引擎会用新样式重新排版（并保留光标与选区）
		MarkupMarshaller->MakeDirty();
		RequestEditorTick();
	}
}

//////////////////////////////////////////////////////////////////////////
// 样式列表
//////////////////////////////////////////////////////////////////////////

TSharedRef<ITableRow> SRichTextComposer::OnGenerateStyleRow(TSharedPtr<FRichTextComposerStyleItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FSlateFontInfo SampleFont = (InItem->Style.Font.Size > 0.0f) ? InItem->Style.Font : EditorTextStyle.Font;
	const FSlateColor SampleColor = InItem->bValidName
		? InItem->Style.ColorAndOpacity
		: FSlateColor(FLinearColor(0.45f, 0.45f, 0.45f));

	const FText ToolTip = InItem->bValidName
		? FText::Format(LOCTEXT("StyleRowTip", "样式行「{0}」\n单击应用到编辑区里选中的文字\n输出标记：<{0}>内容</>"), FText::FromName(InItem->RowName))
		: FText::Format(LOCTEXT("StyleRowInvalidTip", "行名「{0}」不能用作标记：{1}"), FText::FromName(InItem->RowName), FRichTextMarkupRules::GetNameRequirementText());

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

	// 清掉列表选中态，方便再次点击同一样式应用到新的选区
	if (StyleListView.IsValid())
	{
		StyleListView->ClearSelection();
	}
}

FText SRichTextComposer::GetStyleSampleFontWarning(const FRichTextComposerStyleItem& Item) const
{
	if (!Item.bValidName)
	{
		return LOCTEXT("InvalidRowName", "行名非法");
	}
	if (Item.Style.Font.Size <= 0.0f)
	{
		return LOCTEXT("NoFontSize", "未设字号");
	}
	return FText::FromString(FString::Printf(TEXT("%.0fpt"), Item.Style.Font.Size));
}

//////////////////////////////////////////////////////////////////////////
// 所见即所得编辑区
//////////////////////////////////////////////////////////////////////////

void SRichTextComposer::HandleMarkupTextChanged(const FText& NewMarkup)
{
	// 编辑区通过 Marshaller 反向序列化，传出的是标记文本
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

	SetStatus(FText::Format(LOCTEXT("TextChanged", "文本已更新（共 {0} 字），改动尚未输出"), FText::AsNumber(Engine.GetPlainText().Len())));
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
		// SetText 会在下一次 Tick 重排版，引擎内部会保留光标位置
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
		// 注册一次性活动计时器：保证编辑区在下一次 Tick 处理文本/样式变更
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
		Index += Lines[i].Len() + 1; // +1 为 '\n'
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
				? LOCTEXT("CaretDefaultStyle", "光标处：默认样式")
				: FText::Format(LOCTEXT("CaretStyle", "光标处样式：{0}"), FText::FromName(StyleAtCaret));
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
		SetStatus(LOCTEXT("NeedSelection", "请先在编辑区里用鼠标选中要套用样式的文字"));
		return;
	}

	if (!StyleName.IsNone() && !FRichTextMarkupRules::IsValidStyleName(StyleName))
	{
		SetStatus(FText::Format(LOCTEXT("InvalidName", "行名「{0}」不能用作标记：{1}"), FText::FromName(StyleName), FRichTextMarkupRules::GetNameRequirementText()));
		return;
	}

	// 以编辑区当前标记为准重建模型，再套用样式（样式只改标记，不改动文字本身）
	Engine.SetMarkupText(CurrentMarkup);
	Engine.ApplyStyleToRange(Start, End, StyleName);
	SetEditorMarkup(Engine.GetMarkupText());

	SetStatus(StyleName.IsNone()
		? FText::Format(LOCTEXT("ClearedRange", "已清除第 {0}~{1} 个字符的样式"), FText::AsNumber(Start), FText::AsNumber(End))
		: FText::Format(LOCTEXT("Applied", "已对第 {0}~{1} 个字符应用样式「{2}」"), FText::AsNumber(Start), FText::AsNumber(End), FText::FromName(StyleName)));
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
// 编辑区背景色
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
	// 深色背景用亮字、浅色背景用深字，避免自定义背景后看不清无标记文字
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
		// 引擎会缓存样式副本，必须回灌一次；内部会重排并保留光标
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

	SetStatus(FText::Format(LOCTEXT("BackgroundChanged", "编辑区背景色已更新并保存：{0}"), FText::FromString(EditorBackgroundColor.ToString())));
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
// 按钮
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
	SetStatus(LOCTEXT("ClearedAll", "已清除全部样式"));
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
	SetStatus(FText::Format(LOCTEXT("OutputDone", "已输出标记文本（{0} 字符），可在输出框内全选复制"), FText::AsNumber(Markup.Len())));
	return FReply::Handled();
}

FReply SRichTextComposer::HandleClearTextClicked()
{
	Engine.SetPlainText(FString());
	SetEditorMarkup(Engine.GetMarkupText());
	SetStatus(LOCTEXT("TextCleared", "编辑区已清空"));
	return FReply::Handled();
}

FReply SRichTextComposer::HandleImportClicked()
{
	FString Clipboard;
	FPlatformApplicationMisc::ClipboardPaste(Clipboard);
	if (Clipboard.TrimStartAndEnd().IsEmpty())
	{
		SetStatus(LOCTEXT("ClipboardEmpty", "剪贴板为空：请先复制一段富文本标记，例如 <Title>标题</>"));
		return FReply::Handled();
	}

	Engine.SetMarkupText(Clipboard);
	SetEditorMarkup(Engine.GetMarkupText());

	FString Note;
	if (Engine.HadDroppedAttributes())
	{
		Note += TEXT("（已忽略标记上的属性）");
	}
	if (Engine.HadUnrecognizedMarkup())
	{
		Note += TEXT("（有无法识别的 '<' 片段，已按普通文字处理）");
	}

	SetStatus(FText::Format(LOCTEXT("Imported", "已导入并反解析标记文本（共 {0} 字）{1}"), FText::AsNumber(Engine.GetPlainText().Len()), FText::FromString(Note)));
	return FReply::Handled();
}

//////////////////////////////////////////////////////////////////////////
// 状态
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
		LOCTEXT("TextStats", "共 {0} 字 · {1} 处样式"),
		FText::AsNumber(Engine.GetPlainText().Len()),
		FText::AsNumber(StyledRunCount));

	if (UnknownStyles.Num() > 0)
	{
		Stats = FText::Format(LOCTEXT("TextStatsUnknown", "{0} · {1} 个样式名不在当前表中"), Stats, FText::AsNumber(UnknownStyles.Num()));
	}

	return Stats;
}

#undef LOCTEXT_NAMESPACE
