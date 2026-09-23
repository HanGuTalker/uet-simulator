build_lib_example(
  NAME uet-smoke-example
  SOURCE_FILES uet-smoke-example.cc
  LIBRARIES_TO_LINK
    ${libuet}
    ${libnetwork}
)

build_lib_example(
  NAME uet-ai-workload-example
  SOURCE_FILES uet-ai-workload-example.cc
  LIBRARIES_TO_LINK
    ${libai-transport}
    ${libuet}
    ${libnetwork}
    ${libinternet}
    ${libcsma}
    ${libpoint-to-point}
    ${libtraffic-control}
)

build_lib_example(
  NAME uet-rud-example
  SOURCE_FILES uet-rud-example.cc
  LIBRARIES_TO_LINK
    ${libuet}
    ${libnetwork}
)
