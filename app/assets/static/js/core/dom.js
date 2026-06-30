// DOM 与格式化辅助（原 utils.js）。纯函数，无 IO。
export const $ = s => document.querySelector(s);
export const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
export const wan = v => (v == null || v === '' || isNaN(v)) ? '—' : (Number(v).toLocaleString('zh-CN', { maximumFractionDigits: 2 }));
export const esc = s => (s == null ? '' : String(s)).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
// attr: HTML 属性上下文转义（在 esc 基础上再转单引号，用于 title="..." 等属性值）
export const attr = s => esc(s).replace(/'/g, '&#39;');
// jsArg: 把任意值编码为可安全嵌入 HTML 属性的 JS 字符串字面量。
// 先 JSON.stringify 正确转义引号/反斜杠/控制字符，再 attr 编码以放入 HTML 属性，双重防护。
// 用于 onclick="fn(id,${jsArg(用户数据)})" 这类场景，根治 esc 不转义 ' 与 \ 导致的绕过。
export const jsArg = v => attr(JSON.stringify(String(v ?? '')));
// 日期格式化为 xx年xx月xx日
export const fmtDate = s => { if (!s) return '—'; const m = /^(\d{4})[-\/.](\d{1,2})[-\/.](\d{1,2})/.exec(s); if (m) return m[1] + '年' + parseInt(m[2], 10) + '月' + parseInt(m[3], 10) + '日'; return s; };
// 解析日期为 Date，计算天数差
export const parseDate = s => { if (!s) return null; let m = /^(\d{4})[-\/.](\d{1,2})[-\/.](\d{1,2})/.exec(s); if (!m) m = /^(\d{4})\s*年\s*(\d{1,2})\s*月\s*(\d{1,2})/.exec(s); return m ? new Date(+m[1], +m[2] - 1, +m[3]) : null; };
export const dateDiff = (a, b) => { const da = parseDate(a), db = parseDate(b); return (da && db) ? Math.round((db - da) / 86400000) : null; };
// 文物保护单位级别全称
const LEVEL_NAME = { '国保': '全国重点文物保护单位', '省保': '省级文物保护单位', '市保': '市级文物保护单位', '县保': '县级文物保护单位', '未定级': '未定级文物点' };
export const levelName = l => LEVEL_NAME[l] || l || '未定级文物点';
// 文件类型文字标签(替代emoji)
export const extIcon = ext => {
  ext = (ext || '').toLowerCase();
  if (ext === 'pdf') return 'PDF';
  if (['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp'].includes(ext)) return 'IMG';
  if (['doc', 'docx'].includes(ext)) return 'DOC';
  if (['xls', 'xlsx'].includes(ext)) return 'XLS';
  return (ext || 'FILE').toUpperCase().slice(0, 4);
};
