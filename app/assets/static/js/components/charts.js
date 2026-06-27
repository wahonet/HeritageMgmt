// 纯图表渲染组件：输入数据 → SVG 字符串。不做取数（取数在 views/stats.js）。
import { esc } from '../core/dom.js';

export function fmt(v) { v = Number(v || 0); return Math.round(v).toLocaleString('zh-CN'); }
export const METRICS = { 'funding': '中央指标', 'paid': '总已支付', 'pending': '总待支付', 'eng_contract': '工程合同价', 'eng_paid': '工程已支付', 'sup_contract': '监理合同价', 'sup_paid': '监理已支付', 'des_contract': '设计合同价', 'des_paid': '设计已支付' };
// 对比图: 每个指标对应的(总额key, 已付key, 总额名, 已付名)
export const CMP = { 'funding': ['funding', 'paid', '中央指标', '已支付'], 'paid': ['funding', 'paid', '中央指标', '已支付'], 'pending': ['funding', 'paid', '中央指标', '已支付'], 'eng_contract': ['eng_contract', 'eng_paid', '工程合同价', '工程已支付'], 'eng_paid': ['eng_contract', 'eng_paid', '工程合同价', '工程已支付'], 'sup_contract': ['sup_contract', 'sup_paid', '监理合同价', '监理已支付'], 'sup_paid': ['sup_contract', 'sup_paid', '监理合同价', '监理已支付'], 'des_contract': ['des_contract', 'des_paid', '设计合同价', '设计已支付'], 'des_paid': ['des_contract', 'des_paid', '设计合同价', '设计已支付'] };
export const CHART_COLORS = ['#8B5E34', '#C9A063', '#2E8B57', '#D68910', '#6C8EAD', '#9B59B6', '#BDB5A6', '#4A90A4'];

// 柱状图(单系列,柱顶标数值/百分比)
export function barChart(title, items, color, opts) {
  opts = opts || {};
  const short = s => { s = s || ''; return s.length > 15 ? s.slice(0, 15) + '…' : s; };
  const rot = items.some(it => (it.label || '').length > 5) || items.length > 7;
  const growMin = opts.growMin || 560;
  const W = opts.grow ? Math.min(1100, Math.max(growMin, items.length * 58)) : 440, H = 200, padL = 46, padB = rot ? 60 : 38, padT = 16, padR = 10;
  const vals = items.map(i => i.v || 0);
  const total = vals.reduce((s, v) => s + v, 0) || 1;
  const max = opts.asPct ? 100 : Math.max(1, ...vals);
  const slot = (W - padL - padR) / Math.max(1, items.length), bw = Math.min(slot * 0.62, 42);
  const lblV = v => opts.asPct ? (v / total * 100).toFixed(1) + '%' : fmt(v);
  const bars = items.map((it, i) => {
    const val = opts.asPct ? (it.v / total * 100) : (it.v || 0);
    const h = (val / max) * (H - padT - padB), x = padL + i * slot + (slot - bw) / 2, y = H - padB - h;
    return `<rect x="${x.toFixed(1)}" y="${y.toFixed(1)}" width="${bw.toFixed(1)}" height="${h.toFixed(1)}" fill="${color}" rx="3"><title>${esc(it.label)}: ${fmt(it.v || 0)}万</title></rect>
      <text x="${(x + bw / 2).toFixed(1)}" y="${(y - 3).toFixed(1)}" text-anchor="middle" font-size="9" fill="#555">${lblV(it.v || 0)}</text>`;
  }).join('');
  const labels = items.map((it, i) => {
    const x = (padL + i * slot + slot / 2).toFixed(1), y = H - padB + 14, t = esc(short(it.label));
    return rot ? `<text x="${x}" y="${y}" text-anchor="end" font-size="10" fill="#666" transform="rotate(-30 ${x} ${y})">${t}</text>`
              : `<text x="${x}" y="${y}" text-anchor="middle" font-size="10" fill="#666">${t}</text>`;
  }).join('');
  const grid = [0, .5, 1].map(g => { const y = padT + (1 - g) * (H - padT - padB); return `<line x1="${padL}" y1="${y.toFixed(1)}" x2="${W - padR}" y2="${y.toFixed(1)}" stroke="#eee"/><text x="${padL - 5}" y="${(y + 3).toFixed(1)}" text-anchor="end" font-size="8" fill="#999">${opts.asPct ? (max * g).toFixed(0) + '%' : fmt(max * g)}</text>`; }).join('');
  return `<div class="chart-card"${opts.grow ? ` style="max-width:${W}px"` : ''}><h4>${esc(title)}</h4><svg viewBox="0 0 ${W} ${H}" class="chart">${grid}${bars}${labels}<line x1="${padL}" y1="${H - padB}" x2="${W - padR}" y2="${H - padB}" stroke="#ccc"/></svg></div>`;
}

