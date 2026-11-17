/**************************************************************************/
/*  gdscript_test_runner_suite.h                                          */
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

#include "gdscript_test_runner.h"

#include "modules/gdscript/gdscript_cache.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"
#if defined(USE_BREAKPAD) && defined(TESTS_ENABLED)
#include "core/os/thread.h"
#include "modules/breakpad/breakpad.h"

#include <atomic>
#include <cstring>
#endif

namespace GDScriptTests {

#if defined(USE_BREAKPAD) && defined(TESTS_ENABLED)
struct BreakpadTraceThreadTestData {
	std::atomic<bool> ready = false;
	std::atomic<bool> finish = false;
};

static void breakpad_trace_thread_test(void *p_userdata) {
	BreakpadTraceThreadTestData *data = static_cast<BreakpadTraceThreadTestData *>(p_userdata);
	breakpad_gdscript_enter("res://worker.gd", "worker", 7);
	data->ready.store(true, std::memory_order_release);
	while (!data->finish.load(std::memory_order_acquire)) {
		Thread::yield();
	}
	breakpad_gdscript_exit();
}

TEST_CASE("[Modules][GDScript] Breakpad trace captures nested frames and reuses idle slots") {
	breakpad_gdscript_enter("res://outer.gd", "outer", 10);
	breakpad_gdscript_enter("res://inner.gd", "inner", 20);
	breakpad_gdscript_line(21);

	const BreakpadGDScriptTrace *trace = breakpad_get_gdscript_trace_for_testing();
	auto find_thread_with_path = [trace](const char *p_path) -> const BreakpadGDScriptTraceThread * {
		for (size_t i = 0; i < trace->thread_count; i++) {
			if (trace->threads[i].frame_count > 0 && strcmp(trace->threads[i].frames[0].path, p_path) == 0) {
				return &trace->threads[i];
			}
		}
		return nullptr;
	};
	uint64_t thread_id = 0;
	const BreakpadGDScriptTraceThread *thread_trace = find_thread_with_path("res://outer.gd");
	for (size_t i = 0; i < trace->thread_count; i++) {
		if (&trace->threads[i] == thread_trace) {
			thread_id = trace->threads[i].thread_id;
			break;
		}
	}

	REQUIRE(thread_trace != nullptr);
	CHECK(thread_id != 0);
	REQUIRE(thread_trace->frame_count == 2);
	CHECK(String::utf8(thread_trace->frames[0].path) == "res://outer.gd");
	CHECK(String::utf8(thread_trace->frames[0].function) == "outer");
	CHECK(thread_trace->frames[0].line == 10);
	CHECK(String::utf8(thread_trace->frames[1].path) == "res://inner.gd");
	CHECK(String::utf8(thread_trace->frames[1].function) == "inner");
	CHECK(thread_trace->frames[1].line == 21);

	BreakpadTraceThreadTestData thread_data;
	Thread worker_thread;
	REQUIRE(worker_thread.start(breakpad_trace_thread_test, &thread_data) != Thread::UNASSIGNED_ID);
	while (!thread_data.ready.load(std::memory_order_acquire)) {
		Thread::yield();
	}
	const BreakpadGDScriptTraceThread *worker_trace = find_thread_with_path("res://worker.gd");
	REQUIRE(worker_trace != nullptr);
	CHECK(worker_trace->thread_id != thread_trace->thread_id);
	thread_data.finish.store(true, std::memory_order_release);
	worker_thread.wait_to_finish();

	breakpad_gdscript_exit();
	CHECK(thread_trace->frame_count == 1);
	CHECK(thread_trace->frames[0].line == 10);
	breakpad_gdscript_exit();
	CHECK(thread_trace->thread_id == 0);
	CHECK(thread_trace->frame_count == 0);

	for (size_t i = 0; i < BREAKPAD_GDSCRIPT_TRACE_FRAME_COUNT + 1; i++) {
		breakpad_gdscript_enter("res://nested.gd", "nested", (int)i + 1);
	}
	thread_trace = find_thread_with_path("res://nested.gd");
	REQUIRE(thread_trace != nullptr);
	CHECK(thread_trace->frame_count == BREAKPAD_GDSCRIPT_TRACE_FRAME_COUNT);
	CHECK(thread_trace->truncated == 1);
	for (size_t i = 0; i < BREAKPAD_GDSCRIPT_TRACE_FRAME_COUNT + 1; i++) {
		breakpad_gdscript_exit();
	}
	CHECK(thread_trace->thread_id == 0);
	CHECK(thread_trace->frame_count == 0);
	CHECK(thread_trace->truncated == 0);

	breakpad_gdscript_enter("res://reused.gd", "reused", 42);
	thread_trace = find_thread_with_path("res://reused.gd");
	REQUIRE(thread_trace != nullptr);
	CHECK(thread_trace->thread_id == thread_id);
	CHECK(thread_trace->frame_count == 1);
	CHECK(String::utf8(thread_trace->frames[0].path) == "res://reused.gd");
	breakpad_gdscript_exit();

	const String long_path = "res://" + String("p").repeat(BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE * 2);
	const String long_function = String("f").repeat(BREAKPAD_GDSCRIPT_TRACE_FUNCTION_SIZE * 2);
	breakpad_gdscript_enter(long_path, long_function, 99);
	const String expected_long_path = "res://" + String("p").repeat(BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 1 - 6);
	thread_trace = find_thread_with_path(expected_long_path.utf8().get_data());
	REQUIRE(thread_trace != nullptr);
	CHECK(thread_trace->frame_count == 1);
	CHECK(thread_trace->frames[0].path[BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 1] == '\0');
	CHECK(thread_trace->frames[0].function[BREAKPAD_GDSCRIPT_TRACE_FUNCTION_SIZE - 1] == '\0');
	CHECK(strlen(thread_trace->frames[0].path) == BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 1);
	CHECK(strlen(thread_trace->frames[0].function) == BREAKPAD_GDSCRIPT_TRACE_FUNCTION_SIZE - 1);
	breakpad_gdscript_exit();

	const String utf8_boundary_path = String("a").repeat(BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 2) + "é";
	breakpad_gdscript_enter(utf8_boundary_path, "utf8", 100);
	thread_trace = find_thread_with_path(String("a").repeat(BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 2).utf8().get_data());
	REQUIRE(thread_trace != nullptr);
	CHECK(thread_trace->frame_count == 1);
	CHECK(strlen(thread_trace->frames[0].path) == BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 2);
	CHECK(String::utf8(thread_trace->frames[0].path) == String("a").repeat(BREAKPAD_GDSCRIPT_TRACE_PATH_SIZE - 2));
	breakpad_gdscript_exit();
}
#endif

class TestGDScriptCacheAccessor {
public:
	static bool has_shallow(String p_path) {
		return GDScriptCache::singleton->shallow_gdscript_cache.has(p_path);
	}

