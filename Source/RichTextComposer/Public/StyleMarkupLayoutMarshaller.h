#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/ITextLayoutMarshaller.h"
#include "Styling/SlateTypes.h"

#include "StyleMarkupEngine.h"

class FSlateStyleSet;

/**
 * 让 SMultiLineEditableText 直接按 <样式行名>标记 渲染每个运行段的样式。
 *
 * 关键点（已对照 UE 5.8 引擎实现确认）：
 *  - 可编辑文本的布局本身就是普通的 FTextLayout，可以承载多个不同样式的 Run；
 *  - 在光标处插入文字时，引擎会扩充光标所在的 Run（FTextLayout::InsertAt），
 *    因此新输入的文字自动继承该处样式，无需额外处理；
 *  - 本 Marshaller 的 GetText() 把布局里的 Run 反向序列化回标记文本，
 *    所以"编辑区内容"始终等于可交给 RichTextBlock 的标记文本。
 */
class RICHTEXTCOMPOSER_API FStyleMarkupLayoutMarshaller : public ITextLayoutMarshaller
{
public:
	FStyleMarkupLayoutMarshaller();
	virtual ~FStyleMarkupLayoutMarshaller();

	/** 更新样式来源（行名 → FTextBlockStyle）与无标记文本使用的默认样式。 */
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
