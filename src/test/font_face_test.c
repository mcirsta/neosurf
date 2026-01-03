/*
 * Copyright 2024 NeoSurf Contributors
 *
 * This file is part of NeoSurf, http://www.netsurf-browser.org/
 *
 * NeoSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NeoSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Test for font_face functionality.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

/* Forward declaration - implemented in font_face.c, exported from libneosurf */
bool html_font_face_is_available(const char *family_name);

/**
 * Test that html_font_face_is_available returns false for unknown fonts.
 */
static void test_font_not_available(void)
{
    bool available = html_font_face_is_available("NonExistentFont12345");
    assert(!available);
    printf("PASS: html_font_face_is_available returns false for unknown font\n");
}

/**
 * Test that html_font_face_is_available handles empty string.
 */
static void test_font_empty_string(void)
{
    bool available = html_font_face_is_available("");
    assert(!available);
    printf("PASS: html_font_face_is_available returns false for empty string\n");
}

int main(void)
{
    printf("Running font_face tests...\n");

    test_font_not_available();
    test_font_empty_string();

    printf("All font_face tests passed!\n");
    return 0;
}
