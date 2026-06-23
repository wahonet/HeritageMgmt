package main

// 导入器：扫描 Basicdata/ 自动分类入库 + 读取Excel财务匹配
// 逻辑与原Python版等价：识别单位/类型/序号、按关键词分类12类文档、复制归档、Excel财务回填

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/xuri/excelize/v2"
)

// ---- 配置结构体 ----

type DocType struct {
	Code     string   `json:"code"`
	Name     string   `json:"name"`
	Keywords []string `json:"keywords"`
	Stage    string   `json:"stage"`
	Required bool     `json:"required"`
	Desc     string   `json:"desc"`
}
type DocTypeCfg struct {
	Types       []DocType `json:"types"`
	UnknownCode string    `json:"unknown_code"`
	UnknownName string    `json:"unknown_name"`
}
type Stage struct {
	Code string   `json:"code"`
	Name string   `json:"name"`
	Docs []string `json:"docs"`
}
type UnitRule struct {
	Unit     string   `json:"unit"`
	Level    string   `json:"level"`
	Category string   `json:"category"`
	Keywords []string `json:"keywords"`
}
type TypeRule struct {
	Type     string   `json:"type"`
	Keywords []string `json:"keywords"`
}
type Workflow struct {
	Stages []Stage `json:"stages"`
	Units  struct {
		Rules []UnitRule `json:"rules"`
	} `json:"units"`
	ProjectTypes struct {
		Rules []TypeRule `json:"rules"`
	} `json:"project_types"`
}

var docCfg DocTypeCfg
var wfCfg Workflow

// ---- 识别 ----

var seqRe = regexp.MustCompile(`^\s*(\d+)\s*[、.．]\s*`)
var cleanRe = regexp.MustCompile(`^\s*\d+\s*[、.．]\s*`)
var normRe = regexp.MustCompile(`[^一-龥A-Za-z0-9]`)

func parseSeq(folder string) *int64 {
	m := seqRe.FindStringSubmatch(folder)
	if m == nil {
		return nil
	}
	n, _ := strconv.ParseInt(m[1], 10, 64)
	return &n
}
func cleanProjectName(folder string) string {
	return strings.TrimSpace(cleanRe.ReplaceAllString(folder, ""))
}
func cleanTitle(filename string) string {
	base := strings.TrimSuffix(filename, filepath.Ext(filename))
	return strings.TrimSpace(cleanRe.ReplaceAllString(base, "")) + filepath.Ext(filename)
}

func detectUnit(name string) (unit, level, category string) {
	for _, r := range wfCfg.Units.Rules {
		for _, kw := range r.Keywords {
			if strings.Contains(name, kw) {
				return r.Unit, r.Level, r.Category
			}
		}
	}
	return "其他文物", "", ""
}
func detectType(name string) string {
	for _, r := range wfCfg.ProjectTypes.Rules {
		for _, kw := range r.Keywords {
			if strings.Contains(name, kw) {
				return r.Type
			}
		}
	}
	return "其他工程"
}
func classifyDoc(filename string) (code, name string) {
	for _, t := range docCfg.Types {
		for _, kw := range t.Keywords {
			if strings.Contains(filename, kw) {
				return t.Code, t.Name
			}
		}
	}
	return docCfg.UnknownCode, docCfg.UnknownName
}

// ---- Excel 财务 ----

var headerNormRe = regexp.MustCompile(`\s+`)

