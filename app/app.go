package main

// DI 根（C2 过渡：仍在 package main；C3 迁至 cmd/heritage）：NewApp 按
// config → store → services → server 顺序组装全部依赖。
// main 仅负责调用 NewApp 并驱动 CLI/HTTP。

import (
	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/httpapi"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/service"
	"heritage-mgmt/internal/store"
)

// App 持有运行期全部组装产物。
type App struct {
	cfg      *config.Config
	store    *store.Store
	services *service.Services
	server   *httpapi.Server
}

// NewApp 组装应用：构造 LLM 客户端 → services → httpapi.Server。
func NewApp(cfg *config.Config, st *store.Store) *App {
	client := llm.New(cfg.LLM)
	svc := service.NewServices(cfg, st, client)
	srv := httpapi.NewServer(cfg, st, svc, client)
	return &App{cfg: cfg, store: st, services: svc, server: srv}
}

// Server 暴露 HTTP 入口。
func (a *App) Server() *httpapi.Server { return a.server }

// Services 暴露业务服务集合（供 CLI 批量 OCR / 导入等命令直接调用）。
func (a *App) Services() *service.Services { return a.services }

// Store 暴露数据层（CLI 需 CountProjects / InsertLog）。
func (a *App) Store() *store.Store { return a.store }
