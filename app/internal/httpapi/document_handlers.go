package httpapi

// HTTP 处理：文档相关（上传/下载预览/删除文档/删除分类）。均为 *Server 方法。
// 涉及文件名/分类编码的输入一律经 safeJoin/cleanUploadName/validDocType 校验，防目录穿越。

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
	// doc.FilePath 来自 DB（上传/导入时写入，或备份恢复注入），校验防穿越。
	full, err := safeJoin(s.cfg.ProjectsDir, filepath.FromSlash(rel))
	if err != nil {
		writeErr(w, "非法文件路径")
		return
	}
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
	// docType 既拼进归档路径又入库，必须白名单校验，防 "../" 穿越与脏数据。
	if pid == 0 || !s.validDocType(docType) {
		writeErr(w, "缺少 project_id 或文档类型非法")
		return
	}
	proj, err := s.projects.GetProject(pid)
	if err != nil || proj == nil {
		writeErr(w, "工程不存在")
		return
	}
	tname := s.cfg.DocCfg.UnknownName
	for _, t := range s.cfg.DocCfg.Types {
		if t.Code == docType {
			tname = t.Name
			break
		}
	}
	destDir, err := safeJoin(s.cfg.ProjectsDir, proj.Folder, docType)
	if err != nil {
		writeErr(w, "非法归档路径")
		return
	}
	os.MkdirAll(destDir, 0755)
	uploaded := 0
	for _, fh := range r.MultipartForm.File["file"] {
		src, err := fh.Open()
		if err != nil {
			continue
		}
		// fh.Filename 完全用户可控，必须清洗后再拼路径。
		fname, err := cleanUploadName(fh.Filename)
		if err != nil {
			src.Close()
			continue
		}
		dst, err := safeJoin(destDir, fname)
		if err != nil {
			src.Close()
			continue
		}
		if _, err := os.Stat(dst); err == nil {
			base := strings.TrimSuffix(fname, filepath.Ext(fname))
			dst, err = safeJoin(destDir, base+"_"+time.Now().Format("150405")+filepath.Ext(fname))
			if err != nil {
				src.Close()
				continue
			}
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
	full, err := safeJoin(s.cfg.ProjectsDir, filepath.FromSlash(doc.FilePath))
	if err != nil {
		writeErr(w, "非法文件路径")
		return
	}
	if err := os.Remove(full); err != nil && !os.IsNotExist(err) {
		writeErr(w, "删除磁盘文件失败")
		return
	}
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
	// 先校验路径合法性，避免出现"DB 已删但磁盘删不掉"的不一致。
	folder, err := safeJoin(s.cfg.ProjectsDir, proj.Folder, docType)
	if err != nil {
		writeErr(w, "非法分类路径")
		return
	}
	// 删DB记录（doc_type 为参数化绑定，无注入风险）
	deleted, _ := s.docs.DeleteDocsByType(pid, docType)
	// 删磁盘文件夹
	if err := os.RemoveAll(folder); err != nil {
		writeErr(w, "删除分类目录失败")
		return
	}
	pname := proj.Name
	s.logs.InsertLog("删除分类", fmt.Sprintf("工程#%d %s / %s", pid, pname, docType), fmt.Sprintf("删除%d个文件", deleted))
	writeJSON(w, map[string]interface{}{"ok": true, "deleted": deleted})
}

// validDocType 判断 doc_type 是否为合法分类编码（白名单 = 已知类型 + 未识别占位）。
func (s *Server) validDocType(code string) bool {
	if code == "" {
		return false
	}
	if code == s.cfg.DocCfg.UnknownCode {
		return true
	}
	for _, t := range s.cfg.DocCfg.Types {
		if t.Code == code {
			return true
		}
	}
	return false
}
