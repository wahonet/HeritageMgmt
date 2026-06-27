package main

// HTTP 处理：Excel 台账导出。仅做序列化/写入；取数经仓储接口与分析服务。

import (
	"log"
	"net/http"
	"strconv"
	"strings"

	"github.com/xuri/excelize/v2"
)

func (s *Server) handleExportLedger(w http.ResponseWriter, r *http.Request) {
	f := excelize.NewFile()
	sheet := "文物保护工程台账"
	f.SetSheetName(f.GetSheetName(0), sheet)
	headers := []string{
		"序号", "文物单位", "工程名称", "工程类型", "批复文号", "状态",
		"开工日期", "合同约定竣工日期", "实际完工日期",
		"中央指标(万)", "工程合同(万)", "工程已付(万)",
		"监理合同(万)", "监理已付(万)", "设计合同(万)", "设计已付(万)",
		"专家费(万)", "已付合计(万)",
		"完整度%", "缺项", "文档数",
		"施工单位", "施工资质", "设计单位", "设计资质", "监理单位", "监理资质",
		"项目进展情况",
	}
	for i, h := range headers {
		cell, _ := excelize.CoordinatesToCellName(i+1, 1)
		f.SetCellValue(sheet, cell, h)
	}
	hstyle, _ := f.NewStyle(&excelize.Style{
		Font: &excelize.Font{Bold: true, Color: "5C3A1A"},
		Fill: excelize.Fill{Type: "pattern", Color: []string{"#E6D9BE"}, Pattern: 1},
	})
	f.SetCellStyle(sheet, "A1", "AB1", hstyle)
	projects, _ := s.projects.ListProjects(0, "", "")
	uname := map[int64]string{}
	units, _ := s.units.ListUnits()
	for _, u := range units {
		uname[u.ID] = u.Name
	}
	row := 2
	var tFund, tEng, tPaid float64
	for _, p := range projects {
		d := s.proj.Analyze(p.ID)
		seq := ""
		if p.Seq != nil {
			seq = strconv.FormatInt(*p.Seq, 10)
		}
		vals := []interface{}{
			seq, uname[p.UnitID], p.Name, p.Ptype, p.ApprovalNo, p.Status,
			p.SignDate, p.ContractEnd, p.CompleteDate,
			fval(p.CentralFunding), fval(p.EngContract), fval(p.EngPaid),
			fval(p.SupContract), fval(p.SupPaid), fval(p.DesContract), fval(p.DesPaid),
			fval(p.ExpertFee), fval(p.TotalPaid),
			d.Completeness, strings.Join(d.MissingRequired, "、"), s.docs.CountDocs(p.ID),
			p.ConstructionUnit, p.ConstructionQual, p.DesignUnit, p.DesignQual,
			p.SupervisionUnit, p.SupervisionQual, p.ProgressNote,
		}
		for i, v := range vals {
			cell, _ := excelize.CoordinatesToCellName(i+1, row)
			f.SetCellValue(sheet, cell, v)
		}
		if p.CentralFunding != nil {
			tFund += *p.CentralFunding
		}
		if p.EngContract != nil {
			tEng += *p.EngContract
		}
		if p.TotalPaid != nil {
			tPaid += *p.TotalPaid
		}
		row++
	}
	totals := []interface{}{"合计", "", "", "", "", "", "", "", "", tFund, tEng, "", "", "", "", "", "", tPaid, "", "", "", "", "", "", "", "", "", ""}
	for i, v := range totals {
		cell, _ := excelize.CoordinatesToCellName(i+1, row)
		f.SetCellValue(sheet, cell, v)
	}
	tstyle, _ := f.NewStyle(&excelize.Style{Font: &excelize.Font{Bold: true}})
	f.SetCellStyle(sheet, "A"+strconv.Itoa(row), "AB"+strconv.Itoa(row), tstyle)
	f.SetColWidth(sheet, "C", "C", 40)
	f.SetColWidth(sheet, "B", "B", 14)

	w.Header().Set("Content-Type", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet")
	w.Header().Set("Content-Disposition", "attachment; filename*=UTF-8''"+urlEncode("文物保护工程台账.xlsx"))
	if err := f.Write(w); err != nil {
		log.Println("导出Excel失败:", err)
	}
}

func fval(p *float64) interface{} {
	if p == nil {
		return ""
	}
	return *p
}
