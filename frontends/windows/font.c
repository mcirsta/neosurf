/*
 * Copyright 2009 - 2014 Vincent Sanders <vince@netsurf-browser.org>
 * Copyright 2009 - 2013 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Windows font handling and character encoding implementation.
 */

#include "neosurf/utils/config.h"
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <windows.h>

#include <libwapcaplet/libwapcaplet.h>

#include "neosurf/inttypes.h"
#include "neosurf/layout.h"
#include "neosurf/plot_style.h"
#include "neosurf/utf8.h"
#include "neosurf/utils/errors.h"
#include "neosurf/utils/log.h"
#include "neosurf/utils/nsoption.h"
#include "neosurf/utils/utf8.h"

#include "utils/hashmap.h"
#include "windows/font.h"

#define SPLIT_CACHE_MAX_ENTRIES 16384
#define WSTR_CACHE_MAX_BYTES (16 * 1024 * 1024)
#define FONT_CACHE_MAX_ENTRIES 256

HWND font_hwnd;

/* Cached memory DC for text measurement */
static HDC s_text_hdc = NULL;

static HDC get_text_hdc(void)
{
    if (s_text_hdc == NULL) {
        s_text_hdc = CreateCompatibleDC(NULL);
        SetMapMode(s_text_hdc, MM_TEXT);
    }
    return s_text_hdc;
}

/* Font cache keyed by family/size/weight/flags */
typedef struct {
    int family;
    int size;
    int weight;
    int flags;
    char *face;
} font_key_t;

static void *fc_key_clone(void *key)
{
    font_key_t *src = (font_key_t *)key;
    font_key_t *k = malloc(sizeof(font_key_t));
    if (k == NULL) {
        return NULL;
    }
    
    /* Initialize all fields first */
    k->family = src->family;
    k->size = src->size;
    k->weight = src->weight;
    k->flags = src->flags;
    k->face = NULL;
    
    /* Then duplicate the face string if needed */
    if (src->face != NULL) {
        k->face = strdup(src->face);
        if (k->face == NULL) {
            free(k);
            return NULL;
        }
    }
    return k;
}

static void fc_key_destroy(void *key)
{
    font_key_t *k = (font_key_t *)key;
    if (k->face != NULL) {
        free(k->face);
    }
    free(key);
}

static uint32_t fc_key_hash(void *key)
{
    font_key_t *k = (font_key_t *)key;
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)k->family) * 16777619u;
    h = (h ^ (uint32_t)k->size) * 16777619u;
    h = (h ^ (uint32_t)k->weight) * 16777619u;
    h = (h ^ (uint32_t)k->flags) * 16777619u;
    if (k->face != NULL) {
        /* Limit face name length to prevent excessive hashing */
        const char *p = k->face;
        int max_len = 256;  /* Reasonable limit for font face names */
        int i = 0;
        while (*p != '\0' && i < max_len) {
            h = (h ^ (uint32_t)tolower((unsigned char)*p)) * 16777619u;
            p++;
            i++;
        }
    }
    return h;
}

static bool fc_key_eq(void *a, void *b)
{
    font_key_t *ka = (font_key_t *)a;
    font_key_t *kb = (font_key_t *)b;
    
    if (ka->family != kb->family || ka->size != kb->size || ka->weight != kb->weight || ka->flags != kb->flags) {
        return false;
    }
    
    /* Handle NULL face pointers consistently */
    if (ka->face == NULL && kb->face == NULL) {
        return true;
    }
    if (ka->face == NULL || kb->face == NULL) {
        return false;
    }
    
    /* Use same case-insensitive comparison as hash function */
    return strcasecmp(ka->face, kb->face) == 0;
}

typedef struct {
    HFONT font;
    uint64_t gen;
} font_value_t;

static void *fc_value_alloc(void *key)
{
    font_value_t *fv = malloc(sizeof(font_value_t));
    if (fv != NULL) {
        fv->font = NULL;
        fv->gen = 0;
    }
    return fv;
}