// 分组柱状图: a vs b,每柱标数值,b柱内标占比
export function groupBar(title, items, legendA, legendB, opts) {
  opts = opts || {};
  legendA = legendA || '中央指标'; legendB = legendB || '已支付';
  const short = s => { s = s || ''; return s.length > 15 ? s.slice(0, 15) + '…' : s; };
  const rot = items.some(it => (it.label || '').length > 5) || items.length > 6;
  const growMin = opts.growMin || 560;
  const W = opts.grow ? Math.min(1100, Math.max(growMin, items.length * 72)) : 440, H = 200, padL = 46, padB = rot ? 60 : 38, padT = 22, padR = 10;
  const max = Math.max(1, ...items.flatMap(i => [i.a || 0, i.b || 0]));
  const slot = (W - padL - padR) / Math.max(1, items.length), bw = Math.min(slot * 0.36, 22);
  const bars = items.map((it, i) => {
    const gx = padL + i * slot + (slot - bw * 2 - 3) / 2;
    const ha = ((it.a || 0) / max) * (H - padT - padB), hb = ((it.b || 0) / max) * (H - padT - padB);
    const rate = (it.a > 0) ? (it.b / it.a * 100).toFixed(0) : 0;
    return `<rect x="${gx.toFixed(1)}" y="${(H - padB - ha).toFixed(1)}" width="${bw.toFixed(1)}" height="${ha.toFixed(1)}" fill="#8B5E34" rx="2"><title>${esc(legendA)}:${fmt(it.a)}万</title></rect>
      <text x="${(gx + bw / 2).toFixed(1)}" y="${(H - padB - ha - 3).toFixed(1)}" text-anchor="middle" font-size="8" fill="#8B5E34">${fmt(it.a || 0)}</text>
      <rect x="${(gx + bw + 3).toFixed(1)}" y="${(H - padB - hb).toFixed(1)}" width="${bw.toFixed(1)}" height="${hb.toFixed(1)}" fill="#C9A063" rx="2"><title>${esc(legendB)}:${fmt(it.b)}万 占比${rate}%</title></rect>
      <text x="${(gx + bw + 3 + bw / 2).toFixed(1)}" y="${(H - padB - hb - 3).toFixed(1)}" text-anchor="middle" font-size="8" fill="#B8862A">${fmt(it.b || 0)}</text>
      ${hb > 16 ? `<text x="${(gx + bw + 3 + bw / 2).toFixed(1)}" y="${(H - padB - hb / 2 + 3).toFixed(1)}" text-anchor="middle" font-size="8" font-weight="700" fill="#fff">${rate}%</text>` : ''}`;
  }).join('');
  const labels = items.map((it, i) => {
    const x = (padL + i * slot + slot / 2).toFixed(1), y = H - padB + 14, t = esc(short(it.label));
    return rot ? `<text x="${x}" y="${y}" text-anchor="end" font-size="10" fill="#666" transform="rotate(-30 ${x} ${y})">${t}</text>` : `<text x="${x}" y="${y}" text-anchor="middle" font-size="10" fill="#666">${t}</text>`;
  }).join('');
  const grid = [0, .5, 1].map(g => { const y = padT + (1 - g) * (H - padT - padB); return `<line x1="${padL}" y1="${y.toFixed(1)}" x2="${W - padR}" y2="${y.toFixed(1)}" stroke="#eee"/><text x="${padL - 5}" y="${(y + 3).toFixed(1)}" text-anchor="end" font-size="8" fill="#999">${fmt(max * g)}</text>`; }).join('');
  return `<div class="chart-card"${opts.grow ? ` style="max-width:${W}px"` : ''}><h4>${esc(title)}</h4><svg viewBox="0 0 ${W} ${H}" class="chart">${grid}${bars}${labels}<line x1="${padL}" y1="${H - padB}" x2="${W - padR}" y2="${H - padB}" stroke="#ccc"/></svg><div class="legend" style="margin-top:4px"><span class="lg"><i style="background:#8B5E34"></i>${esc(legendA)}</span><span class="lg"><i style="background:#C9A063"></i>${esc(legendB)}(柱内为占比)</span></div></div>`;
}

// 饼图(环形,图例显示 数值+占比)
export function pieChart(title, items, colors, unit) {
  unit = unit || '';
  const cx = 80, cy = 80, r = 66, r2 = 40, total = items.reduce((s, i) => s + (i.v || 0), 0) || 1;
  let a = -Math.PI / 2, paths = '';
  items.forEach((it, i) => {
    if (!it.v) return;
    const frac = it.v / total, a2 = a + frac * Math.PI * 2, lg = frac > 0.5 ? 1 : 0;
    const p = (rr, ang) => [cx + rr * Math.cos(ang), cy + rr * Math.sin(ang)];
    const [x1, y1] = p(r, a), [x2, y2] = p(r, a2), [x3, y3] = p(r2, a2), [x4, y4] = p(r2, a);
    paths += `<path d="M${x1.toFixed(1)} ${y1.toFixed(1)} A${r} ${r} 0 ${lg} 1 ${x2.toFixed(1)} ${y2.toFixed(1)} L${x3.toFixed(1)} ${y3.toFixed(1)} A${r2} ${r2} 0 ${lg} 0 ${x4.toFixed(1)} ${y4.toFixed(1)} Z" fill="${colors[i % colors.length]}"><title>${esc(it.k || it.label)}: ${fmt(it.v)}${unit} (${(it.v / total * 100).toFixed(1)}%)</title></path>`;
    a = a2;
  });
  const legend = items.filter(i => i.v).map((it, i) => `<span class="lg"><i style="background:${colors[i % colors.length]}"></i>${esc(it.k || it.label)} ${fmt(it.v)}${unit} (${(it.v / total * 100).toFixed(1)}%)</span>`).join('');
  return `<div class="chart-card"><h4>${esc(title)}</h4><div class="donut-wrap"><svg viewBox="0 0 160 160" class="donut">${paths}<text x="80" y="78" text-anchor="middle" font-size="16" font-weight="700" fill="#6B4423">${fmt(total)}</text><text x="80" y="92" text-anchor="middle" font-size="9" fill="#888">合计${unit}</text></svg><div class="legend">${legend}</div></div></div>`;
}
