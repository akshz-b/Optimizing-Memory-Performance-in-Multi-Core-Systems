add_library(ramulator-addrmapper OBJECT)

target_sources(
  ramulator-addrmapper PRIVATE
  addr_mapper.h 

  impl/linear_mappers.cpp
  impl/rit.cpp
  impl/rit.h

)

target_link_libraries(
  ramulator
  PRIVATE
  ramulator-addrmapper
)