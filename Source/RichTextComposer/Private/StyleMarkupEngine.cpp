#include "StyleMarkupEngine.h"

#define LOCTEXT_NAMESPACE "RichTextComposer"

//////////////////////////////////////////////////////////////////////////
// FRichTextMarkupRules
//////////////////////////////////////////////////////////////////////////

bool FRichTextMarkupRules::IsValidStyleName(const FName& StyleName)
{
	if (StyleName.IsNone())
	{
		return false;
	}

	const FString Name = StyleName.ToString();
	if (Name.IsEmpty())
	{
		return false;
	}

	for (const TCHAR Char : Name)
	{
		if (!FRichTextMarkupEngine::IsStyleNameChar(Char))
		{
			return false;
		}
	}
	return true;
}

FText FRichTextMarkupRules::GetNameRequirementText()
{
	return LOCTEXT("NameRequirement", "行名只能包含字母、数字、下划线、点、连字符（引擎标记解析器限制）");
}

FString FRichTextMarkupRules::EscapeText(const FString& InText)
{
	FString Out = InText;
	Out.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Out.ReplaceInline(TEXT("\""), TEXT("&quot;"));
	Out.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Out.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	return Out;
}

FString FRichTextMarkupRules::UnescapeText(const FString& InText)
{
	FString Out = InText;
	Out.ReplaceInline(TEXT("&quot;"), TEXT("\""));
	Out.ReplaceInline(TEXT("&lt;"), TEXT("<"));
	Out.ReplaceInline(TEXT("&gt;"), TEXT(">"));
	Out.ReplaceInline(TEXT("&amp;"), TEXT("&"));
	return Out;
}

FString FRichTextMarkupRules::NormalizeLineEndings(const FString& InText)
{
	FString Out = InText;
	Out.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
	Out.ReplaceInline(TEXT("\r"), TEXT("\n"));
	return Out;
}

//////////////////////////////////////////////////////////////////////////
// FRichTextMarkupEngine
//////////////////////////////////////////////////////////////////////////

bool FRichTextMarkupEngine::IsStyleNameChar(TCHAR Char)
{
	return (Char >= TCHAR('A') && Char <= TCHAR('Z'))
		|| (Char >= TCHAR('a') && Char <= TCHAR('z'))
		|| (Char >= TCHAR('0') && Char <= TCHAR('9'))
		|| Char == TCHAR('_')
		|| Char == TCHAR('.')
		|| Char == TCHAR('-');
}

bool FRichTextMarkupEngine::IsWellFormedAttributeString(const FString& Attributes)
{
	// 与引擎正则 (?: (?:[\w\d\.-]+=(?>"(.*?)")))+ 等价的手工校验
	int32 Pos = 0;
	const int32 Len = Attributes.Len();

	while (Pos < Len)
	{
		if (Attributes[Pos] != TCHAR(' ') && Attributes[Pos] != TCHAR('\t'))
		{
			return false;
		}
		while (Pos < Len && (Attributes[Pos] == TCHAR(' ') || Attributes[Pos] == TCHAR('\t')))
		{
			++Pos;
		}

		const int32 KeyStart = Pos;
		while (Pos < Len && IsStyleNameChar(Attributes[Pos]))
		{
			++Pos;
		}
		if (Pos == KeyStart)
		{
			return false;
		}

		if (Pos >= Len || Attributes[Pos] != TCHAR('='))
		{
			return false;
		}
		++Pos;

		if (Pos >= Len || Attributes[Pos] != TCHAR('"'))
		{
			return false;
		}
		++Pos;
		while (Pos < Len && Attributes[Pos] != TCHAR('"'))
		{
			++Pos;
		}
		if (Pos >= Len)
		{
			return false; // 引号未闭合
		}
		++Pos;
	}

	return true;
}

void FRichTextMarkupEngine::SetPlainText(const FString& InPlainText)
{
	PlainText = FRichTextMarkupRules::NormalizeLineEndings(InPlainText);
	bHadUnrecognizedMarkup = false;
	bHadDroppedAttributes = false;

	Runs.Reset();
	Runs.Add(FRichTextStyleRun{ PlainText, NAME_None });
	NormalizeRuns();
}

void FRichTextMarkupEngine::SetMarkupText(const FString& InMarkup)
{
	bHadUnrecognizedMarkup = false;
	bHadDroppedAttributes = false;

	TArray<FRichTextStyleRun> Parsed;
	ParseMarkupIntoRuns(FRichTextMarkupRules::NormalizeLineEndings(InMarkup), Parsed);
	Runs = MoveTemp(Parsed);
	NormalizeRuns();
	RebuildPlainText();
}

