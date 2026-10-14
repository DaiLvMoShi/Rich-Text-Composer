#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/ITextLayoutMarshaller.h"
#include "Styling/SlateTypes.h"

#include "StyleMarkupEngine.h"

class FSlateStyleSet;

/**
 * Lets SMultiLineEditableText render each run with the style named by its <RowName> tag.
 *
 * Key points (confirmed against the engine implementation):
 *  - the editable text layout is a plain FTextLayout and can hold runs with different styles;
 *  - when text is inserted at the caret, the engine extends the run under the caret (FTextLayout::InsertAt),
 *    so newly typed characters inherit the style at that position with no extra work;
 *  - this marshaller's GetText() serializes the layout runs back into markup,
 *    so the editor content always equals the markup you can hand to a RichTextBlock.
 */
class RICHTEXTCOMPOSER_API FStyleMarkupLayoutMarshaller : public ITextLayoutMarshaller
{
public:
	FStyleMarkupLayoutMarshaller();
	virtual ~FStyleMarkupLayoutMarshaller();

	/** Updates the style source (row name -> FTextBlockStyle) and the default style for untagged text. */
	void SetStyles(TSharedPtr<FSlateStyleSet> InStyleSet, const FTextBlockStyle& InDefaultStyle);

	//~ Begin ITextLayoutMarshaller
	virtual void SetText(const FString& SourceString, FTextLayout& TargetTextLayout) override;
	virtual void GetText(FString& TargetString, const FTextLayout& SourceTextLayout) override;
	virtual bool RequiresLiveUpdate() const override;
	virtual void MakeDirty() override;
	virtual void ClearDirty() override;
	virtual bool IsDirty() const override;
	//~ End ITextLayoutMarshaller

private:
	FTextBlockStyle ResolveStyle(FName StyleName) const;
	void AppendRunToLine(const TSharedRef<FString>& LineText, TArray<TSharedRef<IRun>>& LineRuns, const FString& RunText, FName StyleName) const;

	TSharedPtr<FSlateStyleSet> StyleSet;
	FTextBlockStyle DefaultStyle;
	bool bDirty = true;
};
