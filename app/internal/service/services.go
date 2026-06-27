package service

// 业务服务聚合：集中构造全部 service（在 service 包内可直接设置各 struct 的非导出字段），
// 供 httpapi/cmd 装配。各仓储参数由 *store.Store 结构化满足（在调用处校验契合性）。

import (
	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/store"
)

// Services 聚合全部业务服务实例。
type Services struct {
	Proj    *ProjectService
	Stats   *StatsService
	Imp     *ImportService
	Recycle *RecycleService
	OCR     *OCRService
}

// NewServices 构造全部业务服务，注入仓储接口（*store.Store 满足）、*config.Config 与 LLM 客户端。
func NewServices(cfg *config.Config, st *store.Store, client *llm.Client) *Services {
	return &Services{
		Proj:    &ProjectService{projects: st, units: st, docs: st, cfg: cfg},
		Stats:   &StatsService{projects: st, units: st},
		Imp:     &ImportService{store: st, cfg: cfg},
		Recycle: &RecycleService{projects: st, units: st, cfg: cfg},
		OCR:     &OCRService{projects: st, cfg: cfg, llm: client, llmCfg: cfg.LLM},
	}
}
