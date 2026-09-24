build_lib(
  LIBNAME mrc
  SOURCE_FILES
    model/mrc-header.cc
    model/mrc-transport-adapter.cc
  HEADER_FILES
    model/mrc-header.h
    model/mrc-transport-adapter.h
  LIBRARIES_TO_LINK
    ${libai-transport}
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libroce}
)
