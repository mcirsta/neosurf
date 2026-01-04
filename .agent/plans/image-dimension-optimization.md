# Implementation Plan: IMG Width/Height Attribute Optimization

## Goal

Eliminate unnecessary box conversion restarts triggered by IMG elements loading by ensuring HTML `width` and `height` attributes are properly recognized before image fetch completes.

## Problem Analysis

### Current Situation

When images load after initial box construction, they trigger box conversion restarts because the layout engine doesn't know their dimensions initially. This causes:
- Multiple full layout tree rebuilds (2+ restarts observed on hotnews.ro)
- 14+ second page load times
- Poor user experience with layout shifts

### REPLACE_DIM Flag System

The `REPLACE_DIM` flag (set in `box_image()`) tells the layout engine "we know this image's dimensions before it loads, don't restart when it finishes loading."

Currently set only when **CSS dimensions** are specified:
```c
// box_special.c:1135-1142
wtype = css_computed_width(box->style, &value, &wunit);
htype = css_computed_height(box->style, &value, &hunit);

if (wtype == CSS_WIDTH_SET && wunit != CSS_UNIT_PCT &&  
    htype == CSS_HEIGHT_SET && hunit != CSS_UNIT_PCT) {
    box->flags |= REPLACE_DIM;  // Skip restart
}
```

**Problem**: HTML attributes like `<img width="200" height="150">` should also set `REPLACE_DIM`, but currently don't.

### Discovery: Presentational Hints Already Implemented! ✅

**Good news**: Neosurf ALREADY has presentational hint support for IMG width/height!

