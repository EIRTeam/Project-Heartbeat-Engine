/*************************************************************************/
/*  process_unix.h                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef PROCESS_UNIX_H
#define PROCESS_UNIX_H

#ifdef UNIX_ENABLED

#include "process_tiny_process_lib.h"

class ProcessUnix : public ProcessTinyProcessLibrary {
	static bool handle_sigchld(int s_p_id, void *userdata);

	// We keep track of all processes so that they don't disappear before sigchld is called
	static Vector<Ref<ProcessUnix>> processes;

protected:
	static Ref<Process> create_unix(const String &m_path, const Vector<String> &p_arguments, const String &p_working_dir, bool p_open_stdin);

public:
	static void make_default();
	ProcessUnix(const String &m_path, const Vector<String> &p_arguments, const String &p_working_dir, bool p_open_stdin);
};
#endif // UNIX_ENABLED

#endif // PROCESS_UNIX_H