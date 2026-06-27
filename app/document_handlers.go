package main

// HTTP 处理：文档相关（上传/下载预览/删除文档/删除分类）。均为 *Server 方法。

import (
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"heritage-mgmt/internal/classify"
	"heritage-mgmt/internal/domain"
)

func (s *Server) handleFile(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	doc, err := s.docs.DocByID(id)
	if err != nil {
		http.NotFound(w, r)
		return
	}
	rel, orig, ext := doc.FilePath, doc.OrigName, doc.FileExt
	full := filepath.Join(s.cfg.ProjectsDir, filepath.FromSlash(rel))
	if _, err := os.Stat(full); err != nil {
		http.NotFound(w, r)
		return
	}
	inline := map[string]bool{"pdf": true, "jpg": true, "jpeg": true, "png": true, "gif": true, "bmp": true, "webp": true}
	if !inline[strings.ToLower(ext)] {
		w.Header().Set("Content-Disposition", "attachment; filename*=UTF-8''"+urlEncode(orig))
	}
	http.ServeFile(w, r, full)
}

func (s *Server) handleUpload(w http.ResponseWriter, r *http.Request) {
	if err := r.ParseMultipartForm(200 << 20); err != nil {
		writeErr(w, err.Error())
		return
	}
	pid, _ := strconv.ParseInt(r.FormValue("project_id"), 10, 64)
	docType := strings.TrimSpace(r.FormValue("doc_type"))
	if pid == 0 || docType == "" {
		writeErr(w, "缺少 project_id 或 doc_type")
		return
	}
	proj, err := s.projects.GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	tname := "其他"
	for _, t := range s.cfg.DocCfg.Types {
		if t.Code == docType {
			tname = t.Name
			break
		}
	}
	destDir := filepath.Join(s.cfg.ProjectsDir, proj.Folder, docType)
	os.MkdirAll(destDir, 0755)
	uploaded := 0
	for _, fh := range r.MultipartForm.File["file"] {
		src, err := fh.Open()
		if err != nil {
			continue
		}
		fname := fh.Filename
		dst := filepath.Join(destDir, fname)
		if _, err := os.Stat(dst); err == nil {
			base := strings.TrimSuffix(fname, filepath.Ext(fname))
			dst = filepath.Join(destDir, base+"_"+time.Now().Format("150405")+filepath.Ext(fname))
		}
		out, err := os.Create(dst)
		if err != nil {
			src.Close()
			continue
		}
		io.Copy(out, src)
		out.Close()
		src.Close()
		rel, _ := filepath.Rel(s.cfg.ProjectsDir, dst)
		rel = filepath.ToSlash(rel)
		ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(fname), "."))
		fi, _ := os.Stat(dst)
		var size int64
		if fi != nil {
			size = fi.Size()
		}
		s.docs.InsertDocument(domain.Document{
			ProjectID: pid, DocType: docType, DocTypeName: tname,
			Title: classify.CleanTitle(fname), OrigName: fname,
			FilePath: rel, FileExt: ext, FileSize: size,
			Uploaded: time.Now().Format("2006-01-02 15:04:05"), Source: "upload",
		})
		uploaded++
	}
	if uploaded > 0 {
		s.logs.InsertLog("上传文档", fmt.Sprintf("工程#%d %s", pid, tname), fmt.Sprintf("%d个文件", uploaded))
	}
	writeJSON(w, map[string]interface{}{"ok": true, "count": uploaded})
}

func (s *Server) handleDeleteDoc(w http.ResponseWriter, r *http.Request) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	doc, err := s.docs.DocByID(id)
	if err != nil {
		writeErr(w, "文档不存在")
		return
	}
	os.Remove(filepath.Join(s.cfg.ProjectsDir, filepath.FromSlash(doc.FilePath)))
	s.docs.DeleteDocument(id)
	s.logs.InsertLog("删除文档", fmt.Sprintf("工程#%d %s", doc.ProjectID, doc.DocTypeName), doc.OrigName)
	writeJSON(w, map[string]interface{}{"ok": true})
}

// ---- 删除某工程下某分类的全部文件 ----
func (s *Server) handleDeleteDocType(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	docType := r.PathValue("docType")
	if docType == "" {
		writeErr(w, "缺少分类")
		return
	}
	proj, err := s.projects.GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	// 删DB记录
	deleted, _ := s.docs.DeleteDocsByType(pid, docType)
	// 删磁盘文件夹
	folder := filepath.Join(s.cfg.ProjectsDir, proj.Folder, docType)
	os.RemoveAll(folder)
	pname := proj.Name
	s.logs.InsertLog("删除分类", fmt.Sprintf("工程#%d %s / %s", pid, pname, docType), fmt.Sprintf("删除%d个文件", deleted))
	writeJSON(w, map[string]interface{}{"ok": true, "deleted": deleted})
}
