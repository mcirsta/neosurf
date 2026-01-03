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
 * Parse gap shorthand
 *
 * Syntax: gap: <row-gap> <column-gap>?
 * If only one value, it applies to both row-gap and column-gap.
 *
 * \param c       Parsing context
 * \param vector  Vector of tokens to process
 * \param ctx     Pointer to vector iteration context
 * \param result  Pointer to location to receive resulting style
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion,
 *         CSS_INVALID if the input is not valid
 *
 * Post condition: \a *ctx is updated with the next token to process
 *                 If the input is invalid, then \a *ctx remains unchanged.
 */
css_error css__parse_gap(css_language *c, const parserutils_vector *vector, int32_t *ctx, css_style *result)
{
    int32_t orig_ctx = *ctx;
    css_error error;
    const css_token *token;
    css_fixed length[2] = {0};
    uint32_t unit[2] = {0};
    uint16_t value[2] = {0};
    int num_lengths = 0;
    enum flag_value flag_value;
    bool match;

    /* gap: <length-percentage> <length-percentage>? | normal | inherit */
    token = parserutils_vector_peek(vector, *ctx);
    if (token == NULL) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    flag_value = get_css_flag_value(c, token);

    if (flag_value != FLAG_VALUE__NONE) {
        /* inherit/initial/unset/revert */
        parserutils_vector_iterate(vector, ctx);
        error = css_stylesheet_style_flag_value(result, flag_value, CSS_PROP_ROW_GAP);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }
        error = css_stylesheet_style_flag_value(result, flag_value, CSS_PROP_COLUMN_GAP);
        if (error != CSS_OK)
            *ctx = orig_ctx;
        return error;
    }

    /* Check for 'normal' keyword */
    if (token->type == CSS_TOKEN_IDENT &&
        lwc_string_caseless_isequal(token->idata, c->strings[NORMAL], &match) == lwc_error_ok && match) {
        parserutils_vector_iterate(vector, ctx);
        value[0] = ROW_GAP_NORMAL;
        value[1] = COLUMN_GAP_NORMAL;
        num_lengths = 0;

        /* Emit row-gap: normal */
        error = css__stylesheet_style_appendOPV(result, CSS_PROP_ROW_GAP, 0, value[0]);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        /* Emit column-gap: normal */
        error = css__stylesheet_style_appendOPV(result, CSS_PROP_COLUMN_GAP, 0, value[1]);
        if (error != CSS_OK)
            *ctx = orig_ctx;
        return error;
    }

    /* Parse first length/percentage */
    error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX, &length[0], &unit[0]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }

    /* Validate unit (no angle, time, freq) */
    if (unit[0] & UNIT_ANGLE || unit[0] & UNIT_TIME || unit[0] & UNIT_FREQ) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    /* Gap must be non-negative */
    if (length[0] < 0) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    num_lengths = 1;
    value[0] = ROW_GAP_SET;

    consumeWhitespace(vector, ctx);

    /* Try to parse second length (column-gap) */
    token = parserutils_vector_peek(vector, *ctx);
    if (token != NULL && token->type != CSS_TOKEN_EOF) {
        int32_t temp_ctx = *ctx;
        error = css__parse_unit_specifier(c, vector, &temp_ctx, UNIT_PX, &length[1], &unit[1]);
        if (error == CSS_OK) {
            if (unit[1] & UNIT_ANGLE || unit[1] & UNIT_TIME || unit[1] & UNIT_FREQ) {
                /* Invalid unit, ignore second value */
            } else if (length[1] < 0) {
                /* Negative, ignore */
            } else {
                *ctx = temp_ctx;
                num_lengths = 2;
                value[1] = COLUMN_GAP_SET;
            }
        }
    }

    /* If only one length, use it for both */
    if (num_lengths == 1) {
        length[1] = length[0];
        unit[1] = unit[0];
        value[1] = COLUMN_GAP_SET;
    }

    /* Emit row-gap */
    error = css__stylesheet_style_appendOPV(result, CSS_PROP_ROW_GAP, 0, value[0]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }
    error = css__stylesheet_style_append(result, length[0]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }
    error = css__stylesheet_style_append(result, unit[0]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }

    /* Emit column-gap */
    error = css__stylesheet_style_appendOPV(result, CSS_PROP_COLUMN_GAP, 0, value[1]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }
    error = css__stylesheet_style_append(result, length[1]);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }
    error = css__stylesheet_style_append(result, unit[1]);
    if (error != CSS_OK)
        *ctx = orig_ctx;

    return error;
}
