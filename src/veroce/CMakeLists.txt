build_lib(
  LIBNAME veroce
  SOURCE_FILES
    model/veroce-header.cc
    model/veroce-trim-queue-disc.cc
    model/veroce-transport-adapter.cc
  HEADER_FILES
    model/veroce-header.h
    model/veroce-trim-queue-disc.h
    model/veroce-transport-adapter.h
  LIBRARIES_TO_LINK
    ${libai-transport}
    ${libroce}
    ${libcore}
    ${libnetwork}
    ${libinternet}
    ${libtraffic-control}
)
