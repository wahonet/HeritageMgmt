package classify

import (
	"testing"

	"heritage-mgmt/internal/domain"
)

// 自包含的最小配置（无需磁盘/全局），覆盖各分类的关键词匹配。
var testTypes = []domain.DocType{
	{Code: "approval", Name: "批复文", Keywords: []string{"批复文"}},
	{Code: "funding", Name: "资金下达指标文", Keywords: []string{"资金下达指标文", "指标文"}},
	{Code: "construction_contract", Name: "项目合同", Keywords: []string{"项目合同", "施工合同"}},
	{Code: "start_report", Name: "开工报告", Keywords: []string{"开工报告"}},
	{Code: "acceptance", Name: "验收", Keywords: []string{"验收"}},
	{Code: "engineering_fee", Name: "工程费", Keywords: []string{"工程费"}},
	{Code: "design_fee", Name: "设计费", Keywords: []string{"设计费"}},
	{Code: "catalog", Name: "目录", Keywords: []string{"目录"}},
}

// 测试文档分类：按文件名关键词匹配
func TestClassifyDoc(t *testing.T) {
	tests := []struct{ filename, wantCode string }{
		{"1、批复文（某文物保[2019]45号）.pdf", "approval"},
		{"2、资金下达指标文（某财教指[2024]30号）.pdf", "funding"},
		{"3、项目合同（修缮工程合同）.pdf", "construction_contract"},
		{"4、开工报告（修缮工程开工报告）.pdf", "start_report"},
		{"5、验收（四方验评表）.pdf", "acceptance"},
		{"8、工程费.pdf", "engineering_fee"},
		{"9、设计费（修缮工程设计费）.pdf", "design_fee"},
		{"目录.docx", "catalog"},
		{"某个不匹配的文件.xyz", "other"},
	}
	for _, tt := range tests {
		code, name := ClassifyDoc(tt.filename, testTypes, "other", "其他")
		if code != tt.wantCode {
			t.Errorf("ClassifyDoc(%q) = %q(%q), want %q", tt.filename, code, name, tt.wantCode)
		}
	}
}

// 测试序号解析
func TestParseSeq(t *testing.T) {
	if seq := ParseSeq("3、某文物修缮工程"); seq == nil || *seq != 3 {
		t.Error("ParseSeq 应返回 3")
	}
	if seq := ParseSeq("无序号工程"); seq != nil {
		t.Error("ParseSeq 无序号应返回 nil")
	}
}

// 测试工程类型识别
func TestDetectType(t *testing.T) {
	rules := []domain.TypeRule{
		{Type: "安防工程", Keywords: []string{"安防"}},
		{Type: "消防工程", Keywords: []string{"消防"}},
		{Type: "抢险加固工程", Keywords: []string{"抢险"}},
		{Type: "本体保护工程", Keywords: []string{"本体保护"}},
	}
	cases := map[string]string{
		"某文物安防升级改造工程": "安防工程",
		"某古建筑消防工程":    "消防工程",
		"某古建筑抢险修缮工程":  "抢险加固工程",
		"某遗址本体保护工程":   "本体保护工程",
	}
	for name, want := range cases {
		if got := DetectType(name, rules); got != want {
			t.Errorf("DetectType(%q) = %q, want %q", name, got, want)
		}
	}
}

// 测试工程名清理（去序号前缀）
func TestCleanProjectName(t *testing.T) {
	cases := []struct{ in, want string }{
		{"3、某文物修缮工程", "某文物修缮工程"},
		{"12、某消防工程", "某消防工程"},
		{"无序号工程", "无序号工程"},
	}
	for _, c := range cases {
		if got := CleanProjectName(c.in); got != c.want {
			t.Errorf("CleanProjectName(%q) = %q, want %q", c.in, got, c.want)
		}
	}
}

// 测试相似度计算
func TestSimilarity(t *testing.T) {
	if s := Similarity("某文物修缮工程", "某文物修缮工程"); s != 1.0 {
		t.Errorf("相同字符串相似度应为1.0, got %f", s)
	}
	if s := Similarity("", "abc"); s != 0 {
		t.Errorf("空字符串相似度应为0, got %f", s)
	}
	s := Similarity("某文物修缮", "某文物修缮本体保护")
	if s < 0.5 {
		t.Errorf("高度相似字符串相似度应>0.5, got %f", s)
	}
}
