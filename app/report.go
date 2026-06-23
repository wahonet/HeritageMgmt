package main

// 工程报告PDF生成模块
// 使用gopdf生成中文PDF + 调用DeepSeek生成分析报告

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/signintech/gopdf"
)

func findChineseFont() string {
	candidates := []string{
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "simhei.ttf"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "msyh.ttf"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "msyh.ttc"),
		filepath.Join(os.Getenv("WINDIR"), "Fonts", "simsun.ttc"),
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

var levelNames = map[string]string{
	"国保":  "全国重点文物保护单位",
	"省保":  "省级文物保护单位",
	"市保":  "市级文物保护单位",
	"县保":  "县级文物保护单位",
	"未定级": "未定级文物点",
}

func levelNameGo(l string) string {
	if v, ok := levelNames[l]; ok {
		return v
	}
	if l == "" {
		return "未定级文物点"
	}
	return l
}

// generateAnalysis 调用DeepSeek生成分析报告
func generateAnalysis(proj *Project, data map[string]interface{}) (string, error) {
	if strings.TrimSpace(llmCfg.APIKey) == "" {
		return "（未配置大模型API密钥，分析报告功能不可用。）", nil
	}
	cf := proj.CentralFunding
	tp := proj.TotalPaid
	payRate := "—"
	if cf != nil && tp != nil && *cf > 0 {
		payRate = fmt.Sprintf("%.1f%%", *tp / *cf * 100)
	}
	docs, _ := data["documents"].([]Document)
	comp := data["completeness"].(int)
	miss, _ := data["missing_required"].([]string)
	qual, _ := data["qual_warnings"].([]string)

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
		proj.Name, proj.UnitName, levelNameGo(data["unit_level"].(string)),
		proj.Ptype, proj.Status, proj.ApprovalNo,
		proj.SignDate, proj.ContractEnd, proj.CompleteDate,
		fptr(cf), fptr(tp), payRate,
		proj.OwnerUnit,
		proj.ConstructionUnit, proj.ConstructionQual,
		proj.DesignUnit, proj.DesignQual,
		proj.SupervisionUnit, proj.SupervisionQual,
		len(docs), comp,
		strings.Join(miss, "、"),
		strings.Join(qual, "；"),
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

	payload := map[string]interface{}{
		"model": llmCfg.Model, "temperature": 0.3, "max_tokens": 2000,
		"messages": []map[string]string{{"role": "user", "content": prompt}},
	}
	body, _ := json.Marshal(payload)
	url := strings.TrimRight(llmCfg.BaseURL, "/") + "/chat/completions"
	req, _ := http.NewRequest("POST", url, bytes.NewReader(body))
	req.Header.Set("Authorization", "Bearer "+llmCfg.APIKey)
	req.Header.Set("Content-Type", "application/json")
	to := 120 * time.Second
	if llmCfg.TimeoutSeconds > 0 {
		to = time.Duration(llmCfg.TimeoutSeconds) * time.Second
	}
	resp, err := (&http.Client{Timeout: to}).Do(req)
	if err != nil {
		return "", fmt.Errorf("调用大模型失败: %v", err)
	}
	defer resp.Body.Close()
	rbody, _ := io.ReadAll(resp.Body)
	if resp.StatusCode != 200 {
		return "", fmt.Errorf("大模型返回 %d", resp.StatusCode)
	}
	var r struct {
		Choices []struct {
			Message struct {
				Content string `json:"content"`
			} `json:"message"`
		} `json:"choices"`
	}
	json.Unmarshal(rbody, &r)
	if len(r.Choices) == 0 {
		return "", fmt.Errorf("大模型返回空")
	}
	return strings.TrimSpace(r.Choices[0].Message.Content), nil
}

func fptr(v *float64) float64 {
	if v == nil {
		return 0
	}
	return *v
}

// handleReportPDF 生成工程报告PDF
func handleReportPDF(w http.ResponseWriter, r *http.Request) {
	pid, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	data := analyzeProject(pid)
	if data == nil {
		http.NotFound(w, r)
		return
	}

	fontPath := findChineseFont()
	if fontPath == "" {
		writeJSON(w, map[string]interface{}{"error": "未找到中文字体"})
		return
	}

	proj := data["project"].(*Project)
	pdf := gopdf.GoPdf{}
	pdf.Start(gopdf.Config{PageSize: gopdf.Rect{W: 595.28, H: 841.89}})
	pdf.AddPage()
	if err := pdf.AddTTFFont("msyh", fontPath); err != nil {
		writeJSON(w, map[string]interface{}{"error": "字体加载失败: " + err.Error()})
		return
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
		for _, line := range wrapText(text, 38) {
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
	writeKV("保护级别", levelNameGo(data["unit_level"].(string)))
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
	writeKV("必备材料完整度", fmt.Sprintf("%d%%", data["completeness"].(int)))
	miss, _ := data["missing_required"].([]string)
	if len(miss) > 0 {
		writeKV("缺项材料", strings.Join(miss, "、"))
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
	analysis, err := generateAnalysis(proj, data)
	if err != nil {
		analysis = "分析报告生成失败：" + err.Error()
	}
	writeParagraph(analysis, 11)

	// 输出
	filename := fmt.Sprintf("报告_%s_%s.pdf", strings.ReplaceAll(proj.Name, "/", "_"), time.Now().Format("20060102"))
	w.Header().Set("Content-Type", "application/pdf")
	w.Header().Set("Content-Disposition", "attachment; filename*=UTF-8''"+urlEncode(filename))
	pdf.Write(w)
	InsertLog("导出报告", fmt.Sprintf("工程#%d %s", pid, proj.Name), filename)
}

func wrapText(text string, charsPerLine int) []string {
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
