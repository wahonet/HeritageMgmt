package store

// 数据层：连接与生命周期。
// Store 是唯一持有 *sql.DB 的地方；所有 *_repo.go 均为其方法，Store 同时满足
// ProjectRepository/UnitRepository/DocumentRepository/LogRepository 四个接口。
// 纯Go驱动 modernc.org/sqlite，无CGO，便于交叉编译到国产系统。

import (
	"database/sql"
	"fmt"

	"heritage-mgmt/internal/config"

	_ "modernc.org/sqlite"
)

// Store 封装数据库连接与生命周期。其 db 字段可被 Reopen 替换（供数据恢复使用），
// 故所有 service 共享同一个 *Store 指针，方法在调用时读取 s.db。
type Store struct {
	db *sql.DB
}

// NewStore 打开并初始化数据库（建表 + 旧库字段迁移）。
func NewStore(cfg *config.Config) (*Store, error) {
	d, err := openSQLite(cfg.DBPath)
	if err != nil {
		return nil, err
	}
	if _, err := d.Exec(schema); err != nil {
		d.Close()
		return nil, err
	}
	s := &Store{db: d}
	if err := s.migrate(); err != nil {
		d.Close()
		return nil, err
	}
	return s, nil
}

// openSQLite 打开并配置连接：单连接（离线单机优先一致性，避免连接级 PRAGMA 失效与锁竞争）、
// WAL 模式、busy_timeout=5s、外键约束。任一 PRAGMA 失败立即报错（旧版静默忽略会导致
// 外键级联/锁等待行为在不同请求间不一致）。
func openSQLite(path string) (*sql.DB, error) {
	d, err := sql.Open("sqlite", path)
	if err != nil {
		return nil, err
	}
	d.SetMaxOpenConns(1)
	d.SetMaxIdleConns(1)
	for _, q := range []string{
		"PRAGMA foreign_keys = ON",
		"PRAGMA journal_mode = WAL",
		"PRAGMA busy_timeout = 5000",
	} {
		if _, err := d.Exec(q); err != nil {
			d.Close()
			return nil, fmt.Errorf("%s 失败: %w", q, err)
		}
	}
	return d, nil
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
func (s *Store) Reopen(cfg *config.Config) error {
	if s.db != nil {
		s.db.Close()
		s.db = nil
	}
	d, err := openSQLite(cfg.DBPath)
	if err != nil {
		return err
	}
	if _, err := d.Exec(schema); err != nil {
		return err
	}
	s.db = d
	return s.migrate()
}