func loadExcelFinancials(basicdataDir string) []map[string]string {
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

func normName(s string) string {
	return normRe.ReplaceAllString(s, "")
}

// similarity 简易相似度（基于最长公共子序列）
func similarity(a, b string) float64 {
	if a == b {
		return 1
	}
	if a == "" || b == "" {
		return 0
	}
	// LCS
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

func matchFinancial(name string, finRows []map[string]string) (map[string]string, float64) {
	target := normName(name)
	var best map[string]string
	bestScore := 0.0
	for _, rec := range finRows {
		cand := normName(rec["_name"])
		if cand == "" {
			continue
		}
		var score float64
		if strings.Contains(target, cand) || strings.Contains(cand, target) {
			score = 0.95
		} else {
			score = similarity(target, cand)
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

// FIELD_MAP: DB字段 -> Excel表头候选
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

func finGet(fin map[string]string, field string) string {
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

func parseFloat(s string) *float64 {
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

func trimDate(s string) string {
	s = strings.TrimSpace(s)
	if s == "" {
		return ""
	}
	// 形如 "2021-12-10 00:00:00" 取前10位
	if len(s) >= 10 && (strings.Contains(s, "-") || strings.Contains(s, "/")) {
		return s[:10]
	}
	return s
}

func deriveStatus(fin map[string]string) string {
	if fin == nil {
		return ""
	}
	txt := finGet(fin, "progress_note")
	if strings.Contains(txt, "验收") || strings.Contains(txt, "竣工") || strings.Contains(txt, "完工") {
		return "已竣工"
	}
	if strings.Contains(txt, "在建") || strings.Contains(txt, "施工") {
		return "在建"
	}
	if finGet(fin, "sign_date") != "" {
		return "在建"
	}
	return "前期"
}

// ---- 主导入流程 ----

type ImportStats struct {
	Units, Projects, Docs, Matched int
}

func ImportAll(basicdataDir string, verbose bool) (*ImportStats, error) {
	stats := &ImportStats{}
	finRows := loadExcelFinancials(basicdataDir)
	if verbose {
		fmt.Printf("读取明细表, 财务行 %d 条\n", len(finRows))
	}

	entries, err := os.ReadDir(basicdataDir)
	if err != nil {
		return stats, err
	}
	var projDirs []string
	for _, e := range entries {
		if e.IsDir() {
			projDirs = append(projDirs, e.Name())
		}
	}
	sortStrings(projDirs)

	unitCache := map[string]int64{}
	unitSort := map[string]int{"武氏墓群石刻": 1, "曾庙": 2, "青山寺": 3}

	tx, err := db.Begin()
	if err != nil {
		return stats, err
	}
	defer tx.Rollback()
	if err := resetTables(tx); err != nil {
		return stats, err
	}

	for _, folder := range projDirs {
		srcDir := filepath.Join(basicdataDir, folder)
		pname := cleanProjectName(folder)
		unitName, level, category := detectUnit(pname)
		ptype := detectType(pname)
		seq := parseSeq(folder)

		// 单位
		unitID, ok := unitCache[unitName]
		if !ok {
			sort := 99
			if s, ok := unitSort[unitName]; ok {
				sort = s
			}
			res, err := tx.Exec("INSERT INTO units(name,level,category,sort) VALUES(?,?,?,?)",
				unitName, level, category, sort)
			if err != nil {
				return stats, err
			}
			unitID, _ = res.LastInsertId()
			unitCache[unitName] = unitID
			stats.Units++
		}

		fin, score := matchFinancial(pname, finRows)
		matched := fin != nil

		// 插工程
		res, err := tx.Exec(`INSERT INTO projects(
			unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,contract_end,
			central_funding,eng_contract,eng_paid,sup_contract,sup_paid,
			des_contract,des_paid,expert_fee,total_paid,status,progress_note,
			source_dir,created) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`,
			unitID, seq, pname, ptype,
			finGet(fin, "approval_no"),
			trimDate(finGet(fin, "sign_date")), trimDate(finGet(fin, "complete_date")),
			trimDate(finGet(fin, "complete_date")), trimDate(finGet(fin, "contract_end")),
			parseFloat(finGet(fin, "central_funding")),
			parseFloat(finGet(fin, "eng_contract")), parseFloat(finGet(fin, "eng_paid")),
			parseFloat(finGet(fin, "sup_contract")), parseFloat(finGet(fin, "sup_paid")),
			parseFloat(finGet(fin, "des_contract")), parseFloat(finGet(fin, "des_paid")),
			parseFloat(finGet(fin, "expert_fee")), parseFloat(finGet(fin, "total_paid")),
			deriveStatus(fin), finGet(fin, "progress_note"),
			folder, time.Now().Format("2006-01-02 15:04:05"))
		if err != nil {
			return stats, err
		}
		pid, _ := res.LastInsertId()
		if matched {
			stats.Matched++
		}
		projFolder := fmt.Sprintf("P%04d", pid)
		tx.Exec("UPDATE projects SET folder=? WHERE id=?", projFolder, pid)
		destRoot := filepath.Join(projectsDir, projFolder)
		os.MkdirAll(destRoot, 0755)

		// 文件
		files, _ := os.ReadDir(srcDir)
		for _, fe := range files {
			if fe.IsDir() {
				continue
			}
			fname := fe.Name()
			code, tname := classifyDoc(fname)
			destDir := filepath.Join(destRoot, code)
			os.MkdirAll(destDir, 0755)
			dst := filepath.Join(destDir, fname)
			if err := copyFile(filepath.Join(srcDir, fname), dst); err != nil {
				if verbose {
					fmt.Printf("  [警告] 复制失败 %s: %v\n", fname, err)
				}
				continue
			}
			rel, _ := filepath.Rel(projectsDir, dst)
			rel = filepath.ToSlash(rel)
			ext := strings.ToLower(strings.TrimPrefix(filepath.Ext(fname), "."))
			fi, _ := os.Stat(dst)
			var size int64
			if fi != nil {
				size = fi.Size()
			}
			_, err := tx.Exec(`INSERT INTO documents(
				project_id,doc_type,doc_type_name,title,orig_name,
				file_path,file_ext,file_size,uploaded,source) VALUES(?,?,?,?,?,?,?,?,?,?)`,
				pid, code, tname, cleanTitle(fname), fname, rel, ext, size,
				time.Now().Format("2006-01-02 15:04:05"), "import")
			if err != nil {
				return stats, err
			}
			stats.Docs++
		}
		stats.Projects++
		if verbose {
			tag := ""
			if matched {
				amt := finGet(fin, "central_funding")
				tag = fmt.Sprintf(" [财务 score=%.2f 指标=%s万]", score, amt)
			} else {
				tag = " [未匹配财务]"
			}
			fmt.Printf("  OK %s | %s | %s%s\n", unitName, pname, ptype, tag)
		}
	}
	if err := tx.Commit(); err != nil {
		return stats, err
	}
	if verbose {
		fmt.Printf("\n导入完成: 单位 %d / 工程 %d / 文档 %d / 财务匹配 %d\n",
			stats.Units, stats.Projects, stats.Docs, stats.Matched)
	}
	return stats, nil
}

func copyFile(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.Create(dst)
	if err != nil {
		return err
	}
	defer out.Close()
	_, err = io.Copy(out, in)
	return err
}

func sortStrings(s []string) {
	for i := 1; i < len(s); i++ {
		for j := i; j > 0 && s[j-1] > s[j]; j-- {
			s[j-1], s[j] = s[j], s[j-1]
		}
	}
}
