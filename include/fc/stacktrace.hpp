// stacktrace.h (c) 2008, Timo Bingmann from http://idlebox.net/
// published under the WTFPL v2.0

// Downloaded from http://panthema.net/2008/0901-stacktrace-demangled/
// and modified for C++ and FC by Steemit, Inc.

#pragma once

#include <iostream>
#include <string>

namespace fc {

void print_stacktrace(std::ostream& out, unsigned int max_frames = 63, void* caller_overwrite_hack = nullptr, bool addr2line = true);
/// What print_stacktrace() prints, as a string
std::string get_stacktrace(unsigned int max_frames = 63, void* caller_overwrite_hack = nullptr, bool addr2line = true);
void print_stacktrace_on_segfault();

}
