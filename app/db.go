package main

// 数据层：连接与生命周期。
// 纯Go驱动 modernc.org/sqlite，无CGO，便于交叉编译到国产系统。

import (
	"database/sql"
	"os"
	"path/filepath"

	_ "modernc.org/sqlite"
)

var (
	appBase     string
	dataDir     string
	projectsDir string
	dbPath      string
	db          *sql.DB
)

// initPaths 解析数据/配置目录，优先用可执行文件同级的目录
func initPaths() {
	base := "."
	if exe, err := os.Executable(); err == nil {
		base = filepath.Dir(exe)
		// go run 时 exe 在临时目录，回退到工作目录
		if _, e := os.Stat(filepath.Join(base, "config")); e != nil {
			if cwd, e2 := os.Getwd(); e2 == nil {
				if _, e3 := os.Stat(filepath.Join(cwd, "config")); e3 == nil {
					base = cwd
				}
			}
		}
	}
	appBase = base
	dataDir = filepath.Join(base, "data")
	projectsDir = filepath.Join(dataDir, "projects")
	dbPath = filepath.Join(dataDir, "heritage.db")
	os.MkdirAll(dataDir, 0755)
	os.MkdirAll(projectsDir, 0755)
}

// OpenDB 打开并初始化数据库，并对旧库做字段迁移
func OpenDB() error {
	d, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return err
	}
	d.Exec("PRAGMA foreign_keys = ON")
	d.Exec("PRAGMA journal_mode = WAL")
	if _, err := d.Exec(schema); err != nil {
		return err
	}
	db = d
	migrate()
	return nil
}
