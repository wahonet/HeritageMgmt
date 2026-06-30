package httpapi

// HTTP 传输层：Server 持有全部依赖（仓储接口 + service + *config.Config + *store.Store），
// 所有 handler 为其方法，仅做请求解析/校验/序列化，业务逻辑下沉到 service。
// 路由注册集中在此（由 main.go 迁入）。

import (
	"fmt"
	"net/http"
	"net/url"
	"path/filepath"
	"time"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/service"
	"heritage-mgmt/internal/store"
)

// Server 是 HTTP 层的组合根：持有配置、Store、各仓储接口、各 service 与 LLM 客户端。
type Server struct {
	cfg      *config.Config
	store    *store.Store
	projects service.ProjectRepository
	units    service.UnitRepository
	docs     service.DocumentRepository
	logs     service.LogRepository
	proj     *service.ProjectService
	stats    *service.StatsService
	imp      *service.ImportService
	recycle  *service.RecycleService
	ocrSvc   *service.OCRService
	llm      *llm.Client
}

// NewServer 构造 HTTP Server：注入配置、Store（同时满足 4 个仓储接口）、各 service 与 LLM 客户端。
func NewServer(cfg *config.Config, st *store.Store, svc *service.Services, llm *llm.Client) *Server {
	return &Server{
		cfg:      cfg,
		store:    st,
		projects: st,
		units:    st,
		docs:     st,
		logs:     st,
		proj:     svc.Proj,
		stats:    svc.Stats,
		imp:      svc.Imp,
		recycle:  svc.Recycle,
		ocrSvc:   svc.OCR,
		llm:      llm,
	}
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

// ListenAndServe 启动 HTTP 服务（127.0.0.1:5000），用安全中间件包裹路由。
func (s *Server) ListenAndServe(addr string) error {
	fmt.Printf("✓ 文物保护工程管理系统已启动: http://%s  (Ctrl+C 停止)\n", addr)
	srv := &http.Server{
		Addr:              addr,
		Handler:           s.securityMiddleware(s.Routes()),
		ReadHeaderTimeout: 5 * time.Second,
	}
	return srv.ListenAndServe()
}

// allowedHost 允许的本机访问 Host（离线单机系统只接受本机回环访问）。
func allowedHost(host string) bool {
	return host == "127.0.0.1:5000" || host == "localhost:5000" || host == "127.0.0.1" || host == "localhost"
}

// securityMiddleware 防 CSRF：① 校验 Host 为本机回环；② 若带 Origin 须同源；③ 非 GET 请求须带正确 X-Heritage-CSRF。
// 恶意网页跨域读不到 /api/config 的 csrf_token（CORS 拦截读响应），故无法构造合法写请求。
func (s *Server) securityMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if !allowedHost(r.Host) {
			http.Error(w, "forbidden host", http.StatusForbidden)
			return
		}
		if origin := r.Header.Get("Origin"); origin != "" {
			if u, err := url.Parse(origin); err != nil || !allowedHost(u.Host) {
				http.Error(w, "forbidden origin", http.StatusForbidden)
				return
			}
		}
		if r.Method != http.MethodGet && r.Method != http.MethodHead && r.Method != http.MethodOptions {
			if r.Header.Get("X-Heritage-CSRF") != s.cfg.CSRFToken {
				http.Error(w, "bad csrf token", http.StatusForbidden)
				return
			}
		}
		next.ServeHTTP(w, r)
	})
}
