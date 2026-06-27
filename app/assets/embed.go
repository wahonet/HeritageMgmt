// Package assets 集中 //go:embed，保住单二进制交付。
// static/ 与 config/ 内嵌于此包；其他包通过 assets.StaticFS / assets.ConfigFS 引用。
package assets

import "embed"

// StaticFS 内嵌前端静态资源（磁盘 static/ 优先覆盖，见 httpapi.staticFileSystem）。
//
//go:embed all:static
var StaticFS embed.FS

// ConfigFS 内嵌默认配置（磁盘 config/ 优先覆盖，见 config.ReadConfigFile）。
//
//go:embed all:config
var ConfigFS embed.FS
