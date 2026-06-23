package main

import (
	"encoding/json"
	"io/fs"
	"net/http"
	"os"
	"path/filepath"
)

func readConfigFile(name string) ([]byte, error) {
	p := filepath.Join(appBase, "config", name)
	if b, err := os.ReadFile(p); err == nil {
		return b, nil
	}
	return configFS.ReadFile("config/" + name)
}
func loadConfig() error {
	b, err := readConfigFile("doc_types.json")
	if err != nil {
		return err
	}
	if err := json.Unmarshal(b, &docCfg); err != nil {
		return err
	}
	b, err = readConfigFile("workflow.json")
	if err != nil {
		return err
	}
	return json.Unmarshal(b, &wfCfg)
}
func staticFileSystem() http.FileSystem {
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
