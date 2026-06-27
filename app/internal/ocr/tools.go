// Package ocr wraps the external command-line tools (PDF renderers + Tesseract)
// used to turn contract PDFs into OCR text. It only shells out to tools and has
// no dependency on the database, config globals, or the LLM client — paths are
// passed in by the caller.
package ocr

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
)

func fileExists(p string) bool {
	_, err := os.Stat(p)
	return err == nil
}

// FindTool 多路径查找可执行工具: PATH > app/tools > winget包目录 > Program Files
func FindTool(name, appBase string) string {
	if p, err := exec.LookPath(name); err == nil {
		return p
	}
	names := []string{name}
	if filepath.Base(name) == name && runtime.GOOS == "windows" {
		names = append(names, name+".exe")
	}
	// 1) 应用自带 tools 目录
	for _, c := range names {
		p := filepath.Join(appBase, "tools", c)
		if fileExists(p) {
			return p
		}
	}
	// 2) Windows winget 包目录(支持 poppler/mupdf 等便携安装)
	if gd := os.Getenv("LOCALAPPDATA"); gd != "" {
		wgBase := filepath.Join(gd, "Microsoft", "WinGet", "Packages")
		for _, c := range names {
			for _, pat := range []string{
				filepath.Join(wgBase, "*", "*", "Library", "bin", c),
				filepath.Join(wgBase, "*", "poppler-*", "Library", "bin", c),
				filepath.Join(wgBase, "*", "*", "bin", c),
			} {
				if m, _ := filepath.Glob(pat); len(m) > 0 {
					return m[0]
				}
			}
		}
	}
	// 3) Program Files 常见位置(tesseract 等)
	for _, c := range names {
		for _, dir := range []string{`C:\Program Files\Tesseract-OCR`, `C:\Program Files\ImageMagick-7.1.2-Q16`} {
			p := filepath.Join(dir, c)
			if fileExists(p) {
				return p
			}
		}
	}
	return ""
}

// PDFToImages 把PDF渲染成PNG,依次尝试多种工具
func PDFToImages(pdfPath, outDir, appBase string) ([]string, error) {
	prefix := filepath.Join(outDir, "p")
	old, _ := filepath.Glob(prefix + "*.png")
	for _, f := range old {
		os.Remove(f)
	}
	type renderer struct{ name, bin string }
	cands := []renderer{
		{"pdftoppm", FindTool("pdftoppm", appBase)},
		{"mutool", FindTool("mutool", appBase)},
		{"magick", FindTool("magick", appBase)},
		{"convert", FindTool("convert", appBase)},
	}
	maxPages := "6" // 只取前6页(参建单位信息都在合同前几页,避免上百页扫描件耗时失控)
	for _, r := range cands {
		if r.bin == "" {
			continue
		}
		var cmd *exec.Cmd
		switch r.name {
		case "mutool":
			cmd = exec.Command(r.bin, "draw", "-o", prefix+"%03d.png", "-r", "200", pdfPath, "1-"+maxPages)
		case "pdftoppm":
			cmd = exec.Command(r.bin, "-png", "-r", "200", "-f", "1", "-l", maxPages, pdfPath, prefix)
		case "magick", "convert":
			cmd = exec.Command(r.bin, "-density", "200", pdfPath+"[0-5]", prefix+"%03d.png")
		}
		_ = cmd.Run()
		if imgs, _ := filepath.Glob(prefix + "*.png"); len(imgs) > 0 {
			sort.Strings(imgs)
			return imgs, nil
		}
	}
	return nil, fmt.Errorf("未找到可用的PDF渲染工具(需安装 poppler(pdftoppm) 或 mupdf 或 ImageMagick 之一)")
}

// OCRImage 用 tesseract 对单张图片做中文(chi_sim)OCR
func OCRImage(imgPath, dataDir, appBase string) (string, error) {
	tess := FindTool("tesseract", appBase)
	if tess == "" {
		return "", fmt.Errorf("未找到tesseract,请安装Tesseract OCR及chi_sim中文语言包")
	}
	args := []string{imgPath, "stdout", "-l", "chi_sim", "--psm", "6"}
	// 用 --tessdata-dir 直接指定语言包目录(比 TESSDATA_PREFIX 环境变量更可靠)
	tessdataDir := filepath.Join(dataDir, "tessdata")
	if _, e := os.Stat(filepath.Join(tessdataDir, "chi_sim.traineddata")); e == nil {
		args = append([]string{"--tessdata-dir", tessdataDir}, args...)
	}
	cmd := exec.Command(tess, args...)
	out, err := cmd.Output()
	if err != nil {
		return "", err
	}
	return string(out), nil
}
