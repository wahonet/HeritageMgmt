package service

// 导入流程编排：扫描 Basicdata/ → 识别(classify) + Excel财务(excelimport) → 复制归档 → 入库。
// 原子性：文件先写暂存目录，DB 事务 + 磁盘替换全部成功才提交；任一步失败回滚，不破坏现网数据。
// ImportService 依赖注入 *store.Store（事务）与 *config.Config（路径/配置），不再访问包级全局。

import (
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"
	"time"

	"heritage-mgmt/internal/classify"
	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/excelimport"
	"heritage-mgmt/internal/store"
)

// ImportStats 导入统计。
type ImportStats struct {
	Units, Projects, Docs, Matched int
}

// ImportService 编排 Basicdata 批量导入（事务 + 扫描目录 + classify + excelimport + 落盘 + 入库）。
type ImportService struct {
	store *store.Store
	cfg   *config.Config
}

// ImportAll 扫描 cfg.AbsBasicdata 导入全部数据；verbose 控制是否打印进度。
// 流程：文件先全部复制到暂存目录（.import_xxx/projects），DB 事务内 ResetTables+入库，
// 全部成功后原子替换 ProjectsDir 并提交事务；任一步失败回滚（现网 ProjectsDir 与 DB 保持不变）。
func (svc *ImportService) ImportAll(verbose bool) (*ImportStats, error) {
	basicdataDir := svc.cfg.AbsBasicdata
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

	// 暂存目录：文件先复制到这里，DB+磁盘都成功后再原子替换正式 ProjectsDir。
	stage, err := os.MkdirTemp(svc.cfg.DataDir, ".import_*")
	if err != nil {
		return stats, err
	}
	defer os.RemoveAll(stage) // 成功路径下 projects 已被移走、stage 为空；失败路径下清理半成品
	stageProjects := filepath.Join(stage, "projects")

	unitCache := map[string]int64{}
	unitSort := map[string]int{} // 按导入顺序排序

	tx, err := svc.store.Begin()
	if err != nil {
		return stats, err
	}
	committed := false
	defer func() {
		if !committed {
			_ = tx.Rollback()
		}
	}()
	if err := store.ResetTables(tx); err != nil {
		return stats, err
	}

	for _, folder := range projDirs {
		srcDir := filepath.Join(basicdataDir, folder)
		pname := classify.CleanProjectName(folder)
		unitName, level, category := classify.DetectUnit(pname, svc.cfg.WfCfg.Units.Rules)
		ptype := classify.DetectType(pname, svc.cfg.WfCfg.ProjectTypes.Rules)
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
			excelimport.TrimDate(excelimport.FinGet(fin, "accept_date")), excelimport.TrimDate(excelimport.FinGet(fin, "contract_end")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "central_funding")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "eng_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "eng_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "sup_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "sup_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "des_contract")), excelimport.ParseFloat(excelimport.FinGet(fin, "des_paid")),
			excelimport.ParseFloat(excelimport.FinGet(fin, "expert_fee")), excelimport.ParseFloat(excelimport.FinGet(fin, "total_paid")),
			excelimport.DeriveStatus(fin, svc.cfg.Rules.StatusKeywords), excelimport.FinGet(fin, "progress_note"),
			folder, time.Now().Format("2006-01-02 15:04:05"))
		if err != nil {
			return stats, err
		}
		pid, _ := res.LastInsertId()
		if matched {
			stats.Matched++
		}
		projFolder := fmt.Sprintf("P%04d", pid)
		if _, err := tx.Exec("UPDATE projects SET folder=? WHERE id=?", projFolder, pid); err != nil {
			return stats, err
		}
		destRoot := filepath.Join(stageProjects, projFolder)
		if err := os.MkdirAll(destRoot, 0755); err != nil {
			return stats, err
		}

		// 文件：复制到暂存目录；任一失败立即中断（staging 不提交，现网不受影响）。
		files, err := os.ReadDir(srcDir)
		if err != nil {
			return stats, fmt.Errorf("读取工程目录失败 %s: %w", folder, err)
		}
		for _, fe := range files {
			if fe.IsDir() {
				continue
			}
			fname := fe.Name()
			code, tname := classify.ClassifyDoc(fname, svc.cfg.DocCfg.Types, svc.cfg.DocCfg.UnknownCode, svc.cfg.DocCfg.UnknownName)
			destDir := filepath.Join(destRoot, code)
			if err := os.MkdirAll(destDir, 0755); err != nil {
				return stats, err
			}
			dst := filepath.Join(destDir, fname)
			if err := copyFile(filepath.Join(srcDir, fname), dst); err != nil {
				return stats, fmt.Errorf("复制文件失败 %s: %w", fname, err)
			}
			rel, _ := filepath.Rel(stageProjects, dst)
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

	// 原子替换 ProjectsDir：旧目录改名备份 → 暂存 projects 就位 → 提交 DB；任一步失败回滚。
	oldProjects := svc.cfg.ProjectsDir + "_pre_import_" + time.Now().Format("20060102_150405")
	if err := os.Rename(svc.cfg.ProjectsDir, oldProjects); err != nil && !os.IsNotExist(err) {
		return stats, err
	}
	if err := os.Rename(stageProjects, svc.cfg.ProjectsDir); err != nil {
		_ = os.Rename(oldProjects, svc.cfg.ProjectsDir) // 回滚：恢复旧目录
		return stats, err
	}
	if err := tx.Commit(); err != nil {
		// DB 提交失败：移走新目录、恢复旧目录
		_ = os.RemoveAll(svc.cfg.ProjectsDir)
		_ = os.Rename(oldProjects, svc.cfg.ProjectsDir)
		return stats, err
	}
	committed = true
	_ = os.RemoveAll(oldProjects) // 成功：清理旧目录

	if verbose {
		fmt.Printf("\n导入完成: 单位 %d / 工程 %d / 文档 %d / 财务匹配 %d\n",
			stats.Units, stats.Projects, stats.Docs, stats.Matched)
	}
	return stats, nil
}

// copyFile 复制单个文件，检查 Close 错误以保证落盘完整。
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
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		return err
	}
	return out.Close()
}
