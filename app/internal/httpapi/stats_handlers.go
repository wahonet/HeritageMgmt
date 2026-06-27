package httpapi

// HTTP 处理：统计聚合。

import "net/http"

// handleStats GET /api/stats
func (s *Server) handleStats(w http.ResponseWriter, r *http.Request) {
	writeJSON(w, s.stats.Aggregate())
}
