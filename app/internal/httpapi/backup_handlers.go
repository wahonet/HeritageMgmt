package httpapi

// 数据备份/恢复模块
// 备份: SQLite数据库 + 项目文件 + 语言包 → zip
// 恢复: zip → 暂存目录解压(防 Zip Slip) → 原子替换 data/（失败自动回滚）

import (
	"archive/zip"
	"fmt"
	"io"
	"net/http"
	"os"
	"path"
	"path/filepath"
	"strings"
	"time"
)

// ---- 备份 ----
func (s *Server) handleBackup(w http.ResponseWriter, r *http.Request) {
	// 先checkpoint把WAL写入主库
	s.store.Checkpoint()

	stamp := time.Now().Format("20060102_150405")
	filename := fmt.Sprintf("heritage_backup_%s.zip", stamp)
	w.Header().Set("Content-Type", "application/zip")
	w.Header().Set("Content-Disposition", "attachment; filename="+filename)

	zw := zip.NewWriter(w)
	defer zw.Close()

	addToZip := func(filePath, zipPath string) error {
		fi, err := os.Stat(filePath)
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
		f, err := os.Open(filePath)
		if err != nil {
			return err
		}
		defer f.Close()
		_, err = io.Copy(zf, f)
		return err
	}

	// walkDir 递归添加目录（rel 由服务器从 DataDir 计算，非用户输入，无穿越风险）
	var walkErr error
	filepath.Walk(s.cfg.DataDir, func(p string, info os.FileInfo, err error) error {
		if err != nil || info.IsDir() {
			return nil
		}
		rel, _ := filepath.Rel(s.cfg.DataDir, p)
		// 跳过WAL/SHM临时文件(已checkpoint)
		ext := strings.ToLower(filepath.Ext(p))
		if ext == ".wal" || ext == ".shm" {
			return nil
		}
		if e := addToZip(p, filepath.Join("data", rel)); e != nil {
			walkErr = e
		}
		return nil
	})
	_ = walkErr

	s.logs.InsertLog("数据备份", "全量备份", filename)
}

// ---- 恢复（原子化 + Zip Slip 防护）----
func (s *Server) handleRestore(w http.ResponseWriter, r *http.Request) {
	r.Body = http.MaxBytesReader(w, r.Body, 2<<30) // 限 2GB
	f, _, err := r.FormFile("file")
	if err != nil {
		writeErr(w, "未收到文件")
		return
	}
	defer f.Close()

	// 用自命名临时文件接收（不用客户端文件名，防穿越/覆盖同名文件）
	tmp, err := os.CreateTemp("", "heritage_restore_*.zip")
	if err != nil {
		writeErr(w, "创建临时文件失败")
		return
	}
	tmpPath := tmp.Name()
	if _, err := io.Copy(tmp, f); err != nil {
		tmp.Close()
		os.Remove(tmpPath)
		writeErr(w, "保存上传文件失败")
		return
	}
	if err := tmp.Close(); err != nil {
		os.Remove(tmpPath)
		writeErr(w, "保存上传文件失败")
		return
	}
	defer os.Remove(tmpPath)

	zr, err := zip.OpenReader(tmpPath)
	if err != nil {
		writeErr(w, "无法读取zip文件: "+err.Error())
		return
	}
	defer zr.Close()

	// 解压到暂存目录（不直接覆盖现网 data/），全部成功后再原子替换
	stage, err := os.MkdirTemp(s.cfg.AppBase, ".restore_*")
	if err != nil {
		writeErr(w, "创建恢复暂存目录失败")
		return
	}
	defer os.RemoveAll(stage)
	stageData := filepath.Join(stage, "data")
	if err := os.MkdirAll(stageData, 0755); err != nil {
		writeErr(w, "创建恢复数据目录失败")
		return
	}

	count, hasDB, err := extractBackupData(zr, stageData)
	if err != nil {
		writeErr(w, "解压备份失败: "+err.Error())
		return
	}
	if !hasDB {
		writeErr(w, "zip文件中未找到数据库(data/heritage.db)，不是有效的备份文件")
		return
	}

	// 关闭当前库（Windows 文件锁），原子替换 data/，任一步失败自动回滚
	if err := s.store.Close(); err != nil {
		writeErr(w, "关闭数据库失败: "+err.Error())
		return
	}
	backupDir := s.cfg.DataDir + "_pre_restore_" + time.Now().Format("20060102_150405")
	if err := os.Rename(s.cfg.DataDir, backupDir); err != nil {
		_ = s.store.Reopen(s.cfg)
		writeErr(w, "备份当前data目录失败: "+err.Error())
		return
	}
	if err := os.Rename(stageData, s.cfg.DataDir); err != nil {
		_ = os.Rename(backupDir, s.cfg.DataDir)
		_ = s.store.Reopen(s.cfg)
		writeErr(w, "替换data目录失败: "+err.Error())
		return
	}
	if err := s.store.Reopen(s.cfg); err != nil {
		_ = os.RemoveAll(s.cfg.DataDir)
		_ = os.Rename(backupDir, s.cfg.DataDir)
		_ = s.store.Reopen(s.cfg)
		writeErr(w, "恢复后打开数据库失败，已回滚: "+err.Error())
		return
	}
	_ = os.RemoveAll(backupDir) // 成功：清理旧 data 备份

	s.logs.InsertLog("数据恢复", "从备份恢复", fmt.Sprintf("解压%d个文件", count))
	writeJSON(w, map[string]interface{}{"ok": true, "files": count})
}

