#include <fc/log/appender.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/unique_lock.hpp>
#include <unordered_map>
#include <string>
#include <fc/thread/spin_lock.hpp>
#include <fc/thread/scoped_lock.hpp>
#include <fc/log/console_appender.hpp>
#include <fc/variant.hpp>
#include <fc/macros.hpp>
#include "console_defines.h"
#include <iomanip>

#ifndef _WIN32
#include <sys/syscall.h>
#include <time.h>
#endif


namespace fc {

   std::unordered_map<std::string,appender::ptr>& get_appender_map() {
     static std::unordered_map<std::string,appender::ptr> lm;
     return lm;
   }
   std::unordered_map<std::string,appender_factory::ptr>& get_appender_factory_map() {
     static std::unordered_map<std::string,appender_factory::ptr> lm;
     return lm;
   }
   appender::ptr appender::get( const fc::string& s ) {
     static fc::spin_lock appender_spinlock;
      scoped_lock<spin_lock> lock(appender_spinlock);
      return get_appender_map()[s];
   }
   bool appender::register_appender( const fc::string& type, const appender_factory::ptr& f )
   {
      get_appender_factory_map()[type] = f;
      return true;
   }
   appender::ptr appender::create( const fc::string& name, const fc::string& type, const variant& args  )
   {
      auto fact_itr = get_appender_factory_map().find(type);
      if( fact_itr == get_appender_factory_map().end() ) {
         //wlog( "Unknown appender type '%s'", type.c_str() );
         return appender::ptr();
      }
      auto ap = fact_itr->second->create( args );
      get_appender_map()[name] = ap;
      return ap;
   }

   /* static */ std::string appender::format_time_as_string(time_point time, time_format format)
   {
      std::stringstream result;
      switch (format) {
      case appender::time_format::milliseconds_since_epoch:
        result << time.time_since_epoch().count() / 1000 << "ms";
        break;
      case appender::time_format::iso_8601_seconds:
        result << time.to_iso_string_in_seconds();
        break;
      case appender::time_format::iso_8601_milliseconds:
        result << time.to_iso_string_in_milliseconds();
        break;
      case appender::time_format::iso_8601_microseconds:
        result << time.to_iso_string_in_microseconds();
        break;
      case appender::time_format::iso_8601_realtime_microseconds:
        {
#ifndef _WIN32
          // Use syscall directly to bypass libfaketime interception
          struct timespec ts;
          syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts);
          std::time_t seconds = ts.tv_sec;
          std::tm utc_tm;
          gmtime_r(&seconds, &utc_tm);
          result << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S")
                 << '.' << std::setfill('0') << std::setw(6) << (ts.tv_nsec / 1000);
#else
          // On Windows, just use regular time (libfaketime is Linux-only anyway)
          result << time.to_iso_string_in_microseconds();
#endif
        }
        break;
      case appender::time_format::milliseconds_since_hour:
      default:
        result << (time.time_since_epoch().count() % (1000ll * 1000ll * 60ll * 60)) / 1000 << "ms";
      }
      return result.str();
   }


   /*
    Assiging a function return to a static variable allows code exectution on
    initialization. In a perfect world, we would not do this. This results in an
    unused variable warning. Passing the pointer to the variable in to the lambda
    allows us to safely mark the variable as unused. (void)var does not work
    because we cannot execute in this block.
   */
   static bool reg_console_appender = []( __attribute__((unused)) bool* )->bool
   {
      return appender::register_appender<console_appender>( "console" );
   }( &reg_console_appender );
} // namespace fc
