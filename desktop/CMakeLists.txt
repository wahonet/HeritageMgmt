cmake_minimum_required(VERSION 3.16)
project(heritage-desktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 元对象/资源/uic 自动处理；同一份 CMake 兼容 Qt6 与 Qt 5.15（麒麟源可能仅 Qt5）。
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 优先 Qt6，缺失则回退 Qt5。
find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Gui Widgets Sql REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Gui Widgets Sql REQUIRED)

# ---- 核心静态库（无 GUI，便于单测独立链接）----
add_library(heritage_core STATIC
    src/core/storage/Schema.cpp
    src/core/storage/Database.cpp
    src/core/storage/ProjectScanner.cpp
    src/core/storage/UnitRepo.cpp
    src/core/storage/ProjectRepo.cpp
    src/core/storage/DocumentRepo.cpp
    src/core/storage/LogRepo.cpp
    src/core/config/ConfigLoader.cpp
    src/core/config/AppConfig.cpp
    src/core/classify/Classify.cpp
    src/core/analysis/AnalysisService.cpp
    src/core/stats/StatsService.cpp
    src/core/dashboard/DashboardService.cpp
    src/core/documents/DocumentService.cpp
)
target_include_directories(heritage_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(heritage_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql)

# ---- 主程序（GUI + 入口）----
add_executable(heritage-desktop
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/widgets/ProjectDetailPanel.h
    src/ui/widgets/ProjectDetailPanel.cpp
    src/ui/views/DashboardView.h
    src/ui/views/DashboardView.cpp
    src/ui/dialogs/UploadDialog.h
    src/ui/dialogs/UploadDialog.cpp
    src/ui/dialogs/CreateProjectDialog.h
    src/ui/dialogs/CreateProjectDialog.cpp
    src/ui/dialogs/ProjectEditDialog.h
    src/ui/dialogs/ProjectEditDialog.cpp
    resources/heritage.qrc
)
# Windows(mingw) 交叉编译链接兜底：rcc 生成代码引用的 qResourceFeatureZstd() 在
# mingw 版 Qt6Core 导入库缺失，提供桩实现。Linux 原生构建不需要（不编译此文件）。
if(WIN32)
    target_sources(heritage-desktop PRIVATE src/win_zstd_stub.cpp)
endif()
target_link_libraries(heritage-desktop PRIVATE
    heritage_core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui)

# ---- 单测 ----
option(HERITAGE_BUILD_TESTS "Build heritage unit tests" ON)
if(HERITAGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
