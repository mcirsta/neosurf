Rendering core changes since 68fd6df2140a014d463b9f00b98f3342fd33ca30

Scope: 68fd6df2140a014d463b9f00b98f3342fd33ca30..HEAD

Core rendering changes in this range

1) Viewport background expansion rules and absolute border coordinates
   - Core now enforces stricter viewport expansion rules, keyed to fixed positioning and root/body elements, and recalculates border positions for absolute boxes using unscaled coordinates.
   - References:
     - src/content/handlers/html/redraw.c (viewport expansion rules, background clip expansion)
     - src/content/handlers/html/redraw.c (absolute box border coordinate handling)

2) Pseudo-element generation rules
   - Core prevents BOX_NONE pseudo-elements from being added and handles float and inline pseudo-element placement.
   - References:
     - src/content/handlers/html/box_construct.c (pseudo-element creation and float handling)

3) Responsive image selection via srcset
   - Core now parses srcset and selects a URL based on a target width before falling back to src.
   - References:
     - src/content/handlers/html/box_special.c (srcset_select_url and selection logic)

4) SVG intrinsic sizing and viewport-based parsing
   - Core now parses intrinsic SVG dimensions early and delays reformat until a real viewport size is available.
   - References:
     - src/content/handlers/image/svg.c (intrinsic width/height parse and reformat gating)

5) Border drawing refactor
   - Core updated border drawing logic for improved correctness and consistency.
   - References:
     - src/content/handlers/html/redraw_border.c

Qt-synced frontend changes missing in Windows

1) Background repeat tiling alignment and clipping
   - Qt plotter aligns tiled bitmap patterns to the clip rect and uses a tiled pixmap for accurate repeat alignment.
   - Windows plotter tiles by stepping from the initial tile without compensating for clip-aligned offsets, which can shift patterns when the clip region starts mid-tile.
   - References:
     - frontends/qt/plotters.cpp (nsqt_plot_bitmap repeat alignment)
     - frontends/windows/plot.c (bitmap tiling loop without offset alignment)
   - Proposed Windows fix:
     - Compute a fill rectangle from the clip region and adjust the start x/y with an offset modulo tile width/height, matching the Qt offset logic. Use the existing GDI path to draw each tile after alignment.

2) Transform-aware clipping
  - Qt clip implementation accounts for the current transform so the clip rect stays in world coordinates even when a transform is active.
  - Windows clip uses a raw RECT with no transform awareness.
  - SelectClipRgn expects region coordinates in device units, so if a world transform is active (SetWorldTransform), clip coordinates must be transformed to device space before calling SelectClipRgn.
  - References:
    - frontends/qt/plotters.cpp (nsqt_plot_clip inverse transform)
    - frontends/windows/plot.c (clip uses plot_clip directly)
  - Proposed Windows fix:
    - Track the active transform (from push/pop) and map the clip rect to device space before creating the GDI region. If no world transform is set, existing behavior is sufficient.

3) Transform stack support
   - Qt implements push/pop transform, enabling transformed SVG/CSS rendering paths.
   - Windows plotters do not implement push/pop transform hooks.
   - References:
     - frontends/qt/plotters.cpp (nsqt_push_transform/nsqt_pop_transform)
     - frontends/windows/plot.c (win_plotters table has no transform hooks)
  - Proposed Windows fix:
    - Add transform stack support using GDI SetWorldTransform/XForm. Store the stack in plotter state and implement push/pop handlers wired into win_plotters.

4) Gradient plotting
  - Qt provides native linear and radial gradients (SVG and CSS), with path-based fill and transform support.
  - GDI GradientFill supports only linear gradients for rectangles (horizontal/vertical) and triangle meshes with linear interpolation; it does not provide a native radial gradient brush.
  - Windows plotters expose no gradient implementations.
   - References:
     - frontends/qt/plotters.cpp (nsqt_plot_linear_gradient, nsqt_plot_radial_gradient)
     - frontends/windows/plot.c (win_plotters table lacks gradient entries)
  - Proposed Windows fix:
    - Implement only linear gradients with GDI GradientFill. Keep radial gradients on the core fallback path (triangle or rectangle decomposition), and register only the linear gradient hook in win_plotters.

5) Web font (@font-face) loading and native font selection
  - Core now downloads font data for @font-face rules and calls a frontend hook to load the font into the system and mark it available.
  - Qt overrides the core stub and registers a font substitution so CSS family names resolve to the loaded font, then schedules a repaint after loads finish (FOUT strategy).
  - Windows frontend does not override the core stub, so downloaded fonts are never loaded or used.
  - References:
    - src/content/handlers/html/font_face.c (download pipeline and weak stub)
    - src/content/handlers/css/css.c (calls html_font_face_process)
    - frontends/qt/layout.cpp (html_font_face_load_data override, substitution, repaint)
    - frontends/windows/font.c (no @font-face integration)
  - Proposed Windows fix:
    - Implement html_font_face_load_data in the Windows frontend using AddFontMemResourceEx (and RemoveFontMemResourceEx on shutdown if tracked).
    - Register a Windows-side family mapping so CSS family names resolve to the loaded font, or add a lookup in get_font to prefer the loaded family before generic fallbacks.
    - Trigger a repaint when font loads complete, mirroring Qt’s FOUT behavior, by wiring html_font_face_set_done_callback to a frontend repaint callback.

Windows frontend action plan

1) Implement transform-aware clip and transform stack
   - Add plotter state for current transform and stack.
   - Implement push/pop in windows plotter to mirror Qt behavior.
   - Update clip() to inverse-map the clip rect via the current transform.

2) Fix bitmap repeat alignment
   - Rework bitmap() to align tile origin based on clip rect, matching Qt’s offset calculations.

3) Add gradient plotters
  - Add a linear gradient plotter using GDI GradientFill. Leave radial gradients to the core fallback path.

4) Validate against core changes
   - Smoke test viewport background expansion and absolute border placement with pages that rely on fixed backgrounds and transformed elements.
  - Verify @font-face downloads apply to text rendering in Windows by loading a page that requests a custom font and confirming glyphs change after download.
