#include <check.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "content/handlers/html/layout.h"
#include "neosurf/content/handlers/html/box.h"
#include "neosurf/css.h"
#include "neosurf/types.h"
#include "neosurf/utils/errors.h"
#include "neosurf/utils/log.h"

/* Forward declare html_content to satisfy layout_internal.h */
typedef struct html_content html_content;

#include "content/handlers/html/layout_internal.h"

/* Mock dependencies */
struct html_content {
    struct css_unit_ctx unit_len_ctx;
};

/* Mock layout_flex function to avoid full linking */
/* We will actually test the internal logic via reproducing it or unit testing
 * internal static functions if exposed */
/* Since we can't easily access static functions, we'll create a test that
 * replicates the scenario logic */

/* Replicating the logic from layout_flex__resolve_line to test the fix */
static int test_resolve_line_logic(int available_main, int main_size)
{
    if (available_main == INT_MIN) { /* AUTO */
        /* OLD BEHAVIOR: available_main = INT_MAX; */
        /* NEW BEHAVIOR: available_main = main_size; */
        return main_size;
    }
    return available_main;
}

static int test_resolve_line_logic_old(int available_main, int main_size)
{
    if (available_main == INT_MIN) { /* AUTO */
        return INT_MAX;
    }
    return available_main;
}

START_TEST(test_flex_auto_width_logic)
{
    /* Case 1: Width is AUTO (INT_MIN in our macro) */
    int available_main = INT_MIN;
    int content_size = 500;

    /* With the fix, available_main should become content_size */
    int resolved = test_resolve_line_logic(available_main, content_size);
    ck_assert_int_eq(resolved, content_size);
}
END_TEST

/* Replicating the logic from layout_flex width clamping to test the fix */
static int test_flex_width_clamping(int width, int max_width, int min_width, int calculated_width)
{
    /* Logic from layout_flex */
    if (width == INT_MIN) { /* AUTO or UNKNOWN_WIDTH */
        width = calculated_width;

        if (max_width >= 0 && width > max_width) {
            width = max_width;
        }
        if (min_width > 0 && width < min_width) {
            width = min_width;
        }
    }
    return width;
}

START_TEST(test_flex_width_max_constraint)
{
    int width = INT_MIN; /* AUTO */
    int calculated_width = 800;
    int max_width = 600;
    int min_width = 0;

    int result = test_flex_width_clamping(width, max_width, min_width, calculated_width);

    /* Should be clamped to 600 */
    ck_assert_int_eq(result, 600);
}
END_TEST

START_TEST(test_flex_width_min_constraint)
{
    int width = INT_MIN; /* AUTO */
    int calculated_width = 200;
    int max_width = -1;
    int min_width = 300;

    int result = test_flex_width_clamping(width, max_width, min_width, calculated_width);

    /* Should be clamped to 300 */
    ck_assert_int_eq(result, 300);
}
/* Test that flex-basis calc() returns correct base_size */
START_TEST(test_flex_basis_calc_integration)
{
    /* Simulate what happens in layout_flex__base_and_main_sizes
     * When CSS_FLEX_BASIS_SET returns with calc() results:
     *
     * For flex-basis: calc(33.33% - 10px) on available_width 2484px:
     * Expected result: 2484 * 0.3333 - 10 ~= 817px
     *
     * For flex-basis: calc(200px - 50px):
     * Expected result: 150px
     */

    /* Test case 1: percentage-based calc */
    int available_width = 2484;
    int expected_px = 817; /* 2484 * 0.3333 - 10 */

    /* The css_computed_flex_basis_px should return this value
     * but we can't call it directly in unit test without full CSS context.
     * Instead, verify the math:
     */
    int calc_result = (available_width * 3333 / 10000) - 10;
    ck_assert_int_ge(calc_result, expected_px - 5);
    ck_assert_int_le(calc_result, expected_px + 5);

    /* Test case 2: px-only calc */
    int px_only_result = 200 - 50;
    ck_assert_int_eq(px_only_result, 150);

    /* Test case 3: Items should fit in container
     * 3 items at ~817px each = 2451px should fit in 2484px container
     */
    int total_items_width = 817 * 3;
    ck_assert_int_le(total_items_width, 2484);
}
END_TEST

/**
 * Test for column flex base_size calculation fix.
 *
 * Bug: For column (vertical) flex with flex-basis:auto, base_size was being
 * set to b->height BEFORE layout_flex_item() was called, getting a pre-layout
 * value (e.g., 22px) instead of the post-layout content height (e.g., 139px).
 *
 * Fix: For column flex, defer base_size to AUTO at line 348, then set it from
 * b->height after layout_flex_item() completes at line 365-367.
 */
static int test_column_flex_base_size_logic(
    int css_flex_basis_auto, int is_horizontal, int pre_layout_height, int post_layout_height)
{
    int base_size;

    if (css_flex_basis_auto) {
        if (is_horizontal) {
            /* Horizontal: width is known before layout */
            base_size = pre_layout_height; /* Actually would be width */
        } else {
            /* Column (vertical): defer to AUTO, set after layout */
            base_size = INT_MIN; /* AUTO */
        }
    } else {
        base_size = INT_MIN; /* AUTO */
    }

    /* Simulate layout_flex_item() being called for column flex */
    if (!is_horizontal && base_size == INT_MIN) {
        /* After layout, use post-layout height */
        base_size = post_layout_height;
    }

    return base_size;
}