static void fc_value_destroy(void *value)
{
    if (value != NULL) {
        font_value_t *fv = (font_value_t *)value;
        if (fv->font != NULL) {
            DeleteObject(fv->font);
        }
        free(value);
    }
}

static hashmap_parameters_t font_cache_params = {
    fc_key_clone, fc_key_hash, fc_key_eq, fc_key_destroy, fc_value_alloc, fc_value_destroy};

static hashmap_t *font_cache = NULL;
static uint64_t font_gen = 0;

typedef struct {
    int family;
    int size;
    int weight;
    int flags;
    int x;
    size_t len;
    char *str;
} split_key_t;

typedef struct {
    size_t offset;
    int actual_x;
    uint64_t gen;
} split_value_t;

static uint64_t split_gen = 0;

static void *sc_key_clone(void *key)
{
    split_key_t *src = (split_key_t *)key;
    split_key_t *k = malloc(sizeof(split_key_t));
    if (k == NULL) {
        return NULL;
    }
    *k = *src;
    if (src->len > 0) {
        k->str = malloc(src->len);
        if (k->str == NULL) {
            free(k);
            return NULL;
        }
        memcpy(k->str, src->str, src->len);
    } else {
        k->str = NULL;
    }
    return k;
}

static void sc_key_destroy(void *key)
{
    split_key_t *k = (split_key_t *)key;
    if (k->str != NULL) {
        free(k->str);
    }
    free(k);
}

static uint32_t sc_key_hash(void *key)
{
    split_key_t *k = (split_key_t *)key;
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)k->family) * 16777619u;
    h = (h ^ (uint32_t)k->size) * 16777619u;
    h = (h ^ (uint32_t)k->weight) * 16777619u;
    h = (h ^ (uint32_t)k->flags) * 16777619u;
    h = (h ^ (uint32_t)k->x) * 16777619u;
    h = (h ^ (uint32_t)k->len) * 16777619u;
    if (k->str != NULL && k->len > 0) {
        for (size_t i = 0; i < k->len; i++) {
            h = (h ^ (uint32_t)(unsigned char)k->str[i]) * 16777619u;
        }
    }
    return h;
}

static bool sc_key_eq(void *a, void *b)
{
    split_key_t *ka = (split_key_t *)a;
    split_key_t *kb = (split_key_t *)b;
    if (ka->family != kb->family || ka->size != kb->size || ka->weight != kb->weight || ka->flags != kb->flags ||
        ka->x != kb->x || ka->len != kb->len) {
        return false;
    }
    if (ka->len == 0) {
        return true;
    }
    if (ka->str == NULL || kb->str == NULL) {
        return ka->str == kb->str;
    }
    return memcmp(ka->str, kb->str, ka->len) == 0;
}

static void *sc_value_alloc(void *key)
{
    return malloc(sizeof(split_value_t));
}

static void sc_value_destroy(void *value)
{
    if (value != NULL) {
        free(value);
    }
}

static hashmap_parameters_t split_cache_params = {
    sc_key_clone, sc_key_hash, sc_key_eq, sc_key_destroy, sc_value_alloc, sc_value_destroy};

static hashmap_t *split_cache = NULL;

typedef struct {
    size_t len;
    char *str;
} wstr_key_t;

typedef struct {
    WCHAR *wstr;
    int wclen;
    size_t bytes;
    uint64_t gen;
} wstr_value_t;

static uint64_t wstr_gen = 0;
static size_t wstr_total_bytes = 0;
static hashmap_t *wstr_cache = NULL;

static bool wstr_iter_oldest_cb(void *key, void *value, void *ctx)
{
    wstr_value_t *v = (wstr_value_t *)value;
    void **out = (void **)ctx;
    if (out[0] == NULL || v->gen < ((wstr_value_t *)out[1])->gen) {
        out[0] = key;
        out[1] = value;
    }
    return false;
}

