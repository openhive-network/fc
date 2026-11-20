#include <fc/io/sstream.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <algorithm>
#include <cstring>

namespace fc {
  /**
   *  A string with a read and a write position. It keeps the behavior of the std::stringstream it replaces for
   *  reading a given string and for writing one: both positions start at the beginning, a write overwrites and then
   *  extends the string, and the end of data, once hit by peek(), makes reads and writes fail until clear().
   */
  class stringstream::impl {
    public:
    impl( const fc::string& s )
    :buf( s )
    {}

    impl(){}

    fc::string buf;
    size_t     read_pos = 0;
    size_t     write_pos = 0;
    bool       at_eof = false;
  };

  stringstream::stringstream( fc::string& s )
  :my(s) {
  }
  stringstream::stringstream( const fc::string& s )
  :my(s) {
  }
  stringstream::stringstream(){}
  stringstream::~stringstream(){}


  fc::string stringstream::str(){
    return my->buf;
  }

  void stringstream::str(const fc::string& s) {
    my->buf = s;
    my->read_pos = 0;
    my->write_pos = 0;
  }

  void stringstream::clear() {
    my->at_eof = false;
  }


  bool     stringstream::eof()const {
    return my->at_eof;
  }
  size_t stringstream::writesome( const char* buf, size_t len ) {
    if( my->at_eof )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    my->buf.replace( my->write_pos, std::min( len, my->buf.size() - my->write_pos ), buf, len );
    my->write_pos += len;
    return len;
  }
  size_t stringstream::writesome( const std::shared_ptr<const char>& buf, size_t len, size_t offset )
  {
    return writesome(buf.get() + offset, len);
  }

  size_t   stringstream::readsome( char* buf, size_t len ) {
    size_t r = my->at_eof ? 0 : std::min( len, my->buf.size() - my->read_pos );
    if( r == 0 )
    {
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    memcpy( buf, my->buf.data() + my->read_pos, r );
    my->read_pos += r;
    return r;
  }
  size_t   stringstream::readsome( const std::shared_ptr<char>& buf, size_t len, size_t offset )
  {
    return readsome(buf.get() + offset, len);
  }


  void     stringstream::close(){};
  void     stringstream::flush(){};

  /*
  istream&   stringstream::read( char* buf, size_t len ) {
    my->ss.read(buf,len);
    return *this;
  }
  istream& stringstream::read( int64_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint64_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int32_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint32_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int16_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint16_t&    v ) { my->ss >> v; return *this; }
  istream& stringstream::read( int8_t&      v ) { my->ss >> v; return *this; }
  istream& stringstream::read( uint8_t&     v ) { my->ss >> v; return *this; }
  istream& stringstream::read( float&       v ) { my->ss >> v; return *this; }
  istream& stringstream::read( double&      v ) { my->ss >> v; return *this; }
  istream& stringstream::read( bool&        v ) { my->ss >> v; return *this; }
  istream& stringstream::read( char&        v ) { my->ss >> v; return *this; }
  istream& stringstream::read( fc::string&  v ) { my->ss >> *reinterpret_cast<std::string*>(&v); return *this; }

  ostream& stringstream::write( const fc::string& s) {
    my->ss.write( s.c_str(), s.size() );
    return *this;
  }
  */

  char     stringstream::peek()
  {
    if( my->at_eof || my->read_pos == my->buf.size() )
    {
       my->at_eof = true;
       FC_THROW_EXCEPTION( eof_exception, "stringstream" );
    }
    return my->buf[ my->read_pos ];
  }
} 


