#pragma once

#include <initializer_list>
#include <string>

namespace fc {

/**
 * Makes sure the C++ library can build the locale that the environment (LANG, LC_*) names.
 *
 * With libstdc++, std::locale("") throws std::runtime_error when LANG or any LC_* variable names a locale
 * that cannot be loaded (usually one that is not installed, e.g. forwarded by ssh from a client that uses
 * it), unless LC_ALL names one that can. Code that builds that locale lazily then fails far from the cause.
 *
 * If the environment's locale can be loaded, nothing is changed. Otherwise LC_ALL is set to "C.UTF-8", or
 * to "C" when C.UTF-8 cannot be loaded either. That replaces every category, including those that named a
 * usable locale, so a program that depends on one of them should not call this. Child processes inherit
 * the changed environment.
 *
 * Call it at the very start of main(), before any thread is started: it may call setenv(), which is not
 * thread-safe. Library code must not call it, as it changes the environment of the whole process.
 * It does nothing on Windows, and it is not part of HIVE_BUILD_ON_MINIMAL_FC builds.
 *
 * @param print_warning  write the description to stderr as a warning; a program that uses stderr for
 *                       output, or a daemon, can pass false and write the returned description to its log
 * @return a one-line description of what was changed and why, or an empty string if the environment was
 *         left unchanged
 */
std::string sanitize_locale_environment( bool print_warning = true );

namespace detail {

/// What sanitize_locale_environment() does, with the fallback locales as a parameter (exposed for tests).
/// If none of the fallbacks can be loaded, or LC_ALL cannot be set, LC_ALL is restored to its previous value.
std::string replace_unusable_locale( std::initializer_list< const char* > fallbacks );

} // namespace detail

} // namespace fc
