#include <fc/io/stdio.hpp>
#include <cstdio>

namespace fc {

  // std::cout and std::cerr write through to these C streams (they stay synchronized with stdio)
  size_t cout_t::writesome( const char* buf, size_t len ) { fwrite(buf,1,len,stdout); return len; }
  size_t cout_t::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset ) { return writesome(buf.get() + offset, len); }
  void   cout_t::close() {}
  void   cout_t::flush() { fflush(stdout); }

  size_t cerr_t::writesome( const char* buf, size_t len ) { fwrite(buf,1,len,stderr); return len; }
  size_t cerr_t::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset ) { return writesome(buf.get() + offset, len); }
  void   cerr_t::close() {};
  void   cerr_t::flush() { fflush(stderr); }

  std::shared_ptr<cout_t> cout_ptr = std::make_shared<cout_t>();
  std::shared_ptr<cerr_t> cerr_ptr = std::make_shared<cerr_t>();
  cout_t& cout = *cout_ptr;
  cerr_t& cerr = *cerr_ptr;
} // namespace fc
