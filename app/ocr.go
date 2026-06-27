package main

// OCR + 大模型提取模块（可选功能）
// 流程: 合同PDF -> 渲染成图片(mutool/pdftoppm/magick任一) -> tesseract OCR(chi_sim) -> DeepSeek提取结构化字段
// 依赖: 需在执行OCR的机器上安装 Tesseract(含chi_sim) 和 任一PDF渲染工具; 需联网调用DeepSeek。
// 主程序(浏览/编辑/导出)不依赖这些,保持离线纯Go。
//
// 依赖：需在执行OCR的机器上安装 Tesseract(含chi_sim) 和 任一PDF渲染工具; 需联网调用DeepSeek。
// 主程序(浏览/编辑/导出)不依赖这些,保持离线纯Go。
//
// OCRService 依赖注入 projects 仓储、*config.Config 与 LLM 客户端（llm.Client），
// 不再访问任何包级全局。

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"heritage-mgmt/internal/config"
	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/ocr"
)

// llmTimeout 返回调用超时：优先 cfg 的 timeout_seconds，否则用给定默认值。
func llmTimeout(cfg llm.Config, def time.Duration) time.Duration {
	if cfg.TimeoutSeconds > 0 {
		return time.Duration(cfg.TimeoutSeconds) * time.Second
	}
	return def
}

// OCRService 扫描工程合同并经大模型提取结构化字段。
type OCRService struct {
	projects ProjectRepository
	cfg      *config.Config
	llm      *llm.Client
	llmCfg   llm.Config
}

// extractWithLLM 调大模型从OCR文本提取结构化字段
func (svc *OCRService) extractWithLLM(text string) (map[string]string, error) {
	content, err := svc.llm.Chat(
		[]llm.Message{{Role: "user", Content: svc.llmCfg.ExtractionPrompt + "\n\n" + text}},
		llm.Options{
			Temperature: svc.llmCfg.Temperature,
			MaxTokens:   int(svc.llmCfg.MaxTokens),
			JSONObject:  true,
			Timeout:     llmTimeout(svc.llmCfg, 90*time.Second),
		},
	)
	if err != nil {
		return nil, err
	}
	content = strings.TrimPrefix(content, "```json")
	content = strings.TrimPrefix(content, "```")
	content = strings.TrimSuffix(content, "```")
	content = strings.TrimSpace(content)
	var fields map[string]string
	if err := json.Unmarshal([]byte(content), &fields); err != nil {
		return nil, fmt.Errorf("LLM输出非合法JSON: %v; 原文: %s", err, llm.Truncate(content, 300))
	}
	return fields, nil
}

// ScanContracts 扫描某工程的合同PDF并提取字段
func (svc *OCRService) ScanContracts(pid int64) (map[string]string, error) {
	proj, err := svc.projects.GetProject(pid)
	if err != nil || proj == nil {
		return nil, fmt.Errorf("工程不存在")
	}
	tmp, _ := os.MkdirTemp("", fmt.Sprintf("ocr_%d_*", pid))
	defer os.RemoveAll(tmp)
	categories := []string{"construction_contract", "design_contract", "supervision_contract"}
	var combined strings.Builder
	pdfCount := 0
	hasText := false
	for _, t := range categories {
		dir := filepath.Join(svc.cfg.ProjectsDir, proj.Folder, t)
		entries, err := os.ReadDir(dir)
		if err != nil {
			continue
		}
		for _, e := range entries {
			if e.IsDir() || !strings.HasSuffix(strings.ToLower(e.Name()), ".pdf") {
				continue
			}
			pdfCount++
			pdf := filepath.Join(dir, e.Name())
			imgs, err := ocr.PDFToImages(pdf, tmp, svc.cfg.AppBase)
			if err != nil {
				continue
			}
			for _, img := range imgs {
				txt, err := ocr.OCRImage(img, svc.cfg.DataDir, svc.cfg.AppBase)
				if err == nil && strings.TrimSpace(txt) != "" {
					combined.WriteString(fmt.Sprintf("\n==== %s: %s ====\n", t, e.Name()))
					combined.WriteString(txt)
					hasText = true
				}
			}
		}
	}
	if pdfCount == 0 {
		return nil, fmt.Errorf("该工程没有合同PDF(项目/设计/监理合同)")
	}
	if !hasText {
		return nil, fmt.Errorf("未提取到文本: 请确认已安装 tesseract(含chi_sim中文包) 和 PDF渲染工具(mutool/pdftoppm/magick 之一)")
	}
	return svc.extractWithLLM(combined.String())
}

// ApplyFields 把LLM提取的非空字段写回工程
func (svc *OCRService) ApplyFields(pid int64, fields map[string]string) int {
	order := []string{"owner_unit", "construction_unit", "construction_qual", "design_unit",
		"design_qual", "supervision_unit", "supervision_qual", "contract_no", "contract_sign_date", "contract_end"}
	var sets, parts []string
	var args []interface{}
	for _, k := range order {
		if v, ok := fields[k]; ok && strings.TrimSpace(v) != "" {
			sets = append(sets, k+"=?")
			args = append(args, strings.TrimSpace(v))
			parts = append(parts, k+"="+strings.TrimSpace(v))
		}
	}
	if len(sets) > 0 {
		svc.projects.UpdateProjectFields(pid, strings.Join(sets, ","), args)
	}
	return len(sets)
}
