build_lib(
  LIBNAME falcon
  SOURCE_FILES
    model/falcon-control-header.cc
    model/falcon-header.cc
    model/falcon-reliability.cc
  HEADER_FILES
    model/falcon-control-header.h
    model/falcon-header.h
    model/falcon-reliability.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
  TEST_SOURCES
    test/falcon-header-test-suite.cc
    test/falcon-reliability-test-suite.cc
)
