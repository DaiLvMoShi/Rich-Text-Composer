# Rich Text Composer

A WYSIWYG rich-text styling tool for the Unreal Editor — pick a `RichTextStyleRow` DataTable,
style your text visually, and output markup the engine's `RichTextBlock` understands.


---

## The problem it solves

Using styles with the engine's built-in `RichTextBlock` normally means:

1. authoring a `RichTextStyleRow` DataTable, and
2. hand-writing markup like `<Title>text</>` into the text field — easy to get wrong, impossible to preview.

This plugin adds a dedicated editor tool (in the spirit of Fab / Bridge plugins) so you can
**select a style table, type, select text, click a style, and see the final result as you go**.

```
Level Editor toolbar button [Rich Text]   +   Window menu → Rich Text Composer   →   dockable tab
```

---

## Installation

### Blueprint-only project (no compiler required) — recommended

1. Copy the whole `RichTextComposer` folder into your project's `Plugins/` directory:

   ```
   MyProject/
   └── Plugins/
       └── RichTextComposer/
           ├── RichTextComposer.uplugin
           ├── Binaries/Win64/UnrealEditor-RichTextComposer.dll   ← precompiled binary
           └── Source/…
   ```

2. Open the project → **Edit → Plugins** → search for `Rich Text` → enable **Rich Text Composer** → restart the editor.

> The shipped binary was built for **UE 5.8 (Windows) Development Editor** and only works with that exact engine build.
> The plugin is an **Editor-only** module, so it is never included in packaged games.

### C++ project / building from source

Set `"Installed": true` to `false` in `RichTextComposer.uplugin`, regenerate project files and build
(`Build.bat <Project>Editor Win64 Development`). No `Binaries/` folder is required — UBT compiles `Source/` for you.

---

## Prerequisite: a `RichTextStyleRow` DataTable

The tool only accepts the engine's `FRichTextStyleRow` structure:

1. Content Browser → right click → **Miscellaneous → Data Table**.
2. Choose **RichTextStyleRow** as the row structure.
3. Add rows: **the row name is the style name (and the markup tag)**, and the row's `Text Style`
   defines font, size, color, shadow, underline/strikethrough, etc.