void FRichTextMarkupEngine::ParseMarkupIntoRuns(const FString& Markup, TArray<FRichTextStyleRun>& OutRuns)
{
	const int32 Len = Markup.Len();
	int32 Pos = 0;

	while (Pos < Len)
	{
		if (Markup[Pos] == TCHAR('<'))
		{
			// 提取标记名（字母/数字/下划线/点/连字符）
			int32 NameEnd = Pos + 1;
			while (NameEnd < Len && IsStyleNameChar(Markup[NameEnd]))
			{
				++NameEnd;
			}

			const bool bHasName = (NameEnd > Pos + 1);
			const bool bNameTerminated = (NameEnd < Len)
				&& (Markup[NameEnd] == TCHAR('>') || Markup[NameEnd] == TCHAR(' ') || Markup[NameEnd] == TCHAR('\t'));

			if (bHasName && bNameTerminated)
			{
				const int32 GtIndex = Markup.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameEnd);
				if (GtIndex != INDEX_NONE)
				{
					const FString NameStr = Markup.Mid(Pos + 1, NameEnd - Pos - 1);
					const FString AttrStr = Markup.Mid(NameEnd, GtIndex - NameEnd);
					const bool bSelfClosing = (GtIndex > Pos) && (Markup[GtIndex - 1] == TCHAR('/'));

					if (!bSelfClosing && IsWellFormedAttributeString(AttrStr))
					{
						const int32 CloseIndex = Markup.Find(TEXT("</>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, GtIndex + 1);
						if (CloseIndex != INDEX_NONE)
						{
							// 内容中不允许再出现 '<'（引擎同样不支持嵌套）；
							// CloseIndex 处的 '<' 正是闭合标签自身，不算嵌套。
							const int32 NestedLt = Markup.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, GtIndex + 1);
							if (NestedLt == INDEX_NONE || NestedLt >= CloseIndex)
							{
								if (!AttrStr.TrimStartAndEnd().IsEmpty())
								{
									bHadDroppedAttributes = true;
								}

								const FString Content = FRichTextMarkupRules::UnescapeText(Markup.Mid(GtIndex + 1, CloseIndex - GtIndex - 1));
								AddRun(OutRuns, Content, FName(*NameStr));
								Pos = CloseIndex + 3;
								continue;
							}
						}
					}
				}
			}

			// 不是本工具认识的样式标记：按普通文字处理（与引擎行为一致）
			bHadUnrecognizedMarkup = true;
		}

		// 普通文字段：一直取到下一个 '<'（当前字符即使是 '<' 也作为普通文字保留）
		const int32 NextLtIndex = Markup.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos + 1);
		if (NextLtIndex == INDEX_NONE)
		{
			AddRun(OutRuns, FRichTextMarkupRules::UnescapeText(Markup.Mid(Pos)), NAME_None);
			break;
		}

		AddRun(OutRuns, FRichTextMarkupRules::UnescapeText(Markup.Mid(Pos, NextLtIndex - Pos)), NAME_None);
		Pos = NextLtIndex;
	}
}

void FRichTextMarkupEngine::AddRun(TArray<FRichTextStyleRun>& OutRuns, const FString& Text, FName StyleName)
{
	if (Text.IsEmpty())
	{
		return;
	}

	if (OutRuns.Num() > 0 && OutRuns.Last().StyleName == StyleName)
	{
		OutRuns.Last().Text += Text;
		return;
	}

	OutRuns.Add(FRichTextStyleRun{ Text, StyleName });
}

void FRichTextMarkupEngine::ApplyTextEdit(const FString& InNewPlainText)
{
	const FString NewPlainText = FRichTextMarkupRules::NormalizeLineEndings(InNewPlainText);
	if (NewPlainText == PlainText)
	{
		return;
	}

	const int32 OldLen = PlainText.Len();
	const int32 NewLen = NewPlainText.Len();

	int32 Prefix = 0;
	while (Prefix < OldLen && Prefix < NewLen && PlainText[Prefix] == NewPlainText[Prefix])
	{
		++Prefix;
	}

	int32 Suffix = 0;
	while (Suffix < OldLen - Prefix && Suffix < NewLen - Prefix
		&& PlainText[OldLen - 1 - Suffix] == NewPlainText[NewLen - 1 - Suffix])
	{
		++Suffix;
	}

	const int32 DeleteLen = OldLen - Prefix - Suffix;
	const FString Inserted = NewPlainText.Mid(Prefix, NewLen - Prefix - Suffix);

	if (DeleteLen > 0)
	{
		DeleteRange(Prefix, DeleteLen);
		PlainText.RemoveAt(Prefix, DeleteLen);
	}

	const FName InheritedStyle = GetStyleForInsertion(Prefix);
	InsertRange(Prefix, Inserted, InheritedStyle);
	PlainText = PlainText.Left(Prefix) + Inserted + PlainText.Mid(Prefix);

	NormalizeRuns();
}

