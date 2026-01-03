/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2024 CSS Grid Support
 */

#include "utils/utils.h"
#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propget.h"
#include "select/propset.h"

#include "select/properties/helpers.h"
#include "select/properties/properties.h"

css_error css__cascade_grid_auto_flow(uint32_t opv, css_style *style, css_select_state *state)
{
    uint16_t value = CSS_GRID_AUTO_FLOW_INHERIT;

    UNUSED(style);

    if (hasFlagValue(opv) == false) {
        switch (getValue(opv)) {
        case GRID_AUTO_FLOW_ROW:
            value = CSS_GRID_AUTO_FLOW_ROW;
            break;
        case GRID_AUTO_FLOW_COLUMN:
            value = CSS_GRID_AUTO_FLOW_COLUMN;
            break;
        case GRID_AUTO_FLOW_ROW_DENSE:
            value = CSS_GRID_AUTO_FLOW_ROW_DENSE;
            break;
        case GRID_AUTO_FLOW_COLUMN_DENSE:
            value = CSS_GRID_AUTO_FLOW_COLUMN_DENSE;
            break;
        }
    }

    if (css__outranks_existing(getOpcode(opv), isImportant(opv), state, getFlagValue(opv))) {
        return set_grid_auto_flow(state->computed, value);
    }

    return CSS_OK;
}

css_error css__set_grid_auto_flow_from_hint(const css_hint *hint, css_computed_style *style)
{
    return set_grid_auto_flow(style, hint->status);
}

css_error css__initial_grid_auto_flow(css_select_state *state)
{
    return set_grid_auto_flow(state->computed, CSS_GRID_AUTO_FLOW_ROW);
}

css_error css__copy_grid_auto_flow(const css_computed_style *from, css_computed_style *to)
{
    if (from == to) {
        return CSS_OK;
    }

    return set_grid_auto_flow(to, get_grid_auto_flow(from));
}

css_error css__compose_grid_auto_flow(
    const css_computed_style *parent, const css_computed_style *child, css_computed_style *result)
{
    uint8_t type = get_grid_auto_flow(child);

    return css__copy_grid_auto_flow(type == CSS_GRID_AUTO_FLOW_INHERIT ? parent : child, result);
}