static void wstr_cache_evict_if_needed(void)
{
    while (wstr_total_bytes > WSTR_CACHE_MAX_BYTES && wstr_cache != NULL) {
        void *ctx[2] = {NULL, NULL};
        hashmap_iterate(wstr_cache, wstr_iter_oldest_cb, ctx);
        if (ctx[0] == NULL || ctx[1] == NULL)
            break;
        wstr_value_t *ov = (wstr_value_t *)ctx[1];
        size_t obytes = ov->bytes;
        hashmap_remove(wstr_cache, ctx[0]);
        if (wstr_total_bytes >= obytes) {
            wstr_total_bytes -= obytes;
        } else {
            wstr_total_bytes = 0;
        }
    }
}

static bool split_iter_oldest_cb(void *key, void *value, void *ctx)
{
    split_value_t *v = (split_value_t *)value;
    void **out = (void **)ctx;
    if (out[0] == NULL || v->gen < ((split_value_t *)out[1])->gen) {
        out[0] = key;
        out[1] = value;
    }
    return false;
}

static void split_cache_evict_if_needed(void)
{
    if (split_cache == NULL)
        return;
    size_t count = hashmap_count(split_cache);
    if (count <= SPLIT_CACHE_MAX_ENTRIES)
        return;
    size_t target = (SPLIT_CACHE_MAX_ENTRIES * 3) / 4;
    size_t to_remove = count - target;
    for (size_t i = 0; i < to_remove; i++) {
        void *ctx[2] = {NULL, NULL};
        hashmap_iterate(split_cache, split_iter_oldest_cb, ctx);
        if (ctx[0] == NULL)
            break;
        hashmap_remove(split_cache, ctx[0]);
    }
}

static bool font_iter_oldest_cb(void *key, void *value, void *ctx)
{
    font_value_t *v = (font_value_t *)value;
    void **out = (void **)ctx;
    if (out[0] == NULL || v->gen < ((font_value_t *)out[1])->gen) {
        out[0] = key;
        out[1] = value;
    }
    return false;
}

static void font_cache_evict_if_needed(void)
{
    if (font_cache == NULL)
        return;
    size_t count = hashmap_count(font_cache);
    if (count <= FONT_CACHE_MAX_ENTRIES)
        return;
    size_t target = (FONT_CACHE_MAX_ENTRIES * 3) / 4;
    size_t to_remove = count - target;
    for (size_t i = 0; i < to_remove; i++) {
        void *ctx[2] = {NULL, NULL};
        hashmap_iterate(font_cache, font_iter_oldest_cb, ctx);
        if (ctx[0] == NULL)
            break;
        hashmap_remove(font_cache, ctx[0]);
    }
}

static void *wc_key_clone(void *key)
{
    wstr_key_t *src = (wstr_key_t *)key;
    wstr_key_t *k = malloc(sizeof(wstr_key_t));
    if (k == NULL) {
        return NULL;
    }
    k->len = src->len;
    if (src->len > 0) {
        k->str = malloc(src->len);
        if (k->str == NULL) {
            free(k);
            return NULL;
        }
        memcpy(k->str, src->str, src->len);
    } else {
        k->str = NULL;
    }
    return k;
}

static void wc_key_destroy(void *key)
{
    wstr_key_t *k = (wstr_key_t *)key;
    if (k->str != NULL) {
        free(k->str);
    }
    free(k);
}

static uint32_t wc_key_hash(void *key)
{
    wstr_key_t *k = (wstr_key_t *)key;
    uint32_t h = 2166136261u;
    h = (h ^ (uint32_t)k->len) * 16777619u;
    if (k->str != NULL && k->len > 0) {
        for (size_t i = 0; i < k->len; i++) {
            h = (h ^ (uint32_t)(unsigned char)k->str[i]) * 16777619u;
        }
    }
    return h;
}

static bool wc_key_eq(void *a, void *b)
{
    wstr_key_t *ka = (wstr_key_t *)a;
    wstr_key_t *kb = (wstr_key_t *)b;
    if (ka->len != kb->len) {
        return false;
    }
    if (ka->len == 0) {
        return true;
    }
    if (ka->str == NULL || kb->str == NULL) {
        return ka->str == kb->str;
    }
    return memcmp(ka->str, kb->str, ka->len) == 0;
}