void FRichTextMarkupEngine::ApplyStyleToRange(int32 Start, int32 End, FName StyleName)
{
	if (PlainText.IsEmpty())
	{
		return;
	}

	if (!StyleName.IsNone() && !FRichTextMarkupRules::IsValidStyleName(StyleName))
	{
		return;
	}

	Start = FMath::Clamp(Start, 0, PlainText.Len());
	End = FMath::Clamp(End, Start, PlainText.Len());
	if (Start >= End)
	{
		return;
	}

	// 先在区间边界切开运行段，使 [Start, End) 与运行段边界对齐
	SplitRunAt(Start);
	SplitRunAt(End);

	int32 Offset = 0;
	for (FRichTextStyleRun& Run : Runs)
	{
		const int32 RunStart = Offset;
		const int32 RunEnd = Offset + Run.Text.Len();
		Offset = RunEnd;

		if (RunStart >= End)
		{
			break;
		}
		if (RunEnd <= Start)
		{
			continue;
		}

		// 替换语义：新区间样式直接覆盖旧样式
		Run.StyleName = StyleName;
	}

	NormalizeRuns();
}

void FRichTextMarkupEngine::ClearAllStyles()
{
	for (FRichTextStyleRun& Run : Runs)
	{
		Run.StyleName = NAME_None;
	}
	NormalizeRuns();
}

FString FRichTextMarkupEngine::GetMarkupText() const
{
	FString Out;
	for (const FRichTextStyleRun& Run : Runs)
	{
		FString Remaining = Run.Text;
		for (;;)
		{
			int32 NewLineIndex = INDEX_NONE;
			Remaining.FindChar(TEXT('\n'), NewLineIndex);

			const FString Segment = (NewLineIndex == INDEX_NONE) ? Remaining : Remaining.Left(NewLineIndex);
			Remaining = (NewLineIndex == INDEX_NONE) ? FString() : Remaining.Mid(NewLineIndex + 1);

			if (!Segment.IsEmpty())
			{
				if (FRichTextMarkupRules::IsValidStyleName(Run.StyleName))
				{
					Out += TEXT("<");
					Out += Run.StyleName.ToString();
					Out += TEXT(">");
					Out += FRichTextMarkupRules::EscapeText(Segment);
					Out += TEXT("</>");
				}
				else
				{
					Out += FRichTextMarkupRules::EscapeText(Segment);
				}
			}

			if (NewLineIndex == INDEX_NONE)
			{
				break;
			}
			// 标记不能跨行：换行本身放在标记之外
			Out += TEXT("\n");
		}
	}
	return Out;
}

FName FRichTextMarkupEngine::GetStyleAt(int32 Index) const
{
	const int32 Probe = FMath::Clamp(Index, 0, FMath::Max(PlainText.Len() - 1, 0));
	const int32 RunIndex = FindRunIndexAt(Probe);
	return (RunIndex != INDEX_NONE) ? Runs[RunIndex].StyleName : NAME_None;
}

void FRichTextMarkupEngine::CollectUnknownStyles(const TSet<FName>& KnownStyles, TSet<FName>& OutUnknown) const
{
	OutUnknown.Reset();
	for (const FRichTextStyleRun& Run : Runs)
	{
		if (!Run.StyleName.IsNone() && !KnownStyles.Contains(Run.StyleName))
		{
			OutUnknown.Add(Run.StyleName);
		}
	}
}

void FRichTextMarkupEngine::RebuildPlainText()
{
	PlainText.Reset();
	for (const FRichTextStyleRun& Run : Runs)
	{
		PlainText += Run.Text;
	}
}

void FRichTextMarkupEngine::NormalizeRuns()
{
	TArray<FRichTextStyleRun> Merged;
	for (const FRichTextStyleRun& Run : Runs)
	{
		if (Run.Text.IsEmpty())
		{
			continue;
		}

		if (Merged.Num() > 0 && Merged.Last().StyleName == Run.StyleName)
		{
			Merged.Last().Text += Run.Text;
		}
		else
		{
			Merged.Add(Run);
		}
	}

	Runs = MoveTemp(Merged);
	if (Runs.Num() == 0)
	{
		Runs.Add(FRichTextStyleRun{ FString(), NAME_None });
	}
}

