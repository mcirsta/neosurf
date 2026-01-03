/*
 * This file is part of libdom test suite.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#ifndef domts_h_
#define domts_h_

#include <comparators.h>
#include <domtsasserts.h>
#include <domtscondition.h>
#include <foreach.h>
#include <list.h>
#include <utils.h>

dom_document *load_xml(const char *file, bool willBeModified);
dom_document *load_html(const char *file, bool willBeModified);

#endif