static void *wc_value_alloc(void *key)
{
    return malloc(sizeof(wstr_value_t));
}

static void wc_value_destroy(void *value)
{
    if (value != NULL) {
        wstr_value_t *v = (wstr_value_t *)value;
        if (v->wstr != NULL) {
            free(v->wstr);
        }
        free(value);
    }
}

static hashmap_parameters_t wstr_cache_params = {
    wc_key_clone, wc_key_hash, wc_key_eq, wc_key_destroy, wc_value_alloc, wc_value_destroy};


static nserror get_cached_wide(const char *utf8str, size_t utf8len, const WCHAR **out_wstr, int *out_wclen)
{
    if (utf8len == 0) {
        *out_wstr = NULL;
        *out_wclen = 0;
        return NSERROR_OK;
    }
    if (wstr_cache == NULL) {
        wstr_cache = hashmap_create(&wstr_cache_params);
        if (wstr_cache == NULL) {
            return NSERROR_NOMEM;
        }
    }
    wstr_key_t key;
    key.len = utf8len;
    key.str = (char *)utf8str;
    void *slot = hashmap_lookup(wstr_cache, &key);
    if (slot != NULL) {
        wstr_value_t *val = (wstr_value_t *)slot;
        val->gen = ++wstr_gen;
        *out_wstr = val->wstr;
        *out_wclen = val->wclen;
        return NSERROR_OK;
    }
    int alloc = (int)(utf8len);
    if (alloc <= 0)
        alloc = 1;
    WCHAR *buf = (WCHAR *)malloc(sizeof(WCHAR) * alloc);
    if (buf == NULL) {
        return NSERROR_NOMEM;
    }
    int wclen = MultiByteToWideChar(CP_UTF8, 0, utf8str, (int)utf8len, buf, alloc);
    if (wclen == 0) {
        free(buf);
        return NSERROR_NOSPACE;
    }
    void *islot = hashmap_insert(wstr_cache, &key);
    if (islot == NULL) {
        free(buf);
        return NSERROR_NOMEM;
    }
    wstr_value_t *ival = (wstr_value_t *)islot;
    ival->wstr = buf;
    ival->wclen = wclen;
    ival->bytes = (size_t)wclen * sizeof(WCHAR);
    ival->gen = ++wstr_gen;
    wstr_total_bytes += ival->bytes;
    wstr_cache_evict_if_needed();
    *out_wstr = buf;
    *out_wclen = wclen;
    return NSERROR_OK;
}

static int CALLBACK font_enum_proc(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lParam)
{
    int *found = (int *)lParam;
    (void)lf;
    (void)tm;
    (void)type;
    *found = 1;
    return 0;
}

static bool win32_font_family_exists(const char *family_name)
{
    LOGFONTW lf;
    int found = 0;

    if (family_name == NULL || family_name[0] == '\0') {
        return false;
    }

    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    if (MultiByteToWideChar(CP_UTF8, 0, family_name, -1, lf.lfFaceName, LF_FACESIZE) == 0) {
        return false;
    }

    EnumFontFamiliesExW(get_text_hdc(), &lf, font_enum_proc, (LPARAM)&found, 0);
    return found != 0;
}

static const char *select_face_name(const plot_font_style_t *style, DWORD *out_family)
{
    const char *face = NULL;

    if (style->families != NULL) {
        lwc_string *const *families = style->families;
        while (*families != NULL) {
            const char *candidate = lwc_string_data(*families);
            if (win32_font_family_exists(candidate)) {
                if (out_family != NULL) {
                    *out_family = FF_DONTCARE | DEFAULT_PITCH;
                }
                return candidate;
            }
            families++;
        }
    }

    switch (style->family) {
    case PLOT_FONT_FAMILY_SERIF:
        face = nsoption_charp(font_serif);
        if (out_family != NULL) {
            *out_family = FF_ROMAN | DEFAULT_PITCH;
        }
        break;
    case PLOT_FONT_FAMILY_MONOSPACE:
        face = nsoption_charp(font_mono);
        if (out_family != NULL) {
            *out_family = FF_MODERN | DEFAULT_PITCH;
        }
        break;
    case PLOT_FONT_FAMILY_CURSIVE:
        face = nsoption_charp(font_cursive);
        if (out_family != NULL) {
            *out_family = FF_SCRIPT | DEFAULT_PITCH;
        }
        break;
    case PLOT_FONT_FAMILY_FANTASY:
        face = nsoption_charp(font_fantasy);
        if (out_family != NULL) {
            *out_family = FF_DECORATIVE | DEFAULT_PITCH;
        }
        break;
    case PLOT_FONT_FAMILY_SANS_SERIF:
    default:
        face = nsoption_charp(font_sans);
        if (out_family != NULL) {
            *out_family = FF_SWISS | DEFAULT_PITCH;
        }
        break;
    }

    return face;
}

