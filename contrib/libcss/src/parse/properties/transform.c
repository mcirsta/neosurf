/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 Neosurf Contributors
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse a single transform function
 *
 * \param c        Parsing context
 * \param vector   Vector of tokens to process
 * \param ctx      Pointer to vector iteration context
 * \param result   Pointer to location to receive resulting style
 * \param func_type  Transform function type (TRANSFORM_TRANSLATE, etc.)
 * \param num_args   Number of arguments expected (1 or 2)
 * \param allow_pct  Whether percentage values are allowed
 * \param is_angle   Whether values are angles (for rotate)
 * \return CSS_OK on success, CSS_INVALID if input is not valid
 */
static css_error parse_transform_function(css_language *c, const parserutils_vector *vector, int32_t *ctx,
    css_style *result, uint32_t func_type, int num_args, bool allow_pct, bool is_angle)
{
    css_error error;
    css_fixed value1 = 0, value2 = 0;
    uint32_t unit1 = 0, unit2 = 0;
    const css_token *token;
    int32_t orig_ctx = *ctx;
    bool is_scale = (func_type == TRANSFORM_SCALE || func_type == TRANSFORM_SCALEX || func_type == TRANSFORM_SCALEY);

    /* Consume opening parenthesis - already consumed by caller */

    /* Parse first argument */
    consumeWhitespace(vector, ctx);

    if (is_angle) {
        /* Parse angle for rotate() */
        error = css__parse_unit_specifier(c, vector, ctx, UNIT_DEG, &value1, &unit1);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }
        if (!(unit1 & UNIT_ANGLE)) {
            *ctx = orig_ctx;
            return CSS_INVALID;
        }
    } else if (is_scale) {
        /* Scale accepts unitless numbers - parse directly */
        token = parserutils_vector_iterate(vector, ctx);
        if (token == NULL || (token->type != CSS_TOKEN_NUMBER && token->type != CSS_TOKEN_PERCENTAGE)) {
            *ctx = orig_ctx;
            return CSS_INVALID;
        }

        size_t consumed = 0;
        value1 = css__number_from_lwc_string(token->idata, false, &consumed);
        if (consumed != lwc_string_length(token->idata)) {
            *ctx = orig_ctx;
            return CSS_INVALID;
        }

        if (token->type == CSS_TOKEN_PERCENTAGE) {
            unit1 = UNIT_PCT;
            /* Convert percentage to multiplier: 50% = 0.5 */
            value1 = FDIV(value1, F_100);
        } else {
            unit1 = UNIT_NUMBER; /* Unitless number */
        }
    } else {
        /* Parse length/percentage for translate */
        error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX, &value1, &unit1);
        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }
        /* Translate accepts lengths and percentages */
        if ((unit1 & UNIT_ANGLE) || (unit1 & UNIT_TIME) || (unit1 & UNIT_FREQ)) {
            *ctx = orig_ctx;
            return CSS_INVALID;
        }
        if (!allow_pct && (unit1 & UNIT_PCT)) {
            *ctx = orig_ctx;
            return CSS_INVALID;
        }
    }

    consumeWhitespace(vector, ctx);

    /* Check for optional second argument */
    if (num_args == 2) {
        token = parserutils_vector_peek(vector, *ctx);
        if (token != NULL && tokenIsChar(token, ',')) {
            /* Consume comma */
            parserutils_vector_iterate(vector, ctx);
            consumeWhitespace(vector, ctx);

            if (is_angle) {
                error = css__parse_unit_specifier(c, vector, ctx, UNIT_DEG, &value2, &unit2);
                if (error != CSS_OK) {
                    *ctx = orig_ctx;
                    return error;
                }
            } else if (is_scale) {
                /* Scale second argument - parse unitless number directly */
                token = parserutils_vector_iterate(vector, ctx);
                if (token == NULL || (token->type != CSS_TOKEN_NUMBER && token->type != CSS_TOKEN_PERCENTAGE)) {
                    *ctx = orig_ctx;
                    return CSS_INVALID;
                }

                size_t consumed2 = 0;
                value2 = css__number_from_lwc_string(token->idata, false, &consumed2);
                if (consumed2 != lwc_string_length(token->idata)) {
                    *ctx = orig_ctx;
                    return CSS_INVALID;
                }

                if (token->type == CSS_TOKEN_PERCENTAGE) {
                    unit2 = UNIT_PCT;
                    value2 = FDIV(value2, F_100);
                } else {
                    unit2 = UNIT_NUMBER;
                }
            } else {
                error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX, &value2, &unit2);
                if (error != CSS_OK) {
                    *ctx = orig_ctx;
                    return error;
                }
            }

            consumeWhitespace(vector, ctx);
        } else {
            /* No second arg - default behavior */
            if (func_type == TRANSFORM_TRANSLATE) {
                /* translate(x) means translate(x, 0) */
                value2 = 0;
                unit2 = UNIT_PX;
            } else if (func_type == TRANSFORM_SCALE) {
                /* scale(x) means scale(x, x) */
                value2 = value1;
                unit2 = unit1;
            }
        }
    }

    /* Consume closing parenthesis */
    token = parserutils_vector_iterate(vector, ctx);
    if (token == NULL || !tokenIsChar(token, ')')) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    /* Append function type */
    error = css__stylesheet_style_append(result, func_type);
    if (error != CSS_OK)
        return error;

    /* Append first value and unit */
    error = css__stylesheet_style_vappend(result, 2, value1, unit1);
    if (error != CSS_OK)
        return error;

    /* Append second value and unit if applicable */
    if (num_args == 2 || func_type == TRANSFORM_TRANSLATE || func_type == TRANSFORM_SCALE) {
        error = css__stylesheet_style_vappend(result, 2, value2, unit2);
        if (error != CSS_OK)
            return error;
    }

    return CSS_OK;
}

