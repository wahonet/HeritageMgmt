package classify

import "strings"

// Similarity 基于 rune（字符）的最长公共子序列相似度。
// 必须按 rune 而非 byte 比较，否则中文 UTF-8 多字节会被拆碎，相似度失真。
func Similarity(a, b string) float64 {
	if a == b {
		return 1
	}
	if a == "" || b == "" {
		return 0
	}
	ra, rb := []rune(a), []rune(b)
	la, lb := len(ra), len(rb)
	dp := make([]int, lb+1) // 滚动数组省内存
	for i := 1; i <= la; i++ {
		prev := 0 // dp[i-1][j-1]
		for j := 1; j <= lb; j++ {
			tmp := dp[j]
			if ra[i-1] == rb[j-1] {
				dp[j] = prev + 1
			} else if dp[j-1] > dp[j] {
				dp[j] = dp[j-1]
			}
			prev = tmp
		}
	}
	lcs := dp[lb]
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
