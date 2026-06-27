package main

// HTTP 传输层：Server 持有全部依赖（仓储接口 + service + *config.Config + *store.Store），
// 所有 handler 为其方法，仅做请求解析/校验/序列化，业务逻辑下沉到 service。
// 路由注册集中在此（由 main.go 迁入）。

import (
	"fmt"
	"net/http"
	"path/filepath"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/store"
)

// Server 是 HTTP 层的组合根：持有配置、Store、各仓储接口、各 service 与 LLM 客户端。
type Server struct {
	cfg      *config.Config
	store    *store.Store
	projects ProjectRepository
	units    UnitRepository
	docs     DocumentRepository
	logs     LogRepository
	proj     *ProjectService
	stats    *StatsService
	imp      *ImportService
	recycle  *RecycleService
	ocrSvc   *OCRService
	llm      *llm.Client
}

// Routes 组装 mux 并注册全部路由（从 main.go 迁入）。
func (s *Server) Routes() *http.ServeMux {
	mux := http.NewServeMux()
	mux.HandleFunc("GET /api/config", s.handleConfig)
	mux.HandleFunc("GET /api/units", s.handleUnits)
	mux.HandleFunc("DELETE /api/unit/{id}", s.handleDeleteUnit)
	mux.HandleFunc("GET /api/projects", s.handleProjects)
	mux.HandleFunc("GET /api/project/{id}", s.handleProject)
	mux.HandleFunc("PUT /api/project/{id}", s.handleUpdateProject)
	mux.HandleFunc("DELETE /api/project/{id}", s.handleDeleteProject)
	mux.HandleFunc("DELETE /api/project/{id}/doctype/{docType}", s.handleDeleteDocType)
	mux.HandleFunc("POST /api/project/create", s.handleCreateProject)
	mux.HandleFunc("GET /api/project/{id}/tree", s.handleProjectTree)
	mux.HandleFunc("GET /api/dashboard", s.handleDashboard)
	mux.HandleFunc("GET /api/file/{id}", s.handleFile)
	mux.HandleFunc("POST /api/upload", s.handleUpload)
	mux.HandleFunc("DELETE /api/document/{id}", s.handleDeleteDoc)
	mux.HandleFunc("POST /api/import", s.handleImport)
	mux.HandleFunc("GET /api/stats", s.handleStats)
	mux.HandleFunc("GET /api/logs", s.handleLogs)
	mux.HandleFunc("GET /api/export/ledger", s.handleExportLedger)
	mux.HandleFunc("GET /api/backup", s.handleBackup)
	mux.HandleFunc("POST /api/restore", s.handleRestore)
	mux.HandleFunc("GET /api/recycle", s.handleRecycle)
	mux.HandleFunc("POST /api/project/{id}/restore", s.handleRestoreProject)
	mux.HandleFunc("DELETE /api/project/{id}/purge", s.handlePurgeProject)
	mux.HandleFunc("GET /api/project/{id}/report", s.handleReportPDF)
	mux.HandleFunc("POST /api/ocr/scan", s.handleOCRScan)
	mux.HandleFunc("GET /logo.png", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, filepath.Join(s.cfg.AppBase, "logo.png"))
	})
	mux.Handle("/", noCacheFS(http.FileServer(staticFileSystem(s.cfg.AppBase))))
	return mux
}

// ListenAndServe 启动 HTTP 服务（127.0.0.1:5000）。
func (s *Server) ListenAndServe(addr string) error {
	fmt.Printf("✓ 文物保护工程管理系统已启动: http://%s  (Ctrl+C 停止)\n", addr)
	srv := &http.Server{Addr: addr, Handler: s.Routes()}
	return srv.ListenAndServe()
}
