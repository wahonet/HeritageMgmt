package main

// 数据备份/恢复模块
// 备份: SQLite数据库 + 项目文件 + 语言包 → zip
// 恢复: zip → 还原(覆盖)

import (
	"archive/zip"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"time"
)

// ---- 备份 ----
func handleBackup(w http.ResponseWriter, r *http.Request) {
	// 先checkpoint把WAL写入主库
	db.Exec("PRAGMA wal_checkpoint(TRUNCATE)")

	stamp := time.Now().Format("20060102_150405")
	filename := fmt.Sprintf("heritage_backup_%s.zip", stamp)
	w.Header().Set("Content-Type", "application/zip")
	w.Header().Set("Content-Disposition", "attachment; filename="+filename)

	zw := zip.NewWriter(w)
	defer zw.Close()

	addToZip := func(path, zipPath string) error {
		fi, err := os.Stat(path)
		if err != nil {
			return nil // 文件不存在跳过
		}
		if fi.IsDir() {
			return nil
		}
		fh, err := zip.FileInfoHeader(fi)
		if err != nil {
			return err
		}
		fh.Name = filepath.ToSlash(zipPath)
		fh.Method = zip.Deflate
		zf, err := zw.CreateHeader(fh)
		if err != nil {
			return err
		}
		f, err := os.Open(path)
		if err != nil {
			return err
		}
		defer f.Close()
		_, err = io.Copy(zf, f)
		return err
	}

	// walkDir 递归添加目录
	var walkErr error
	filepath.Walk(dataDir, func(path string, info os.FileInfo, err error) error {
		if err != nil || info.IsDir() {
			return nil
		}
		rel, _ := filepath.Rel(dataDir, path)
		// 跳过WAL/SHM临时文件(已checkpoint)
		ext := strings.ToLower(filepath.Ext(path))
		if ext == ".wal" || ext == ".shm" {
			return nil
		}
		if e := addToZip(path, filepath.Join("data", rel)); e != nil {
			walkErr = e
		}
		return nil
	})
	_ = walkErr

	InsertLog("数据备份", "全量备份", filename)
}

// ---- 恢复 ----
func handleRestore(w http.ResponseWriter, r *http.Request) {
	// 接收上传的zip文件
	f, header, err := r.FormFile("file")
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "未收到文件"})
		return
	}
	defer f.Close()

	// 保存到临时文件
	tmpPath := filepath.Join(os.TempDir(), header.Filename)
	tmpOut, err := os.Create(tmpPath)
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": err.Error()})
		return
	}
	io.Copy(tmpOut, f)
	tmpOut.Close()
	defer os.Remove(tmpPath)

	// 打开zip
	zr, err := zip.OpenReader(tmpPath)
	if err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "无法读取zip文件: " + err.Error()})
		return
	}
	defer zr.Close()

	// 检查zip内是否有 data/heritage.db
	hasDB := false
	for _, f := range zr.File {
		if strings.HasSuffix(f.Name, "heritage.db") {
			hasDB = true
			break
		}
	}
	if !hasDB {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "zip文件中未找到数据库(heritage.db)，不是有效的备份文件"})
		return
	}

	// 关闭数据库连接
	db.Close()

	// 备份当前data目录(以防恢复失败)
	backupDir := dataDir + "_pre_restore_" + time.Now().Format("150405")
	os.Rename(dataDir, backupDir)
	os.MkdirAll(dataDir, 0755)
	os.MkdirAll(projectsDir, 0755)

	// 解压
	count := 0
	for _, f := range zr.File {
		// 只解压 data/ 下的文件
		zipName := filepath.ToSlash(f.Name)
		zipName = strings.TrimPrefix(zipName, "data/")
		if zipName == f.Name {
			continue // 不在data/下
		}
		dest := filepath.Join(dataDir, zipName)
		os.MkdirAll(filepath.Dir(dest), 0755)
		if f.FileInfo().IsDir() {
			os.MkdirAll(dest, 0755)
			continue
		}
		rc, err := f.Open()
		if err != nil {
			continue
		}
		out, err := os.Create(dest)
		if err != nil {
			rc.Close()
			continue
		}
		io.Copy(out, rc)
		out.Close()
		rc.Close()
		count++
	}

	// 重新打开数据库
	if err := OpenDB(); err != nil {
		writeJSON(w, map[string]interface{}{"ok": false, "error": "恢复后打开数据库失败: " + err.Error()})
		return
	}

	InsertLog("数据恢复", "从备份恢复", fmt.Sprintf("解压%d个文件", count))
	writeJSON(w, map[string]interface{}{"ok": true, "files": count})
}
