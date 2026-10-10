/**
 * Project: openRender
 *
 * File: logging.cpp
 *
 * Description:
 *   The single definition of current_log_level (declared in
 *   includes/logging.hpp). It used to be an inline variable, i.e. one weak
 *   copy per image; the default diagnostic sink in this library and
 *   orender's -x option must read and write the same object (spec 023).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "logging.hpp"

LogLevel current_log_level = LogLevel::INFO;
