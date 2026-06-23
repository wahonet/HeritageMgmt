package main

// 数据层：SQLite 表结构与查询（纯Go驱动 modernc.org/sqlite，无CGO，便于交叉编译到国产系统）

import (
	"database/sql"
	"os"
	"path/filepath"
	"time"

	_ "modernc.org/sqlite"
)

var (
	appBase     string
	dataDir     string
	projectsDir string
	dbPath      string
	db          *sql.DB
)

// initPaths 解析数据/配置目录，优先用可执行文件同级的目录
func initPaths() {
	base := "."
	if exe, err := os.Executable(); err == nil {
		base = filepath.Dir(exe)
		// go run 时 exe 在临时目录，回退到工作目录
		if _, e := os.Stat(filepath.Join(base, "config")); e != nil {
			if cwd, e2 := os.Getwd(); e2 == nil {
				if _, e3 := os.Stat(filepath.Join(cwd, "config")); e3 == nil {
					base = cwd
				}
			}
		}
	}
	appBase = base
	dataDir = filepath.Join(base, "data")
	projectsDir = filepath.Join(dataDir, "projects")
	dbPath = filepath.Join(dataDir, "heritage.db")
	os.MkdirAll(dataDir, 0755)
	os.MkdirAll(projectsDir, 0755)
}

const schema = `
CREATE TABLE IF NOT EXISTS units (
    id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT UNIQUE NOT NULL,
    level TEXT, category TEXT, intro TEXT, sort INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS projects (
    id INTEGER PRIMARY KEY AUTOINCREMENT, unit_id INTEGER NOT NULL, seq INTEGER,
    name TEXT NOT NULL, ptype TEXT, approval_no TEXT,
    sign_date TEXT, complete_date TEXT, accept_date TEXT, contract_end TEXT,
    owner_unit TEXT, contract_no TEXT, contract_sign_date TEXT,
    central_funding REAL, eng_contract REAL, eng_paid REAL,
    sup_contract REAL, sup_paid REAL, des_contract REAL, des_paid REAL,
    expert_fee REAL, total_paid REAL, status TEXT, progress_note TEXT,
    source_dir TEXT, folder TEXT, created TEXT,
    construction_unit TEXT, construction_qual TEXT,
    design_unit TEXT, design_qual TEXT,
    supervision_unit TEXT, supervision_qual TEXT,
    FOREIGN KEY(unit_id) REFERENCES units(id));
CREATE TABLE IF NOT EXISTS documents (
    id INTEGER PRIMARY KEY AUTOINCREMENT, project_id INTEGER NOT NULL,
    doc_type TEXT NOT NULL, doc_type_name TEXT, title TEXT, orig_name TEXT,
    file_path TEXT NOT NULL, file_ext TEXT, file_size INTEGER, uploaded TEXT,
    source TEXT DEFAULT 'import',
    FOREIGN KEY(project_id) REFERENCES projects(id) ON DELETE CASCADE);
CREATE TABLE IF NOT EXISTS logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, action TEXT, target TEXT, detail TEXT);
CREATE INDEX IF NOT EXISTS idx_doc_project ON documents(project_id);
CREATE INDEX IF NOT EXISTS idx_proj_unit ON projects(unit_id);
CREATE INDEX IF NOT EXISTS idx_logs_ts ON logs(ts);
`

// OpenDB 打开并初始化数据库，并对旧库做字段迁移
func OpenDB() error {
	d, err := sql.Open("sqlite", dbPath)
	if err != nil {
		return err
	}
	d.Exec("PRAGMA foreign_keys = ON")
	d.Exec("PRAGMA journal_mode = WAL")
	if _, err := d.Exec(schema); err != nil {
		return err
	}
	db = d
	migrate()
	return nil
}

// migrate 为旧版本数据库补齐新增字段
func migrate() {
	cols := tableColumns("projects")
	add := []string{
		"construction_unit TEXT", "construction_qual TEXT",
		"design_unit TEXT", "design_qual TEXT",
		"supervision_unit TEXT", "supervision_qual TEXT",
		"contract_end TEXT", "owner_unit TEXT", "contract_no TEXT", "contract_sign_date TEXT",
		"deleted INTEGER DEFAULT 0",
	}
	for _, c := range add {
		name := c[:idxSpace(c)]
		if !cols[name] {
			db.Exec("ALTER TABLE projects ADD COLUMN " + c)
		}
	}
}
func idxSpace(s string) int {
	for i, c := range s {
		if c == ' ' {
			return i
		}
	}
	return len(s)
}
func tableColumns(table string) map[string]bool {
	m := map[string]bool{}
	rows, err := db.Query("PRAGMA table_info(" + table + ")")
	if err != nil {
		return m
	}
	defer rows.Close()
	for rows.Next() {
		var cid, notnull, pk int
		var name, ctype string
		var dflt sql.NullString
		rows.Scan(&cid, &name, &ctype, &notnull, &dflt, &pk)
		m[name] = true
	}
	return m
}

