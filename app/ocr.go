package main

// OCR + 大模型提取模块（可选功能）
// 流程: 合同PDF -> 渲染成图片(mutool/pdftoppm/magick任一) -> tesseract OCR(chi_sim) -> DeepSeek提取结构化字段
// 依赖: 需在执行OCR的机器上安装 Tesseract(含chi_sim) 和 任一PDF渲染工具; 需联网调用DeepSeek。
// 主程序(浏览/编辑/导出)不依赖这些,保持离线纯Go。

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"heritage-mgmt/internal/llm"
	"heritage-mgmt/internal/ocr"
)

// llmCfg/llmClient: 大模型配置与客户端（HTTP 逻辑见 internal/llm）。
var (
	llmCfg    llm.Config
	llmClient = llm.New(llm.Config{})
)

// llmTimeout 返回调用超时：优先配置的 timeout_seconds，否则用给定默认值。
func llmTimeout(def time.Duration) time.Duration {
	if llmCfg.TimeoutSeconds > 0 {
		return time.Duration(llmCfg.TimeoutSeconds) * time.Second
	}
	return def
}

// loadLLMConfig 加载大模型配置:基础字段从config/llm.json(磁盘优先,否则内嵌模板);
// 密钥绝不内嵌——优先从 应用根目录/llm.json(磁盘) 读取,其次环境变量 DEEPSEEK_API_KEY。
func loadLLMConfig() {
	var raw map[string]interface{}
	if b, err := readConfigFile("llm.json"); err == nil {
		json.Unmarshal(b, &raw)
	}
	if raw == nil {
		raw = map[string]interface{}{}
	}
	g := func(k string) string {
		if v, ok := raw[k].(string); ok {
			return v
		}
		return ""
	}
	llmCfg.BaseURL = g("base_url")
	llmCfg.Model = g("model")
	llmCfg.ExtractionPrompt = g("extraction_prompt")
	llmCfg.APIKey = g("api_key") // 模板里为空
	if v, ok := raw["temperature"].(float64); ok {
		llmCfg.Temperature = v
	}
	if v, ok := raw["max_tokens"].(float64); ok {
		llmCfg.MaxTokens = v
	}
	if v, ok := raw["timeout_seconds"].(float64); ok {
		llmCfg.TimeoutSeconds = int(v)
	}
	// 密钥优先级: app/llm.json(磁盘,不入二进制) > 环境变量
	if kb, e := os.ReadFile(filepath.Join(appBase, "llm.json")); e == nil {
		var kf map[string]interface{}
		if json.Unmarshal(kb, &kf) == nil {
			if v, ok := kf["api_key"].(string); ok && v != "" {
				llmCfg.APIKey = v
			}
		}
	}
	if llmCfg.APIKey == "" {
		llmCfg.APIKey = os.Getenv("DEEPSEEK_API_KEY")
	}
	llmClient = llm.New(llmCfg)
}

// extractWithLLM 调大模型从OCR文本提取结构化字段
func extractWithLLM(text string) (map[string]string, error) {
	content, err := llmClient.Chat(
		[]llm.Message{{Role: "user", Content: llmCfg.ExtractionPrompt + "\n\n" + text}},
		llm.Options{
			Temperature: llmCfg.Temperature,
			MaxTokens:   int(llmCfg.MaxTokens),
			JSONObject:  true,
			Timeout:     llmTimeout(90 * time.Second),
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

// scanProjectContracts 扫描某工程的合同PDF并提取字段
func scanProjectContracts(pid int64) (map[string]string, error) {
	proj, err := GetProject(pid)
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
		dir := filepath.Join(projectsDir, proj.Folder, t)
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
			imgs, err := ocr.PDFToImages(pdf, tmp, appBase)
			if err != nil {
				continue
			}
			for _, img := range imgs {
				txt, err := ocr.OCRImage(img, dataDir, appBase)
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
	return extractWithLLM(combined.String())
}

// applyExtractedFields 把LLM提取的非空字段写回工程
func applyExtractedFields(pid int64, fields map[string]string) int {
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
		UpdateProjectFields(pid, strings.Join(sets, ","), args)
	}
	return len(sets)
}
