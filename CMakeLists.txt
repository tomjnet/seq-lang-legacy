cmake_minimum_required(VERSION 3.20)

project(seqc_legacy VERSION 0.1.0 LANGUAGES CXX)

# C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The built-in dataset is a committed text file. It is embedded into the
# compiler as a raw string literal.
set(SEQ_LEGACY_DATASET_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/data/sample-company-transaction-db.txt")
set(SEQ_LEGACY_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(READ "${SEQ_LEGACY_DATASET_FILE}" SEQ_LEGACY_DATASET_TEXT)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/sema/sample_company_transaction_db.inc.in"
  "${SEQ_LEGACY_GENERATED_DIR}/sample_company_transaction_db.inc"
  @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${SEQ_LEGACY_DATASET_FILE}")

add_library(seqlegacycore STATIC
  src/assembler/assembler.cc
  src/cli/commands.cc
  src/codegen/x64_codegen.cc
  src/driver/build.cc
  src/driver/run.cc
  src/ir/ir.cc
  src/ir/lowering.cc
  src/lexer/lexer.cc
  src/linker/elf_writer.cc
  src/linker/linker.cc
  src/parser/parser.cc
  src/project/project.cc
  src/sema/builtins.cc
  src/sema/request.cc
  src/sema/validator.cc
  src/support/diagnostics.cc
  src/support/filesystem.cc
  src/support/util.cc
)
target_include_directories(seqlegacycore PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${SEQ_LEGACY_GENERATED_DIR}")
target_compile_definitions(seqlegacycore PUBLIC
  SEQ_LEGACY_VERSION="${PROJECT_VERSION}")

add_executable(seqc_legacy src/cli/main.cc)
target_link_libraries(seqc_legacy PRIVATE seqlegacycore)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
