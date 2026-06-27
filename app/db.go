package main

// 数据层：连接与生命周期。
// Store 是唯一持有 *sql.DB 的地方；所有 *_repo.go 均为其方法，Store 同时满足
// ProjectRepository/UnitRepository/DocumentRepository/LogRepository 四个接口。
// 纯Go驱动 modernc.org/sqlite，无CGO，便于交叉编译到国产系统。

import (
	"database/sql"

	_ "modernc.org/sqlite"
)

// Store 封装数据库连接与生命周期。其 db 字段可被 Reopen 替换（供数据恢复使用），
// 故所有 service 共享同一个 *Store 指针，方法在调用时读取 s.db。
type Store struct {
	db *sql.DB
}

// NewStore 打开并初始化数据库（建表 + 旧库字段迁移）。
func NewStore(cfg *Config) (*Store, error) {
	d, err := sql.Open("sqlite", cfg.DBPath)
	if err != nil {
		return nil, err
	}
	d.Exec("PRAGMA foreign_keys = ON")
	d.Exec("PRAGMA journal_mode = WAL")
	if _, err := d.Exec(schema); err != nil {
		d.Close()
		return nil, err
	}
	s := &Store{db: d}
	s.migrate()
	return s, nil
}

// Begin 开启一个事务（供导入主流程批量写入）。
func (s *Store) Begin() (*sql.Tx, error) { return s.db.Begin() }

// Checkpoint 把 WAL 写回主库（供备份前调用）。
func (s *Store) Checkpoint() { s.db.Exec("PRAGMA wal_checkpoint(TRUNCATE)") }

// Close 关闭连接并置空，保证后续 Reopen 能重新打开（多次调用安全）。
func (s *Store) Close() error {
	if s.db == nil {
		return nil
	}
	err := s.db.Close()
	s.db = nil
	return err
}

// Reopen 关闭当前连接并以同一路径重新打开、建表、迁移（供数据恢复后重启使用）。
func (s *Store) Reopen(cfg *Config) error {
	if s.db != nil {
		s.db.Close()
		s.db = nil
	}
	d, err := sql.Open("sqlite", cfg.DBPath)
	if err != nil {
		return err
	}
	d.Exec("PRAGMA foreign_keys = ON")
	d.Exec("PRAGMA journal_mode = WAL")
	if _, err := d.Exec(schema); err != nil {
		return err
	}
	s.db = d
	s.migrate()
	return nil
}
