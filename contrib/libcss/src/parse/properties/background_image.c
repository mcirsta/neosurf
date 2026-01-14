/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2024 The NetSurf Browser Project.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse background-image
 *
 * Supports: none, inherit, initial, revert, unset, url(),
 *           linear-gradient(), radial-gradient()
 *
 * Note: Gradients are parsed but treated as 'none' (fallback behavior).
 *       This allows background-color fallbacks to work correctly.
 *
 * \\param c       Parsing context
 * \\param vector  Vector of tokens to process
 * \\param ctx     Pointer to vector iteration context
 * \\param result  Pointer to location to receive resulting style
 * \\return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_INVALID if the input is not valid
 *
 * Post condition: \\a *ctx is updated with the next token to process
 *                 If the input is invalid, then \\a *ctx remains unchanged.
 */
css_error
css__parse_background_image(css_language *c, const parserutils_vector *vector, int32_t *ctx, css_style *result)
{
    int32_t orig_ctx = *ctx;
    css_error error;
    const css_token *token;
    bool match;

    token = parserutils_vector_iterate(vector, ctx);
    if (token == NULL) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    /* Handle IDENT tokens (none, inherit, initial, revert, unset) */
    if (token->type == CSS_TOKEN_IDENT) {
        if ((lwc_string_caseless_isequal(token->idata, c->strings[INHERIT], &match) == lwc_error_ok && match)) {
            error = css_stylesheet_style_inherit(result, CSS_PROP_BACKGROUND_IMAGE);
        } else if ((lwc_string_caseless_isequal(token->idata, c->strings[NONE], &match) == lwc_error_ok && match)) {
            error = css__stylesheet_style_appendOPV(result, CSS_PROP_BACKGROUND_IMAGE, 0, BACKGROUND_IMAGE_NONE);
        } else if ((lwc_string_caseless_isequal(token->idata, c->strings[INITIAL], &match) == lwc_error_ok && match)) {
            error = css_stylesheet_style_initial(result, CSS_PROP_BACKGROUND_IMAGE);
        } else if ((lwc_string_caseless_isequal(token->idata, c->strings[REVERT], &match) == lwc_error_ok && match)) {
            error = css_stylesheet_style_revert(result, CSS_PROP_BACKGROUND_IMAGE);
        } else if ((lwc_string_caseless_isequal(token->idata, c->strings[UNSET], &match) == lwc_error_ok && match)) {
            error = css_stylesheet_style_unset(result, CSS_PROP_BACKGROUND_IMAGE);
        } else {
            error = CSS_INVALID;
        }

        if (error != CSS_OK)
            *ctx = orig_ctx;
        return error;
    }

    /* Handle URI tokens */
    if (token->type == CSS_TOKEN_URI) {
        lwc_string *uri = NULL;
        uint32_t uri_snumber;

        error = c->sheet->resolve(c->sheet->resolve_pw, c->sheet->url, token->idata, &uri);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        error = css__stylesheet_string_add(c->sheet, uri, &uri_snumber);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        error = css__stylesheet_style_appendOPV(result, CSS_PROP_BACKGROUND_IMAGE, 0, BACKGROUND_IMAGE_URI);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        error = css__stylesheet_style_append(result, uri_snumber);
        if (error != CSS_OK)
            *ctx = orig_ctx;
        return error;
    }

    /* Handle gradient functions - parse content but treat as 'none' */
    if (token->type == CSS_TOKEN_FUNCTION) {
        bool is_gradient = false;

        if ((lwc_string_caseless_isequal(token->idata, c->strings[LINEAR_GRADIENT], &match) == lwc_error_ok && match)) {
            is_gradient = true;
        } else if ((lwc_string_caseless_isequal(token->idata, c->strings[RADIAL_GRADIENT], &match) == lwc_error_ok &&
                       match)) {
            is_gradient = true;
        }

        if (is_gradient) {
            /* Skip gradient function content until closing ')' */
            int depth = 1; /* We're already inside the function */

            while (depth > 0) {
                token = parserutils_vector_iterate(vector, ctx);
                if (token == NULL) {
                    *ctx = orig_ctx;
                    return CSS_INVALID;
                }

                if (token->type == CSS_TOKEN_FUNCTION) {
                    depth++; /* Nested function (e.g., rgb() inside gradient) */
                } else if (token->type == CSS_TOKEN_CHAR && lwc_string_length(token->idata) == 1 &&
                    lwc_string_data(token->idata)[0] == ')') {
                    depth--;
                }
            }

            /* Treat gradient as 'none' - this allows background-color to show through */
            error = css__stylesheet_style_appendOPV(result, CSS_PROP_BACKGROUND_IMAGE, 0, BACKGROUND_IMAGE_NONE);
            if (error != CSS_OK)
                *ctx = orig_ctx;
            return error;
        }
    }

    *ctx = orig_ctx;
    return CSS_INVALID;
}
