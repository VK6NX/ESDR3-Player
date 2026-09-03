find_package(Qt6 6.10 REQUIRED COMPONENTS Test)

qt_add_executable(tst_wav tst_wav.cpp)
target_link_libraries(tst_wav PRIVATE esdr3core Qt6::Test)
target_compile_definitions(tst_wav PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_wav COMMAND tst_wav)

qt_add_executable(tst_demod tst_demod.cpp)
target_link_libraries(tst_demod PRIVATE esdr3core Qt6::Test)
add_test(NAME tst_demod COMMAND tst_demod)
