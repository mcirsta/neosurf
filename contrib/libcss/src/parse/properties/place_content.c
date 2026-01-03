/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2024 Neosurf CSS Grid Support
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse place-content shorthand
 *
 * Syntax: place-content: <align-content> <justify-content>?
 * If only one value, it applies to both align-content and justify-content.
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_INVALID if the input is not valid
 */
css_error css__parse_place_content(css_language *c, const parserutils_vector *vector, int32_t *ctx, css_style *result)
{
    int32_t orig_ctx = *ctx;
    css_error error;
    const css_token *token;
    css_style *align_style = NULL;
    css_style *justify_style = NULL;

    /* place-content: <align-content> <justify-content>? */
    token = parserutils_vector_peek(vector, *ctx);
    if (token == NULL) {
        return CSS_INVALID;
    }

    /* Try to parse align-content first */
    error = css__stylesheet_style_create(c->sheet, &align_style);
    if (error != CSS_OK)
        return error;

    error = css__parse_align_content(c, vector, ctx, align_style);
    if (error != CSS_OK) {
        css__stylesheet_style_destroy(align_style);
        *ctx = orig_ctx;
        return error;
    }

    consumeWhitespace(vector, ctx);

    /* Try to parse justify-content (optional second value) */
    token = parserutils_vector_peek(vector, *ctx);
    if (token != NULL && token->type != CSS_TOKEN_EOF) {
        int32_t temp_ctx = *ctx;

        error = css__stylesheet_style_create(c->sheet, &justify_style);
        if (error != CSS_OK) {
            css__stylesheet_style_destroy(align_style);
            return error;
        }

        error = css__parse_justify_content(c, vector, &temp_ctx, justify_style);
        if (error == CSS_OK) {
            *ctx = temp_ctx;
        } else {
            /* No second value - use align-content value for both */
            css__stylesheet_style_destroy(justify_style);
            justify_style = NULL;
        }
    }

    /* Emit align-content */
    error = css__stylesheet_merge_style(result, align_style);
    css__stylesheet_style_destroy(align_style);
    if (error != CSS_OK) {
        if (justify_style)
            css__stylesheet_style_destroy(justify_style);
        *ctx = orig_ctx;
        return error;
    }

    /* Emit justify-content */
    if (justify_style != NULL) {
        error = css__stylesheet_merge_style(result, justify_style);
        css__stylesheet_style_destroy(justify_style);
    } else {
        /* Use same value for justify-content - re-parse from original
         */
        int32_t reparse_ctx = orig_ctx;
        css_style *justify_copy = NULL;

        error = css__stylesheet_style_create(c->sheet, &justify_copy);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        /* Parse align-content value but store as justify-content */
        error = css__parse_justify_content(c, vector, &reparse_ctx, justify_copy);
        if (error == CSS_OK) {
            error = css__stylesheet_merge_style(result, justify_copy);
        }
        css__stylesheet_style_destroy(justify_copy);
    }

    if (error != CSS_OK)
        *ctx = orig_ctx;

    return error;
}
