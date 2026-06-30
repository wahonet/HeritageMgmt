package httpapi

// HTTP 响应辅助：统一 JSON 输出与文件名编码。

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"strings"
)

// writeJSON 以 UTF-8 JSON 写出响应。
func writeJSON(w http.ResponseWriter, v interface{}) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	json.NewEncoder(w).Encode(v)
}

// writeErr 写出统一的失败响应：{"ok":false,"error":...}
func writeErr(w http.ResponseWriter, msg string) {
	writeJSON(w, map[string]interface{}{"ok": false, "error": msg})
}

// writeInternal 记录内部错误日志并返回统一友好消息（不向前端泄露 SQL/路径等内部细节）。
func writeInternal(w http.ResponseWriter, err error) {
	log.Println("internal error:", err)
	writeErr(w, "服务器内部错误")
}

// urlEncode 对文件名等做百分号编码（用于 Content-Disposition）。
func urlEncode(s string) string {
	var b strings.Builder
	for _, c := range []byte(s) {
		if (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' {
			b.WriteByte(c)
		} else {
			b.WriteString(fmt.Sprintf("%%%02X", c))
		}
	}
	return b.String()
}
