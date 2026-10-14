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

/** One entry of the style list (= one DataTable row). */
struct FRichTextComposerStyleItem
{
	/** DataTable row name; also the tag name written to the output. */
	FName RowName;

	/** The row's text style (used for the list sample and for editor rendering). */
	FTextBlockStyle Style;

	/** Whether the row name follows the engine's markup naming rules. */
	bool bValidName = true;
};

/**
 * The Rich Text Composer tool window.
 *
 * Layout: style table picker -> style list + [WYSIWYG editor] (editable, rendered with the table styles)
 *       -> read-only markup output.
 *
 * The editor uses FStyleMarkupLayoutMarshaller so editable text renders per-run styles:
 * typed characters inherit the style at their position, and what you see is what a RichTextBlock renders.
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
	// Style table
	//////////////////////////////////////////////////////////////////////////
	void RefreshTableList();
	TSharedRef<SWidget> MakeTableComboBoxEntry(TSharedPtr<FAssetData> InItem);
	FText GetSelectedTableText() const;
	void HandleTableSelected(TSharedPtr<FAssetData> InItem, ESelectInfo::Type SelectInfo);
	bool ValidateSelectedTable();
	void RebuildStyleEntries();
	void RebuildEditorStyleSet();

	//////////////////////////////////////////////////////////////////////////
	// Style list
	//////////////////////////////////////////////////////////////////////////
	TSharedRef<ITableRow> OnGenerateStyleRow(TSharedPtr<FRichTextComposerStyleItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleStyleSelectionChanged(TSharedPtr<FRichTextComposerStyleItem> InItem, ESelectInfo::Type SelectInfo);
	FText GetStyleSampleFontWarning(const FRichTextComposerStyleItem& Item) const;

	//////////////////////////////////////////////////////////////////////////
	// WYSIWYG editor
	//////////////////////////////////////////////////////////////////////////
	void HandleMarkupTextChanged(const FText& NewMarkup);
	void HandleCursorMoved(const FTextLocation& InLocation);

	/** Refreshes the editor from markup (the engine preserves the caret). */
	void SetEditorMarkup(const FString& InMarkup);

	/** Makes the editor re-marshal on the next tick (both SetText and style changes rely on this). */
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
	// Editor background
	//////////////////////////////////////////////////////////////////////////
	/** Loads the last saved background color from the editor config. */
	void LoadEditorBackgroundColor();

	/** Applies a background color (optionally saving it) and adjusts the default text color to match its brightness. */
	void SetEditorBackgroundColor(FLinearColor InColor, bool bSaveToConfig);

	/** Picks the default text color from the background brightness so text stays readable. */
	void UpdateDefaultTextColorForBackground();

	FReply HandleBackgroundPresetClicked(FLinearColor InColor);
	FReply HandleCustomBackgroundClicked();

	//////////////////////////////////////////////////////////////////////////
	// Buttons
	//////////////////////////////////////////////////////////////////////////
	FReply HandleRefreshTablesClicked();
	FReply HandleClearStyleClicked();
	FReply HandleClearAllStylesClicked();
	FReply HandleOutputClicked();
	FReply HandleClearTextClicked();
	FReply HandleImportClicked();

	//////////////////////////////////////////////////////////////////////////
	// State
	//////////////////////////////////////////////////////////////////////////
	void SetStatus(const FText& InStatus);
	EVisibility GetOutputDirtyVisibility() const;
	FText GetTextStatsText() const;

	/** Currently selected DataTable (kept alive through FGCObject). */
	TObjectPtr<UDataTable> SelectedTable = nullptr;

	TArray<TSharedPtr<FAssetData>> TableItems;
	TSharedPtr<SComboBox<TSharedPtr<FAssetData>>> TableComboBox;

	TArray<TSharedPtr<FRichTextComposerStyleItem>> StyleItems;
	TSharedPtr<SListView<TSharedPtr<FRichTextComposerStyleItem>>> StyleListView;

	/** The merged editor: editable and rendered with the DataTable styles. */
	TSharedPtr<SMultiLineEditableText> TextEditor;
	TSharedPtr<FStyleMarkupLayoutMarshaller> MarkupMarshaller;
	TSharedPtr<STextBlock> CaretStyleBlock;
	TSharedPtr<STextBlock> StatusTextBlock;

	/** Read-only markup output. */
	TSharedPtr<SMultiLineEditableText> OutputBox;

	TSharedPtr<FSlateColorBrush> PanelBrush;
	TSharedPtr<FSlateColorBrush> EditorBrush;

	TSharedPtr<FSlateStyleSet> EditorStyleSet;
	FTextBlockStyle EditorTextStyle;
	FTextBlockStyle OutputTextStyle;

	FRichTextMarkupEngine Engine;
	TSet<FName> KnownStyles;
	TSet<FName> UnknownStyles;

	/** Current markup of the editor (the single source of truth synced with the UI). */
	FString CurrentMarkup;

	/** Editor background color (customizable, stored in the editor config). */
	FLinearColor EditorBackgroundColor = FLinearColor(0.01f, 0.01f, 0.01f);

	FText StatusMessage;
	FText CaretStyleText;
	bool bOutputDirty = false;
};