In [`css/hints.c:1358-1365`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/hints.c#L1358-L1365):
```c
case DOM_HTML_ELEMENT_TYPE_IMG:
    css_hint_margin_hspace_vspace(pw, node);
    /* fall through */
case DOM_HTML_ELEMENT_TYPE_EMBED:
case DOM_HTML_ELEMENT_TYPE_IFRAME:
case DOM_HTML_ELEMENT_TYPE_OBJECT:
    css_hint_height(pw, node);  // ⭐ Already calls this!
    css_hint_width(pw, node);   // ⭐ Already calls this!
```

The helper functions ([`css_hint_width()`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/hints.c#L941-L958) and [`css_hint_height()`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/hints.c#L922-L939)) properly parse HTML attributes and convert to CSS hints.

## Root Cause Investigation Required

**Critical Question**: If hints are already implemented, why isn't `REPLACE_DIM` being set?

### Hypothesis 1: Timing Issue
Hints might be applied AFTER `box_image()` checks for dimensions. Need to verify call order:
1. Does `box_image()` run before or after hint application?
2. Are hints applied during box construction or during CSS selection?

### Hypothesis 2: Style Computation Issue  
`css_computed_width/height()` might not see the presentational hints. Possible causes:
- H

ints have lower priority and are overridden by user/author CSS
- Hints aren't being added to the computed style correctly
- Issue with how libcss merges hints into computed styles

### Hypothesis 3: Parse Dimension Issue
`parse_dimension()` might be failing to parse certain attribute formats:
- `width="200"` (no unit) - should default to pixels
- `width="50%"` (percentage) - should be detected and excluded from REPLACE_DIM
- Invalid formats causing hints to be silently dropped

## Implementation Plan

### Phase 1: Diagnosis (30-60 minutes)

**Goal**: Understand why existing hints don't work

**Tasks**:
1. **Add debug logging** to verify hint application
   - Log in `css_hint_width()` when width attribute is found
   - Log in `css_hint_height()` when height attribute is found
   - Log in `node_presentational_hint()` to see total hints generated
   
2. **Add debug logging** in `box_image()` to see computed values
   - Log what `css_computed_width()` returns
   - Log what `css_computed_height()` returns  
   - Log whether `REPLACE_DIM` is set

3. **Test on simple HTML** with known dimensions:
   ```html
   <img src="test.png" width="200" height="150">
   ```

4. **Analyze logs** to find where the chain breaks

**Expected outcome**: Identify exact point where width/height information is lost

### Phase 2: Fix Implementation (1-3 hours)

Based on diagnosis, implement appropriate fix:

#### Option A: If hints aren't being seen by css_computed_*() 

**Root cause**: Priority or merging issue in libcss

**Solution**: Investigate libcss selection code to ensure hints are properly merged

**Files to modify**:
- Possibly `src/content/handlers/css/select.c`
- May need to check libcss configuration

#### Option B: If parse_dimension() is failing

**Root cause**: Attribute parsing doesn't handle unitless numbers

**Solution**: Fix `parse_dimension()` to default to pixels for unitless values

**Files to modify**:
- [`src/content/handlers/css/hints.c`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/hints.c) - `parse_dimension()`

**Change**:
```c
// Current: strict mode might reject unitless numbers
parse_dimension(data, false, &value, &unit)

// Might need: always default to PX for unitless IMG dimensions
if (parse_number(data, false, true, &value, &consumed)) {
    *unit = CSS_UNIT_PX;  // Default for HTML attributes
    return true;
}
```

#### Option C: If timing issue (hints applied too late)

**Root cause**: `box_image()` runs before hints are applied to style

**Solution**: Re-check dimensions in `box_image()` after fetching object, or check attributes directly

**Files to modify**:
- [`src/content/handlers/html/box_special.c`](file:///mnt/netac/proj/neosurf/src/content/handlers/html/box_special.c) - `box_image()`

**Change**: Add fallback to directly check HTML attributes:
```c
// In box_image(), after css_computed_width/height check fails:
if (!(box->flags & REPLACE_DIM)) {
    // Fallback: check HTML attributes directly
    dom_string *width_attr, *height_attr;
    // ... check attributes and parse dimensions ...
    // If both valid and non-percentage, set REPLACE_DIM
}
```

### Phase 3: Testing & Verification (30 minutes)

1. **Unit test**: Simple HTML with IMG width/height attributes
2. **Real-world test**: hotnews.ro - verify restart count reduced
3. **Performance measurement**: Compare load times before/after
4. **Regression testing**: Ensure CSS dimensions still work

**Success criteria**:
- `REPLACE_DIM` set for `<img width="200" height="150">`
- Hotnews.ro: **2 restarts → 0-1 restart**
- Load time: **~14s → ~5-7s**

## Files Involved

### Investigation Files
- [`src/content/handlers/css/hints.c`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/hints.c) - Presentational hints
- [`src/content/handlers/html/box_special.c`](file:///mnt/netac/proj/neosurf/src/content/handlers/html/box_special.c) - Box construction for IMG
- [`src/content/handlers/css/select.c`](file:///mnt/netac/proj/neosurf/src/content/handlers/css/select.c) - CSS selection with hints

### Likely Modification Points
- **Primary**: `hints.c` - Fix dimension parsing if needed
- **Secondary**: `box_special.c` - Fallback dimension check if needed
- **Unlikely**: `select.c` - Only if libcss integration issue

## Risk Assessment

**Low Risk**: 
- Hints infrastructure already exists
- Only need to fix dimension recognition
- Changes are isolated to IMG handling

**Potential Issues**:
- Percentage dimensions need special handling (shouldn't set REPLACE_DIM)
- Inherited/computed dimensions might conflict
- Need to ensure CSS can still override HTML attributes

## Timeline

- **Diagnosis**: 30-60 min
- **Implementation**: 1-3 hours (depends on root cause)
- **Testing**: 30 min
- **Total**: 2-5 hours

## Success Metrics

1. ✅ `REPLACE_DIM` flag set for images with HTML width/height attributes
2. ✅ CSS dimensions still work (no regression)
3. ✅ Restart count reduced on hotnews.ro (2 → 0-1)
4. ✅ Page load time improved (14s → 5-7s)
5. ✅ No visual regressions (images display at correct size)
