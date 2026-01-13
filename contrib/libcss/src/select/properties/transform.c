/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 Neosurf Contributors
 */

#include <stdio.h>
#include <stdlib.h>

#include "utils/utils.h"
#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propget.h"
#include "select/propset.h"
#include "select/unit.h"

#include "select/properties/helpers.h"
#include "select/properties/properties.h"

css_error css__cascade_transform(uint32_t opv, css_style *style, css_select_state *state)
{
    uint16_t value = CSS_TRANSFORM_INHERIT;
    uint32_t n_functions = 0;
    css_transform_function *functions = NULL;

    if (hasFlagValue(opv) == false) {
        uint16_t v = getValue(opv);

        if (v == TRANSFORM_NONE) {
            value = CSS_TRANSFORM_NONE;
        } else {
            /* Has transform functions - read count and parse functions */
            value = CSS_TRANSFORM_FUNCTIONS;

            n_functions = *((uint32_t *)style->bytecode);
            advance_bytecode(style, sizeof(n_functions));

            if (n_functions > 0) {
                /* Allocate array for functions */
                functions = malloc(n_functions * sizeof(css_transform_function));
                if (functions == NULL) {
                    return CSS_NOMEM;
                }

                for (uint32_t i = 0; i < n_functions; i++) {
                    css_transform_function *func = &functions[i];

                    /* Read function type */
                    uint32_t func_type = *((uint32_t *)style->bytecode);
                    advance_bytecode(style, sizeof(func_type));
                    func->type = (uint8_t)func_type;

                    /* Read value1 and unit1 */
                    func->value1 = *((css_fixed *)style->bytecode);
                    advance_bytecode(style, sizeof(css_fixed));
                    uint32_t raw_unit1 = *((uint32_t *)style->bytecode);
                    advance_bytecode(style, sizeof(uint32_t));
                    func->unit1 = css__to_css_unit(raw_unit1);

                    /* Read value2 and unit2 for translate/scale */
                    if (func_type == TRANSFORM_TRANSLATE || func_type == TRANSFORM_SCALE) {
                        func->value2 = *((css_fixed *)style->bytecode);
                        advance_bytecode(style, sizeof(css_fixed));
                        uint32_t raw_unit2 = *((uint32_t *)style->bytecode);
                        advance_bytecode(style, sizeof(uint32_t));
                        func->unit2 = css__to_css_unit(raw_unit2);
                    } else {
                        func->value2 = 0;
                        func->unit2 = CSS_UNIT_PX;
                    }
                }
            }
        }
    }

    int outranks = css__outranks_existing(getOpcode(opv), isImportant(opv), state, getFlagValue(opv));

    if (outranks) {
        css_error error = set_transform(state->computed, value, n_functions, functions);
        if (error != CSS_OK) {
            if (functions)
                free(functions);
            return error;
        }
    } else {
        /* Not using, free the allocated functions */
        if (functions)
            free(functions);
    }

    return CSS_OK;
}

css_error css__set_transform_from_hint(const css_hint *hint, css_computed_style *style)
{
    /* Hints don't support complex transform data */
    return set_transform(style, hint->status, 0, NULL);
}

css_error css__initial_transform(css_select_state *state)
{
    /* Default is none */
    return set_transform(state->computed, CSS_TRANSFORM_NONE, 0, NULL);
}

css_error css__copy_transform(const css_computed_style *from, css_computed_style *to)
{
    if (from == to) {
        return CSS_OK;
    }

    uint32_t n_functions = 0;
    css_transform_function *functions = NULL;
    uint8_t type = get_transform(from, &n_functions, &functions);

    if (type == CSS_TRANSFORM_FUNCTIONS && n_functions > 0 && functions != NULL) {
        /* Copy the functions array */
        css_transform_function *new_funcs = malloc(n_functions * sizeof(css_transform_function));
        if (new_funcs == NULL) {
            return CSS_NOMEM;
        }
        memcpy(new_funcs, functions, n_functions * sizeof(css_transform_function));
        return set_transform(to, type, n_functions, new_funcs);
    }

    return set_transform(to, type, 0, NULL);
}

css_error
css__compose_transform(const css_computed_style *parent, const css_computed_style *child, css_computed_style *result)
{
    uint32_t n_functions = 0;
    css_transform_function *functions = NULL;
    uint8_t type = get_transform(child, &n_functions, &functions);

    return css__copy_transform(type == CSS_TRANSFORM_INHERIT ? parent : child, result);
}