void FRichTextMarkupEngine::DeleteRange(int32 Start, int32 Length)
{
	if (Length <= 0)
	{
		return;
	}

	const int32 End = Start + Length;
	TArray<FRichTextStyleRun> NewRuns;
	int32 Offset = 0;

	for (const FRichTextStyleRun& Run : Runs)
	{
		const int32 RunStart = Offset;
		const int32 RunEnd = Offset + Run.Text.Len();
		Offset = RunEnd;

		if (RunEnd <= Start || RunStart >= End)
		{
			NewRuns.Add(Run);
			continue;
		}

		const FString LeftPart = Run.Text.Left(FMath::Max(0, Start - RunStart));
		const FString RightPart = Run.Text.Mid(FMath::Min(Run.Text.Len(), End - RunStart));

		if (!LeftPart.IsEmpty())
		{
			NewRuns.Add(FRichTextStyleRun{ LeftPart, Run.StyleName });
		}
		if (!RightPart.IsEmpty())
		{
			NewRuns.Add(FRichTextStyleRun{ RightPart, Run.StyleName });
		}
	}

	Runs = MoveTemp(NewRuns);
	NormalizeRuns();
}

void FRichTextMarkupEngine::InsertRange(int32 Start, const FString& Text, FName StyleName)
{
	if (Text.IsEmpty())
	{
		return;
	}

	TArray<FRichTextStyleRun> NewRuns;
	int32 Offset = 0;
	bool bInserted = false;

	for (const FRichTextStyleRun& Run : Runs)
	{
		const int32 RunStart = Offset;
		const int32 RunEnd = Offset + Run.Text.Len();
		Offset = RunEnd;

		if (!bInserted && Start >= RunStart && Start <= RunEnd)
		{
			const FString LeftPart = Run.Text.Left(Start - RunStart);
			const FString RightPart = Run.Text.Mid(Start - RunStart);

			if (!LeftPart.IsEmpty())
			{
				NewRuns.Add(FRichTextStyleRun{ LeftPart, Run.StyleName });
			}
			NewRuns.Add(FRichTextStyleRun{ Text, StyleName });
			bInserted = true;
			if (!RightPart.IsEmpty())
			{
				NewRuns.Add(FRichTextStyleRun{ RightPart, Run.StyleName });
			}
		}
		else
		{
			NewRuns.Add(Run);
		}
	}

	if (!bInserted)
	{
		NewRuns.Add(FRichTextStyleRun{ Text, StyleName });
	}

	Runs = MoveTemp(NewRuns);
	NormalizeRuns();
}

void FRichTextMarkupEngine::SplitRunAt(int32 CharIndex)
{
	int32 Offset = 0;
	for (int32 RunIndex = 0; RunIndex < Runs.Num(); ++RunIndex)
	{
		FRichTextStyleRun& Run = Runs[RunIndex];
		const int32 RunStart = Offset;
		const int32 RunEnd = Offset + Run.Text.Len();
		Offset = RunEnd;

		if (CharIndex > RunStart && CharIndex < RunEnd)
		{
			const FRichTextStyleRun RightPart{ Run.Text.Mid(CharIndex - RunStart), Run.StyleName };
			Run.Text.LeftInline(CharIndex - RunStart);
			Runs.Insert(RightPart, RunIndex + 1);
			return;
		}
	}
}

int32 FRichTextMarkupEngine::FindRunIndexAt(int32 CharIndex) const
{
	int32 Offset = 0;
	for (int32 RunIndex = 0; RunIndex < Runs.Num(); ++RunIndex)
	{
		const int32 Len = Runs[RunIndex].Text.Len();
		if (CharIndex >= Offset && CharIndex < Offset + Len)
		{
			return RunIndex;
		}
		Offset += Len;
	}

	return (Runs.Num() > 0 && CharIndex >= Offset) ? Runs.Num() - 1 : INDEX_NONE;
}

FName FRichTextMarkupEngine::GetStyleForInsertion(int32 Index) const
{
	if (PlainText.IsEmpty())
	{
		return NAME_None;
	}

	const int32 Probe = FMath::Clamp(Index, 0, PlainText.Len() - 1);
	const int32 RunIndex = FindRunIndexAt(Probe);
	return (RunIndex != INDEX_NONE) ? Runs[RunIndex].StyleName : NAME_None;
}

#undef LOCTEXT_NAMESPACE
