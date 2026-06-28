# 单测：均为无 GUI 的控制台程序，独立链接 heritage_core，不依赖 QtTest 模块。

add_executable(storage_test StorageTest.cpp)
target_link_libraries(storage_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
add_test(NAME storage_test COMMAND storage_test)

# classify 纯函数单测（搬运 Go classify_test）
add_executable(classify_test ClassifyTest.cpp)
target_link_libraries(classify_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core)
add_test(NAME classify_test COMMAND classify_test)

# analysis / stats 集成单测：需读资源目录里的默认配置
set(_res_dir "${CMAKE_CURRENT_SOURCE_DIR}/../resources")
add_executable(analysis_test AnalysisTest.cpp)
target_link_libraries(analysis_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
target_compile_definitions(analysis_test PRIVATE HERITAGE_RESOURCE_DIR="${_res_dir}")
add_test(NAME analysis_test COMMAND analysis_test)

add_executable(stats_test StatsTest.cpp)
target_link_libraries(stats_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
add_test(NAME stats_test COMMAND stats_test)

# 文档服务单测：上传(复制+插记录+日志)/删除单文档/删除分类
add_executable(document_test DocumentTest.cpp)
target_link_libraries(document_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
target_compile_definitions(document_test PRIVATE HERITAGE_RESOURCE_DIR="${_res_dir}")
add_test(NAME document_test COMMAND document_test)

# excelimport 单测：自写 xlsx 解析器读真实 .xlsx fixture，验 LoadFinancials/finGet/...
set(_fix_dir "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_executable(excel_test ExcelTest.cpp)
target_link_libraries(excel_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core)
target_compile_definitions(excel_test PRIVATE HERITAGE_FIXTURE_DIR="${_fix_dir}")
add_test(NAME excel_test COMMAND excel_test)

# 批量导入集成单测：构造 Basicdata → ImportService.importAll → 验入库+财务
add_executable(import_test ImportTest.cpp)
target_link_libraries(import_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)
target_compile_definitions(import_test PRIVATE
    HERITAGE_FIXTURE_DIR="${_fix_dir}"
    HERITAGE_RESOURCE_DIR="${_res_dir}")
add_test(NAME import_test COMMAND import_test)

# 报告单测：生成 PDF 验证（QPdfWriter 需 GUI → QGuiApplication offscreen）
add_executable(report_test ReportTest.cpp)
target_link_libraries(report_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui)
add_test(NAME report_test COMMAND report_test)

# LLM 离线单测：buildRequestBody / parseContent / configured / buildAnalysisPrompt（不联网）
add_executable(llm_test LlmTest.cpp)
target_link_libraries(llm_test PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Core)
add_test(NAME llm_test COMMAND llm_test)

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
