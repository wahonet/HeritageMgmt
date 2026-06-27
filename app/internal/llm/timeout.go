package llm

import "time"

// Timeout 返回调用超时：优先 cfg.TimeoutSeconds，否则用给定默认值。
// 供 OCR 提取与报告分析两处共享，避免重复定义。
func Timeout(cfg Config, def time.Duration) time.Duration {
	if cfg.TimeoutSeconds > 0 {
		return time.Duration(cfg.TimeoutSeconds) * time.Second
	}
	return def
}
