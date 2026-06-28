# 单测：均为无 GUI 的控制台程序，独立链接 heritage_core，不依赖 QtTest 模块。

add_executable(storage_test StorageTest.cpp)
target_link_libraries(storage_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
add_test(NAME storage_test COMMAND storage_test)

# config_test 同时链接 qrc，以验证“磁盘读不到 → 回退 :/config 资源”。
add_executable(config_test ConfigTest.cpp ../resources/heritage.qrc)
target_link_libraries(config_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
target_compile_definitions(config_test PRIVATE
    HERITAGE_RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../resources")
if(WIN32)
    target_sources(config_test PRIVATE ../src/win_zstd_stub.cpp)
endif()
add_test(NAME config_test COMMAND config_test)
