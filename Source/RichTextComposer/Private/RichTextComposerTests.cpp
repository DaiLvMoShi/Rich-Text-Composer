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
 * Rich Text Composer 核心逻辑测试：纯文本 ↔ 标记 的转换、区间样式替换、
 * 跨行拆分、转义、打字继承、非法行名与反解析行为。
 * 命令行运行：
 *   UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests RichTextComposer" -TestExit="Automation Test Queue Empty" -unattended -nopause -nullrhi
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRichTextComposerMarkupTest, "RichTextComposer.Markup.Engine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRichTextComposerMarkupTest::RunTest(const FString& Parameters)
{
	// 1) 纯文本 → 标记
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Hello World"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Title"));
		TestEqual(TEXT("基本标记输出"), Engine.GetMarkupText(), FString(TEXT("<Title>Hello</> World")));
	}

	// 2) 反解析往返一致
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title>Hello</> World"));
		TestEqual(TEXT("反解析得到的纯文本"), Engine.GetPlainText(), FString(TEXT("Hello World")));
		TestEqual(TEXT("反解析后重新输出"), Engine.GetMarkupText(), FString(TEXT("<Title>Hello</> World")));
	}

	// 3) 重叠区间按“替换”处理，并保留未重叠部分
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title>Hello</> World"));
		Engine.ApplyStyleToRange(3, 8, TEXT("Warning"));
		TestEqual(TEXT("重叠区间替换"), Engine.GetMarkupText(), FString(TEXT("<Title>Hel</><Warning>lo Wo</>rld")));
	}

	// 4) 跨行区间按行拆成多个标记
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("ab\ncd"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Note"));
		TestEqual(TEXT("跨行按行拆分标记"), Engine.GetMarkupText(), FString(TEXT("<Note>ab</>\n<Note>cd</>")));
	}

	// 5) 特殊字符转义与往返
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("a<b>&\"c"));
		Engine.ApplyStyleToRange(0, 7, TEXT("Code"));
		TestEqual(TEXT("特殊字符转义"), Engine.GetMarkupText(), FString(TEXT("<Code>a&lt;b&gt;&amp;&quot;c</>")));

		FRichTextMarkupEngine Round;
		Round.SetMarkupText(Engine.GetMarkupText());
		TestEqual(TEXT("转义往返"), Round.GetPlainText(), FString(TEXT("a<b>&\"c")));
	}

	// 6) 在已套样式的文字中继续输入 / 删除
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Hello"));
		Engine.ApplyStyleToRange(0, 5, TEXT("Title"));
		Engine.ApplyTextEdit(TEXT("HelloX"));
		TestEqual(TEXT("插入继承样式"), Engine.GetMarkupText(), FString(TEXT("<Title>HelloX</>")));

		Engine.ApplyTextEdit(TEXT("HelX"));
		TestEqual(TEXT("删除后标记仍正确"), Engine.GetMarkupText(), FString(TEXT("<Title>HelX</>")));
	}

	// 7) 非法行名（含空格等）不生成标记
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(TEXT("Test"));
		TestFalse(TEXT("含空格行名非法"), FRichTextMarkupRules::IsValidStyleName(FName(TEXT("Bad Name"))));
		TestTrue(TEXT("常规行名合法"), FRichTextMarkupRules::IsValidStyleName(FName(TEXT("Title_01.x-y"))));

		Engine.ApplyStyleToRange(0, 4, FName(TEXT("Bad Name")));
		TestEqual(TEXT("非法行名不产生标记"), Engine.GetMarkupText(), FString(TEXT("Test")));
	}

	// 8) 反解析：属性被忽略；非样式标记按字面文字处理
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<Title key=\"v\">Hi</>"));
		TestEqual(TEXT("标记属性被忽略"), Engine.GetMarkupText(), FString(TEXT("<Title>Hi</>")));
		TestTrue(TEXT("记录了属性丢弃"), Engine.HadDroppedAttributes());

		FRichTextMarkupEngine Unknown;
		Unknown.SetMarkupText(TEXT("a<img src=\"x\"/>b"));
		TestEqual(TEXT("自闭合标签按字面处理"), Unknown.GetPlainText(), FString(TEXT("a<img src=\"x\"/>b")));
		TestTrue(TEXT("记录了未识别标记"), Unknown.HadUnrecognizedMarkup());
	}

	// 9) 清除样式
	{
		FRichTextMarkupEngine Engine;
		Engine.SetMarkupText(TEXT("<A>one</>two<B>three</>"));
		Engine.ClearAllStyles();
		TestEqual(TEXT("清除全部样式"), Engine.GetMarkupText(), FString(TEXT("onetwothree")));
	}

	// 10) 空文本与纯样式查询
	{
		FRichTextMarkupEngine Engine;
		Engine.SetPlainText(FString());
		TestEqual(TEXT("空文本输出为空"), Engine.GetMarkupText(), FString());
		TestTrue(TEXT("空文本无样式"), Engine.GetStyleAt(0).IsNone());
	}

	// 11) 所见即所得编辑区的 Marshaller：按运行段解析、反向序列化、打字继承样式
	// 本段用到的 API（FSlateTextLayout::Create / FSlateTextRun::Create / FTextLayout::InsertAt /
	// GetLineModels / FRunModel::AppendTextTo）经官方 API 文档核对，5.2 与 5.7/5.8 完全一致。
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
		TestEqual(TEXT("单行解析行数"), Lines.Num(), 1);
		if (Lines.Num() == 1)
		{
			TestEqual(TEXT("单行运行段数"), Lines[0].Runs.Num(), 2);
			TestEqual(TEXT("模型纯文本"), *Lines[0].Text, FString(TEXT("Hi there")));
			TestEqual(TEXT("首个运行段标记名"), Lines[0].Runs[0].GetRun()->GetRunInfo().Name, FString(TEXT("Title")));
			TestEqual(TEXT("第二个运行段无标记"), Lines[0].Runs[1].GetRun()->GetRunInfo().Name, FString());
		}

		FString RoundTrip;
		Marshaller->GetText(RoundTrip, *Layout);
		TestEqual(TEXT("编辑区标记反向序列化"), RoundTrip, FString(TEXT("<Title>Hi</> there")));

		// 光标在带样式的运行段内插入 → 运行段被扩充，样式自动继承
		Layout->InsertAt(FTextLocation(0, 1), TEXT("X"));
		FString AfterInsert;
		Marshaller->GetText(AfterInsert, *Layout);
		TestEqual(TEXT("插入继承所在运行段样式"), AfterInsert, FString(TEXT("<Title>HXi</> there")));

		// 光标在无样式运行段内插入 → 保持无样式
		Layout->InsertAt(FTextLocation(0, 7), TEXT("Y"));
		FString AfterInsert2;
		Marshaller->GetText(AfterInsert2, *Layout);
		TestEqual(TEXT("无样式处插入保持无样式"), AfterInsert2, FString(TEXT("<Title>HXi</> theYre")));

		// 多行
		TSharedRef<FSlateTextLayout> MultiLineLayout = FSlateTextLayout::Create(&OwnerWidget.Get(), DefaultStyle);
		Marshaller->SetText(TEXT("<Title>ab</>\ncd"), *MultiLineLayout);
		TestEqual(TEXT("多行解析行数"), MultiLineLayout->GetLineModels().Num(), 2);

		FString MultiRoundTrip;
		Marshaller->GetText(MultiRoundTrip, *MultiLineLayout);
		TestEqual(TEXT("多行标记往返"), MultiRoundTrip, FString(TEXT("<Title>ab</>\ncd")));
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
