add_executable(test_tokenizer test_tokenizer.cpp)
target_link_libraries(test_tokenizer PRIVATE osshell)
add_test(NAME tokenizer COMMAND test_tokenizer)

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser PRIVATE osshell)
add_test(NAME parser COMMAND test_parser)

add_executable(test_executor test_executor.cpp)
target_link_libraries(test_executor PRIVATE osshell)
add_test(NAME executor COMMAND test_executor)

add_executable(test_jobs test_jobs.cpp)
target_link_libraries(test_jobs PRIVATE osshell)
add_test(NAME jobs COMMAND test_jobs)

add_executable(test_scheduler test_scheduler.cpp)
target_link_libraries(test_scheduler PRIVATE osshell)
add_test(NAME scheduler COMMAND test_scheduler)

add_executable(test_stream test_stream.cpp)
target_link_libraries(test_stream PRIVATE osshell)
add_test(NAME stream COMMAND test_stream)

add_executable(test_v4_history test_v4_history.cpp)
target_link_libraries(test_v4_history PRIVATE osshell)
add_test(NAME v4_history COMMAND test_v4_history)