static HFONT get_cached_font(const plot_font_style_t *style)
{
    font_key_t key;
    void *slot;
    HFONT font;
    const char *face;

    face = select_face_name(style, NULL);
    key.family = style->family;
    key.size = style->size;
    key.weight = style->weight;
    key.flags = style->flags;
    key.face = (char *)face;  /* Safe: face points to static config strings or NULL */

    if (font_cache == NULL) {
        font_cache = hashmap_create(&font_cache_params);
        if (font_cache == NULL) {
            return NULL;
        }
    }

    slot = hashmap_lookup(font_cache, &key);
    if (slot != NULL) {
        font_value_t *fv = (font_value_t *)slot;
        font = fv->font;
        if (font != NULL) {
            fv->gen = ++font_gen;
            return font;
        }
    }

    font = get_font(style);
    if (font != NULL) {
        slot = hashmap_insert(font_cache, &key);
        if (slot != NULL) {
            font_value_t *fv = (font_value_t *)slot;
            fv->font = font;
            fv->gen = ++font_gen;
            font_cache_evict_if_needed();
        }
    }
    return font;
}

/* exported interface documented in windows/font.h */
nserror utf8_to_font_encoding(const struct font_desc *font, const char *string, size_t len, char **result)
{
    return utf8_to_enc(string, font->encoding, len, result);
}


/**
 * Convert a string to UCS2 from UTF8
 *
 * \param[in] string source string
 * \param[in] len source string length
 * \param[out] result The UCS2 string
 */
static nserror utf8_to_local_encoding(const char *string, size_t len, char **result)
{
    return utf8_to_enc(string, "UCS-2", len, result);
}


/**
 * Convert a string to UTF8 from local encoding
 *
 * \param[in] string source string
 * \param[in] len source string length
 * \param[out] result The UTF8 string
 */
static nserror utf8_from_local_encoding(const char *string, size_t len, char **result)
{
    assert(string && result);

    if (len == 0) {
        len = strlen(string);
    }

    *result = strndup(string, len);
    if (!(*result)) {
        return NSERROR_NOMEM;
    }

    return NSERROR_OK;
}


/* exported interface documented in windows/font.h */
HFONT get_font(const plot_font_style_t *style)
{
    const char *face = NULL;
    DWORD family;
    int nHeight;
    HDC hdc;
    HFONT font;

    face = select_face_name(style, &family);
    if (face == NULL) {
        face = "";
        family = FF_DONTCARE | DEFAULT_PITCH;
    }

    nHeight = -10;

    hdc = GetDC(font_hwnd);
    nHeight = -MulDiv(style->size, GetDeviceCaps(hdc, LOGPIXELSY), 72 * PLOT_STYLE_SCALE);
    ReleaseDC(font_hwnd, hdc);

    font = CreateFont(nHeight, 0, 0, 0, style->weight, (style->flags & FONTF_ITALIC) ? TRUE : FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, family, face);

    if (font == NULL) {
        if (style->family == PLOT_FONT_FAMILY_MONOSPACE) {
            font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
        } else {
            font = (HFONT)GetStockObject(ANSI_VAR_FONT);
        }
    }

    if (font == NULL) {
        font = (HFONT)GetStockObject(SYSTEM_FONT);
    }

    return font;
}

