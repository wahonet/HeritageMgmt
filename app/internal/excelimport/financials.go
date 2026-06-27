// Package excelimport reads the financial ledger spreadsheet (.xlsx) found in
// the Basicdata directory and maps its columns onto project fields.
package excelimport

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"

	"github.com/xuri/excelize/v2"
)

var headerNormRe = regexp.MustCompile(`\s+`)

// LoadFinancials 读取 basicdataDir 下首个 .xlsx，返回以表头为键的行记录。
// 每条记录附带 "_name" 字段为项目名称（第二列）。
func LoadFinancials(basicdataDir string) []map[string]string {
	var xlsx string
	entries, _ := os.ReadDir(basicdataDir)
	for _, e := range entries {
		if !e.IsDir() && strings.HasSuffix(strings.ToLower(e.Name()), ".xlsx") {
			xlsx = filepath.Join(basicdataDir, e.Name())
			break
		}
	}
	if xlsx == "" {
		return nil
	}
	f, err := excelize.OpenFile(xlsx)
	if err != nil {
		fmt.Println("  [警告] 打开Excel失败:", err)
		return nil
	}
	defer f.Close()
	sheet := f.GetSheetName(0)
	rows, err := f.GetRows(sheet)
	if err != nil || len(rows) == 0 {
		return nil
	}
	// 找表头行(含"项目名称")
	headerIdx := -1
	for i, r := range rows {
		for _, c := range r {
			if strings.Contains(c, "项目名称") {
				headerIdx = i
				break
			}
		}
		if headerIdx >= 0 {
			break
		}
	}
	if headerIdx < 0 {
		return nil
	}
	header := []string{}
	for _, c := range rows[headerIdx] {
		header = append(header, headerNormRe.ReplaceAllString(strings.TrimSpace(c), ""))
	}
	var out []map[string]string
	for _, r := range rows[headerIdx+1:] {
		if len(r) == 0 || r[0] == "" {
			continue
		}
		first := strings.TrimSpace(r[0])
		if _, err := strconv.ParseFloat(first, 64); err != nil {
			continue // 非数字序号行(小计/合计等)
		}
		rec := map[string]string{}
		for i, v := range r {
			if i < len(header) && header[i] != "" {
				rec[header[i]] = strings.TrimSpace(v)
			}
		}
		if len(r) > 1 {
			rec["_name"] = strings.TrimSpace(r[1])
		}
		out = append(out, rec)
	}
	return out
}

// fieldMap: DB字段 -> Excel表头候选
var fieldMap = map[string][]string{
	"approval_no":     {"批复文件"},
	"sign_date":       {"开工日期"},
	"complete_date":   {"完工日期"},
	"contract_end":    {"合同约定完工日期"},
	"central_funding": {"财政指标文下达金额", "财政指标", "下达金额"},
	"eng_contract":    {"工程合同金额", "工程合同额"},
	"eng_paid":        {"工程支付金额", "工程支出累计"},
	"sup_contract":    {"监理合同金额", "监理合同额"},
	"sup_paid":        {"监理支付金额", "监理支出累计"},
	"des_contract":    {"设计合同金额", "设计合同额"},
	"des_paid":        {"设计支付金额", "设计支出累计"},
	"expert_fee":      {"专家评审费"},
	"total_paid":      {"已支付金额", "总支出累计"},
	"progress_note":   {"项目建设情况", "项目进展缓慢原因"},
}

// FinGet 按 DB 字段名从财务记录取值（依次尝试候选表头）
func FinGet(fin map[string]string, field string) string {
	if fin == nil {
		return ""
	}
	for _, k := range fieldMap[field] {
		if v, ok := fin[k]; ok && v != "" {
			return v
		}
	}
	return ""
}

// ParseFloat 解析金额，空或非法返回 nil
func ParseFloat(s string) *float64 {
	s = strings.TrimSpace(s)
	if s == "" {
		return nil
	}
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		return nil
	}
	return &v
}

// TrimDate 规范化日期，形如 "2021-12-10 00:00:00" 取前10位
func TrimDate(s string) string {
	s = strings.TrimSpace(s)
	if s == "" {
		return ""
	}
	if len(s) >= 10 && (strings.Contains(s, "-") || strings.Contains(s, "/")) {
		return s[:10]
	}
	return s
}

// DeriveStatus 依据财务记录的进展说明/开工日期推导工程状态。
// statusKeywords 为各状态的关键词（来自 config/rules.json，由调用方注入，保持本包无配置依赖）。
// 按固定优先级匹配：已竣工 > 在建；均不匹配且有开工日期视为在建，否则前期。
func DeriveStatus(fin map[string]string, statusKeywords map[string][]string) string {
	if fin == nil {
		return ""
	}
	txt := FinGet(fin, "progress_note")
	for _, status := range []string{"已竣工", "在建"} {
		for _, kw := range statusKeywords[status] {
			if strings.Contains(txt, kw) {
				return status
			}
		}
	}
	if FinGet(fin, "sign_date") != "" {
		return "在建"
	}
	return "前期"
}
