# Rich Text Composer —— 富文本样式编排器（UE 5.8 编辑器插件）

## 它解决什么问题

引擎自带的 `RichTextBlock` 要用样式，得先做一张 `RichTextStyleRow` 数据表，再在文本里**手写** `<样式行名>文字</>` 标记，既容易写错也看不见效果。

本插件在编辑器里加一个**独立工具窗口**（形态类似 Fab / Bridge）：

```
工具栏按钮 [Rich Text]  +  主菜单 Window → Rich Text Composer  →  可停靠页签
```

窗口里是**所见即所得的编辑区**：直接打字、选中文字点样式，**当场就能看到最终效果**（用数据表里的真实样式渲染），完事一键输出标记文本。

---

## 一、安装

### 纯蓝图项目（推荐路径，无需编译）

1. 把整个 `RichTextComposer` 文件夹复制到项目的 `Plugins/` 目录：
   ```
   MyProject/
   └── Plugins/
       └── RichTextComposer/
           ├── RichTextComposer.uplugin
           ├── Binaries/Win64/UnrealEditor-RichTextComposer.dll   ← 预编译二进制
           └── Source/…
   ```
2. 打开项目 → 菜单 **Edit → Plugins** → 搜索 `Rich Text` → 勾选 **Rich Text Composer** → 重启编辑器。

> 预编译二进制是用 **UE 5.8（Windows）Development Editor** 编译的，只匹配同一引擎版本。
> 本插件是 **Editor-only 模块**，打包发行游戏时不会被包含，不影响包体。

### C++ 项目 / 需要自行编译

把 `RichTextComposer.uplugin` 里的 `"Installed": true` 改为 `false`，重新生成项目文件并编译即可
（`Build.bat <Project>Editor Win64 Development`）。

---

## 二、前提：准备样式表（数据表）

工具只认引擎的 `FRichTextStyleRow` 结构：

1. 内容浏览器右键 → **其他（Miscellaneous）→ 数据表（Data Table）**；
2. 弹出窗口里选择行结构 **RichTextStyleRow**；
3. 打开表，添加行：**行名 = 样式名（也就是标记名）**，行里的 `Text Style` 决定字体、字号、颜色、描边、下划线/删除线等。

命名限制（引擎解析器决定）：**行名只能包含 字母、数字、下划线、点、连字符**（`[A-Za-z0-9_.-]`）。
含空格/中文的行会被工具标灰提示，因为写进标记后引擎无法识别。

---

## 三、使用流程（所见即所得）

```
┌─ Rich Text Composer ──────────────────────────────────────────────────────┐
│ 样式表 [ DT_RichTextStyles ▼ ] [刷新]   状态提示…                           │
│ ┌── 样式（单击应用到选中文字）──┐ ┌── 编辑区（所见即所得）─────────────────┐ │
│ │ Title      24pt             │ │ 光标处样式：Warning   共 12 字 · 2 处样式│ │
│ │ Warning    16pt 红          │ │ 背景色 ■ ■ ■ ■  自定义…                 │ │
│ │ Note       14pt             │ │ ┌────────────────────────────────────┐ │ │
│ │ [清除选中样式][清除全部样式] │ │ │ 玩家请注意，前方高能                │ │ │
│ └─────────────────────────────┘ │ │ （这里直接编辑，样式实时呈现）      │ │ │
│                                 │ └────────────────────────────────────┘ │ │
│                                 └────────────────────────────────────────┘ │
│                                 ┌── 输出（只读，可全选复制）─────────────┐ │
│                                 │ 玩家<Warning>请注意</>，前方高能        │ │
│                                 │              ● 有改动未输出             │ │
│                                 │ [输出 markup][从剪贴板导入标记][清空文本]│ │
│                                 └────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

1. **选样式表**：顶部下拉只列出 Row Structure = `RichTextStyleRow` 的表；点 **刷新** 重新扫描项目。
2. **直接编辑**：在编辑区打字，**看到的样式就是最终样式**（不是纯文本预览）。
3. **套样式**：鼠标选中一段文字 → **单击左侧样式名** → 选中的文字立刻变成该样式。
4. **继续打字**：光标在已套样式的文字中间，新输入的字会**自动继承该处样式**（和 Word 一样）。
5. **调背景色**：编辑区上方的 **背景色** 一行有 4 个预设色块（近黑 / 深灰 / 中灰 / 浅色）+ **自定义…**（取色器）。选择会**自动保存**，下次打开仍然生效；浅色背景会自动把默认（无标记）文字变成深色，避免看不清。
6. **输出**：点 **输出 markup** → 输出框里得到 `<行名>文字</>`，`Ctrl+A` / `Ctrl+C` 复制走。

> 背景色设置保存在编辑器配置 `Saved/Config/<平台>Editor/EditorPerProjectUserSettings.ini`
> 的 `[RichTextComposer] EditorBackgroundColor=` 一行，删掉该行即恢复默认。
> 背景色只作用于文字画布，标题与按钮始终在面板底色上，任何背景色下都清晰。

右上角还会显示**光标处样式**（如"光标处样式：Warning"），方便确认光标当前位置的格式。

### 详细行为

| 场景 | 行为 |
|---|---|
| 在已套样式的文字里继续打字 | 新字**继承**所在位置的样式（引擎按运行段插入） |
| 在已有样式上再套另一个样式 | **替换**（新区间覆盖旧样式，未重叠部分保留） |
| 选中区间跨行 | 输出时**按行拆成多个标记**（引擎要求标记不能跨行） |
| 文字里有 `& < > "` | 自动转义为 `&amp; &lt; &gt; &quot;`，不会再打架 |
| 行名含空格/中文 | 列表中标灰并提示原因，点了会给出错误说明 |
| 从剪贴板导入标记 | 反解析回编辑区继续改（会忽略标记上的属性，并提示） |
| 未知样式名 | 输出时保留原名，编辑按引擎行为显示为字面文字，并在统计里提示 |
| 切换样式表 | 编辑区用新样式重新渲染，**光标与选区保持不变** |
| 自定义背景色 | 预设或取色器任选，自动记忆；浅色背景自动切换默认文字为深色 |

