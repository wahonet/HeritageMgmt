package classify

import "strings"

// Similarity 简易相似度（基于最长公共子序列 LCS）
func Similarity(a, b string) float64 {
	if a == b {
		return 1
	}
	if a == "" || b == "" {
		return 0
	}
	la, lb := len(a), len(b)
	dp := make([][]int, la+1)
	for i := range dp {
		dp[i] = make([]int, lb+1)
	}
	for i := 1; i <= la; i++ {
		for j := 1; j <= lb; j++ {
			if a[i-1] == b[j-1] {
				dp[i][j] = dp[i-1][j-1] + 1
			} else if dp[i-1][j] > dp[i][j-1] {
				dp[i][j] = dp[i-1][j]
			} else {
				dp[i][j] = dp[i][j-1]
			}
		}
	}
	lcs := dp[la][lb]
	return 2.0 * float64(lcs) / float64(la+lb)
}

// MatchFinancial 在财务行中为某工程名找最相似的一条（阈值 0.6）。
// finRows 中每条记录的 "_name" 字段为该行的项目名称。
func MatchFinancial(name string, finRows []map[string]string) (map[string]string, float64) {
	target := NormName(name)
	var best map[string]string
	bestScore := 0.0
	for _, rec := range finRows {
		cand := NormName(rec["_name"])
		if cand == "" {
			continue
		}
		var score float64
		if strings.Contains(target, cand) || strings.Contains(cand, target) {
			score = 0.95
		} else {
			score = Similarity(target, cand)
		}
		if score > bestScore {
			best, bestScore = rec, score
		}
	}
	if best != nil && bestScore >= 0.6 {
		return best, bestScore
	}
	return nil, bestScore
}
