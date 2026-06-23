package main

// OCR + 大模型提取模块（可选功能）
// 流程: 合同PDF -> 渲染成图片(mutool/pdftoppm/magick任一) -> tesseract OCR(chi_sim) -> DeepSeek提取结构化字段
// 依赖: 需在执行OCR的机器上安装 Tesseract(含chi_sim) 和 任一PDF渲染工具; 需联网调用DeepSeek。
// 主程序(浏览/编辑/导出)不依赖这些,保持离线纯Go。

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"time"
)

type llmConfig struct {
	BaseURL, Model, APIKey, ExtractionPrompt string
	Temperature                              float64
	MaxTokens                                float64
	TimeoutSeconds                           int
}

var llmCfg llmConfig

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
}

// fileExists 判断路径是否存在
func fileExists(p string) bool {
	_, err := os.Stat(p)
	return err == nil
}

// findTool 多路径查找可执行工具: PATH > app/tools > winget包目录 > Program Files
func findTool(name string) string {
	if p, err := exec.LookPath(name); err == nil {
		return p
	}
	names := []string{name}
	if filepath.Base(name) == name && runtime.GOOS == "windows" {
		names = append(names, name+".exe")
	}
	// 1) 应用自带 tools 目录
	for _, c := range names {
		p := filepath.Join(appBase, "tools", c)
		if fileExists(p) {
			return p
		}
	}
	// 2) Windows winget 包目录(支持 poppler/mupdf 等便携安装)
	if gd := os.Getenv("LOCALAPPDATA"); gd != "" {
		wgBase := filepath.Join(gd, "Microsoft", "WinGet", "Packages")
		for _, c := range names {
			for _, pat := range []string{
				filepath.Join(wgBase, "*", "*", "Library", "bin", c),
				filepath.Join(wgBase, "*", "poppler-*", "Library", "bin", c),
				filepath.Join(wgBase, "*", "*", "bin", c),
			} {
				if m, _ := filepath.Glob(pat); len(m) > 0 {
					return m[0]
				}
			}
		}
	}
	// 3) Program Files 常见位置(tesseract 等)
	for _, c := range names {
		for _, dir := range []string{`C:\Program Files\Tesseract-OCR`, `C:\Program Files\ImageMagick-7.1.2-Q16`} {
			p := filepath.Join(dir, c)
			if fileExists(p) {
				return p
			}
		}
	}
	return ""
}

// pdfToImages 把PDF渲染成PNG,依次尝试多种工具
func pdfToImages(pdfPath, outDir string) ([]string, error) {
	prefix := filepath.Join(outDir, "p")
	old, _ := filepath.Glob(prefix + "*.png")
	for _, f := range old {
		os.Remove(f)
	}
	type renderer struct{ name, bin string }
	cands := []renderer{
		{"pdftoppm", findTool("pdftoppm")},
		{"mutool", findTool("mutool")},
		{"magick", findTool("magick")},
		{"convert", findTool("convert")},
	}
	maxPages := "6" // 只取前6页(参建单位信息都在合同前几页,避免上百页扫描件耗时失控)
	for _, r := range cands {
		if r.bin == "" {
			continue
		}
		var cmd *exec.Cmd
		switch r.name {
		case "mutool":
			cmd = exec.Command(r.bin, "draw", "-o", prefix+"%03d.png", "-r", "200", pdfPath, "1-"+maxPages)
		case "pdftoppm":
			cmd = exec.Command(r.bin, "-png", "-r", "200", "-f", "1", "-l", maxPages, pdfPath, prefix)
		case "magick", "convert":
			cmd = exec.Command(r.bin, "-density", "200", pdfPath+"[0-5]", prefix+"%03d.png")
		}
		_ = cmd.Run()
		if imgs, _ := filepath.Glob(prefix + "*.png"); len(imgs) > 0 {
			sort.Strings(imgs)
			return imgs, nil
		}
	}
	return nil, fmt.Errorf("未找到可用的PDF渲染工具(需安装 poppler(pdftoppm) 或 mupdf 或 ImageMagick 之一)")
}

