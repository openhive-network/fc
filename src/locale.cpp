#include <fc/locale.hpp>

#ifndef _WIN32
# include <cstdlib>
# include <exception>
# include <iostream>
# include <locale>
#endif

namespace fc {

#ifndef _WIN32
namespace {

bool environment_locale_is_usable()
{
  try
  {
    const std::locale probe( "" );
    return true;
  }
  catch( const std::exception& )
  {
    return false;
  }
}

// Lists the locale variables whose values cannot be loaded, e.g. "LC_TIME=pl_PL.UTF-8".
std::string unusable_locale_variables()
{
  std::string result;
  for( const char* name : { "LC_ALL", "LC_CTYPE", "LC_NUMERIC", "LC_TIME", "LC_COLLATE", "LC_MONETARY",
                            "LC_MESSAGES", "LC_PAPER", "LC_NAME", "LC_ADDRESS", "LC_TELEPHONE",
                            "LC_MEASUREMENT", "LC_IDENTIFICATION", "LANG" } )
  {
    const char* value = std::getenv( name );
    if( value == nullptr || *value == '\0' )
      continue;
    try
    {
      const std::locale probe( value );
    }
    catch( const std::exception& )
    {
      if( !result.empty() )
        result += ", ";
      result += std::string( name ) + "=" + value;
    }
  }
  return result.empty() ? std::string( "LANG/LC_*" ) : result;
}

} // namespace
#endif

namespace detail {

std::string replace_unusable_locale( std::initializer_list< const char* > fallbacks )
{
#ifndef _WIN32
  if( environment_locale_is_usable() )
    return std::string();

  const std::string problem = "The locale named in the environment (" + unusable_locale_variables() +
                              ") could not be loaded";

  const char* previous = std::getenv( "LC_ALL" );
  const bool had_lc_all = previous != nullptr;
  const std::string previous_lc_all = had_lc_all ? previous : "";
  const auto restore_lc_all = [&]()
  {
    if( had_lc_all )
      setenv( "LC_ALL", previous_lc_all.c_str(), 1 );
    else
      unsetenv( "LC_ALL" );
  };

  std::string tried;
  for( const char* fallback : fallbacks )
  {
    if( setenv( "LC_ALL", fallback, 1 ) != 0 )
    {
      restore_lc_all();
      return problem + ", and LC_ALL could not be set";
    }
    if( environment_locale_is_usable() )
      return problem + ", using LC_ALL=" + fallback;
    tried += ( tried.empty() ? "" : ", " ) + std::string( fallback );
  }

  restore_lc_all();
  return problem + ", and none of the fallbacks (" + tried + ") could be loaded either";
#else
  (void)fallbacks;
  return std::string();
#endif
}

} // namespace detail

std::string sanitize_locale_environment( bool print_warning )
{
#ifndef _WIN32
  const std::string description = detail::replace_unusable_locale( { "C.UTF-8", "C" } );
  if( print_warning && !description.empty() )
    std::cerr << "Warning: " << description << std::endl;
  return description;
#else
  (void)print_warning;
  return std::string();
#endif
}

} // namespace fc
