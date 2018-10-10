add_executable(create_freq_index create_freq_index.cpp)
target_link_libraries(create_freq_index
  ${Boost_LIBRARIES}
  FastPFor
  streamvbyte
  MaskedVByte
  )

add_executable(encode encode.cpp)
target_link_libraries(encode
  ${Boost_LIBRARIES}
  )

add_executable(decode decode.cpp)
target_link_libraries(decode
  ${Boost_LIBRARIES}
  )

add_executable(check_index check_index.cpp)
target_link_libraries(check_index
  ${Boost_LIBRARIES}
  FastPFor
  streamvbyte
  MaskedVByte
  )

add_executable(queries queries.cpp)
target_link_libraries(queries
  ${Boost_LIBRARIES}
  FastPFor
  streamvbyte
  MaskedVByte
  )

add_executable(create_wand_data create_wand_data.cpp)
target_link_libraries(create_wand_data
  ${Boost_LIBRARIES}
  )
