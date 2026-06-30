package httpapi

// 路径安全：防止上传/下载/删除/解压时的目录穿越（path traversal / zip slip）。
// 即便是离线单机系统也需防护——被注入的 DB 记录或恶意备份包可借 "../" 越出 data/projects，
// 造成任意文件读取/覆盖/删除。所有用户可控的路径拼接一律经过本文件的校验。

import (
	"fmt"
	"os"
	"path"
	"path/filepath"
	"strings"
	"unicode/utf8"
)

// safeJoin 把 elems 拼接到 root 下，并校验结果仍落在 root 目录树内（防 "../" 与绝对路径穿越）。
// 返回最终绝对路径；越界或异常时返回错误。
func safeJoin(root string, elems ...string) (string, error) {
	rootAbs, err := filepath.Abs(root)
	if err != nil {
		return "", err
	}
	targetAbs, err := filepath.Abs(filepath.Join(append([]string{root}, elems...)...))
	if err != nil {
		return "", err
	}
	rel, err := filepath.Rel(rootAbs, targetAbs)
	if err != nil {
		return "", err
	}
	// rel == ".." 或以 "../" 开头表示目标在 root 之外；IsAbs 处理跨盘/绝对路径情况。
	if rel == ".." || strings.HasPrefix(rel, ".."+string(os.PathSeparator)) || filepath.IsAbs(rel) {
		return "", fmt.Errorf("非法路径")
	}
	return targetAbs, nil
}

// cleanUploadName 清洗上传文件名：取 basename、拒绝路径分隔符与控制字符、限长。
// 防止 multipart filename 形如 "../../etc/foo" 或含空字节造成的穿越与污染。
func cleanUploadName(name string) (string, error) {
	name = strings.ReplaceAll(name, "\\", "/")
	name = path.Base(name)
	name = strings.TrimSpace(name)
	if name == "" || name == "." || name == ".." {
		return "", fmt.Errorf("非法文件名")
	}
	if strings.ContainsAny(name, `/\`) {
		return "", fmt.Errorf("非法文件名")
	}
	if !utf8.ValidString(name) || strings.ContainsRune(name, 0) {
		return "", fmt.Errorf("非法文件名")
	}
	if len([]rune(name)) > 180 {
		return "", fmt.Errorf("文件名过长")
	}
	return name, nil
}
