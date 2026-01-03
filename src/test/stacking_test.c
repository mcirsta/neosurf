/*
 * Copyright 2024 Neosurf contributors
 *
 * This file is part of Neosurf, http://www.netsurf-browser.org/
 *
 * Neosurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * Neosurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Tests for stacking context (z-index) utilities.
 */

#include "utils/config.h"

#include <check.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "content/handlers/html/stacking.h"

/**
 * Test stacking context initialization
 */
START_TEST(stacking_context_init_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);

    ck_assert(ctx.entries == NULL);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(ctx.capacity, 0);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test adding elements to stacking context
 */
START_TEST(stacking_context_add_test)
{
    struct stacking_context ctx;
    bool result;

    stacking_context_init(&ctx);

    /* Add first element */
    result = stacking_context_add(&ctx, (struct box *)0x1000, 5, 10, 20);
    ck_assert(result == true);
    ck_assert_int_eq(ctx.count, 1);
    ck_assert(ctx.capacity >= 1);
    ck_assert(ctx.entries != NULL);
    ck_assert(ctx.entries[0].box == (struct box *)0x1000);
    ck_assert_int_eq(ctx.entries[0].z_index, 5);
    ck_assert_int_eq(ctx.entries[0].x_parent, 10);
    ck_assert_int_eq(ctx.entries[0].y_parent, 20);

    /* Add second element */
    result = stacking_context_add(&ctx, (struct box *)0x2000, -3, 30, 40);
    ck_assert(result == true);
    ck_assert_int_eq(ctx.count, 2);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test adding many elements (triggers realloc)
 */
START_TEST(stacking_context_add_many_test)
{
    struct stacking_context ctx;
    bool result;
    int i;

    stacking_context_init(&ctx);

    /* Add more elements than initial capacity */
    for (i = 0; i < 100; i++) {
        result = stacking_context_add(&ctx, (struct box *)(uintptr_t)(0x1000 + i), i - 50, i * 10, i * 20);
        ck_assert(result == true);
    }

    ck_assert_int_eq(ctx.count, 100);
    ck_assert(ctx.capacity >= 100);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test sorting by z-index
 */
START_TEST(stacking_context_sort_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);

    /* Add elements in non-sorted order */
    stacking_context_add(&ctx, (struct box *)0x1000, 5, 0, 0); /* z=5 */
    stacking_context_add(&ctx, (struct box *)0x2000, -10, 0, 0); /* z=-10 */
    stacking_context_add(&ctx, (struct box *)0x3000, 100, 0, 0); /* z=100 */
    stacking_context_add(&ctx, (struct box *)0x4000, 0, 0, 0); /* z=0 */
    stacking_context_add(&ctx, (struct box *)0x5000, -5, 0, 0); /* z=-5 */

    stacking_context_sort(&ctx);

    /* Check sorted order: -10, -5, 0, 5, 100 */
    ck_assert_int_eq(ctx.entries[0].z_index, -10);
    ck_assert_int_eq(ctx.entries[1].z_index, -5);
    ck_assert_int_eq(ctx.entries[2].z_index, 0);
    ck_assert_int_eq(ctx.entries[3].z_index, 5);
    ck_assert_int_eq(ctx.entries[4].z_index, 100);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test that sort is stable (preserves document order for equal z-index)
 */
START_TEST(stacking_context_stable_sort_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);

    /* Add elements with same z-index in order 1,2,3,4 */
    stacking_context_add(&ctx, (struct box *)0x1000, 5, 0, 0); /* first */
    stacking_context_add(&ctx, (struct box *)0x2000, 5, 0, 0); /* second */
    stacking_context_add(&ctx, (struct box *)0x3000, 5, 0, 0); /* third */
    stacking_context_add(&ctx, (struct box *)0x4000, 5, 0, 0); /* fourth */

    stacking_context_sort(&ctx);

    /* Order should be preserved (stable sort) */
    ck_assert(ctx.entries[0].box == (struct box *)0x1000);
    ck_assert(ctx.entries[1].box == (struct box *)0x2000);
    ck_assert(ctx.entries[2].box == (struct box *)0x3000);
    ck_assert(ctx.entries[3].box == (struct box *)0x4000);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test sorting empty context
 */
START_TEST(stacking_context_sort_empty_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);

    /* Should not crash */
    stacking_context_sort(&ctx);

    ck_assert_int_eq(ctx.count, 0);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test sorting single element
 */
START_TEST(stacking_context_sort_single_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);
    stacking_context_add(&ctx, (struct box *)0x1000, 5, 0, 0);

    /* Should not crash */
    stacking_context_sort(&ctx);

    ck_assert_int_eq(ctx.count, 1);
    ck_assert_int_eq(ctx.entries[0].z_index, 5);

    stacking_context_fini(&ctx);
}
END_TEST

/**
 * Test finalization clears state
 */
START_TEST(stacking_context_fini_test)
{
    struct stacking_context ctx;

    stacking_context_init(&ctx);
    stacking_context_add(&ctx, (struct box *)0x1000, 5, 0, 0);
    stacking_context_add(&ctx, (struct box *)0x2000, 10, 0, 0);

    stacking_context_fini(&ctx);

    ck_assert(ctx.entries == NULL);
    ck_assert_int_eq(ctx.count, 0);
    ck_assert_int_eq(ctx.capacity, 0);
}
END_TEST

/**
 * Test negative z-index sorting (renders before positive)
 */
START_TEST(stacking_context_negative_zindex_test)
{
    struct stacking_context ctx;
    int i;
    int negative_count = 0;
    int positive_count = 0;
    bool found_transition = false;
    int transition_index = -1;

    stacking_context_init(&ctx);

    /* Add mix of negative and positive z-index values */
    stacking_context_add(&ctx, (struct box *)0x1000, 10, 0, 0); /* positive */
    stacking_context_add(&ctx, (struct box *)0x2000, -5, 0, 0); /* negative */
    stacking_context_add(&ctx, (struct box *)0x3000, 5, 0, 0); /* positive */
    stacking_context_add(&ctx, (struct box *)0x4000, -100, 0, 0); /* negative */
    stacking_context_add(&ctx, (struct box *)0x5000, 0, 0, 0); /* zero (positive side) */
    stacking_context_add(&ctx, (struct box *)0x6000, -1, 0, 0); /* negative */

    stacking_context_sort(&ctx);

    /* Count negatives and positives, verify all negatives come first */
    for (i = 0; i < (int)ctx.count; i++) {
        if (ctx.entries[i].z_index < 0) {
            negative_count++;
            /* After finding positives, shouldn't find more
             * negatives */
            ck_assert_msg(!found_transition, "Negative z-index found after positive at index %d", i);
        } else {
            if (!found_transition) {
                found_transition = true;
                transition_index = i;
            }
            positive_count++;
        }
    }

    /* Verify counts */
    ck_assert_int_eq(negative_count, 3); /* -100, -5, -1 */
    ck_assert_int_eq(positive_count, 3); /* 0, 5, 10 */

    /* Verify sorted order within negatives: -100, -5, -1 */
    ck_assert_int_eq(ctx.entries[0].z_index, -100);
    ck_assert_int_eq(ctx.entries[1].z_index, -5);
    ck_assert_int_eq(ctx.entries[2].z_index, -1);

    /* Verify sorted order within positives: 0, 5, 10 */
    ck_assert_int_eq(ctx.entries[3].z_index, 0);
    ck_assert_int_eq(ctx.entries[4].z_index, 5);
    ck_assert_int_eq(ctx.entries[5].z_index, 10);

    stacking_context_fini(&ctx);
}
END_TEST

static TCase *stacking_context_case_create(void)
{
    TCase *tc;
    tc = tcase_create("Stacking context");

    tcase_add_test(tc, stacking_context_init_test);
    tcase_add_test(tc, stacking_context_add_test);
    tcase_add_test(tc, stacking_context_add_many_test);
    tcase_add_test(tc, stacking_context_sort_test);
    tcase_add_test(tc, stacking_context_stable_sort_test);
    tcase_add_test(tc, stacking_context_sort_empty_test);
    tcase_add_test(tc, stacking_context_sort_single_test);
    tcase_add_test(tc, stacking_context_fini_test);
    tcase_add_test(tc, stacking_context_negative_zindex_test);

    return tc;
}

static Suite *stacking_suite_create(void)
{
    Suite *s;
    s = suite_create("Stacking context API");

    suite_add_tcase(s, stacking_context_case_create());

    return s;
}

int main(int argc, char **argv)
{
    int number_failed;
    SRunner *sr;

    sr = srunner_create(stacking_suite_create());

    srunner_run_all(sr, CK_ENV);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
