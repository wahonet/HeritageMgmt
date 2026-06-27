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
func (c *Client) Chat(messages []Message, opts Options) (string, error) {
	if strings.TrimSpace(c.cfg.APIKey) == "" {
		return "", fmt.Errorf("未配置API Key,请编辑 config/llm.json")
	}
	msgs := make([]map[string]string, len(messages))
	for i, m := range messages {
		msgs[i] = map[string]string{"role": m.Role, "content": m.Content}
	}
	payload := map[string]interface{}{
		"model":       c.cfg.Model,
		"temperature": opts.Temperature,
		"max_tokens":  opts.MaxTokens,
		"messages":    msgs,
	}
	if opts.JSONObject {
		payload["response_format"] = map[string]string{"type": "json_object"}
	}
	body, _ := json.Marshal(payload)
	url := strings.TrimRight(c.cfg.BaseURL, "/") + "/chat/completions"
	req, _ := http.NewRequest("POST", url, bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+c.cfg.APIKey)
	req.Header.Set("Content-Type", "application/json")
	to := opts.Timeout
	if to == 0 {
		to = 90 * time.Second
	}
	resp, err := (&http.Client{Timeout: to}).Do(req)
	if err != nil {
		return "", fmt.Errorf("调用大模型失败(需联网): %v", err)
	}
	defer resp.Body.Close()
	rbody, _ := io.ReadAll(resp.Body)
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("大模型API返回 %d: %s", resp.StatusCode, Truncate(string(rbody), 400))
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
		return "", fmt.Errorf("大模型错误: %s", r.Error.Message)
	}
	if len(r.Choices) == 0 {
		return "", fmt.Errorf("大模型返回为空: %s", Truncate(string(rbody), 300))
	}
	return strings.TrimSpace(r.Choices[0].Message.Content), nil
}

// Truncate 截断字符串用于错误信息展示。
func Truncate(s string, n int) string {
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
}
