# Find Boost unit_test_framework for tests
if(NOT DEFINED Boost_USE_STATIC_LIBS)
  set(Boost_USE_STATIC_LIBS ON)
endif()
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable( bip_lock bip_lock.cpp )
target_link_libraries( bip_lock fc )

add_executable( api api.cpp )
target_link_libraries( api fc )

if( ECC_IMPL STREQUAL secp256k1 )
    add_executable( blind all_tests.cpp crypto/blind.cpp )
    target_link_libraries( blind fc Boost::unit_test_framework )
endif()

add_executable( hash_benchmark crypto/hash_benchmark.cpp )
target_link_libraries( hash_benchmark fc )

add_executable( ntp_test all_tests.cpp network/ntp_test.cpp )
target_link_libraries( ntp_test fc Boost::unit_test_framework )

add_executable( task_cancel_test all_tests.cpp thread/task_cancel.cpp )
target_link_libraries( task_cancel_test fc Boost::unit_test_framework )

add_executable( thread_test all_tests.cpp thread/thread_tests.cpp )
target_link_libraries( thread_test fc Boost::unit_test_framework )

add_executable( bloom_test all_tests.cpp bloom_test.cpp )
target_link_libraries( bloom_test fc Boost::unit_test_framework )

add_executable( real128_test all_tests.cpp real128_test.cpp )
target_link_libraries( real128_test fc Boost::unit_test_framework )

add_executable( hmac_test hmac_test.cpp )
target_link_libraries( hmac_test fc Boost::unit_test_framework )

add_executable( ecc_test crypto/ecc_test.cpp )
target_link_libraries( ecc_test fc )

add_executable( log_test crypto/log_test.cpp )
target_link_libraries( log_test fc )

add_executable( saturation_test saturation_test.cpp )
target_link_libraries( saturation_test fc )

add_executable( ecdsa_canon_test crypto/ecdsa_canon_test.cpp )
target_link_libraries( ecdsa_canon_test fc )

#add_executable( test_aes aes_test.cpp )
#target_link_libraries( test_aes fc ${rt_library} ${pthread_library} )
#add_executable( test_sleep sleep.cpp )
#target_link_libraries( test_sleep fc )
#add_executable( test_rate_limiting rate_limiting.cpp )
#target_link_libraries( test_rate_limiting fc )

add_executable( sha_test sha_test.cpp )
target_link_libraries( sha_test fc )

add_executable( variant_test all_tests.cpp variant_test.cpp )
target_link_libraries( variant_test fc Boost::unit_test_framework )

add_executable( all_tests all_tests.cpp
                          compress/compress.cpp
                          crypto/aes_test.cpp
                          crypto/base_n_tests.cpp
                          crypto/bigint_test.cpp
                          crypto/blind.cpp
                          crypto/blowfish_test.cpp
                          crypto/rand_test.cpp
                          crypto/sha_tests.cpp
                          network/ntp_test.cpp
                          network/http/websocket_test.cpp
                          thread/task_cancel.cpp
                          thread/thread_tests.cpp
                          bloom_test.cpp
                          real128_test.cpp
                          utf8_test.cpp
                          )
target_link_libraries( all_tests fc Boost::unit_test_framework )

if(NOT Boost_USE_STATIC_LIBS)
  if(TARGET blind)
    target_compile_definitions( blind PRIVATE BOOST_TEST_DYN_LINK )
  endif()
  target_compile_definitions( ntp_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( task_cancel_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( thread_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( bloom_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( real128_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( hmac_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( variant_test PRIVATE BOOST_TEST_DYN_LINK )
  target_compile_definitions( all_tests PRIVATE BOOST_TEST_DYN_LINK )
endif()