### 反解析（导入已有标记）

点 **从剪贴板导入标记**：读取剪贴板里的 `<行名>文字</>`，还原成可编辑内容 + 样式。
适合把已有控件里的富文本拿回来继续改。

---

## 四、正式控件怎么配（关键一步）

输出的标记要能正确显示，**运行时那个 RichTextBlock 必须用同一张样式表**：

1. Widget 蓝图里放引擎自带的 **Rich Text Block**；
2. 细节面板 → **Appearance → Text Style Set** → 选你刚才那张 `RichTextStyleRow` 数据表；
3. **Content → Text** → 粘贴工具输出的标记文本。

> 忘了第 2 步的话，标记会原样显示成 `<Warning>请注意</>` 这样的文字。

---

## 五、实现要点（为什么编辑区能"又编辑又渲染"）

可编辑文本（`SMultiLineEditableText`）本身支持多运行段布局，插件给它换了一个自定义
`ITextLayoutMarshaller`（`FStyleMarkupLayoutMarshaller`）：

- **正向**：把 `<行名>文字</>` 解析成多个 `FSlateTextRun`，每个 run 用数据表里对应行的
  `FTextBlockStyle` 渲染 → 编辑区里直接看到最终样式；
- **反向**：把布局里的 run 序列化回标记文本 → 编辑区内容天然等于可直接交付的标记；
- **打字继承**：引擎在光标处插入文字时会扩充光标所在的 run（`FTextLayout::InsertAt`），
  因此新字的样式自动跟上下文一致，无需额外处理；
- **切换样式表**：给 Marshaller 打脏标记 + 一次 Tick，引擎会用新样式重排并保留光标/选区。
- **背景色**：只改文字画布的 `FSlateColorBrush::TintColor` 并重绘；设置写入
  `EditorPerProjectUserSettings.ini`（`[RichTextComposer] EditorBackgroundColor`）持久化，
  同时按背景亮度自动取反默认文字颜色（浅底深字 / 深底亮字）。

```
RichTextComposer/
├── RichTextComposer.uplugin
├── Binaries/Win64/UnrealEditor-RichTextComposer.dll   （预编译，纯蓝图项目直接用）
├── Source/RichTextComposer/
│   ├── RichTextComposer.Build.cs
│   ├── Public/
│   │   ├── RichTextComposerModule.h            模块：工具栏按钮 / Window 菜单 / 页签
│   │   ├── SRichTextComposer.h                 工具窗口
│   │   ├── StyleMarkupEngine.h                 纯逻辑：标记解析、序列化、区间样式
│   │   └── StyleMarkupLayoutMarshaller.h       让可编辑文本按运行段渲染样式
│   └── Private/
│       ├── RichTextComposerModule.cpp
│       ├── SRichTextComposer.cpp
│       ├── StyleMarkupEngine.cpp
│       ├── StyleMarkupLayoutMarshaller.cpp
│       └── RichTextComposerTests.cpp           自动化测试
└── README.md
```

---

## 六、已知限制

- **标记名**受引擎限制：`[A-Za-z0-9_.-]`，不能有空格/中文。
- **不支持嵌套**：一个区间只能有一个样式（引擎解析器如此），需要"又粗又红"请在建表时把两者做进同一行样式。
- **标记不能跨行**：工具会自动按行拆分，无需手动处理。
- 反解析时，标记上的**属性会被忽略**（引擎 writer 会写出 `key="value"`，本工具只保留行名）。
- 非样式标记（如 `<img …/>`）在反解析时按**普通文字**处理并在状态栏提示，以免破坏其它装饰器语义。
- 编辑区里样式取自数据表；若某行没设字号，列表会提示"未设字号"。
- 工具读的是资产注册表的 `RowStructure` 标签；刚新建还未保存的数据表请先保存一次再点"刷新"。

---

## 七、版本兼容（UE 5.2 ~ 5.8）

插件源码按 **5.2 ~ 5.8** 编写，只有两个版本分支点（都基于 `RichTextComposerVersionCompat.h` 的辅助宏）：

