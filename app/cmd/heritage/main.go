// Command heritage 是文物保护工程管理系统的入口。
// 职责仅限：解析 CLI → 组装依赖(config→store→services→server) → 启动。
// 已无任何包级可变全局；分层依赖由编译器强制（domain←store←service←httpapi）。
//
// 构建：  go build -o heritage-mgmt.exe ./cmd/heritage
// 交叉：  CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o heritage-mgmt-linux-amd64 ./cmd/heritage
package main

import (
	"fmt"
	"log"
	"os"
	"strconv"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/httpapi"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/service"
	"heritage-mgmt/internal/store"
)

func main() {
	cfg, err := config.NewConfig()
	if err != nil {
		log.Fatal("加载配置失败: ", err)
	}
	st, err := store.NewStore(cfg)
	if err != nil {
		log.Fatal("打开数据库失败: ", err)
	}
	client := llm.New(cfg.LLM)
	svc := service.NewServices(cfg, st, client)

	// 命令行: -ocr-all 批量OCR所有工程(已三类单位齐全的自动跳过)
	force := len(os.Args) > 1 && os.Args[1] == "-ocr-force"
	if len(os.Args) > 1 && (os.Args[1] == "-ocr-all" || force) {
		projs, _ := st.ListProjects(0, "", "")
		fmt.Printf("批量OCR(%s): 共 %d 个工程\n", map[bool]string{true: "强制重跑", false: "增量"}[force], len(projs))
		ok, skip, fail := 0, 0, 0
		for i, p := range projs {
			fmt.Printf("\n[%d/%d] %s\n", i+1, len(projs), p.Name)
			if !force && p.ConstructionUnit != "" {
				fmt.Println("  ↳ 已有施工单位数据,跳过")
				skip++
				continue
			}
			fields, err := svc.OCR.ScanContracts(p.ID)
			if err != nil {
				fmt.Println("  ✗", err)
				fail++
				continue
			}
			n := svc.OCR.ApplyFields(p.ID, fields)
			st.InsertLog("OCR识别(批量)", fmt.Sprintf("工程#%d %s", p.ID, p.Name), fmt.Sprintf("回填%d个字段", n))
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
		fields, err := svc.OCR.ScanContracts(pid)
		if err != nil {
			fmt.Println("❌ OCR失败:", err)
			os.Exit(1)
		}
		n := svc.OCR.ApplyFields(pid, fields)
		fmt.Printf("✅ 提取并回填 %d 个字段:\n", n)
		for _, k := range []string{"construction_unit", "construction_qual", "design_unit", "design_qual", "supervision_unit", "supervision_qual", "contract_end"} {
			fmt.Printf("  %-20s = %s\n", k, fields[k])
		}
		return
	}

	// 命令行: -import 强制重新导入
	if len(os.Args) > 1 && os.Args[1] == "-import" {
		svc.Imp.ImportAll(true)
		return
	}
	// 数据库为空则自动导入
	if st.CountProjects() == 0 {
		fmt.Println("⚠ 数据库为空，正在自动导入 Basicdata ...")
		svc.Imp.ImportAll(true)
	}

	// 启动 HTTP 服务
	srv := httpapi.NewServer(cfg, st, svc, client)
	if err := srv.ListenAndServe("127.0.0.1:5000"); err != nil {
		log.Fatal(err)
	}
}
