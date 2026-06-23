package main

import "testing"

// TestMain 初始化配置(测试需要 docCfg/wfCfg)
func TestMain(m *testing.M) {
	initPaths()
	loadConfig()
	m.Run()
}

// 测试文档分类：按文件名关键词匹配12类
func TestClassifyDoc(t *testing.T) {
	tests := []struct{ filename, wantCode string }{
		{"1、批复文（鲁文旅保[2019]45号）.pdf", "approval"},
		{"2、资金下达指标文（嘉财教指[2024]30号）.pdf", "funding"},
		{"3、项目合同（修缮工程合同）.pdf", "construction_contract"},
		{"4、开工报告（修缮工程开工报告）.pdf", "start_report"},
		{"5、验收（四方验评表）.pdf", "acceptance"},
		{"8、工程费.pdf", "engineering_fee"},
		{"9、设计费（修缮工程设计费）.pdf", "design_fee"},
		{"目录.docx", "catalog"},
		{"某个不匹配的文件.xyz", "other"},
	}
	for _, tt := range tests {
		code, name := classifyDoc(tt.filename)
		if code != tt.wantCode {
			t.Errorf("classifyDoc(%q) = %q(%q), want %q", tt.filename, code, name, tt.wantCode)
		}
	}
}

// 测试日期规范化
func TestTrimDate(t *testing.T) {
	cases := []struct{ in, want string }{
		{"2021-12-10 00:00:00", "2021-12-10"},
		{"2021-12-10", "2021-12-10"},
		{"", ""},
		{"2021年12月10日", "2021年12月10日"}, // 无-或/，原样返回
	}
	for _, c := range cases {
		if got := trimDate(c.in); got != c.want {
			t.Errorf("trimDate(%q) = %q, want %q", c.in, got, c.want)
		}
	}
}

// 测试序号解析
func TestParseSeq(t *testing.T) {
	if seq := parseSeq("3、嘉祥武氏墓群石刻安防工程"); seq == nil || *seq != 3 {
		t.Error("parseSeq 应返回 3")
	}
	if seq := parseSeq("无序号工程"); seq != nil {
		t.Error("parseSeq 无序号应返回 nil")
	}
}

// 测试文物单位识别
func TestDetectUnit(t *testing.T) {
	unit, level, _ := detectUnit("嘉祥武氏墓群石刻安防升级改造工程")
	if unit != "武氏墓群石刻" || level != "国保" {
		t.Errorf("detectUnit 武氏: got unit=%q level=%q", unit, level)
	}
	unit2, _, _ := detectUnit("曾庙宗圣寝殿修缮工程")
	if unit2 != "曾庙" {
		t.Errorf("detectUnit 曾庙: got %q", unit2)
	}
	unit3, _, _ := detectUnit("青山寺万佛阁抢险修缮工程")
	if unit3 != "青山寺" {
		t.Errorf("detectUnit 青山寺: got %q", unit3)
	}
}

// 测试工程类型识别
func TestDetectType(t *testing.T) {
	cases := map[string]string{
		"嘉祥武氏墓群石刻安防升级改造工程": "安防工程",
		"曾庙消防工程":             "消防工程",
		"青山寺万佛阁抢险修缮工程":      "抢险加固工程",
		"武氏墓群石刻本体保护工程":      "本体保护工程",
	}
	for name, want := range cases {
		if got := detectType(name); got != want {
			t.Errorf("detectType(%q) = %q, want %q", name, got, want)
		}
	}
}

// 测试工程名清理（去序号前缀）
func TestCleanProjectName(t *testing.T) {
	cases := []struct{ in, want string }{
		{"3、嘉祥武氏墓群石刻安防工程", "嘉祥武氏墓群石刻安防工程"},
		{"12、曾庙消防工程", "曾庙消防工程"},
		{"无序号工程", "无序号工程"},
	}
	for _, c := range cases {
		if got := cleanProjectName(c.in); got != c.want {
			t.Errorf("cleanProjectName(%q) = %q, want %q", c.in, got, c.want)
		}
	}
}

// 测试相似度计算
func TestSimilarity(t *testing.T) {
	if s := similarity("嘉祥武氏墓群石刻安防工程", "嘉祥武氏墓群石刻安防工程"); s != 1.0 {
		t.Errorf("相同字符串相似度应为1.0, got %f", s)
	}
	if s := similarity("", "abc"); s != 0 {
		t.Errorf("空字符串相似度应为0, got %f", s)
	}
	s := similarity("嘉祥武氏墓群石刻", "嘉祥武氏墓群石刻本体保护")
	if s < 0.5 {
		t.Errorf("高度相似字符串相似度应>0.5, got %f", s)
	}
}