/* size of temporary wide character string for computing string width */
#define WSTRLEN 4096


/**
 * Measure the width of a string.
 *
 * \param[in] style plot style for this text
 * \param[in] utf8str string encoded in UTF-8 to measure
 * \param[in] utf8len length of string, in bytes
 * \param[out] width updated to width of string[0..length)
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror win32_font_width(const plot_font_style_t *style, const char *utf8str, size_t utf8len, int *width)
{
    nserror ret = NSERROR_OK;
    HDC hdc;
    HFONT font;
    HFONT fontbak;
    SIZE sizl;
    BOOL wres;
    const WCHAR *wstr;
    int wclen;

    if (utf8len == 0) {
        *width = 0;
        return ret;
    }

    hdc = get_text_hdc();
    font = get_cached_font(style);
    fontbak = SelectObject(hdc, font);

    ret = get_cached_wide(utf8str, utf8len, &wstr, &wclen);
    if (ret == NSERROR_OK) {
        wres = GetTextExtentPoint32W(hdc, wstr, wclen, &sizl);
        if (wres == FALSE) {
            ret = NSERROR_INVALID;
        } else {
            *width = sizl.cx;
        }
    }

    SelectObject(hdc, fontbak);

    return ret;
}


/**
 * Find the position in a string where an x coordinate falls.
 *
 * \param  style css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
 * \param  utf8str string to measure encoded in UTF-8
 * \param  utf8len length of string
 * \param  x coordinate to search for
 * \param  char_offset updated to offset in string of actual_x, [0..length]
 * \param  actual_x updated to x coordinate of character closest to x
 * \return NSERROR_OK on success otherwise appropriate error code
 */
static nserror win32_font_position(
    const plot_font_style_t *style, const char *utf8str, size_t utf8len, int x, size_t *char_offset, int *actual_x)
{
    HDC hdc;
    HFONT font;
    HFONT fontbak;
    SIZE s;
    nserror ret = NSERROR_OK;

    /* deal with zero length input or invalid search co-ordiate */
    if ((utf8len == 0) || (x < 1)) {
        *char_offset = 0;
        *actual_x = 0;
        return ret;
    }

    hdc = get_text_hdc();
    font = get_cached_font(style);
    fontbak = SelectObject(hdc, font);

    const WCHAR *wstr;
    int wclen;
    ret = get_cached_wide(utf8str, utf8len, &wstr, &wclen);
    if (ret == NSERROR_OK) {
        int fit = 0;
        if ((GetTextExtentExPointW(hdc, wstr, wclen, x, &fit, NULL, &s) != 0) &&
            (GetTextExtentPoint32W(hdc, wstr, fit, &s) != 0)) {
            size_t boff = utf8_bounded_byte_length(utf8str, utf8len, (size_t)fit);
            *char_offset = boff;
            *actual_x = s.cx;
        } else {
            ret = NSERROR_UNKNOWN;
        }
    }

    SelectObject(hdc, fontbak);

    return ret;
}


/**
 * Find where to split a string to make it fit a width.
 *
 * \param  style	css_style for this text, with style->font_size.size ==
 *			CSS_FONT_SIZE_LENGTH
 * \param  string	UTF-8 string to measure
 * \param  length	length of string
 * \param  x		width available
 * \param[out] offset updated to offset in string of actual_x, [0..length]
 * \param  actual_x	updated to x coordinate of character closest to x
 * \return NSERROR_OK on success otherwise appropriate error code
 *
 * On exit, [char_offset == 0 ||
 *	   string[char_offset] == ' ' ||
 *	   char_offset == length]
 */
