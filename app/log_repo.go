package main

// 数据层：操作日志(logs)。

import (
	"time"

	"heritage-mgmt/internal/domain"
)

func InsertLog(action, target, detail string) {
	if db == nil {
		return
	}
	db.Exec("INSERT INTO logs(ts,action,target,detail) VALUES(?,?,?,?)",
		time.Now().Format("2006-01-02 15:04:05"), action, target, detail)
}

func ListLogs(limit int) ([]domain.LogEntry, error) {
	rows, err := db.Query("SELECT id,ts,action,target,detail FROM logs ORDER BY id DESC LIMIT ?", limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	var out []domain.LogEntry
	for rows.Next() {
		var l domain.LogEntry
		rows.Scan(&l.ID, &l.Ts, &l.Action, &l.Target, &l.Detail)
		out = append(out, l)
	}
	return out, nil
}