/**
 * Parse transform
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
css_error css__parse_transform(css_language *c, const parserutils_vector *vector, int32_t *ctx, css_style *result)
{
    int32_t orig_ctx = *ctx;
    css_error error = CSS_OK;
    const css_token *token;
    enum flag_value flag_value;
    bool match;
    uint32_t func_count = 0;
    int32_t count_pos;

    token = parserutils_vector_iterate(vector, ctx);
    if (token == NULL) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    /* Check for inherit/initial/revert/unset */
    flag_value = get_css_flag_value(c, token);
    if (flag_value != FLAG_VALUE__NONE) {
        return css_stylesheet_style_flag_value(result, flag_value, CSS_PROP_TRANSFORM);
    }

    /* Check for 'none' */
    if ((token->type == CSS_TOKEN_IDENT) &&
        (lwc_string_caseless_isequal(token->idata, c->strings[NONE], &match) == lwc_error_ok && match)) {
        return css__stylesheet_style_appendOPV(result, CSS_PROP_TRANSFORM, 0, TRANSFORM_NONE);
    }

    /* Must be a list of transform functions */
    /* Output bytecode format:
     * [OPV: CSS_PROP_TRANSFORM | 0 | value]
     * [function count]
     * For each function:
     *   [function type]
     *   [value1, unit1]
     *   [value2, unit2] (if applicable)
     */

    /* Append OPV for transform functions */
    error = css__stylesheet_style_appendOPV(result, CSS_PROP_TRANSFORM, 0, TRANSFORM_NONE + 1);
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }

    /* Remember position for function count - we'll write it after parsing */
    count_pos = result->used;
    error = css__stylesheet_style_append(result, 0); /* placeholder */
    if (error != CSS_OK) {
        *ctx = orig_ctx;
        return error;
    }

    /* Go back to first token */
    *ctx = orig_ctx;

    /* Parse function list */
    while (1) {
        token = parserutils_vector_iterate(vector, ctx);
        if (token == NULL)
            break;

        consumeWhitespace(vector, ctx);

        if (token->type != CSS_TOKEN_FUNCTION) {
            /* End of function list */
            (*ctx)--; /* Put token back */
            break;
        }

        /* Check which function */
        if (lwc_string_caseless_isequal(token->idata, c->strings[TRANSLATE], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_TRANSLATE, 2, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[TRANSLATEX], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_TRANSLATEX, 1, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[TRANSLATEY], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_TRANSLATEY, 1, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[SCALE], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_SCALE, 2, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[SCALEX], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_SCALEX, 1, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[SCALEY], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_SCALEY, 1, true, false);
        } else if (lwc_string_caseless_isequal(token->idata, c->strings[ROTATE], &match) == lwc_error_ok && match) {
            error = parse_transform_function(c, vector, ctx, result, TRANSFORM_ROTATE, 1, false, true);
        } else {
            /* Unknown function */
            *ctx = orig_ctx;
            return CSS_INVALID;
        }

        if (error != CSS_OK) {
            *ctx = orig_ctx;
            return error;
        }

        func_count++;
        consumeWhitespace(vector, ctx);
    }

    if (func_count == 0) {
        *ctx = orig_ctx;
        return CSS_INVALID;
    }

    /* Update function count at the remembered position */
    result->bytecode[count_pos] = func_count;

    return CSS_OK;
}
