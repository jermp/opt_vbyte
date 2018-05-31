add_executable(drop_caches drop_caches.c)

add_executable(scan_perftest scan_perftest.cpp)
target_link_libraries(scan_perftest
  ${Boost_LIBRARIES}
  FastPFor
  streamvbyte
  MaskedVByte
  )

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark
  ${Boost_LIBRARIES}
  FastPFor
  streamvbyte
  MaskedVByte
  )

