build_lib(
  LIBNAME falcon
  SOURCE_FILES
    model/falcon-header.cc
  HEADER_FILES
    model/falcon-header.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
  TEST_SOURCES
    test/falcon-header-test-suite.cc
)
