package main

// HTTP 静态资源服务辅助。内嵌的 staticFS/configFS 仍为包级（编译期内嵌常量，非可变全局）。

import (
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
)

// staticFileSystem 优先用磁盘 static/ 目录（便于替换前端），否则回退内嵌资源。
func staticFileSystem(appBase string) http.FileSystem {
	disk := filepath.Join(appBase, "static")
	if _, err := os.Stat(disk); err == nil {
		return http.Dir(disk)
	}
	sub, _ := fs.Sub(staticFS, "static")
	return http.FS(sub)
}

func noCacheFS(h http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Cache-Control", "no-store, no-cache, must-revalidate")
		w.Header().Set("Pragma", "no-cache")
		h.ServeHTTP(w, r)
	})
}
