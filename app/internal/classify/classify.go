// Package classify holds pure, IO-free functions that recognize and parse
// project folder names and classify document filenames by keyword rules.
//
// All recognition functions take their config (rules/types) as parameters
// rather than reading globals, so they can be unit-tested with no database,
// no filesystem, and no global state.
package classify

import (
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"heritage-mgmt/internal/domain"
)

var (
	seqRe   = regexp.MustCompile(`^\s*(\d+)\s*[、.．]\s*`)
	cleanRe = regexp.MustCompile(`^\s*\d+\s*[、.．]\s*`)
	normRe  = regexp.MustCompile(`[^一-龥A-Za-z0-9]`)
)

// ParseSeq 从文件夹名提取前缀序号（如 "3、xxx" → 3）
func ParseSeq(folder string) *int64 {
	m := seqRe.FindStringSubmatch(folder)
	if m == nil {
		return nil
	}
	n, _ := strconv.ParseInt(m[1], 10, 64)
	return &n
}

// CleanProjectName 去掉工程名的序号前缀
func CleanProjectName(folder string) string {
	return strings.TrimSpace(cleanRe.ReplaceAllString(folder, ""))
}

// CleanTitle 去掉文件名的序号前缀（保留扩展名）
func CleanTitle(filename string) string {
	base := strings.TrimSuffix(filename, filepath.Ext(filename))
	return strings.TrimSpace(cleanRe.ReplaceAllString(base, "")) + filepath.Ext(filename)
}

// DetectUnit 按关键词规则识别文物单位（名称/级别/类别），未命中返回默认值
func DetectUnit(name string, rules []domain.UnitRule) (unit, level, category string) {
	for _, r := range rules {
		for _, kw := range r.Keywords {
			if strings.Contains(name, kw) {
				return r.Unit, r.Level, r.Category
			}
		}
	}
	return "其他文物", "", ""
}

// DetectType 按关键词规则识别工程类型，未命中返回"其他工程"
func DetectType(name string, rules []domain.TypeRule) string {
	for _, r := range rules {
		for _, kw := range r.Keywords {
			if strings.Contains(name, kw) {
				return r.Type
			}
		}
	}
	return "其他工程"
}

// ClassifyDoc 按文件名关键词把文档归入某分类，未命中返回 unknown
func ClassifyDoc(filename string, types []domain.DocType, unknownCode, unknownName string) (code, name string) {
	for _, t := range types {
		for _, kw := range t.Keywords {
			if strings.Contains(filename, kw) {
				return t.Code, t.Name
			}
		}
	}
	return unknownCode, unknownName
}

// NormName 归一化名称（仅保留中文/字母/数字），用于相似度匹配
func NormName(s string) string {
	return normRe.ReplaceAllString(s, "")
}
