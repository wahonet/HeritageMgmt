package main

// DI 根：NewApp 按 config → store → repos → services → server 顺序组装全部依赖，
// 消灭包级全局单例。main 仅负责调用 NewApp 并驱动 CLI/HTTP。

import "heritage-mgmt/internal/llm"

// App 持有运行期全部组装产物。
type App struct {
	cfg    *Config
	store  *Store
	server *Server
}

// NewApp 组装应用：构造各 service（注入仓储接口 *Store + *Config + LLM 客户端）与 Server。
func NewApp(cfg *Config, store *Store) *App {
	client := llm.New(cfg.LLM)
	proj := &ProjectService{projects: store, units: store, docs: store, cfg: cfg}
	stats := &StatsService{projects: store, units: store}
	imp := &ImportService{store: store, cfg: cfg}
	recycle := &RecycleService{projects: store, units: store, cfg: cfg}
	ocrSvc := &OCRService{projects: store, cfg: cfg, llm: client, llmCfg: cfg.LLM}
	srv := &Server{
		cfg:      cfg,
		store:    store,
		projects: store,
		units:    store,
		docs:     store,
		logs:     store,
		proj:     proj,
		stats:    stats,
		imp:      imp,
		recycle:  recycle,
		ocrSvc:   ocrSvc,
		llm:      client,
	}
	return &App{cfg: cfg, store: store, server: srv}
}

// Server 暴露 HTTP 入口。
func (a *App) Server() *Server { return a.server }