static nserror win32_font_split(
    const plot_font_style_t *style, const char *string, size_t length, int x, size_t *offset, int *actual_x)
{
    nserror res;
    int c_off;

    if (split_cache == NULL) {
        split_cache = hashmap_create(&split_cache_params);
    }
    if (split_cache != NULL) {
        split_key_t key;
        key.family = style->family;
        key.size = style->size;
        key.weight = style->weight;
        key.flags = style->flags;
        key.x = x;
        key.len = length;
        key.str = (char *)string;
        void *slot = hashmap_lookup(split_cache, &key);
        if (slot != NULL) {
            split_value_t *val = (split_value_t *)slot;
            val->gen = ++split_gen;
            *offset = val->offset;
            *actual_x = val->actual_x;
            return NSERROR_OK;
        }
        res = win32_font_position(style, string, length, x, offset, actual_x);
        if (res != NSERROR_OK) {
            return res;
        }
        if (*offset == length) {
            void *islot = hashmap_insert(split_cache, &key);
            if (islot != NULL) {
                split_value_t *ival = (split_value_t *)islot;
                ival->offset = *offset;
                ival->actual_x = *actual_x;
                ival->gen = ++split_gen;
                split_cache_evict_if_needed();
            }
            return NSERROR_OK;
        }
        c_off = *offset;
        while ((string[*offset] != ' ') && (*offset > 0)) {
            (*offset)--;
        }
        if (*offset == 0) {
            *offset = c_off;
            while ((*offset < length) && (string[*offset] != ' ')) {
                (*offset)++;
            }
        }
        res = win32_font_width(style, string, *offset, actual_x);
        if (res == NSERROR_OK) {
            void *islot2 = hashmap_insert(split_cache, &key);
            if (islot2 != NULL) {
                split_value_t *ival2 = (split_value_t *)islot2;
                ival2->offset = *offset;
                ival2->actual_x = *actual_x;
                ival2->gen = ++split_gen;
                split_cache_evict_if_needed();
            }
        }
        return res;
    }

    res = win32_font_position(style, string, length, x, offset, actual_x);
    if (res != NSERROR_OK) {
        return res;
    }
    if (*offset == length) {
        return NSERROR_OK;
    }
    c_off = *offset;
    while ((string[*offset] != ' ') && (*offset > 0)) {
        (*offset)--;
    }
    if (*offset == 0) {
        *offset = c_off;
        while ((*offset < length) && (string[*offset] != ' ')) {
            (*offset)++;
        }
    }
    res = win32_font_width(style, string, *offset, actual_x);
    return res;
}

void win32_font_caches_flush(void)
{
    if (split_cache != NULL) {
        hashmap_destroy(split_cache);
        split_cache = NULL;
    }
    if (wstr_cache != NULL) {
        hashmap_destroy(wstr_cache);
        wstr_cache = NULL;
    }
    wstr_total_bytes = 0;
    wstr_gen = 0;
    split_gen = 0;
}

nserror html_font_face_load_data(const char *family_name, const uint8_t *data, size_t size)
{
    DWORD num_fonts = 0;
    HANDLE font_handle;

    /* Validate input parameters */
    if (family_name == NULL || data == NULL || size == 0) {
        return NSERROR_BAD_PARAMETER;
    }

    /* Check for reasonable size limits to prevent memory issues */
    if (size > 50 * 1024 * 1024) {  /* 50MB limit for font files */
        NSLOG(netsurf, WARNING, "Font '%s' size %zu exceeds reasonable limit", family_name, size);
        return NSERROR_BAD_PARAMETER;
    }

    font_handle = AddFontMemResourceEx((PVOID)data, (DWORD)size, NULL, &num_fonts);
    if (font_handle == NULL || num_fonts == 0) {
        NSLOG(netsurf, WARNING, "Failed to load font '%s' into Windows (error=%lu)", family_name, GetLastError());
        return NSERROR_INVALID;
    }

    win32_font_caches_flush();
    return NSERROR_OK;
}


/** win32 font operations table */
static struct gui_layout_table layout_table = {
    .width = win32_font_width,
    .position = win32_font_position,
    .split = win32_font_split,
};

struct gui_layout_table *win32_layout_table = &layout_table;

/** win32 utf8 encoding operations table */
static struct gui_utf8_table utf8_table = {
    .utf8_to_local = utf8_to_local_encoding,
    .local_to_utf8 = utf8_from_local_encoding,
};

struct gui_utf8_table *win32_utf8_table = &utf8_table;
