package store

// 数据层：文档(documents)相关查询与维护（均为 *Store 方法，满足 DocumentRepository）。

import "heritage-mgmt/internal/domain"

func (s *Store) ListDocuments(projectID int64) ([]domain.Document, error) {
	rows, err := s.db.Query("SELECT id,project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source FROM documents WHERE project_id=? ORDER BY id", projectID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []domain.Document
	for rows.Next() {
		var d domain.Document
		rows.Scan(&d.ID, &d.ProjectID, &d.DocType, &d.DocTypeName, &d.Title, &d.OrigName,
			&d.FilePath, &d.FileExt, &d.FileSize, &d.Uploaded, &d.Source)
		out = append(out, d)
	}
	return out, nil
}

func (s *Store) CountDocs(projectID int64) int {
	var n int
	s.db.QueryRow("SELECT COUNT(*) FROM documents WHERE project_id=?", projectID).Scan(&n)
	return n
}

// DocByID 按ID查文档
func (s *Store) DocByID(id int64) (*domain.Document, error) {
	var d domain.Document
	err := s.db.QueryRow("SELECT id,project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source FROM documents WHERE id=?", id).
		Scan(&d.ID, &d.ProjectID, &d.DocType, &d.DocTypeName, &d.Title, &d.OrigName, &d.FilePath, &d.FileExt, &d.FileSize, &d.Uploaded, &d.Source)
	if err != nil {
		return nil, err
	}
	return &d, nil
}

// DeleteDocument 删除文档记录(不删文件)
func (s *Store) DeleteDocument(id int64) {
	s.db.Exec("DELETE FROM documents WHERE id=?", id)
}

// InsertDocument 插入一条文档记录
func (s *Store) InsertDocument(d domain.Document) error {
	_, err := s.db.Exec(`INSERT INTO documents(project_id,doc_type,doc_type_name,title,orig_name,file_path,file_ext,file_size,uploaded,source) VALUES(?,?,?,?,?,?,?,?,?,?)`,
		d.ProjectID, d.DocType, d.DocTypeName, d.Title, d.OrigName, d.FilePath, d.FileExt, d.FileSize, d.Uploaded, d.Source)
	return err
}

// DeleteDocsByType 删除某工程某分类的全部文档记录，返回删除条数
func (s *Store) DeleteDocsByType(pid int64, docType string) (int64, error) {
	res, err := s.db.Exec("DELETE FROM documents WHERE project_id=? AND doc_type=?", pid, docType)
	if err != nil {
		return 0, err
	}
	return res.RowsAffected()
}
