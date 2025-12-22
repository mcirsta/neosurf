/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2024 CSS Grid Support
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_grid_template_rows(uint32_t opv,
					  css_style *style,
					  css_select_state *state)
{
	uint16_t value = CSS_GRID_TEMPLATE_INHERIT;

	UNUSED(style);

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case GRID_TEMPLATE_NONE:
			value = CSS_GRID_TEMPLATE_NONE;
			break;
		case GRID_TEMPLATE_SET:
			/* TODO: Handle track list values */
			value = CSS_GRID_TEMPLATE_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv),
				   isImportant(opv),
				   state,
				   getFlagValue(opv))) {
		return set_grid_template_rows(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_grid_template_rows_from_hint(const css_hint *hint,
						css_computed_style *style)
{
	return set_grid_template_rows(style, hint->status);
}

css_error css__initial_grid_template_rows(css_select_state *state)
{
	return set_grid_template_rows(state->computed, CSS_GRID_TEMPLATE_NONE);
}

css_error css__copy_grid_template_rows(const css_computed_style *from,
				       css_computed_style *to)
{
	if (from == to) {
		return CSS_OK;
	}

	return set_grid_template_rows(to, get_grid_template_rows(from));
}

css_error css__compose_grid_template_rows(const css_computed_style *parent,
					  const css_computed_style *child,
					  css_computed_style *result)
{
	uint8_t type = get_grid_template_rows(child);

	return css__copy_grid_template_rows(
		type == CSS_GRID_TEMPLATE_INHERIT ? parent : child, result);
}
