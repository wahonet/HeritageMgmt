package reporting

// GenerateAnalysis 单测：注入 mock LLMClient（无需联网/无 API Key）。
// 证明步骤7把 reporting 收 LLMClient 接口后可 mock。

import (
	"errors"
	"testing"
	"time"

	"heritage-mgmt/internal/domain"
	"heritage-mgmt/internal/llm"
)

type mockLLM struct {
	configured bool
	resp       string
	err        error
}

func (m mockLLM) Chat([]llm.Message, llm.Options) (string, error) { return m.resp, m.err }
func (m mockLLM) Configured() bool                                { return m.configured }

// 未配置密钥 → 返回占位文本、无错误
func TestGenerateAnalysisUnconfigured(t *testing.T) {
	rd := ReportData{Project: &domain.Project{Name: "测试工程"}}
	out, err := GenerateAnalysis(mockLLM{configured: false}, rd, time.Second)
	if err != nil {
		t.Fatalf("未配置应无错误, got %v", err)
	}
	if out == "" {
		t.Error("未配置应返回占位说明文本")
	}
}

// 已配置 → 透传客户端返回的分析文本
func TestGenerateAnalysisConfigured(t *testing.T) {
	rd := ReportData{Project: &domain.Project{Name: "测试工程"}}
	out, err := GenerateAnalysis(mockLLM{configured: true, resp: "这是分析报告正文"}, rd, time.Second)
	if err != nil {
		t.Fatalf("err = %v", err)
	}
	if out != "这是分析报告正文" {
		t.Errorf("应透传客户端返回, got %q", out)
	}
}

// 客户端返回错误 → 透传错误
func TestGenerateAnalysisError(t *testing.T) {
	rd := ReportData{Project: &domain.Project{Name: "测试工程"}}
	_, err := GenerateAnalysis(mockLLM{configured: true, err: errors.New("boom")}, rd, time.Second)
	if err == nil {
		t.Error("客户端错误应透传")
	}
}
