set(REQUIRED_SOURCES)

set(TEST_LIBRARIES event Threads::Threads)

# Build tests
add_executable(event_bench
    event_bench.c
    ${REQUIRED_SOURCES}
)
target_link_libraries(event_bench ${TEST_LIBRARIES})

add_executable(event_pp
    event_pp_client.c
    event_pp_server.c
    ${REQUIRED_SOURCES}
)
target_link_libraries(event_pp ${TEST_LIBRARIES})
