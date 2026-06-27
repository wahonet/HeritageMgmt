package excelimport

import "testing"

// 测试日期规范化
func TestTrimDate(t *testing.T) {
	cases := []struct{ in, want string }{
		{"2021-12-10 00:00:00", "2021-12-10"},
		{"2021-12-10", "2021-12-10"},
		{"", ""},
		{"2021年12月10日", "2021年12月10日"},
	}
	for _, c := range cases {
		if got := TrimDate(c.in); got != c.want {
			t.Errorf("TrimDate(%q) = %q, want %q", c.in, got, c.want)
		}
	}
}

// 状态推导关键词（与 config/rules.json 默认一致）
var testStatusKW = map[string][]string{
	"已竣工": {"验收", "竣工", "完工"},
	"在建":   {"在建", "施工"},
}

// 测试工程状态推导（关键词来自注入的 rules，本包不依赖配置）
func TestDeriveStatus(t *testing.T) {
	cases := []struct {
		name    string
		fin     map[string]string
		want    string
	}{
		{"含验收→已竣工", map[string]string{"项目建设情况": "已组织竣工验收"}, "已竣工"},
		{"含竣工→已竣工", map[string]string{"项目建设情况": "工程竣工"}, "已竣工"},
		{"含施工→在建", map[string]string{"项目建设情况": "正在施工"}, "在建"},
		{"无关键词无开工日期→前期", map[string]string{"项目建设情况": "前期准备"}, "前期"},
		{"无关键词但有开工日期→在建", map[string]string{"项目建设情况": "", "开工日期": "2021-05-01"}, "在建"},
		{"nil→空", nil, ""},
	}
	for _, c := range cases {
		if got := DeriveStatus(c.fin, testStatusKW); got != c.want {
			t.Errorf("DeriveStatus(%s) = %q, want %q", c.name, got, c.want)
		}
	}
}
