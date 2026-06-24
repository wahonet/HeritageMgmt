package main

// 入口：embed、CLI、路由注册、启动服务

import (
	"embed"
	"fmt"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
)

//go:embed all:static
var staticFS embed.FS

//go:embed all:config
var configFS embed.FS

const basicdataDir = "../Basicdata"

var absBasicdata string

func main() {
	initPaths()
	if abs, err := filepath.Abs(basicdataDir); err == nil {
		absBasicdata = abs
	} else {
		absBasicdata = basicdataDir
	}
	if err := loadConfig(); err != nil {
		log.Fatal("加载配置失败: ", err)
	}
	if err := OpenDB(); err != nil {
		log.Fatal("打开数据库失败: ", err)
	}
	loadLLMConfig()

	// 命令行: -ocr-all 批量OCR所有工程(已三类单位齐全的自动跳过)
	force := len(os.Args) > 1 && os.Args[1] == "-ocr-force"
	if len(os.Args) > 1 && (os.Args[1] == "-ocr-all" || force) {
		projs, _ := ListProjects(0, "", "")
		fmt.Printf("批量OCR(%s): 共 %d 个工程\n", map[bool]string{true: "强制重跑", false: "增量"}[force], len(projs))
		ok, skip, fail := 0, 0, 0
		for i, p := range projs {
			fmt.Printf("\n[%d/%d] %s\n", i+1, len(projs), p.Name)
			if !force && p.ConstructionUnit != "" {
				fmt.Println("  ↳ 已有施工单位数据,跳过")
				skip++
				continue
			}
			fields, err := scanProjectContracts(p.ID)
			if err != nil {
				fmt.Println("  ✗", err)
				fail++
				continue
			}
			n := applyExtractedFields(p.ID, fields)
			InsertLog("OCR识别(批量)", fmt.Sprintf("工程#%d %s", p.ID, p.Name), fmt.Sprintf("回填%d个字段", n))
			fmt.Printf("  ✅ 回填%d个: 建设=%s | 施工=%s | 设计=%s | 监理=%s | 合同号=%s\n",
				n, fields["owner_unit"], fields["construction_unit"], fields["design_unit"], fields["supervision_unit"], fields["contract_no"])
			ok++
		}
		fmt.Printf("\n===== 完成: 成功 %d / 跳过 %d / 失败 %d =====\n", ok, skip, fail)
		return
	}

	// 命令行: -ocr <pid> 对某工程合同做OCR+大模型提取
	if len(os.Args) > 2 && os.Args[1] == "-ocr" {
		pid, _ := strconv.ParseInt(os.Args[2], 10, 64)
		fmt.Printf("OCR识别 工程#%d 的合同...\n", pid)
		fields, err := scanProjectContracts(pid)
		if err != nil {
			fmt.Println("❌ OCR失败:", err)
			os.Exit(1)
		}
		n := applyExtractedFields(pid, fields)
		fmt.Printf("✅ 提取并回填 %d 个字段:\n", n)
		for _, k := range []string{"construction_unit", "construction_qual", "design_unit", "design_qual", "supervision_unit", "supervision_qual", "contract_end"} {
			fmt.Printf("  %-20s = %s\n", k, fields[k])
		}
		return
	}

	// 命令行: -import 强制重新导入
	if len(os.Args) > 1 && os.Args[1] == "-import" {
		ImportAll(absBasicdata, true)
		return
	}
	// 数据库为空则自动导入
	var n int
	db.QueryRow("SELECT COUNT(*) FROM projects").Scan(&n)
	if n == 0 {
		fmt.Println("⚠ 数据库为空，正在自动导入 Basicdata ...")
		ImportAll(absBasicdata, true)
	}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /api/config", handleConfig)
	mux.HandleFunc("GET /api/units", handleUnits)
	mux.HandleFunc("DELETE /api/unit/{id}", handleDeleteUnit)
	mux.HandleFunc("GET /api/projects", handleProjects)
	mux.HandleFunc("GET /api/project/{id}", handleProject)
	mux.HandleFunc("PUT /api/project/{id}", handleUpdateProject)
	mux.HandleFunc("DELETE /api/project/{id}", handleDeleteProject)
	mux.HandleFunc("DELETE /api/project/{id}/doctype/{docType}", handleDeleteDocType)
	mux.HandleFunc("POST /api/project/create", handleCreateProject)
	mux.HandleFunc("GET /api/project/{id}/tree", handleProjectTree)
	mux.HandleFunc("GET /api/dashboard", handleDashboard)
	mux.HandleFunc("GET /api/file/{id}", handleFile)
	mux.HandleFunc("POST /api/upload", handleUpload)
	mux.HandleFunc("DELETE /api/document/{id}", handleDeleteDoc)
	mux.HandleFunc("POST /api/import", handleImport)
	mux.HandleFunc("GET /api/stats", handleStats)
	mux.HandleFunc("GET /api/logs", handleLogs)
	mux.HandleFunc("GET /api/export/ledger", handleExportLedger)
	mux.HandleFunc("GET /api/backup", handleBackup)
	mux.HandleFunc("POST /api/restore", handleRestore)
	mux.HandleFunc("GET /api/recycle", handleRecycle)
	mux.HandleFunc("POST /api/project/{id}/restore", handleRestoreProject)
	mux.HandleFunc("DELETE /api/project/{id}/purge", handlePurgeProject)
	mux.HandleFunc("GET /api/project/{id}/report", handleReportPDF)
	mux.HandleFunc("POST /api/ocr/scan", handleOCRScan)
	mux.HandleFunc("GET /logo.png", func(w http.ResponseWriter, r *http.Request) {
		http.ServeFile(w, r, filepath.Join(appBase, "logo.png"))
	})
	mux.Handle("/", noCacheFS(http.FileServer(staticFileSystem())))

	fmt.Println("✓ 文物保护工程管理系统已启动: http://127.0.0.1:5000  (Ctrl+C 停止)")
	srv := &http.Server{Addr: "127.0.0.1:5000", Handler: mux}
	if err := srv.ListenAndServe(); err != nil {
		log.Fatal(err)
	}
}