// resetTables 清空工程/单位/文档/日志并重置自增序列
func resetTables(tx *sql.Tx) error {
	for _, t := range []string{"documents", "projects", "units"} {
		if _, err := tx.Exec("DELETE FROM " + t); err != nil {
			return err
		}
	}
	for _, t := range []string{"documents", "projects", "units"} {
		tx.Exec("DELETE FROM sqlite_sequence WHERE name=?", t)
	}
	return nil
}

// ---- 结构体 ----

type Unit struct {
	ID       int64  `json:"id"`
	Name     string `json:"name"`
	Level    string `json:"level"`
	Category string `json:"category"`
	Sort     int    `json:"sort"`
}

type Project struct {
	ID               int64    `json:"id"`
	UnitID           int64    `json:"unit_id"`
	Seq              *int64   `json:"seq"`
	Name             string   `json:"name"`
	Ptype            string   `json:"ptype"`
	ApprovalNo       string   `json:"approval_no"`
	SignDate         string   `json:"sign_date"`
	CompleteDate     string   `json:"complete_date"`
	AcceptDate       string   `json:"accept_date"`
	ContractEnd      string   `json:"contract_end"`
	OwnerUnit        string   `json:"owner_unit"`
	ContractNo       string   `json:"contract_no"`
	ContractSignDate string   `json:"contract_sign_date"`
	CentralFunding   *float64 `json:"central_funding"`
	EngContract      *float64 `json:"eng_contract"`
	EngPaid          *float64 `json:"eng_paid"`
	SupContract      *float64 `json:"sup_contract"`
	SupPaid          *float64 `json:"sup_paid"`
	DesContract      *float64 `json:"des_contract"`
	DesPaid          *float64 `json:"des_paid"`
	ExpertFee        *float64 `json:"expert_fee"`
	TotalPaid        *float64 `json:"total_paid"`
	Status           string   `json:"status"`
	ProgressNote     string   `json:"progress_note"`
	SourceDir        string   `json:"source_dir"`
	Folder           string   `json:"folder"`
	Created          string   `json:"created"`
	// 参建单位与资质
	ConstructionUnit string `json:"construction_unit"`
	ConstructionQual string `json:"construction_qual"`
	DesignUnit       string `json:"design_unit"`
	DesignQual       string `json:"design_qual"`
	SupervisionUnit  string `json:"supervision_unit"`
	SupervisionQual  string `json:"supervision_qual"`
	// 附带展示字段
	UnitName string `json:"unit_name,omitempty"`
	DocCount int    `json:"doc_count,omitempty"`
}

type Document struct {
	ID          int64  `json:"id"`
	ProjectID   int64  `json:"project_id"`
	DocType     string `json:"doc_type"`
	DocTypeName string `json:"doc_type_name"`
	Title       string `json:"title"`
	OrigName    string `json:"orig_name"`
	FilePath    string `json:"file_path"`
	FileExt     string `json:"file_ext"`
	FileSize    int64  `json:"file_size"`
	Uploaded    string `json:"uploaded"`
	Source      string `json:"source"`
}

type LogEntry struct {
	ID     int64  `json:"id"`
	Ts     string `json:"ts"`
	Action string `json:"action"`
	Target string `json:"target"`
	Detail string `json:"detail"`
}

// 项目列顺序常量（与 schema 一致）
const projectCols = `id,unit_id,seq,name,ptype,approval_no,sign_date,complete_date,accept_date,
central_funding,eng_contract,eng_paid,sup_contract,sup_paid,des_contract,des_paid,
expert_fee,total_paid,status,progress_note,source_dir,folder,created,
construction_unit,construction_qual,design_unit,design_qual,supervision_unit,supervision_qual,contract_end,
owner_unit,contract_no,contract_sign_date`

