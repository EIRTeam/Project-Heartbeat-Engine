/*************************************************************************/
/*  process_unix.cpp                                                     */
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

#ifdef UNIX_ENABLED

#include "process_unix.h"
#include "drivers/unix/os_unix.h"

void ProcessUnix::make_default() {
	_create = create_unix;
	//OS_Unix::get_singleton()->add_sigchld_callback(&handle_sigchld, nullptr);
}

Vector<Ref<ProcessUnix>> ProcessUnix::processes;

Ref<Process> ProcessUnix::create_unix(const String &p_path, const Vector<String> &p_arguments, const String &p_working_dir, bool p_open_stdin) {
	Ref<ProcessUnix> proc = memnew(ProcessUnix(p_path, p_arguments, p_working_dir, p_open_stdin));
	processes.push_back(proc.ptr());
	return proc;
}

bool ProcessUnix::handle_sigchld(int s_p_id, void *userdata) {
	for (int i = 0; i < processes.size(); i++) {
		Ref<ProcessUnix> process = processes[i];
		if (process->get_id() == s_p_id) {
			// TinyProcessLib will reap the process if needed
			// when get_exit_status is called
			process->get_exit_status();
			processes.erase(process);
			return true;
		}
	}
	return false;
}

ProcessUnix::ProcessUnix(const String &m_path, const Vector<String> &p_arguments, const String &p_working_dir, bool p_open_stdin) :
		ProcessTinyProcessLibrary(m_path, p_arguments, p_working_dir, p_open_stdin) {
}

#endif // UNIX_ENABLED
