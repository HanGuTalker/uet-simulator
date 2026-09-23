build_lib(
  LIBNAME roce
  SOURCE_FILES
    model/roce-v2-header.cc
    model/roce-simulation-tag.cc
    model/roce-v2-transport-adapter.cc
  HEADER_FILES
    model/roce-v2-header.h
    model/roce-simulation-tag.h
    model/roce-v2-transport-adapter.h
  LIBRARIES_TO_LINK
    ${libai-transport}
    ${libcore}
    ${libnetwork}
    ${libinternet}
)
