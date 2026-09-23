build_lib(
  LIBNAME ai-transport
  SOURCE_FILES
    model/ai-transport-endpoint.cc
    model/ai-transport-factory.cc
    model/ai-transport-types.cc
  HEADER_FILES
    model/ai-transport-endpoint.h
    model/ai-transport-factory.h
    model/ai-transport-types.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
)
