add_library(ramulator-translation OBJECT)

target_sources(
  ramulator-translation PRIVATE
  translation.h

  impl/no_translation.cpp
  impl/random_translation.cpp
  impl/random_translation2.cpp
  impl/Dynamic_migration.cpp
  impl/Local_to_requester.cpp
)

target_link_libraries(
  ramulator
  PRIVATE
  ramulator-translation
)