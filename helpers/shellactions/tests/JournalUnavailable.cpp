#include <cerrno>

// Linked only into the failure-path test helper, never the installed binary.
extern "C" int __wrap_sd_journal_stream_fd(const char *, int, int)
{
    return -ECONNREFUSED;
}
