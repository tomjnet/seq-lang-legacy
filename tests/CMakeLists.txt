add_executable(seq_legacy_unit_tests
  unit/test_main.cc
  unit/test_backend.cc
  unit/test_builtins.cc
  unit/test_front_end.cc
  unit/test_support.cc
)
target_link_libraries(seq_legacy_unit_tests PRIVATE seqlegacycore)
target_compile_definitions(seq_legacy_unit_tests PRIVATE
  SEQ_LEGACY_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  SEQ_LEGACY_DATASET_FILE="${SEQ_LEGACY_DATASET_FILE}")

add_test(NAME unit COMMAND seq_legacy_unit_tests)

add_test(NAME cli.version COMMAND seqc_legacy --version)
add_test(NAME cli.help COMMAND seqc_legacy --help)
add_test(NAME cli.unknown_command COMMAND seqc_legacy frobnicate)
set_tests_properties(cli.unknown_command PROPERTIES WILL_FAIL TRUE)
add_test(NAME examples.top3Company
         COMMAND seqc_legacy check
                 "${PROJECT_SOURCE_DIR}/examples/top3Company/src/main.seq")

# The run stage needs Linux, so the end-to-end script does too.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME integration.end_to_end
           COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/integration/end_to_end.sh"
                   "$<TARGET_FILE:seqc_legacy>")
endif()
