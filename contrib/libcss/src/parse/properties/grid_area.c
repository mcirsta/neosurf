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
 * Parse a single grid line value (auto or integer)
 */
static css_error parse_grid_line_value(css_language *c,
				       const parserutils_vector *vector,
				       int32_t *ctx,
				       uint16_t *value,
				       css_fixed *integer)
{
	const css_token *token;
	bool match;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) {
		return CSS_INVALID;
	}

	if (token->type == CSS_TOKEN_IDENT &&
	    lwc_string_caseless_isequal(
		    token->idata, c->strings[AUTO], &match) == lwc_error_ok &&
	    match) {
		parserutils_vector_iterate(vector, ctx);
		*value = CSS_GRID_LINE_AUTO;
		*integer = 0;
		return CSS_OK;
	}

	if (token->type == CSS_TOKEN_NUMBER) {
		size_t consumed = 0;
		css_fixed num = css__number_from_lwc_string(token->idata,
							    true,
							    &consumed);
		if (consumed != lwc_string_length(token->idata)) {
			return CSS_INVALID;
		}
		if (num == 0) {
			return CSS_INVALID;
		}
		parserutils_vector_iterate(vector, ctx);
		*value = CSS_GRID_LINE_SET;
		*integer = num;
		return CSS_OK;
	}

	return CSS_INVALID;
}

/**
 * Parse grid-area shorthand
 *
 * Syntax: grid-area: <row-start> [ / <column-start> [ / <row-end> [ /
 * <column-end> ]? ]? ]? Where <grid-line> = auto | <integer>
 *
 * Missing values default to auto.
 */
css_error css__parse_grid_area(css_language *c,
			       const parserutils_vector *vector,
			       int32_t *ctx,
			       css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	uint16_t values[4] = {CSS_GRID_LINE_AUTO,
			      CSS_GRID_LINE_AUTO,
			      CSS_GRID_LINE_AUTO,
			      CSS_GRID_LINE_AUTO};
	css_fixed integers[4] = {0, 0, 0, 0};
	int num_values = 0;
	enum flag_value flag_value;
	/* Order: row-start, column-start, row-end, column-end */
	uint32_t props[4] = {CSS_PROP_GRID_ROW_START,
			     CSS_PROP_GRID_COLUMN_START,
			     CSS_PROP_GRID_ROW_END,
			     CSS_PROP_GRID_COLUMN_END};

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	/* Check for inherit/initial/unset/revert */
	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		for (int i = 0; i < 4; i++) {
			error = css_stylesheet_style_flag_value(result,
								flag_value,
								props[i]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
		}
		return CSS_OK;
	}

	/* Parse up to 4 values separated by '/' */
	for (int i = 0; i < 4; i++) {
		error = parse_grid_line_value(
			c, vector, ctx, &values[i], &integers[i]);
		if (error != CSS_OK) {
			if (i == 0) {
				/* Must have at least one value */
				*ctx = orig_ctx;
				return error;
			}
			break;
		}
		num_values++;

		consumeWhitespace(vector, ctx);

		/* Check for '/' separator */
		token = parserutils_vector_peek(vector, *ctx);
		if (token == NULL || !tokenIsChar(token, '/')) {
			break;
		}
		parserutils_vector_iterate(vector, ctx);
		consumeWhitespace(vector, ctx);
	}

	/* Emit all 4 properties */
	for (int i = 0; i < 4; i++) {
		error = css__stylesheet_style_appendOPV(
			result, props[i], 0, values[i]);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
		if (values[i] == CSS_GRID_LINE_SET) {
			error = css__stylesheet_style_append(result,
							     integers[i]);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
		}
	}

	return CSS_OK;
}