| 位置 | 分支 | 原因 |
|---|---|---|
| `RichTextComposerVersionCompat.h` | `RTC_ENGINE_OLDER_THAN(Major, Minor)` | 官方 `UE_VERSION_OLDER_THAN` 在 5.7 起变成 **三参数**（Major/Minor/Patch），直接使用会跨版本编译失败，故基于 `ENGINE_MAJOR_VERSION/ENGINE_MINOR_VERSION` 自定义 |
| `SRichTextComposer.cpp`（FGCObject） | `#if RTC_ENGINE_OLDER_THAN(5, 7)` | 5.7 起 `AddReferencedObject` 要求传 `TObjectPtr`（裸指针在增量 GC 下不安全） |

### 已用官方 API 文档逐项核对（5.2 vs 5.7/5.8 完全一致）

| API | 用途 |
|---|---|
| `ITextLayoutMarshaller` 六个虚函数（`SetText/GetText/RequiresLiveUpdate/MakeDirty/ClearDirty/IsDirty`） | 编辑区按运行段渲染的核心 |
| `SMultiLineEditableText::GetSelection()` → `FTextSelection` | 取选区（旧版 `FRealtimeSelection` 在 5.2 已不存在） |
| `SMultiLineEditableText::SetTextStyle(const FTextBlockStyle*)` | 回灌样式（指针形式） |
| `FSlateTextLayout::Create(SWidget*, FTextBlockStyle)` | 自动化测试用 |
| `FAppStyle::Get/GetAppStyleSetName/GetFontStyle/GetBrush` | 编辑器样式（5.1 起取代 `FEditorStyle`） |
| `FARFilter::ClassPaths`、`FAssetData::GetTagValue` | 扫描 RichTextStyleRow 数据表 |
| `FColorPickerArgs`（`OnColorCommitted/bUseAlpha/InitialColor/ParentWidget`） | 自定义背景色取色器 |
| `SColorBlock`（`Color/Size/UseSRGB`） | 预设色块 |
| `SWidget::Invalidate(EInvalidateWidgetReason)` | 背景色重绘 |
| `UDataTable::FindRow(FName, const TCHAR*, bool)` | 读样式行 |

其余用到的（`ToolMenus`、`FSlateStyleSet`、`FTextLayout::InsertAt/GetLineModels`、`FSlateTextRun::Create`、
`RegisterActiveTimer`、`FGCObject` 等）都是 4.x 时代就存在的老接口，5.2 起必然可用。

> **实测范围说明**：5.7 / 5.8 已实测（编译 + 自动化测试全部通过）；
> 5.2 ~ 5.6 未实机编译（本机只装了 5.7/5.8），风险点已按官方文档逐项排除。
> 另需注意：`打字继承样式`、`套样式后光标不跳` 这类**行为**在 5.7/5.8 源码中确认，
> 文档不描述行为，若要 100% 确认需在对应版本上跑一次自动化测试。

### ⚠️ 关于 `.uplugin` 的 EngineVersion 字段

`.uplugin` 里**故意不写 `"EngineVersion"`**：该字段一旦写了，只要和当前引擎版本不完全一致，
引擎就会把它判为 "Incompatible" 并弹窗询问，无人值守时直接 **跳过加载**（实测：写 5.2.0 在 5.8 上直接不加载）。
不写该字段则任何版本都能加载，由编译器/二进制本身去决定兼容性。

### 预编译二进制与版本的关系（重要）

插件里带的 `Binaries/Win64/UnrealEditor-RichTextComposer.dll` 是**用 5.8 编译的预编译二进制**，
它让**纯蓝图项目**无需编译器即可直接使用；但 DLL 与具体引擎构建绑定：

| 你的项目 | 做法 |
|---|---|
| **UE 5.8 + 纯蓝图** | 直接放 `Plugins/` 启用（`Installed: true` + 自带二进制），**免编译** |
| **UE 5.2 ~ 5.7** | 把 `.uplugin` 的 `"Installed": true` 改成 `false`，由 **C++ 项目**编译一次（纯蓝图项目需先转 C++，或另取对应版本的预编译 DLL） |
| **引擎热修升级**（如 5.8.0 → 5.8.3） | 若加载报版本不匹配，同样改 `Installed: false` 重编一次即可 |

想做"每个版本都免编译"就要为每个引擎版本各出一份二进制（Fab 上的做法是每个版本一个插件目录），
这属于打包工程而非代码改动。

---

## 八、自测

插件内置自动化测试（`RichTextComposer.Markup.Engine`），覆盖：

- 标记序列化 / 反解析往返、重叠替换、跨行拆分、特殊字符转义、非法行名；
- 所见即所得机制：运行段拆分与标记名、模型纯文本、反向序列化、
  **在带样式运行段内插入文字继承样式**、在无样式处插入保持无样式、多行往返。

```bat
UnrealEditor-Cmd.exe <Project>.uproject ^
  -ExecCmds="Automation RunTests RichTextComposer" ^
  -TestExit="Automation Test Queue Empty" ^
  -unattended -nopause -nullrhi -log
```
