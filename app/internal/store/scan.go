package store

// 数据层：行扫描辅助（projects 列顺序、NULL 处理）。

import (
	"database/sql"

	"heritage-mgmt/internal/domain"
)

// 项目列顺序常量（与 schema 一致）
const projectCols = `id,unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,
central_funding,eng_contract,eng_paid,sup_contract,sup_paid,des_contract,des_paid,
expert_fee,total_paid,status,progress_note,source_dir,folder,created,
construction_unit,construction_qual,design_unit,design_qual,supervision_unit,supervision_qual,contract_end,
owner_unit,contract_no,contract_sign_date`

// scanProject 将一行扫描为 domain.Project
func scanProject(rs interface{ Scan(...interface{}) error }) (*domain.Project, error) {
	var p domain.Project
	var seq sql.NullInt64
	var name, ptype, ap, sd, cd, ad, st, pn, src, fol, cr sql.NullString
	var cu, cq, du, dq, su, sq, cend, own, cno, csd sql.NullString
	var cf, ec, ep, suc, sup, dec, dep, ef, tp sql.NullFloat64
	err := rs.Scan(&p.ID, &p.UnitID, &seq, &name, &ptype, &ap, &sd, &cd, &ad,
		&cf, &ec, &ep, &suc, &sup, &dec, &dep, &ef, &tp, &st, &pn, &src, &fol, &cr,
		&cu, &cq, &du, &dq, &su, &sq, &cend, &own, &cno, &csd)
	if err != nil {
		return nil, err
	}
	if seq.Valid {
		v := seq.Int64
		p.Seq = &v
	}
	p.Name = name.String
	p.Ptype = ns(ptype)
	p.ApprovalNo = ns(ap)
	p.SignDate = ns(sd)
	p.CompleteDate = ns(cd)
	p.AcceptDate = ns(ad)
	p.CentralFunding = nf(cf)
	p.EngContract = nf(ec)
	p.EngPaid = nf(ep)
	p.SupContract = nf(suc)
	p.SupPaid = nf(sup)
	p.DesContract = nf(dec)
	p.DesPaid = nf(dep)
	p.ExpertFee = nf(ef)
	p.TotalPaid = nf(tp)
	p.Status = ns(st)
	p.ProgressNote = ns(pn)
	p.SourceDir = ns(src)
	p.Folder = ns(fol)
	p.Created = ns(cr)
	p.ConstructionUnit = ns(cu)
	p.ConstructionQual = ns(cq)
	p.DesignUnit = ns(du)
	p.DesignQual = ns(dq)
	p.SupervisionUnit = ns(su)
	p.SupervisionQual = ns(sq)
	p.ContractEnd = ns(cend)
	p.OwnerUnit = ns(own)
	p.ContractNo = ns(cno)
	p.ContractSignDate = ns(csd)
	return &p, nil
}

func ns(s sql.NullString) string {
	if s.Valid {
		return s.String
	}
	return ""
}
func nf(s sql.NullFloat64) *float64 {
	if s.Valid {
		return &s.Float64
	}
	return nil
}