	static bool has_full(String p_path) {
		return GDScriptCache::singleton->full_gdscript_cache.has(p_path);
	}
};

// TODO: Handle some cases failing on release builds. See: https://github.com/godotengine/godot/pull/88452
#ifdef TOOLS_ENABLED
TEST_SUITE("[Modules][GDScript]") {
	TEST_CASE("Script compilation and runtime") {
		bool print_filenames = OS::get_singleton()->get_cmdline_args().find("--print-filenames") != nullptr;
		bool use_binary_tokens = OS::get_singleton()->get_cmdline_args().find("--use-binary-tokens") != nullptr;
		GDScriptTestRunner runner("modules/gdscript/tests/scripts", true, print_filenames, use_binary_tokens);
		int fail_count = runner.run_tests();
		INFO("Make sure `*.out` files have expected results.");
		REQUIRE_MESSAGE(fail_count == 0, "All GDScript tests should pass.");
	}
}
#endif // TOOLS_ENABLED

TEST_CASE("[Modules][GDScript] Load source code dynamically and run it") {
	GDScriptLanguage::get_singleton()->init();
	Ref<GDScript> gdscript = memnew(GDScript);
	gdscript->set_source_code(R"(
extends RefCounted

func _init():
	set_meta("result", 42)
)");
	// A spurious `Condition "err" is true` message is printed (despite parsing being successful and returning `OK`).
	// Silence it.
	ERR_PRINT_OFF;
	const Error error = gdscript->reload();
	ERR_PRINT_ON;
	CHECK_MESSAGE(error == OK, "The script should parse successfully.");

	// Run the script by assigning it to a reference-counted object.
	Ref<RefCounted> ref_counted = memnew(RefCounted);
	ref_counted->set_script(gdscript);
	CHECK_MESSAGE(int(ref_counted->get_meta("result")) == 42, "The script should assign object metadata successfully.");
}

TEST_CASE("[Modules][GDScript] Loading keeps ResourceCache and GDScriptCache in sync") {
	const String path = TestUtils::get_temp_path("gdscript_load_test.gd");

	{
		Ref<FileAccess> fa = FileAccess::open(path, FileAccess::ModeFlags::WRITE);
		fa->store_string("extends Node\n");
		fa->close();
	}

	CHECK(!ResourceCache::has(path));
	CHECK(!TestGDScriptCacheAccessor::has_shallow(path));
	CHECK(!TestGDScriptCacheAccessor::has_full(path));

	Ref<GDScript> loaded = ResourceLoader::load(path);

	CHECK(ResourceCache::has(path));
	CHECK(!TestGDScriptCacheAccessor::has_shallow(path));
	CHECK(TestGDScriptCacheAccessor::has_full(path));
}

TEST_CASE("[Modules][GDScript] Validate built-in API") {
	GDScriptLanguage *lang = GDScriptLanguage::get_singleton();

	// Validate methods.
	List<MethodInfo> builtin_methods;
	lang->get_public_functions(&builtin_methods);

	SUBCASE("[Modules][GDScript] Validate built-in methods") {
		for (const MethodInfo &mi : builtin_methods) {
			for (int64_t i = 0; i < mi.arguments.size(); ++i) {
				TEST_COND((mi.arguments[i].name.is_empty() || mi.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in method '%s'.", i, mi.name));
			}
		}
	}

	// Validate annotations.
	List<MethodInfo> builtin_annotations;
	lang->get_public_annotations(&builtin_annotations);

	SUBCASE("[Modules][GDScript] Validate built-in annotations") {
		for (const MethodInfo &ai : builtin_annotations) {
			for (int64_t i = 0; i < ai.arguments.size(); ++i) {
				TEST_COND((ai.arguments[i].name.is_empty() || ai.arguments[i].name.begins_with("_unnamed_arg")),
						vformat("Unnamed argument in position %d of built-in annotation '%s'.", i, ai.name));
			}
		}
	}
}

} // namespace GDScriptTests