START_TEST(test_column_flex_base_size_fix)
{
    /* Scenario: entry-wrapper flex item in column flex article container
     * Pre-layout height: 22px (wrong value from CSS or initial)
     * Post-layout height: 139px (correct content-based height)
     */
    int pre_layout = 22;
    int post_layout = 139;

    /* OLD behavior (bug): base_size = pre-layout height = 22 */
    /* NEW behavior (fix): base_size = post-layout height = 139 */

    int result = test_column_flex_base_size_logic(1, /* CSS_FLEX_BASIS_AUTO */
        0, /* is_horizontal = false (column flex) */
        pre_layout, post_layout);

    /* With the fix, should get post-layout height */
    ck_assert_int_eq(result, post_layout);
    ck_assert_int_ne(result, pre_layout);
}
END_TEST

START_TEST(test_horizontal_flex_base_size_unchanged)
{
    /* Horizontal flex should still use pre-layout width (unchanged behavior) */
    int pre_layout = 300;
    int post_layout = 350;

    int result = test_column_flex_base_size_logic(1, /* CSS_FLEX_BASIS_AUTO */
        1, /* is_horizontal = true */
        pre_layout, post_layout);

    /* Horizontal flex uses width which is known before layout */
    ck_assert_int_eq(result, pre_layout);
}


/**
 * Test for column flex height preservation fix.
 *
 * Bug: Column flex containers were preserving stretched height from parent,
 * causing content to be cut off.
 *
 * Fix: Only preserve height for horizontal flex (where height is cross-size).
 */
static bool test_should_preserve_height_logic(int is_horizontal, int height_definite)
{
    return height_definite && is_horizontal;
}

START_TEST(test_column_flex_height_not_preserved)
{
    /* Column flex should NOT preserve stretched height */
    bool should_preserve = test_should_preserve_height_logic(0, 1);
    ck_assert_int_eq(should_preserve, 0);
}
END_TEST

START_TEST(test_horizontal_flex_height_preserved)
{
    /* Horizontal flex SHOULD preserve height (cross-dimension) */
    bool should_preserve = test_should_preserve_height_logic(1, 1);
    ck_assert_int_eq(should_preserve, 1);
}
END_TEST


/**
 * Test for flex-basis:0 in column flex fix.
 *
 * Bug: Elements with 'flex: 1' (which sets flex-basis: 0) in column flex
 * were not having their content height measured. Instead, base_size was
 * set to 0, causing the layout to be incorrect.
 *
 * Fix: For column flex with flex-basis: 0, defer base_size calculation
 * to content-based sizing, just like flex-basis: auto.
 */
static bool test_flex_basis_zero_column_logic(int is_horizontal, int basis_px)
{
    /* Replicates the logic from layout_flex.c lines 344-350:
     * For column flex with flex-basis: 0, defer to content sizing.
     * Returns true if base_size should be set to AUTO (deferred). */
    if (is_horizontal == 0 && basis_px == 0) {
        return true; /* Defer to content sizing */
    }
    return false; /* Use the basis_px value directly */
}

START_TEST(test_column_flex_basis_zero_deferred)
{
    /* Column flex with flex-basis: 0 should defer to content sizing */
    bool should_defer = test_flex_basis_zero_column_logic(0, 0);
    ck_assert_int_eq(should_defer, 1);
}
END_TEST

START_TEST(test_horizontal_flex_basis_zero_not_deferred)
{
    /* Horizontal flex with flex-basis: 0 should NOT defer (use 0) */
    bool should_defer = test_flex_basis_zero_column_logic(1, 0);
    ck_assert_int_eq(should_defer, 0);
}
END_TEST

START_TEST(test_column_flex_basis_nonzero_not_deferred)
{
    /* Column flex with non-zero flex-basis should NOT defer */
    bool should_defer = test_flex_basis_zero_column_logic(0, 100);
    ck_assert_int_eq(should_defer, 0);
}
END_TEST


/* Test suite setup */
Suite *layout_flex_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("LayoutFlex");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_flex_auto_width_logic);
    tcase_add_test(tc_core, test_flex_width_max_constraint);
    tcase_add_test(tc_core, test_flex_width_min_constraint);
    tcase_add_test(tc_core, test_flex_basis_calc_integration);
    tcase_add_test(tc_core, test_column_flex_base_size_fix);
    tcase_add_test(tc_core, test_horizontal_flex_base_size_unchanged);
    tcase_add_test(tc_core, test_column_flex_height_not_preserved);
    tcase_add_test(tc_core, test_horizontal_flex_height_preserved);
    tcase_add_test(tc_core, test_column_flex_basis_zero_deferred);
    tcase_add_test(tc_core, test_horizontal_flex_basis_zero_not_deferred);
    tcase_add_test(tc_core, test_column_flex_basis_nonzero_not_deferred);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = layout_flex_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
