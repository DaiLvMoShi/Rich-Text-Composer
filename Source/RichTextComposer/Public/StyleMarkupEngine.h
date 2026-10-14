#pragma once

#include "CoreMinimal.h"

/** A text run with a style row name; NAME_None means "no tag" (default style). */
struct RICHTEXTCOMPOSER_API FRichTextStyleRun
{
	FString Text;
	FName StyleName;
};

/** Naming rules and escaping helpers for the engine rich text markup. */
struct RICHTEXTCOMPOSER_API FRichTextMarkupRules
{
	/**
	 * Whether a tag name (= DataTable row name) can be used as markup.
	 * The engine FDefaultRichTextMarkupParser matches element names with [\w\d\.-]+,
	 * so only letters, digits, underscore, dot and hyphen are allowed.
	 */
	static bool IsValidStyleName(const FName& StyleName);

	/** Reason shown in the UI when a row name is invalid. */
	static FText GetNameRequirementText();

	static FString EscapeText(const FString& InText);
	static FString UnescapeText(const FString& InText);
	static FString NormalizeLineEndings(const FString& InText);
};

/**
 * Two-way conversion between plain text and rich text markup, plus range styling (the plugin core logic, UI-independent).
 *
 * Markup format: <RowName>content</> (the same form the engine RichTextBlock TextStyleSet expects)
 * Constraints (matching the engine parser):
 *  - row names may only contain [A-Za-z0-9_.-];
 *  - no nesting: one range carries one style;
 *  - tags cannot span lines; serialization splits them per line;
 *  - & < > " inside content are escaped.
 */
class RICHTEXTCOMPOSER_API FRichTextMarkupEngine
{
public:
	/** Replaces the content with plain text (clearing all styles). */
	void SetPlainText(const FString& InPlainText);

	/** Reverse parse: loads existing markup back as editable content. */
	void SetMarkupText(const FString& InMarkup);

	/**
	 * Text edit entry point: diffs the new plain text against the current one,
	 * inserted characters inherit the style of the character before the caret (Word-like).
	 */
	void ApplyTextEdit(const FString& InNewPlainText);

	/** Replaces the style of [Start, End) with StyleName (NAME_None clears it). */
	void ApplyStyleToRange(int32 Start, int32 End, FName StyleName);

	/** Clears all styles. */
	void ClearAllStyles();

	const FString& GetPlainText() const { return PlainText; }

	/** Generates engine rich text markup. */
	FString GetMarkupText() const;

	const TArray<FRichTextStyleRun>& GetRuns() const { return Runs; }

	/** Style name at a character index (clamped to the nearest run). */
	FName GetStyleAt(int32 Index) const;

	/** Collects style names missing from KnownStyles (used for UI notices after reverse parsing). */
	void CollectUnknownStyles(const TSet<FName>& KnownStyles, TSet<FName>& OutUnknown) const;

	/** Whether the last reverse parse met '<' fragments that are not style tags. */
	bool HadUnrecognizedMarkup() const { return bHadUnrecognizedMarkup; }

	/** Whether the last reverse parse dropped tag attributes (the engine writer emits metakey="value"). */
	bool HadDroppedAttributes() const { return bHadDroppedAttributes; }

	/** A single character allowed in a tag name. */
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
