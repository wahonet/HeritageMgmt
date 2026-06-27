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