func ocrImage(imgPath string) (string, error) {
	tess := findTool("tesseract")
	if tess == "" {
		return "", fmt.Errorf("未找到tesseract,请安装Tesseract OCR及chi_sim中文语言包")
	}
	args := []string{imgPath, "stdout", "-l", "chi_sim", "--psm", "6"}
	// 用 --tessdata-dir 直接指定语言包目录(比 TESSDATA_PREFIX 环境变量更可靠)
	tessdataDir := filepath.Join(dataDir, "tessdata")
	if _, e := os.Stat(filepath.Join(tessdataDir, "chi_sim.traineddata")); e == nil {
		args = append([]string{"--tessdata-dir", tessdataDir}, args...)
	}
	cmd := exec.Command(tess, args...)
	out, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}

// extractWithLLM 调DeepSeek从OCR文本提取结构化字段
func extractWithLLM(text string) (map[string]string, error) {
	if strings.TrimSpace(llmCfg.APIKey) == "" {
		return nil, fmt.Errorf("未配置API Key,请编辑 config/llm.json")
	}
	payload := map[string]interface{}{
		"model":           llmCfg.Model,
		"temperature":     llmCfg.Temperature,
		"max_tokens":      int(llmCfg.MaxTokens),
		"response_format": map[string]string{"type": "json_object"},
		"messages": []map[string]string{
			{"role": "user", "content": llmCfg.ExtractionPrompt + "\n\n" + text},
		},
	}
	body, _ := json.Marshal(payload)
	url := strings.TrimRight(llmCfg.BaseURL, "/") + "/chat/completions"
	req, _ := http.NewRequest("POST", url, bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+llmCfg.APIKey)
	req.Header.Set("Content-Type", "application/json")
	to := time.Duration(llmCfg.TimeoutSeconds) * time.Second
	if to == 0 {
		to = 90 * time.Second
	}
	resp, err := (&http.Client{Timeout: to}).Do(req)
	if err != nil {
		return nil, fmt.Errorf("调用DeepSeek失败(需联网): %v", err)
	}
	defer resp.Body.Close()
	rbody, _ := io.ReadAll(resp.Body)
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("DeepSeek API返回 %d: %s", resp.StatusCode, truncate(string(rbody), 400))
	}
	var r struct {
		Choices []struct {
			Message struct {
				Content string `json:"content"`
			} `json:"message"`
		} `json:"choices"`
		Error *struct {
			Message string `json:"message"`
		} `json:"error"`
	}
	json.Unmarshal(rbody, &r)
	if r.Error != nil {
		return nil, fmt.Errorf("DeepSeek错误: %s", r.Error.Message)
	}
	if len(r.Choices) == 0 {
		return nil, fmt.Errorf("DeepSeek返回为空: %s", truncate(string(rbody), 300))
	}
	content := strings.TrimSpace(r.Choices[0].Message.Content)
	content = strings.TrimPrefix(content, "```json")
	content = strings.TrimPrefix(content, "```")
	content = strings.TrimSuffix(content, "```")
	content = strings.TrimSpace(content)
	var fields map[string]string
	if err := json.Unmarshal([]byte(content), &fields); err != nil {
		return nil, fmt.Errorf("LLM输出非合法JSON: %v; 原文: %s", err, truncate(content, 300))
	}
	return fields, nil
}

func truncate(s string, n int) string {
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
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
			imgs, err := pdfToImages(pdf, tmp)
			if err != nil {
				continue
			}
			for _, img := range imgs {
				txt, err := ocrImage(img)
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
		args = append(args, pid)
		db.Exec("UPDATE projects SET "+strings.Join(sets, ",")+" WHERE id=?", args...)
	}
	return len(sets)
}
