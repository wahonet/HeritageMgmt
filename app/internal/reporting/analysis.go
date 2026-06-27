package reporting

import (
	"fmt"
	"strings"
	"time"

	"heritage-mgmt/internal/llm"
)

// LLMClient 是 reporting 消费大模型所需的端口（仅依赖 Chat/Configured）。
// *llm.Client 结构化满足该接口，调用方可注入替身做无网络单测。
type LLMClient interface {
	Chat(messages []llm.Message, opts llm.Options) (string, error)
	Configured() bool
}

// GenerateAnalysis 调用大模型基于报告数据生成分析文本。
// 未配置密钥时返回占位说明（无错误），与原行为一致。
func GenerateAnalysis(client LLMClient, rd ReportData, timeout time.Duration) (string, error) {
	if !client.Configured() {
		return "（未配置大模型API密钥，分析报告功能不可用。）", nil
	}
	proj := rd.Project
	cf := proj.CentralFunding
	tp := proj.TotalPaid
	payRate := "—"
	if cf != nil && tp != nil && *cf > 0 {
		payRate = fmt.Sprintf("%.1f%%", *tp / *cf * 100)
	}

	summary := fmt.Sprintf(`工程名称：%s
文物单位：%s  级别：%s
工程类型：%s  当前状态：%s
批复文号：%s
合同工期：%s 至 %s  实际完工：%s
中央指标：%.0f万元  已支付：%.0f万元  支付率：%s
建设单位：%s
施工单位：%s（资质：%s）
设计单位：%s（资质：%s）
监理单位：%s（资质：%s）
文档总数：%d  完整度：%d%%
缺项：%s
资质校验：%s`,
		proj.Name, proj.UnitName, LevelName(rd.UnitLevel),
		proj.Ptype, proj.Status, proj.ApprovalNo,
		proj.SignDate, proj.ContractEnd, proj.CompleteDate,
		fptr(cf), fptr(tp), payRate,
		proj.OwnerUnit,
		proj.ConstructionUnit, proj.ConstructionQual,
		proj.DesignUnit, proj.DesignQual,
		proj.SupervisionUnit, proj.SupervisionQual,
		len(rd.Documents), rd.Completeness,
		strings.Join(rd.MissingRequired, "、"),
		strings.Join(rd.QualWarnings, "；"),
	)

	prompt := `你是文物保护工程档案分析专家。请根据以下工程数据，生成一份专业、正式的工程管理分析报告（约500-800字）。报告应包含：
1. 工程概况评价
2. 资金执行评价（支付率是否合理）
3. 进度评价（工期是否逾期）
4. 参建单位资质评价
5. 档案完整性评价（缺项分析）
6. 综合结论与建议
语气正式客观，不用markdown，直接输出纯文本。

工程数据：
` + summary

	return client.Chat(
		[]llm.Message{{Role: "user", Content: prompt}},
		llm.Options{Temperature: 0.3, MaxTokens: 2000, Timeout: timeout},
	)
}

func fptr(v *float64) float64 {
	if v == nil {
		return 0
	}
	return *v
}