// safeZipRel 校验 zip 条目名：必须位于 data/ 下且无 "../" 穿越或绝对路径，返回 data/ 内的相对路径。
// 借 path.Clean("/"+rel) 绝对化后消除 "../"，确保条目无法越出 data/。
func safeZipRel(name string) (string, bool) {
	name = filepath.ToSlash(name)
	if !strings.HasPrefix(name, "data/") {
		return "", false
	}
	rel := strings.TrimSpace(strings.TrimPrefix(name, "data/"))
	if rel == "" {
		return "", false
	}
	clean := strings.TrimPrefix(path.Clean("/"+rel), "/")
	if clean == "." || clean == "" || strings.HasPrefix(clean, "../") || strings.Contains(clean, "/../") {
		return "", false
	}
	return clean, true
}

// extractBackupData 把 zip 内容解压到 dstDataDir，逐条经 safeZipRel/safeJoin 校验，错误立即中断。
// 返回写入文件数、是否含 heritage.db、错误。
func extractBackupData(zr *zip.ReadCloser, dstDataDir string) (count int, hasDB bool, err error) {
	for _, zf := range zr.File {
		rel, ok := safeZipRel(zf.Name)
		if !ok {
			continue // 跳过 data/ 之外的可疑条目
		}
		if rel == "heritage.db" {
			hasDB = true
		}
		dest, e := safeJoin(dstDataDir, filepath.FromSlash(rel))
		if e != nil {
			return count, hasDB, fmt.Errorf("非法路径: %s", zf.Name)
		}
		if zf.FileInfo().IsDir() {
			if e := os.MkdirAll(dest, 0755); e != nil {
				return count, hasDB, e
			}
			continue
		}
		if e := os.MkdirAll(filepath.Dir(dest), 0755); e != nil {
			return count, hasDB, e
		}
		rc, e := zf.Open()
		if e != nil {
			return count, hasDB, e
		}
		out, e := os.OpenFile(dest, os.O_CREATE|os.O_WRONLY|os.O_TRUNC, zf.Mode())
		if e != nil {
			rc.Close()
			return count, hasDB, e
		}
		_, copyErr := io.Copy(out, rc)
		closeErr := out.Close()
		rc.Close()
		if copyErr != nil {
			return count, hasDB, copyErr
		}
		if closeErr != nil {
			return count, hasDB, closeErr
		}
		count++
	}
	return count, hasDB, nil
}
