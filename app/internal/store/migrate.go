package store

// 数据层：表结构定义、旧库字段迁移、整库重置。

import (
	"database/sql"
	"fmt"
)

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

// migrate 为旧版本数据库补齐新增字段；任一字段添加失败立即返回错误（旧版静默吞错会导致
// 后续查询失败、列表为空等运行时问题）。
func (s *Store) migrate() error {
	cols, err := s.tableColumns("projects")
	if err != nil {
		return err
	}
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
			if _, err := s.db.Exec("ALTER TABLE projects ADD COLUMN " + c); err != nil {
				return fmt.Errorf("迁移字段 %s 失败: %w", name, err)
			}
		}
	}
	return nil
}

func idxSpace(s string) int {
	for i, c := range s {
		if c == ' ' {
			return i
		}
	}
	return len(s)
}

func (s *Store) tableColumns(table string) (map[string]bool, error) {
	m := map[string]bool{}
	rows, err := s.db.Query("PRAGMA table_info(" + table + ")")
	if err != nil {
		return m, err
	}
	defer rows.Close()
	for rows.Next() {
		var cid, notnull, pk int
		var name, ctype string
		var dflt sql.NullString
		if err := rows.Scan(&cid, &name, &ctype, &notnull, &dflt, &pk); err != nil {
			return m, err
		}
		m[name] = true
	}
	return m, rows.Err()
}

// ResetTables 清空工程/单位/文档并重置自增序列（供导入主流程在事务内调用）。
func ResetTables(tx *sql.Tx) error {
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