// scanProject 将一行扫描为 Project
func scanProject(rs interface{ Scan(...interface{}) error }) (*Project, error) {
	var p Project
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

// ---- 查询 ----

func ListUnits() ([]Unit, error) {
	rows, err := db.Query("SELECT id,name,level,category,sort FROM units ORDER BY sort,id")
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []Unit
	for rows.Next() {
		var u Unit
		rows.Scan(&u.ID, &u.Name, &u.Level, &u.Category, &u.Sort)
		out = append(out, u)
	}
	return out, nil
}

func unitLevel(unitID int64) string {
	var lvl string
	db.QueryRow("SELECT level FROM units WHERE id=?", unitID).Scan(&lvl)
	return lvl
}

func GetProject(id int64) (*Project, error) {
	row := db.QueryRow("SELECT "+projectCols+" FROM projects WHERE id=?", id)
	return scanProject(row)
}

func ListProjects(unitID int64, status, q string) ([]Project, error) {
	sqlStr := "SELECT " + projectCols + " FROM projects WHERE COALESCE(deleted,0)=0"
	var args []interface{}
	if unitID > 0 {
		sqlStr += " AND unit_id=?"
		args = append(args, unitID)
	}
	if status != "" {
		sqlStr += " AND status=?"
		args = append(args, status)
	}
	if q != "" {
		sqlStr += " AND (name LIKE ? OR approval_no LIKE ?)"
		args = append(args, "%"+q+"%", "%"+q+"%")
	}
	sqlStr += " ORDER BY unit_id,seq,id"
	rows, err := db.Query(sqlStr, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []Project
	for rows.Next() {
		p, err := scanProject(rows)
		if err != nil {
			return nil, err
		}
		out = append(out, *p)
	}
	return out, nil
}

func ListDocuments(projectID int64) ([]Document, error) {
	rows, err := db.Query("SELECT id,project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source FROM documents WHERE project_id=? ORDER BY id", projectID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []Document
	for rows.Next() {
		var d Document
		rows.Scan(&d.ID, &d.ProjectID, &d.DocType, &d.DocTypeName, &d.Title, &d.OrigName,
			&d.FilePath, &d.FileExt, &d.FileSize, &d.Uploaded, &d.Source)
		out = append(out, d)
	}
	return out, nil
}

func CountDocs(projectID int64) int {
	var n int
	db.QueryRow("SELECT COUNT(*) FROM documents WHERE project_id=?", projectID).Scan(&n)
	return n
}

// ---- 操作日志 ----

func InsertLog(action, target, detail string) {
	if db == nil {
		return
	}
	db.Exec("INSERT INTO logs(ts,action,target,detail) VALUES(?,?,?,?)",
		time.Now().Format("2006-01-02 15:04:05"), action, target, detail)
}

func ListLogs(limit int) ([]LogEntry, error) {
	rows, err := db.Query("SELECT id,ts,action,target,detail FROM logs ORDER BY id DESC LIMIT ?", limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []LogEntry
	for rows.Next() {
		var l LogEntry
		rows.Scan(&l.ID, &l.Ts, &l.Action, &l.Target, &l.Detail)
		out = append(out, l)
	}
	return out, nil
}

// SoftDeleteProject 软删除工程(放入回收站)
func SoftDeleteProject(id int64) error {
	_, err := db.Exec("UPDATE projects SET deleted=1 WHERE id=?", id)
	return err
}

// MoveProjectToRecycle 移动工程文件夹到回收站
func MoveProjectToRecycle(id int64) error {
	proj, err := GetProject(id)
	if err != nil || proj == nil || proj.Folder == "" {
		return nil
	}
	src := filepath.Join(projectsDir, proj.Folder)
	if _, err := os.Stat(src); err != nil {
		return nil // 文件夹不存在也无妨
	}
	recycleDir := filepath.Join(dataDir, "recycle")
	os.MkdirAll(recycleDir, 0755)
	dst := filepath.Join(recycleDir, proj.Folder)
	return os.Rename(src, dst)
}

// ---- P3: 为 handlers.go 提供封装查询，避免绕过数据层 ----

func UnitName(unitID int64) string {
	var name string
	db.QueryRow("SELECT name FROM units WHERE id=?", unitID).Scan(&name)
	return name
}

func ProjectName(id int64) string {
	var name string
	db.QueryRow("SELECT name FROM projects WHERE id=?", id).Scan(&name)
	return name
}

// DocByID 按ID查文档
func DocByID(id int64) (*Document, error) {
	var d Document
	err := db.QueryRow("SELECT id,project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source FROM documents WHERE id=?", id).
		Scan(&d.ID, &d.ProjectID, &d.DocType, &d.DocTypeName, &d.Title, &d.OrigName, &d.FilePath, &d.FileExt, &d.FileSize, &d.Uploaded, &d.Source)
	if err != nil {
		return nil, err
	}
	return &d, nil
}

// UnitStats 查某单位的工程数/文档数/中央指标合计
func UnitStats(unitID int64) (projCount, docCount int, funding float64) {
	db.QueryRow("SELECT COUNT(*) FROM projects WHERE unit_id=?", unitID).Scan(&projCount)
	db.QueryRow("SELECT COUNT(*) FROM documents d JOIN projects p ON p.id=d.project_id WHERE p.unit_id=?", unitID).Scan(&docCount)
	db.QueryRow("SELECT COALESCE(SUM(central_funding),0) FROM projects WHERE unit_id=?", unitID).Scan(&funding)
	return
}

// ProjectDocTypes 查某工程已有的文档类型集合
func ProjectDocTypes(pid int64) map[string]bool {
	m := map[string]bool{}
	rows, _ := db.Query("SELECT DISTINCT doc_type FROM documents WHERE project_id=?", pid)
	if rows != nil {
		defer rows.Close()
		for rows.Next() {
			var c string
			rows.Scan(&c)
			m[c] = true
		}
	}
	return m
}

// UpdateProjectFields 按字段更新工程
func UpdateProjectFields(id int64, sets string, args ...interface{}) error {
	_, err := db.Exec("UPDATE projects SET "+sets+" WHERE id=?", args...)
	return err
}

// DeleteDocument 删除文档记录(不删文件)
func DeleteDocument(id int64) {
	db.Exec("DELETE FROM documents WHERE id=?", id)
}
