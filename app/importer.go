package main

// 导入流程编排：扫描 Basicdata/ → 识别(classify) + Excel财务(excelimport) → 复制归档 → 入库。
// 识别与财务解析的纯逻辑已分别迁至 internal/classify 与 internal/excelimport。

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"heritage-mgmt/internal/classify"
	"heritage-mgmt/internal/domain"
	"heritage-mgmt/internal/excelimport"
)

// 配置单例（由 config.go 加载，analyze/handlers/importer 等共享读取）
var docCfg domain.DocTypeCfg
var wfCfg domain.Workflow

type ImportStats struct {
	Units, Projects, Docs, Matched int
}

func ImportAll(basicdataDir string, verbose bool) (*ImportStats, error) {
	stats := &ImportStats{}
	finRows := excelimport.LoadFinancials(basicdataDir)
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
	sort.Strings(projDirs)

	unitCache := map[string]int64{}
	unitSort := map[string]int{} // 按导入顺序排序

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
		pname := classify.CleanProjectName(folder)
		unitName, level, category := classify.DetectUnit(pname, wfCfg.Units.Rules)
		ptype := classify.DetectType(pname, wfCfg.ProjectTypes.Rules)
		seq := classify.ParseSeq(folder)

		// 单位
		unitID, ok := unitCache[unitName]
		if !ok {
			sortVal := 99
			if s, ok := unitSort[unitName]; ok {
				sortVal = s
			}
			res, err := tx.Exec("INSERT INTO units(name,level,category,sort) VALUES(?,?,?,?)",
				unitName, level, category, sortVal)
			if err != nil {
				return stats, err
			}
			unitID, _ = res.LastInsertId()
			unitCache[unitName] = unitID
			stats.Units++
		}

		fin, score := classify.MatchFinancial(pname, finRows)
		matched := fin != nil

		// 插工程
		res, err := tx.Exec(`INSERT INTO projects(
			unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,contract_end,
			central_funding,eng_contract,eng_paid,sup_contract,sup_paid,
			des_contract,des_paid,expert_fee,total_paid,status,progress_note,
			source_dir,created) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`,
			unitID, seq, pname, ptype,
			excelimport.FinGet(fin, "approval_no"),
			excelimport.TrimDate(excelimport.FinGet(fin, "sign_date")), excelimport.TrimDate(excelimport.FinGet(fin, "complete_date")),
			excelimport.TrimDate(excelimport.FinGet(fin, "complete_date")), excelimport.TrimDate(excelimport.FinGet(fin, "contract_end")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "central_funding")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "eng_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "eng_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "sup_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "sup_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "des_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "des_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "expert_fee")), excelimport.ParseFloat(excelimport.FinGet(fin, "total_paid")),
			excelimport.DeriveStatus(fin), excelimport.FinGet(fin, "progress_note"),
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
			code, tname := classify.ClassifyDoc(fname, docCfg.Types, docCfg.UnknownCode, docCfg.UnknownName)
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
				pid, code, tname, classify.CleanTitle(fname), fname, rel, ext, size,
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
				amt := excelimport.FinGet(fin, "central_funding")
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
