#include <fc/filesystem.hpp>
#include <fc/stacktrace.hpp>

namespace fc {

void print_stacktrace(std::ostream& out, unsigned int max_frames /* = 63 */, void* caller_overwrite_hack /* = nullptr */, bool addr2line /* = true */ )
{
   out << "stack trace not supported on this compiler" << std::endl;
}

std::string get_stacktrace(unsigned int max_frames /* = 63 */, void* caller_overwrite_hack /* = nullptr */, bool addr2line /* = true */ )
{
   return "stack trace not supported on this compiler\n";
}

void print_stacktrace_on_segfault() {}

}
