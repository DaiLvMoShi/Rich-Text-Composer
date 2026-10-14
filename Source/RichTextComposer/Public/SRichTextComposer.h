#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/DataTable.h"
#include "UObject/GCObject.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboBox.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Text/TextLayout.h"

#include "StyleMarkupEngine.h"
#include "StyleMarkupLayoutMarshaller.h"

/** 样式列表中的一行（= 数据表的一行）。 */
struct FRichTextComposerStyleItem
{
	/** 数据表行名，同时也是输出的标记名。 */
	FName RowName;

	/** 该行的文本样式（列表样本与编辑区渲染用）。 */
	FTextBlockStyle Style;

	/** 行名是否符合引擎标记命名规则。 */
	bool bValidName = true;
};

/**
 * Rich Text Composer 工具窗口。
 *
 * 布局：样式表选择 → 样式列表 + 【所见即所得编辑区】（可编辑、且按数据表样式渲染）
 *       → 只读标记输出。
 *
 * 编辑区通过 FStyleMarkupLayoutMarshaller 让可编辑文本按运行段渲染样式：
 * 输入的文字会继承所在位置的样式，界面上看到的效果就是 RichTextBlock 的最终效果。
 */
class RICHTEXTCOMPOSER_API SRichTextComposer : public SCompoundWidget, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SRichTextComposer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	//~ Begin FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject

private:
	//////////////////////////////////////////////////////////////////////////
	// 样式表
	//////////////////////////////////////////////////////////////////////////
	void RefreshTableList();
	TSharedRef<SWidget> MakeTableComboBoxEntry(TSharedPtr<FAssetData> InItem);
	FText GetSelectedTableText() const;
	void HandleTableSelected(TSharedPtr<FAssetData> InItem, ESelectInfo::Type SelectInfo);
	bool ValidateSelectedTable();
	void RebuildStyleEntries();
	void RebuildEditorStyleSet();

	//////////////////////////////////////////////////////////////////////////
	// 样式列表
	//////////////////////////////////////////////////////////////////////////
	TSharedRef<ITableRow> OnGenerateStyleRow(TSharedPtr<FRichTextComposerStyleItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleStyleSelectionChanged(TSharedPtr<FRichTextComposerStyleItem> InItem, ESelectInfo::Type SelectInfo);
	FText GetStyleSampleFontWarning(const FRichTextComposerStyleItem& Item) const;

	//////////////////////////////////////////////////////////////////////////
	// 所见即所得编辑区
	//////////////////////////////////////////////////////////////////////////
	void HandleMarkupTextChanged(const FText& NewMarkup);
	void HandleCursorMoved(const FTextLocation& InLocation);

	/** 用标记文本刷新编辑区（引擎会保留光标位置）。 */
	void SetEditorMarkup(const FString& InMarkup);

	/** 让编辑区在下一次 Tick 完成重排（SetText / 样式变更都依赖它生效）。 */
	void RequestEditorTick();
	EActiveTimerReturnType HandleEditorTick(double InCurrentTime, float InDeltaTime);

	FString GetEditorPlainText() const;
	bool CursorLocationToPlainIndex(const FTextLocation& Location, int32& OutIndex) const;
	bool GetSelectionRange(int32& OutStart, int32& OutEnd) const;

	void UpdateCaretStyleText();
	void ApplyStyleToSelection(FName StyleName);
	void RecomputeUnknownStyles();
	void MarkOutputDirty();

	//////////////////////////////////////////////////////////////////////////
	// 编辑区背景色
	//////////////////////////////////////////////////////////////////////////
	/** 从编辑器配置读取上次保存的背景色。 */
	void LoadEditorBackgroundColor();

	/** 应用背景色（可选写入配置）；同时按背景亮度调整无标记文字的默认颜色。 */
	void SetEditorBackgroundColor(FLinearColor InColor, bool bSaveToConfig);

	/** 依据背景亮度自动选择默认文字颜色，避免浅色背景上看不清。 */
	void UpdateDefaultTextColorForBackground();

	FReply HandleBackgroundPresetClicked(FLinearColor InColor);
	FReply HandleCustomBackgroundClicked();

	//////////////////////////////////////////////////////////////////////////
	// 按钮
	//////////////////////////////////////////////////////////////////////////
	FReply HandleRefreshTablesClicked();
	FReply HandleClearStyleClicked();
	FReply HandleClearAllStylesClicked();
	FReply HandleOutputClicked();
	FReply HandleClearTextClicked();
	FReply HandleImportClicked();

	//////////////////////////////////////////////////////////////////////////
	// 状态
	//////////////////////////////////////////////////////////////////////////
	void SetStatus(const FText& InStatus);
	EVisibility GetOutputDirtyVisibility() const;
	FText GetTextStatsText() const;

	/** 当前选中的数据表（通过 FGCObject 保持强引用）。 */
	TObjectPtr<UDataTable> SelectedTable = nullptr;

	TArray<TSharedPtr<FAssetData>> TableItems;
	TSharedPtr<SComboBox<TSharedPtr<FAssetData>>> TableComboBox;

	TArray<TSharedPtr<FRichTextComposerStyleItem>> StyleItems;
	TSharedPtr<SListView<TSharedPtr<FRichTextComposerStyleItem>>> StyleListView;

	/** 合并后的编辑区：可编辑 + 按数据表样式渲染。 */
	TSharedPtr<SMultiLineEditableText> TextEditor;
	TSharedPtr<FStyleMarkupLayoutMarshaller> MarkupMarshaller;
	TSharedPtr<STextBlock> CaretStyleBlock;
	TSharedPtr<STextBlock> StatusTextBlock;

	/** 只读标记输出。 */
	TSharedPtr<SMultiLineEditableText> OutputBox;

	TSharedPtr<FSlateColorBrush> PanelBrush;
	TSharedPtr<FSlateColorBrush> EditorBrush;

	TSharedPtr<FSlateStyleSet> EditorStyleSet;
	FTextBlockStyle EditorTextStyle;
	FTextBlockStyle OutputTextStyle;

	FRichTextMarkupEngine Engine;
	TSet<FName> KnownStyles;
	TSet<FName> UnknownStyles;

	/** 编辑区当前的标记文本（与界面同步的唯一真源）。 */
	FString CurrentMarkup;

	/** 编辑区背景色（可自定义，保存在编辑器配置里）。 */
	FLinearColor EditorBackgroundColor = FLinearColor(0.01f, 0.01f, 0.01f);

	FText StatusMessage;
	FText CaretStyleText;
	bool bOutputDirty = false;
};
