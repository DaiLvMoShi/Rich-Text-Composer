#pragma once

#include "CoreMinimal.h"

/** 一段带样式行名的文本运行段；StyleName 为 NAME_None 表示无标记（默认样式）。 */
struct RICHTEXTCOMPOSER_API FRichTextStyleRun
{
	FString Text;
	FName StyleName;
};

/** 引擎富文本标记的命名约束与转义工具。 */
struct RICHTEXTCOMPOSER_API FRichTextMarkupRules
{
	/**
	 * 标记名（= 数据表行名）是否可用于标记。
	 * 引擎 FDefaultRichTextMarkupParser 的元素名正则为 [\w\d\.-]+，
	 * 因此只允许：字母、数字、下划线、点、连字符。
	 */
	static bool IsValidStyleName(const FName& StyleName);

	/** 非法行名的原因说明（界面提示用）。 */
	static FText GetNameRequirementText();

	static FString EscapeText(const FString& InText);
	static FString UnescapeText(const FString& InText);
	static FString NormalizeLineEndings(const FString& InText);
};

/**
 * 纯文本 ↔ 富文本标记 的双向转换 + 区间样式编辑（本插件核心逻辑，不依赖任何 UI）。
 *
 * 标记格式：<样式行名>内容</>（与引擎 RichTextBlock 的 TextStyleSet 用法一致）
 * 约束（与引擎解析器一致）：
 *  - 行名只能含 [A-Za-z0-9_.-]；
 *  - 不支持嵌套，一个区间只有一个样式；
 *  - 标记不能跨行，序列化时按行自动拆分；
 *  - 内容中的 & < > " 会被转义。
 */
class RICHTEXTCOMPOSER_API FRichTextMarkupEngine
{
public:
	/** 用纯文本覆盖内容（清除全部样式）。 */
	void SetPlainText(const FString& InPlainText);

	/** 反解析：把已有标记文本载入为可继续编辑的内容。 */
	void SetMarkupText(const FString& InMarkup);

	/**
	 * 文本编辑入口：与当前纯文本做前后缀差异比较，
	 * 新插入的文字继承插入点前一个字符的样式（与 Word 手感一致）。
	 */
	void ApplyTextEdit(const FString& InNewPlainText);

	/** 把 [Start, End) 区间的样式替换为 StyleName（NAME_None = 清除该区间样式）。 */
	void ApplyStyleToRange(int32 Start, int32 End, FName StyleName);

	/** 清除全部样式。 */
	void ClearAllStyles();

	const FString& GetPlainText() const { return PlainText; }

	/** 生成引擎富文本标记文本。 */
	FString GetMarkupText() const;

	const TArray<FRichTextStyleRun>& GetRuns() const { return Runs; }

	/** 取某个字符处的样式名（越界时返回最近运行段的样式）。 */
	FName GetStyleAt(int32 Index) const;

	/** 收集不在 KnownStyles 中的样式名（反解析后用于界面提示）。 */
	void CollectUnknownStyles(const TSet<FName>& KnownStyles, TSet<FName>& OutUnknown) const;

	/** 最近一次反解析是否遇到过无法识别为样式标记的 '<' 片段。 */
	bool HadUnrecognizedMarkup() const { return bHadUnrecognizedMarkup; }

	/** 最近一次反解析是否丢弃过标记属性（引擎 writer 会写出 metakey="value"）。 */
	bool HadDroppedAttributes() const { return bHadDroppedAttributes; }

	/** 标记名允许的单个字符。 */
	static bool IsStyleNameChar(TCHAR Char);

private:
	void ParseMarkupIntoRuns(const FString& Markup, TArray<FRichTextStyleRun>& OutRuns);
	static void AddRun(TArray<FRichTextStyleRun>& OutRuns, const FString& Text, FName StyleName);
	static bool IsWellFormedAttributeString(const FString& Attributes);

	void RebuildPlainText();
	void NormalizeRuns();
	void DeleteRange(int32 Start, int32 Length);
	void InsertRange(int32 Start, const FString& Text, FName StyleName);
	void SplitRunAt(int32 CharIndex);
	int32 FindRunIndexAt(int32 CharIndex) const;
	FName GetStyleForInsertion(int32 Index) const;

	FString PlainText;
	TArray<FRichTextStyleRun> Runs;
	bool bHadUnrecognizedMarkup = false;
	bool bHadDroppedAttributes = false;
};
