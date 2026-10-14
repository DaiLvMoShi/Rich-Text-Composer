#include "StyleMarkupLayoutMarshaller.h"

#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/TextLayout.h"
#include "Styling/SlateStyle.h"

FStyleMarkupLayoutMarshaller::FStyleMarkupLayoutMarshaller() = default;
FStyleMarkupLayoutMarshaller::~FStyleMarkupLayoutMarshaller() = default;

void FStyleMarkupLayoutMarshaller::SetStyles(TSharedPtr<FSlateStyleSet> InStyleSet, const FTextBlockStyle& InDefaultStyle)
{
	StyleSet = MoveTemp(InStyleSet);
	DefaultStyle = InDefaultStyle;
}

FTextBlockStyle FStyleMarkupLayoutMarshaller::ResolveStyle(FName StyleName) const
{
	if (!StyleName.IsNone() && StyleSet.IsValid() && StyleSet->HasWidgetStyle<FTextBlockStyle>(StyleName))
	{
		return StyleSet->GetWidgetStyle<FTextBlockStyle>(StyleName);
	}
	return DefaultStyle;
}

void FStyleMarkupLayoutMarshaller::AppendRunToLine(const TSharedRef<FString>& LineText, TArray<TSharedRef<IRun>>& LineRuns, const FString& RunText, FName StyleName) const
{
	if (RunText.IsEmpty())
	{
		return;
	}

	// Run 的文本范围指向"本行"的模型字符串（与引擎 FRichTextLayoutMarshaller 做法一致）
	const FTextRange ModelRange(LineText->Len(), LineText->Len() + RunText.Len());
	*LineText += RunText;

	FRunInfo RunInfo(StyleName.IsNone() ? FString() : StyleName.ToString());
	LineRuns.Add(FSlateTextRun::Create(RunInfo, LineText, ResolveStyle(StyleName), ModelRange));
}

void FStyleMarkupLayoutMarshaller::SetText(const FString& SourceString, FTextLayout& TargetTextLayout)
{
	// 复用工具同一套标记解析规则：得到「文本 + 样式行名」的运行段
	FRichTextMarkupEngine Parser;
	Parser.SetMarkupText(SourceString);

	TArray<FTextLayout::FNewLineData> LinesToAdd;

	TSharedRef<FString> CurrentLineText = MakeShareable(new FString());
	TArray<TSharedRef<IRun>> CurrentLineRuns;

	for (const FRichTextStyleRun& Run : Parser.GetRuns())
	{
		FString Remaining = Run.Text;
		for (;;)
		{
			int32 NewLineIndex = INDEX_NONE;
			Remaining.FindChar(TEXT('\n'), NewLineIndex);

			const FString Segment = (NewLineIndex == INDEX_NONE) ? Remaining : Remaining.Left(NewLineIndex);
			Remaining = (NewLineIndex == INDEX_NONE) ? FString() : Remaining.Mid(NewLineIndex + 1);

			AppendRunToLine(CurrentLineText, CurrentLineRuns, Segment, Run.StyleName);

			if (NewLineIndex == INDEX_NONE)
			{
				break;
			}

			// 换行：结束当前行（空行也要保留一个空 Run，保证光标可用）
			if (CurrentLineRuns.Num() == 0)
			{
				CurrentLineRuns.Add(FSlateTextRun::Create(FRunInfo(), CurrentLineText, DefaultStyle, FTextRange(0, 0)));
			}
			LinesToAdd.Emplace(CurrentLineText, MoveTemp(CurrentLineRuns));

			CurrentLineText = MakeShareable(new FString());
			CurrentLineRuns.Reset();
		}
	}

	if (CurrentLineRuns.Num() == 0)
	{
		CurrentLineRuns.Add(FSlateTextRun::Create(FRunInfo(), CurrentLineText, DefaultStyle, FTextRange(0, 0)));
	}
	LinesToAdd.Emplace(CurrentLineText, MoveTemp(CurrentLineRuns));

	TargetTextLayout.AddLines(LinesToAdd);
}

void FStyleMarkupLayoutMarshaller::GetText(FString& TargetString, const FTextLayout& SourceTextLayout)
{
	TargetString.Reset();

	const TArray<FTextLayout::FLineModel>& LineModels = SourceTextLayout.GetLineModels();
	for (int32 LineIndex = 0; LineIndex < LineModels.Num(); ++LineIndex)
	{
		if (LineIndex > 0)
		{
			TargetString += TEXT("\n");
		}

		for (const FTextLayout::FRunModel& RunModel : LineModels[LineIndex].Runs)
		{
			FString RunText;
			RunModel.AppendTextTo(RunText);
			if (RunText.IsEmpty())
			{
				continue;
			}

			const FString TagName = RunModel.GetRun()->GetRunInfo().Name;
			if (FRichTextMarkupRules::IsValidStyleName(FName(*TagName)))
			{
				TargetString += TEXT("<");
				TargetString += TagName;
				TargetString += TEXT(">");
				TargetString += FRichTextMarkupRules::EscapeText(RunText);
				TargetString += TEXT("</>");
			}
			else
			{
				TargetString += FRichTextMarkupRules::EscapeText(RunText);
			}
		}
	}
}

bool FStyleMarkupLayoutMarshaller::RequiresLiveUpdate() const
{
	return false;
}

void FStyleMarkupLayoutMarshaller::MakeDirty()
{
	bDirty = true;
}

void FStyleMarkupLayoutMarshaller::ClearDirty()
{
	bDirty = false;
}

bool FStyleMarkupLayoutMarshaller::IsDirty() const
{
	return bDirty;
}
