/**************************************************************************/
/*  breakpad.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

#include <cstddef>
#include <cstdint>

void initialize_breakpad(bool register_handlers);
void disable_breakpad();
void report_user_data_dir_usable();

// Due to Mono runtime initialization in release mode overriding signal handlers, Breakpad needs re-initialization after loading it
void report_mono_loaded_to_breakpad();

// Linux crash handling goes through this
void breakpad_handle_signal(int sig);

// Windows crash handling goes through this
// TODO: should Windows header be included here to use an actual type?
void breakpad_handle_exception_pointers(void *exinfo);

// Versioned, fixed-layout GDScript trace data is stored in process-lifetime
// memory and registered with Breakpad. Keep this layout stable for dump readers.
constexpr uint32_t BREAKPAD_GDSCRIPT_TRACE_MAGIC = 0x47535452; // "GSTR"
constexpr uint32_t BREAKPAD_GDSCRIPT_TRACE_VERSION = 1;
constexpr size_t BREAKPAD_GDSCRIPT_TRACE_THREAD_COUNT = 32;
constexpr size_t BREAKPAD_GDSCRIPT_TRACE_FRAME_COUNT = 32;
constexpr size_t BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE = 128;
constexpr size_t BREAKPAD_GDSCRIPT_TRACE_FUNCTION_SIZE = 64;

struct BreakpadGDScriptTraceFrame {
	int32_t line;
	char path[BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE];
	char function[BREAKPAD_GDSCRIPT_TRACE_FUNCTION_SIZE];
};

struct BreakpadGDScriptTraceThread {
	uint64_t thread_id;
	uint32_t frame_count;
	uint32_t truncated;
	BreakpadGDScriptTraceFrame frames[BREAKPAD_GDSCRIPT_TRACE_FRAME_COUNT];
};

struct BreakpadGDScriptTrace {
	uint32_t magic;
	uint32_t version;
	uint32_t thread_count;
	uint32_t frame_capacity;
	BreakpadGDScriptTraceThread threads[BREAKPAD_GDSCRIPT_TRACE_THREAD_COUNT];
};

void breakpad_gdscript_enter(const String &path, const String &function, int line);
void breakpad_gdscript_exit();
void breakpad_gdscript_line(int line);
