using UnrealBuildTool;

public class RichTextComposer : ModuleRules
{
	public RichTextComposer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore",      // EKeys（SListView / SComboBox 模板需要）
			"AppFramework",   // SColorPicker（自定义背景色对话框）
			"UMG",            // FRichTextStyleRow
			"UnrealEd",
			"ToolMenus",      // 工具栏按钮 / 主菜单
			"AssetRegistry",  // 扫描 RichTextStyleRow 数据表
			"ApplicationCore" // 剪贴板
		});
	}
}
