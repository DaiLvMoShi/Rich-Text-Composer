#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "StyleMarkupEngine.h"
#include "StyleMarkupLayoutMarshaller.h"

#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Text/TextLayout.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Widgets/Layout/SBox.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Core logic tests for Rich Text Composer: plain text <-> markup conversion, range style replacement,
 * per-line splitting, escaping, typing inheritance, invalid row names and reverse parsing.
 * Run it from the command line:
 *   UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests RichTextComposer" -TestExit="Automation Test Queue Empty" -unattended -nopause -nullrhi
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRichTextComposerMarkupTest, "RichTextComposer.Markup.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRichTextComposerMarkupTest::RunTest(const FString& Parameters)
{
	// 1) plain text -> markup
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Hello World"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Title"));
		TestEqual(TEXT("basic markup output"), Engine.GetMarkupText(), FString(TEXT("<Title>Hello</> World")));
	}

	// 2) reverse parse round-trip
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title>Hello</> World"));
		TestEqual(TEXT("plain text from reverse parse"), Engine.GetPlainText(), FString(TEXT("Hello World")));
		TestEqual(TEXT("markup re-serialized after reverse parse"), Engine.GetMarkupText(), FString(TEXT("<Title>Hello</> World")));
	}

	// 3) overlapping ranges are replaced while untouched parts are kept
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title>Hello</> World"));
		Engine.ApplyStyleToRange(3, 8, TEXT("Warning"));
		TestEqual(TEXT("overlapping range replaced"), Engine.GetMarkupText(), FString(TEXT("<Title>Hel</><Warning>lo Wo</>rld")));
	}

	// 4) multi-line ranges split into one tag per line
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("ab\ncd"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Note"));
		TestEqual(TEXT("multi-line range split per line"), Engine.GetMarkupText(), FString(TEXT("<Note>ab</>\n<Note>cd</>")));
	}

	// 5) escaping and round-trip
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("a<b>&\"c"));
		Engine.ApplyStyleToRange(0, 7, TEXT("Code"));
		TestEqual(TEXT("special characters escaped"), Engine.GetMarkupText(), FString(TEXT("<Code>a&lt;b&gt;&amp;&quot;c</>")));

		FRichTextMarkupEngine Round;
		Round.SetMarkupText(Engine.GetMarkupText());
		TestEqual(TEXT("escape round-trip"), Round.GetPlainText(), FString(TEXT("a<b>&\"c")));
	}

	// 6) typing and deleting inside styled text
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Hello"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Title"));
		Engine.ApplyTextEdit(TEXT("HelloX"));
		TestEqual(TEXT("inserted text inherits style"), Engine.GetMarkupText(), FString(TEXT("<Title>HelloX</>")));

		Engine.ApplyTextEdit(TEXT("HelX"));
		TestEqual(TEXT("markup still correct after deletion"), Engine.GetMarkupText(), FString(TEXT("<Title>HelX</>")));
	}

	// 7) invalid row names (spaces, etc.) produce no markup
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Test"));
		TestFalse(TEXT("row name with a space is invalid"), FRichTextMarkupRules::IsValidStyleName(FName(TEXT("Bad Name"))));
		TestTrue(TEXT("normal row name is valid"), FRichTextMarkupRules::IsValidStyleName(FName(TEXT("Title_01.x-y"))));

		Engine.ApplyStyleToRange(0, 4, FName(TEXT("Bad Name")));
		TestEqual(TEXT("invalid row name produces no markup"), Engine.GetMarkupText(), FString(TEXT("Test")));
	}

	// 8) reverse parse: attributes are ignored, non-style tags become literal text
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title key=\"v\">Hi</>"));
		TestEqual(TEXT("markup attributes ignored"), Engine.GetMarkupText(), FString(TEXT("<Title>Hi</>")));
		TestTrue(TEXT("attributes-dropped flag recorded"), Engine.HadDroppedAttributes());

		FRichTextMarkupEngine Unknown;
		Unknown.SetMarkupText(TEXT("a<img src=\"x\"/>b"));
		TestEqual(TEXT("self-closing tag kept as literal text"), Unknown.GetPlainText(), FString(TEXT("a<img src=\"x\"/>b")));
		TestTrue(TEXT("unrecognized-markup flag recorded"), Unknown.HadUnrecognizedMarkup());
	}

	// 9) clearing styles
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<A>one</>two<B>three</>"));
		Engine.ClearAllStyles();
		TestEqual(TEXT("Clear All Styles"), Engine.GetMarkupText(), FString(TEXT("onetwothree")));
	}

	// 10) empty text and style queries
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(FString());
		TestEqual(TEXT("empty text yields empty output"), Engine.GetMarkupText(), FString());
		TestTrue(TEXT("empty text has no style"), Engine.GetStyleAt(0).IsNone());
	}

	// 11) the WYSIWYG editor marshaller: per-run parsing, reverse serialization, style inheritance while typing
	// The APIs used here (FSlateTextLayout::Create / FSlateTextRun::Create / FTextLayout::InsertAt /
	// GetLineModels / FRunModel::AppendTextTo) were checked against the official API docs: identical in 5.2 and 5.7/5.8.
	{
		const FTextBlockStyle DefaultStyle = FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText"));

		TSharedRef<FSlateStyleSet> StyleSet = MakeShareable(new FSlateStyleSet(TEXT("RichTextComposerTestStyleSet")));
		FTextBlockStyle TitleStyle = DefaultStyle;
		TitleStyle.SetFontSize(24.0f);
		TitleStyle.SetColorAndOpacity(FSlateColor(FLinearColor::Red));
		StyleSet->Set(TEXT("Title"), TitleStyle);

		TSharedRef<FStyleMarkupLayoutMarshaller> Marshaller = MakeShareable(new FStyleMarkupLayoutMarshaller());
		Marshaller->SetStyles(StyleSet, DefaultStyle);

		TSharedRef<SWidget> OwnerWidget = SNew(SBox);
		TSharedRef<FSlateTextLayout> Layout = FSlateTextLayout::Create(&OwnerWidget.Get(), DefaultStyle);

		Marshaller->SetText(TEXT("<Title>Hi</> there"), *Layout);

		const TArray<FTextLayout::FLineModel>& Lines = Layout->GetLineModels();
		TestEqual(TEXT("single line: line count"), Lines.Num(), 1);
		if (Lines.Num() == 1)
		{
			TestEqual(TEXT("single line: run count"), Lines[0].Runs.Num(), 2);
			TestEqual(TEXT("model plain text"), *Lines[0].Text, FString(TEXT("Hi there")));
			TestEqual(TEXT("first run tag name"), Lines[0].Runs[0].GetRun()->GetRunInfo().Name, FString(TEXT("Title")));
			TestEqual(TEXT("second run has no tag"), Lines[0].Runs[1].GetRun()->GetRunInfo().Name, FString());
		}

		FString RoundTrip;
		Marshaller->GetText(RoundTrip, *Layout);
		TestEqual(TEXT("editor markup re-serialized"), RoundTrip, FString(TEXT("<Title>Hi</> there")));

		// Insert inside a styled run -> the run is extended and the style is inherited
		Layout->InsertAt(FTextLocation(0, 1), TEXT("X"));
		FString AfterInsert;
		Marshaller->GetText(AfterInsert, *Layout);
		TestEqual(TEXT("insert inherits the run style at the caret"), AfterInsert, FString(TEXT("<Title>HXi</> there")));

		// Insert inside an unstyled run -> stays unstyled
		Layout->InsertAt(FTextLocation(0, 7), TEXT("Y"));
		FString AfterInsert2;
		Marshaller->GetText(AfterInsert2, *Layout);
		TestEqual(TEXT("insert in an unstyled run stays unstyled"), AfterInsert2, FString(TEXT("<Title>HXi</> theYre")));

		// multi-line
		TSharedRef<FSlateTextLayout> MultiLineLayout = FSlateTextLayout::Create(&OwnerWidget.Get(), DefaultStyle);
		Marshaller->SetText(TEXT("<Title>ab</>\ncd"), *MultiLineLayout);
		TestEqual(TEXT("multi-line: line count"), MultiLineLayout->GetLineModels().Num(), 2);

		FString MultiRoundTrip;
		Marshaller->GetText(MultiRoundTrip, *MultiLineLayout);
		TestEqual(TEXT("multi-line markup round-trip"), MultiRoundTrip, FString(TEXT("<Title>ab</>\ncd")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