Naming rule (enforced by the engine's markup parser): **row names may only contain letters, digits,
underscore, dot and hyphen** (`[A-Za-z0-9_.-]`). Rows with spaces or non-ASCII characters are greyed out
in the tool and cannot be used as tags.

---

## Usage (WYSIWYG)

```
┌─ Rich Text Composer ──────────────────────────────────────────────────────┐
│ Style Table [ DT_RichTextStyles ▼ ] [Refresh]   status message…            │
│ ┌── Styles (click to apply) ──┐ ┌── Editor (WYSIWYG) ─────────────────────┐ │
│ │ Title      24pt             │ │ Caret style: Warning  12 chars · 2 runs │ │
│ │ Warning    16pt red         │ │ Background ■ ■ ■ ■  Custom...           │ │
│ │ Note       14pt             │ │ ┌─────────────────────────────────────┐│ │
│ │ [Clear Style][Clear All]    │ │ │ Player <here you type and style>    ││ │
│ └─────────────────────────────┘ │ └─────────────────────────────────────┘│ │
│                                 └─────────────────────────────────────────┘ │
│                                 ┌── Output (read-only, select all to copy)┐ │
│                                 │ Player<Warning>watch out</>, incoming   │ │
│                                 │              ● Changes not output yet   │ │
│                                 │ [Output Markup][Import Markup][Clear]   │ │
│                                 └─────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

1. **Pick a style table** — the dropdown lists only DataTables whose Row Structure is `RichTextStyleRow`.
   Press **Refresh** to rescan the project.
2. **Edit directly** — what you see in the editor *is* the final rendering (no separate preview pane).
3. **Apply a style** — select some text with the mouse, then **click a style name** on the left.
4. **Keep typing** — new characters **inherit the style at the caret** (Word-like behaviour).
5. **Background color** — four presets (Near Black / Dark Gray / Mid Gray / Light) plus **Custom...**
   (opens the color picker). The choice is **saved automatically**; on light backgrounds the default
   (unstyled) text color switches to dark automatically.
6. **Output** — click **Output Markup** to render `<RowName>text</>` into the output box, then select-all and copy.

### Detailed behaviour

| Situation | Behaviour |
|---|---|
| Typing inside already-styled text | New characters **inherit** the style at the caret (the engine extends the run under the cursor) |
| Applying a style over an existing one | **Replace** — the new range overrides the old style, untouched parts keep theirs |
| Selection spanning multiple lines | Output splits it into **one tag per line** (the engine cannot parse tags that span lines) |
| Text containing `& < > "` | Automatically escaped as `&amp; &lt; &gt; &quot;` |
| Row name with spaces / non-ASCII | Greyed out in the list with an explanation; clicking it reports the reason |
| Import Markup | Reads markup from the clipboard and parses it back into the editor (markup attributes are dropped, with a notice) |
| Unknown style names | Preserved on output; rendered as literal text by the engine (a notice appears in the stats) |
| Switching style tables | The editor re-renders with the new styles, **keeping the caret and selection** |

---

## Wiring up the final widget (important step)

For the generated markup to display correctly, the runtime `RichTextBlock` must use the **same DataTable**:

1. Add the engine's **Rich Text Block** to your Widget Blueprint.
2. Details panel → **Appearance → Text Style Set** → pick your `RichTextStyleRow` DataTable.
3. **Content → Text** → paste the markup produced by this tool.

> If you skip step 2, the markup is displayed literally as `<Warning>watch out</>`.

---

## Why the editor can be edited *and* render styles

`SMultiLineEditableText` already supports a multi-run text layout; the plugin supplies a custom
`ITextLayoutMarshaller` (`FStyleMarkupLayoutMarshaller`):

- **Forward**: `<RowName>text</>` is parsed into several `FSlateTextRun`s, each rendered with the
  `FTextBlockStyle` of that row → the editor shows the final styling while you type.
- **Backward**: the runs in the layout are serialized back to markup → the editor content *is* the
  deliverable markup.
- **Style inheritance while typing**: the engine inserts text by extending the run under the caret
  (`FTextLayout::InsertAt`), so new characters automatically match their context.
- **Switching tables**: marking the marshaller dirty plus one tick makes the engine re-marshal with the
  new styles while preserving caret/selection.
- **Background color**: only the text canvas' `FSlateColorBrush::TintColor` changes; the setting is stored in
  `EditorPerProjectUserSettings.ini` (`[RichTextComposer] EditorBackgroundColor`), and the default text
  color is inverted based on background luminance.

---

## Version compatibility (UE 5.2 – 5.8)

The source is written for **5.2 – 5.8** with only two version branches (both based on the helper macro in
`RichTextComposerVersionCompat.h`):

| Location | Branch | Reason |
|---|---|---|
| `RichTextComposerVersionCompat.h` | `RTC_ENGINE_OLDER_THAN(Major, Minor)` | The official `UE_VERSION_OLDER_THAN` takes **three** arguments since 5.7 (Major/Minor/Patch) but two before that, so it cannot be used directly across versions; the macro is redefined on top of `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` |
| `SRichTextComposer.cpp` (FGCObject) | `#if RTC_ENGINE_OLDER_THAN(5, 7)` | Since 5.7 `AddReferencedObject` requires a `TObjectPtr` (raw pointers are unsafe with incremental GC) |

### Verified against the official per-version API docs (5.2 vs 5.7/5.8 — identical)

| API | Used for |
|---|---|
| `ITextLayoutMarshaller` virtuals (`SetText/GetText/RequiresLiveUpdate/MakeDirty/ClearDirty/IsDirty`) | per-run rendering in the editor |
| `SMultiLineEditableText::GetSelection()` → `FTextSelection` | reading the selection (`FRealtimeSelection` is already gone in 5.2) |
| `SMultiLineEditableText::SetTextStyle(const FTextBlockStyle*)` | pushing the default style back in |
| `FSlateTextLayout::Create(SWidget*, FTextBlockStyle)` | automation tests |
| `FAppStyle::Get/GetAppStyleSetName/GetFontStyle/GetBrush` | editor styling (replaced `FEditorStyle` in 5.1) |
| `FARFilter::ClassPaths`, `FAssetData::GetTagValue` | discovering RichTextStyleRow DataTables |
| `FColorPickerArgs` (`OnColorCommitted/bUseAlpha/InitialColor/ParentWidget`) | custom background color picker |
| `SColorBlock` (`Color/Size/UseSRGB`) | preset swatches |
| `SWidget::Invalidate(EInvalidateWidgetReason)` | repainting the canvas |
| `UDataTable::FindRow(FName, const TCHAR*, bool)` | reading style rows |

Everything else (`ToolMenus`, `FSlateStyleSet`, `FTextLayout::InsertAt/GetLineModels`, `FSlateTextRun::Create`,
`RegisterActiveTimer`, `FGCObject`, …) predates 5.2 by years.

> **Tested range**: **5.2 (5.2.1), 5.7 (5.7.4) and 5.8 (5.8.3) are verified** — each of them compiles, passes the
> full automation test suite, and registers the toolbar button / Window entry / dockable tab correctly.
> 5.3 – 5.6 have not been compiled on a real installation; the risky API surface was instead checked
> item by item against the official docs.

### Portability fixes found by building on 5.2

Building on 5.2 surfaced three real incompatibilities (all silently fine on 5.7/5.8):

| Issue | Fix |
|---|---|
| `EAllowShrinking::No` in `FString::RemoveAt` does not exist before 5.5 (5.2 takes a `bool`) | drop the argument and use the default — valid on every version, same semantics |
| `error C4458: declaration of 'Cursor' hides class member` (5.2 treats it as an error) — the local shadowed `SWidget::Cursor` | local renamed to `CaretLocation` |
| same for `WeakThis` | local renamed to `WeakSelf` |

Both fixes are version-agnostic (no `#if` needed), so the same source builds on 5.2 and 5.8 unchanged.

### ⚠️ Do not put `EngineVersion` in the `.uplugin`

If the field is present and does not match the running engine **exactly**, the engine marks the plugin as
*Incompatible* and — unattended — **skips loading it** (verified: setting `5.2.0` made 5.8 skip the plugin
entirely). Omitting the field lets any version load it; compatibility is then decided by the compiler or
by the binary's build id.

### Precompiled binaries vs. engine versions

| Your project | What to do |
|---|---|
| **UE 5.8 + Blueprint-only** | Drop into `Plugins/`, enable, done (`Installed: true` + shipped binary) |
| **UE 5.2 – 5.7** | Set `"Installed": true` → `false` and build once from a C++ project (Blueprint-only projects need a matching precompiled binary instead) |
| **Engine hotfix upgrade** (5.8.0 → 5.8.3) | If loading reports a version mismatch, set `Installed: false` and rebuild |

Shipping "no compiler needed" for every engine version means one binary set per version
(which is why binary plugins on Fab ship per-version folders) — that is packaging work, not code.

---

## Known limitations

- **Row names** are limited to `[A-Za-z0-9_.-]` by the engine parser (no spaces, no non-ASCII).
- **No nesting**: one range can carry one style (engine limitation). For "bold AND red", bake both into a single row.
- **Tags cannot span lines** — the tool splits ranges per line automatically.
- When importing markup, **attributes on tags are dropped** (the engine writer emits `key="value"`; only the tag name is kept).
- Non-style tags (e.g. `<img …/>`) are treated as **plain text** on import, with a status notice, so other decorators are not silently broken.
- Styles come from the DataTable; rows without a font size are marked "No font size" in the list.
- Table discovery uses the `RowStructure` asset registry tag — save a newly created DataTable once, then press **Refresh**.

---

## Automated tests

`RichTextComposer.Markup.Engine` covers:

- markup serialization / reverse parsing round-trips, overlapping replacement, per-line splitting,
  special-character escaping, invalid row names;
- the WYSIWYG machinery: run splitting and tag names, model plain text, reverse serialization,
  **style inheritance when inserting into a styled run**, staying unstyled when inserting elsewhere,
  multi-line round-trips.

```bat
UnrealEditor-Cmd.exe <Project>.uproject ^
  -ExecCmds="Automation RunTests RichTextComposer" ^
  -TestExit="Automation Test Queue Empty" ^
  -unattended -nopause -nullrhi -log
```

---

## Repository layout

```
RichTextComposer/
├── RichTextComposer.uplugin
├── Binaries/Win64/UnrealEditor-RichTextComposer.dll   (precompiled; Blueprint-only projects)
├── Source/RichTextComposer/
│   ├── RichTextComposer.Build.cs
│   ├── Public/
│   │   ├── RichTextComposerModule.h              module: toolbar button / Window menu / dockable tab
│   │   ├── RichTextComposerVersionCompat.h       multi-version helper macro (5.2 – 5.8)
│   │   ├── SRichTextComposer.h                   the tool window
│   │   ├── StyleMarkupEngine.h                   plain logic: markup parsing, serialization, range styling
│   │   └── StyleMarkupLayoutMarshaller.h         per-run rendering for editable text
│   └── Private/
│       ├── RichTextComposerModule.cpp
│       ├── SRichTextComposer.cpp
│       ├── StyleMarkupEngine.cpp
│       ├── StyleMarkupLayoutMarshaller.cpp
│       └── RichTextComposerTests.cpp
└── README.md
```

---

## License

Not specified yet — add a `LICENSE` file before publishing; without one, all rights are reserved by default.
