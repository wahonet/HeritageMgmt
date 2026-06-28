// 仅用于 Windows(mingw) 交叉编译的链接兜底；Linux 原生构建不编译此文件（见 CMakeLists 的 WIN32 守卫）。
//
// 背景：Qt6.7 mingw 包的 Qt6Core 导入库缺失 rcc 生成代码所引用的
//   unsigned char qResourceFeatureZstd();
// 导致 link 报 undefined reference。这里提供"无 zstd 支持"的实现（返回 0），与 Qt 行为一致。
// 若 Qt6Core 已导出该符号则不会触发未定义引用，本文件也不参与 Linux 构建，故无重复定义风险。

unsigned char qResourceFeatureZstd() { return 0; }
