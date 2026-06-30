// Package llm is a thin client for an OpenAI-compatible chat API (DeepSeek).
// It is the single place that builds chat/completions requests, sharing the
// request/auth/timeout/parse logic between the OCR extractor and the PDF
// report analysis (previously duplicated in ocr.go and report.go).
package llm

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"
)

// Config holds the LLM endpoint settings (loaded from llm.json / env).
type Config struct {
	BaseURL          string
	Model            string
	APIKey           string
	ExtractionPrompt string
	Temperature      float64
	MaxTokens        float64
	TimeoutSeconds   int
}

// Client calls an OpenAI-compatible chat/completions endpoint.
type Client struct{ cfg Config }

// New builds a client from cfg.
func New(cfg Config) *Client { return &Client{cfg: cfg} }

// Configured reports whether an API key is present.
func (c *Client) Configured() bool { return strings.TrimSpace(c.cfg.APIKey) != "" }

// Message is a single chat message.
type Message struct {
	Role    string
	Content string
}

// Options are per-call parameters.
type Options struct {
	Temperature float64
	MaxTokens   int
	JSONObject  bool          // 要求 response_format=json_object
	Timeout     time.Duration // 0 => 默认 90s
}

// Chat calls chat/completions and returns the first choice's content (trimmed).
// 对临时性错误（网络失败 / 429 / 5xx）最多重试 3 次（线性退避）；其余错误立即返回。
func (c *Client) Chat(messages []Message, opts Options) (string, error) {
	if err := c.validate(); err != nil {
		return "", err
	}
	body, err := json.Marshal(buildPayload(c.cfg, messages, opts))
	if err != nil {
		return "", fmt.Errorf("构造请求失败: %w", err)
	}
	url := strings.TrimRight(c.cfg.BaseURL, "/") + "/chat/completions"
	to := opts.Timeout
	if to == 0 {
		to = 90 * time.Second
	}
	client := &http.Client{Timeout: to}

	var lastErr error
	for attempt := 0; attempt < 3; attempt++ {
		req, err := http.NewRequest("POST", url, bytes.NewReader(body))
		if err != nil {
			return "", fmt.Errorf("构造请求失败: %w", err)
		}
		req.Header.Set("Authorization", "Bearer "+c.cfg.APIKey)
		req.Header.Set("Content-Type", "application/json")

		resp, err := client.Do(req)
		if err != nil {
			lastErr = err // 网络错误可重试
		} else {
			content, retry, err := parseChatResponse(resp)
			if !retry {
				return content, err
			}
			lastErr = err
		}
		time.Sleep(time.Duration(attempt+1) * time.Second)
	}
	if lastErr == nil {
		lastErr = fmt.Errorf("未知错误")
	}
	return "", fmt.Errorf("调用大模型失败(已重试，需联网): %v", lastErr)
}

// validate 校验必要配置，避免只配了 Key 却缺 base_url/model 时在运行时报怪错。
func (c *Client) validate() error {
	if strings.TrimSpace(c.cfg.APIKey) == "" {
		return fmt.Errorf("未配置API Key,请编辑 config/llm.json 或设置 DEEPSEEK_API_KEY")
	}
	if strings.TrimSpace(c.cfg.BaseURL) == "" {
		return fmt.Errorf("未配置 base_url,请编辑 config/llm.json")
	}
	if strings.TrimSpace(c.cfg.Model) == "" {
		return fmt.Errorf("未配置 model,请编辑 config/llm.json")
	}
	return nil
}

// buildPayload 构造 chat/completions 请求体（纯函数，便于测试）。
func buildPayload(cfg Config, messages []Message, opts Options) map[string]interface{} {
	msgs := make([]map[string]string, len(messages))
	for i, m := range messages {
		msgs[i] = map[string]string{"role": m.Role, "content": m.Content}
	}
	payload := map[string]interface{}{
		"model":       cfg.Model,
		"temperature": opts.Temperature,
		"max_tokens":  opts.MaxTokens,
		"messages":    msgs,
	}
	if opts.JSONObject {
		payload["response_format"] = map[string]string{"type": "json_object"}
	}
	return payload
}

// parseChatResponse 解析响应：限读 1MB 避免异常响应耗内存；检查 JSON 解析错误；
// 429/5xx 返回 retry=true 供调用方重试，其余错误立即返回。
func parseChatResponse(resp *http.Response) (content string, retry bool, err error) {
	defer resp.Body.Close()
	rbody, e := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if e != nil {
		return "", false, fmt.Errorf("读取响应失败: %w", e)
	}
	if resp.StatusCode == http.StatusTooManyRequests || resp.StatusCode >= 500 {
		return "", true, fmt.Errorf("大模型API临时错误 %d: %s", resp.StatusCode, Truncate(string(rbody), 400))
	}
	if resp.StatusCode != http.StatusOK {
		return "", false, fmt.Errorf("大模型API返回 %d: %s", resp.StatusCode, Truncate(string(rbody), 400))
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
	if e := json.Unmarshal(rbody, &r); e != nil {
		return "", false, fmt.Errorf("大模型返回JSON解析失败: %w; 原文: %s", e, Truncate(string(rbody), 300))
	}
	if r.Error != nil {
		return "", false, fmt.Errorf("大模型错误: %s", r.Error.Message)
	}
	if len(r.Choices) == 0 {
		return "", false, fmt.Errorf("大模型返回为空: %s", Truncate(string(rbody), 300))
	}
	return strings.TrimSpace(r.Choices[0].Message.Content), false, nil
}

// Truncate 截断字符串用于错误信息展示。
func Truncate(s string, n int) string {
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
}
