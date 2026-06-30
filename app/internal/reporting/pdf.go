// Package reporting renders a project report PDF (gopdf) from a typed
// ReportData, and (in analysis.go) generates the optional LLM analysis section.
package reporting

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"heritage-mgmt/internal/domain"

	"github.com/signintech/gopdf"
)

// ReportData 报告所需的全部数据（由调用方从分析结果填充）。
type ReportData struct {
	Project         *domain.Project
	UnitLevel       string
	Completeness    int
	MissingRequired []string
	Documents       []domain.Document
	QualWarnings    []string
	Analysis        string // 智能分析文本（由 GenerateAnalysis 生成）
}

var levelNames = map[string]string{
	"国保":  "全国重点文物保护单位",
	"省保":  "省级文物保护单位",
	"市保":  "市级文物保护单位",
	"县保":  "县级文物保护单位",
	"未定级": "未定级文物点",
}

// LevelName 返回保护级别全称
func LevelName(l string) string {
	if v, ok := levelNames[l]; ok {
		return v
	}
	if l == "" {
		return "未定级文物点"
	}
	return l
}

// FindChineseFont 查找可用中文字体(应用自带优先,其次系统常见路径)
func FindChineseFont(appBase string) string {
	candidates := []string{
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "simhei.ttf"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "msyh.ttf"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "msyh.ttc"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "simsun.ttc"),
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
		"/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/opentype/source-han-sans/SourceHanSansCN-Regular.otf",
		"/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
		"/usr/share/fonts/wqy-microhei/wqy-microhei.ttc",
	}
	appFont := filepath.Join(appBase, "fonts", "msyh.ttf")
	if _, err := os.Stat(appFont); err == nil {
		return appFont
	}
	for _, p := range candidates {
		if _, err := os.Stat(p); err == nil {
			return p
		}
	}
	return ""
}

// WrapText 按每行字符数(按 rune)折行
func WrapText(text string, charsPerLine int) []string {
	var lines []string
	for _, para := range strings.Split(text, "\n") {
		para = strings.TrimSpace(para)
		if para == "" {
			lines = append(lines, "")
			continue
		}
		runes := []rune(para)
		for i := 0; i < len(runes); i += charsPerLine {
			end := i + charsPerLine
			if end > len(runes) {
				end = len(runes)
			}
			lines = append(lines, string(runes[i:end]))
		}
	}
	return lines
}

// Generate 渲染报告PDF并返回字节流。font 缺失/加载失败返回错误。
func Generate(rd ReportData, appBase string) ([]byte, error) {
	fontPath := FindChineseFont(appBase)
	if fontPath == "" {
		return nil, fmt.Errorf("未找到中文字体")
	}
	proj := rd.Project
	pdf := gopdf.GoPdf{}
	pdf.Start(gopdf.Config{PageSize: gopdf.Rect{W: 595.28, H: 841.89}})
	pdf.AddPage()
	if err := pdf.AddTTFFont("msyh", fontPath); err != nil {
		return nil, fmt.Errorf("字体加载失败: %v", err)
	}
	pdf.SetFont("msyh", "", 12)

	ss := func(s string) string {
		if s == "" {
			return "—"
		}
		return s
	}
	sf := func(v *float64) string {
		if v == nil || *v == 0 {
			return "—"
		}
		return fmt.Sprintf("%.2f", *v)
	}

	writeTitle := func(text string, size float64) {
		pdf.SetFontSize(size)
		pdf.SetTextColor(107, 68, 35)
		pdf.Text(text)
	}
	writeKV := func(k, v string) {
		pdf.SetFontSize(11)
		pdf.SetTextColor(120, 120, 120)
		pdf.Text(k + "：")
		pdf.SetTextColor(30, 30, 30)
		pdf.Text(v)
		pdf.Br(18)
	}
	writeParagraph := func(text string, size float64) {
		pdf.SetFontSize(size)
		pdf.SetTextColor(40, 40, 40)
		for _, line := range WrapText(text, 38) {
			if pdf.GetY() > 790 { // 超出底部边距则分页（A4 高 841.89）
				pdf.AddPage()
				pdf.SetY(60)
			}
			pdf.SetX(60)
			pdf.Text(line)
			pdf.Br(size + 4)
		}
	}

	// 封面
	pdf.SetY(80)
	pdf.SetX(100)
	writeTitle("文物保护工程管理报告", 24)
	pdf.Br(20)
	pdf.SetX(100)
	pdf.SetFontSize(11)
	pdf.SetTextColor(150, 150, 150)
	pdf.Text("生成日期：" + time.Now().Format("2006年01月02日"))
	pdf.Br(40)

	// 一、基本信息
	pdf.SetX(60)
	writeTitle("一、工程基本信息", 14)
	pdf.Br(28)
	pdf.SetX(60)
	writeKV("工程名称", ss(proj.Name))
	writeKV("文物单位", ss(proj.UnitName))
	writeKV("保护级别", LevelName(rd.UnitLevel))
	writeKV("工程类型", ss(proj.Ptype))
	writeKV("当前状态", ss(proj.Status))
	writeKV("批复文号", ss(proj.ApprovalNo))
	writeKV("合同编号", ss(proj.ContractNo))
	writeKV("建设单位", ss(proj.OwnerUnit))
	writeKV("施工单位", ss(proj.ConstructionUnit)+"（资质："+ss(proj.ConstructionQual)+"）")
	writeKV("设计单位", ss(proj.DesignUnit)+"（资质："+ss(proj.DesignQual)+"）")
	writeKV("监理单位", ss(proj.SupervisionUnit)+"（资质："+ss(proj.SupervisionQual)+"）")

	// 二、资金
	pdf.Br(10)
	pdf.SetX(60)
	writeTitle("二、资金执行情况（万元）", 14)
	pdf.Br(28)
	pdf.SetX(60)
	cf := proj.CentralFunding
	tp := proj.TotalPaid
	rate := "—"
	if cf != nil && tp != nil && *cf > 0 {
		rate = fmt.Sprintf("%.1f%%", *tp / *cf * 100)
	}
	writeKV("中央指标", sf(proj.CentralFunding))
	writeKV("工程合同价", sf(proj.EngContract))
	writeKV("监理合同价", sf(proj.SupContract))
	writeKV("设计合同价", sf(proj.DesContract))
	writeKV("已支付合计", sf(proj.TotalPaid))
	writeKV("整体支付率", rate)
	writeKV("合同工期", ss(proj.SignDate)+" 至 "+ss(proj.ContractEnd))
	writeKV("实际完工", ss(proj.CompleteDate))

	// 三、档案完整度
	pdf.Br(10)
	pdf.SetX(60)
	writeTitle("三、档案完整度", 14)
	pdf.Br(28)
	pdf.SetX(60)
	writeKV("必备材料完整度", fmt.Sprintf("%d%%", rd.Completeness))
	if len(rd.MissingRequired) > 0 {
		writeKV("缺项材料", strings.Join(rd.MissingRequired, "、"))
	} else {
		writeKV("缺项材料", "无（全部齐全）")
	}

	// 四、智能分析
	pdf.AddPage()
	pdf.SetY(60)
	pdf.SetX(60)
	writeTitle("四、智能分析报告", 14)
	pdf.Br(28)
	pdf.SetX(60)
	pdf.SetFontSize(10)
	pdf.SetTextColor(150, 150, 150)
	pdf.Text("（以下分析由人工智能基于工程数据生成，供参考）")
	pdf.Br(24)
	pdf.SetX(60)
	writeParagraph(rd.Analysis, 11)

	return pdf.GetBytesPdfReturnErr()
}
