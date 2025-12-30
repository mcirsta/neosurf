/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2025 Neosurf Grid Support
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

#define MAX_GRID_TRACKS 32

/**
 * Parse grid-template-rows
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
css_error css__parse_grid_template_rows(css_language *c,
					const parserutils_vector *vector,
					int32_t *ctx,
					css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error = CSS_OK;
	const css_token *token;
	bool match;
	css_fixed track_values[MAX_GRID_TRACKS];
	uint32_t track_units[MAX_GRID_TRACKS];
	int num_tracks = 0;

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	/* Check for keywords first */
	if (token->type == CSS_TOKEN_IDENT) {
		if ((lwc_string_caseless_isequal(token->idata,
						 c->strings[INHERIT],
						 &match) == lwc_error_ok &&
		     match)) {
			return css_stylesheet_style_inherit(
				result, CSS_PROP_GRID_TEMPLATE_ROWS);
		} else if ((lwc_string_caseless_isequal(token->idata,
							c->strings[INITIAL],
							&match) ==
				    lwc_error_ok &&
			    match)) {
			return css__stylesheet_style_appendOPV(
				result,
				CSS_PROP_GRID_TEMPLATE_ROWS,
				0,
				GRID_TEMPLATE_NONE);
		} else if ((lwc_string_caseless_isequal(
				    token->idata, c->strings[REVERT], &match) ==
				    lwc_error_ok &&
			    match)) {
			return css_stylesheet_style_revert(
				result, CSS_PROP_GRID_TEMPLATE_ROWS);
		} else if ((lwc_string_caseless_isequal(
				    token->idata, c->strings[UNSET], &match) ==
				    lwc_error_ok &&
			    match)) {
			return css__stylesheet_style_appendOPV(
				result,
				CSS_PROP_GRID_TEMPLATE_ROWS,
				FLAG_VALUE_UNSET,
				0);
		} else if ((lwc_string_caseless_isequal(
				    token->idata, c->strings[NONE], &match) ==
				    lwc_error_ok &&
			    match)) {
			return css__stylesheet_style_appendOPV(
				result,
				CSS_PROP_GRID_TEMPLATE_ROWS,
				0,
				GRID_TEMPLATE_NONE);
		} else if ((lwc_string_caseless_isequal(
				    token->idata, c->strings[AUTO], &match) ==
				    lwc_error_ok &&
			    match)) {
			/* Single 'auto' track */
			track_values[0] = INTTOFIX(1);
			track_units[0] = UNIT_FR;
			num_tracks = 1;
			goto emit_tracks;
		}
	}

	/* Parse track list - reset context to re-parse first token */
	*ctx = orig_ctx;

	while (num_tracks < MAX_GRID_TRACKS) {
		css_fixed length = 0;
		uint32_t unit = 0;

		/* Skip whitespace */
		token = parserutils_vector_peek(vector, *ctx);
		if (token == NULL) {
			break; /* End of input */
		}

		/* Try to parse a dimension/percentage/number */
		error = css__parse_unit_specifier(c,
						  vector,
						  ctx,
						  UNIT_PX, /* default unit */
						  &length,
						  &unit);
		if (error == CSS_OK) {
			track_values[num_tracks] = length;
			track_units[num_tracks] = unit;
			num_tracks++;
		} else if (error == CSS_INVALID) {
			/* Check for 'auto' keyword */
			token = parserutils_vector_peek(vector, *ctx);
			if (token != NULL && token->type == CSS_TOKEN_IDENT) {
				if ((lwc_string_caseless_isequal(
					     token->idata,
					     c->strings[AUTO],
					     &match) == lwc_error_ok &&
				     match)) {
					/* 'auto' track - treat as 1fr */
					parserutils_vector_iterate(vector, ctx);
					track_values[num_tracks] = INTTOFIX(1);
					track_units[num_tracks] = UNIT_FR;
					num_tracks++;
					continue;
				}
			}
			/* Unknown token - stop parsing */
			break;
		} else {
			/* Error */
			*ctx = orig_ctx;
			return error;
		}
	}

	if (num_tracks == 0) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

emit_tracks:
	/* Emit the opcode with track count */
	error = css__stylesheet_style_appendOPV(
		result, CSS_PROP_GRID_TEMPLATE_ROWS, 0, GRID_TEMPLATE_SET);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	/* Emit number of tracks */
	error = css__stylesheet_style_append(result, num_tracks);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	/* Emit each track's value and unit */
	for (int i = 0; i < num_tracks; i++) {
		error = css__stylesheet_style_append(result, track_values[i]);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
		error = css__stylesheet_style_append(result, track_units[i]);
		if (error != CSS_OK) {
			*ctx = orig_ctx;
			return error;
		}
	}

	return CSS_OK;
}
