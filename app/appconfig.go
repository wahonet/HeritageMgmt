package main

// 应用配置容器（DI 根的数据来源）：集中持有路径、文档/工作流配置、LLM 配置，
// 取代原先散落的包级全局单例（appBase/dataDir/projectsDir/dbPath/docCfg/wfCfg/absBasicdata/llmCfg）。
// 由 main 在启动时通过 NewConfig 自顶向下构造，再注入 Store/Service/Server。

import (
	"encoding/json"
	"os"
	"path/filepath"

	"heritage-mgmt/internal/domain"
	"heritage-mgmt/internal/llm"
)

// basicdataDir 默认 Basicdata 目录（相对可执行文件同级）
const basicdataDir = "../Basicdata"

// Config 持有全部运行期配置。所有层通过依赖注入读取它，不再访问包级全局。
type Config struct {
	AppBase      string
	DataDir      string
	ProjectsDir  string
	DBPath       string
	AbsBasicdata string
	DocCfg       domain.DocTypeCfg
	WfCfg        domain.Workflow
	LLM          llm.Config
}

// ResolvePaths 解析数据/配置目录（优先可执行文件同级，go run 时回退工作目录），
// 并创建所需目录。返回 (appBase,dataDir,projectsDir,dbPath)。
func ResolvePaths() (appBase, dataDir, projectsDir, dbPath string) {
	base := "."
	if exe, err := os.Executable(); err == nil {
		base = filepath.Dir(exe)
		if _, e := os.Stat(filepath.Join(base, "config")); e != nil {
			if cwd, e2 := os.Getwd(); e2 == nil {
				if _, e3 := os.Stat(filepath.Join(cwd, "config")); e3 == nil {
					base = cwd
				}
			}
		}
	}
	appBase = base
	dataDir = filepath.Join(base, "data")
	projectsDir = filepath.Join(dataDir, "projects")
	dbPath = filepath.Join(dataDir, "heritage.db")
	os.MkdirAll(dataDir, 0755)
	os.MkdirAll(projectsDir, 0755)
	return
}

// readConfigFile 读取 config/<name>：磁盘优先，否则回退内嵌模板。
func readConfigFile(appBase, name string) ([]byte, error) {
	if b, err := os.ReadFile(filepath.Join(appBase, "config", name)); err == nil {
		return b, nil
	}
	return configFS.ReadFile("config/" + name)
}

// LoadDocWorkflow 加载 doc_types.json 与 workflow.json。
func LoadDocWorkflow(appBase string) (domain.DocTypeCfg, domain.Workflow, error) {
	var doc domain.DocTypeCfg
	b, err := readConfigFile(appBase, "doc_types.json")
	if err != nil {
		return doc, domain.Workflow{}, err
	}
	if err := json.Unmarshal(b, &doc); err != nil {
		return doc, domain.Workflow{}, err
	}
	var wf domain.Workflow
	b, err = readConfigFile(appBase, "workflow.json")
	if err != nil {
		return doc, wf, err
	}
	if err := json.Unmarshal(b, &wf); err != nil {
		return doc, wf, err
	}
	return doc, wf, nil
}

// LoadLLM 加载大模型配置：基础字段来自 config/llm.json（磁盘优先，否则内嵌模板）；
// 密钥绝不内嵌——优先 app/llm.json（磁盘），其次环境变量 DEEPSEEK_API_KEY。
func LoadLLM(appBase string) llm.Config {
	var raw map[string]interface{}
	if b, err := readConfigFile(appBase, "llm.json"); err == nil {
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
	var cfg llm.Config
	cfg.BaseURL = g("base_url")
	cfg.Model = g("model")
	cfg.ExtractionPrompt = g("extraction_prompt")
	cfg.APIKey = g("api_key") // 模板里为空
	if v, ok := raw["temperature"].(float64); ok {
		cfg.Temperature = v
	}
	if v, ok := raw["max_tokens"].(float64); ok {
		cfg.MaxTokens = v
	}
	if v, ok := raw["timeout_seconds"].(float64); ok {
		cfg.TimeoutSeconds = int(v)
	}
	// 密钥优先级: app/llm.json(磁盘,不入二进制) > 环境变量
	if kb, e := os.ReadFile(filepath.Join(appBase, "llm.json")); e == nil {
		var kf map[string]interface{}
		if json.Unmarshal(kb, &kf) == nil {
			if v, ok := kf["api_key"].(string); ok && v != "" {
				cfg.APIKey = v
			}
		}
	}
	if cfg.APIKey == "" {
		cfg.APIKey = os.Getenv("DEEPSEEK_API_KEY")
	}
	return cfg
}

// NewConfig 组装全部配置：解析路径 → 取 Basicdata 绝对路径 → 加载文档/工作流/LLM 配置。
func NewConfig() (*Config, error) {
	appBase, dataDir, projectsDir, dbPath := ResolvePaths()
	abs := basicdataDir
	if a, err := filepath.Abs(basicdataDir); err == nil {
		abs = a
	}
	docCfg, wfCfg, err := LoadDocWorkflow(appBase)
	if err != nil {
		return nil, err
	}
	return &Config{
		AppBase:      appBase,
		DataDir:      dataDir,
		ProjectsDir:  projectsDir,
		DBPath:       dbPath,
		AbsBasicdata: abs,
		DocCfg:       docCfg,
		WfCfg:        wfCfg,
		LLM:          LoadLLM(appBase),
	}, nil
}
