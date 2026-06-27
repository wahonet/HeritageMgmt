// 统计分析视图（固定图表 + 自定义图表）。取数+装配；纯渲染在 components/charts.js。
import { state } from '../core/state.js';
import { $, esc } from '../core/dom.js';
import { get, API } from '../core/api.js';
import { fmt, METRICS, CMP, CHART_COLORS, barChart, groupBar, pieChart } from '../components/charts.js';

export async function showStats() {
  state.currentProjectId = null;
  document.querySelectorAll('.proj-item').forEach(e => e.classList.remove('active'));
  history.replaceState(null, '', '#stats');
  $('#content').innerHTML = '<div class="loading">加载统计中…</div>';
  const d = await get('/stats');
  state.statsData = d;
  const projs = await get('/projects');
  state.projectsList = projs.map(p => ({
    id: p.id, k: p.name, funding: p.central_funding || 0, paid: p.total_paid || 0,
    pending: (p.central_funding || 0) - (p.total_paid || 0), eng_contract: p.eng_contract || 0, eng_paid: p.eng_paid || 0,
    sup_contract: p.sup_contract || 0, sup_paid: p.sup_paid || 0, des_contract: p.des_contract || 0, des_paid: p.des_paid || 0,
  }));
  const t = d.total || {};
  const yearItems = d.by_year.map(y => ({ label: y.k, a: y.funding, b: y.paid }));
  const unitItems = d.by_unit.map(u => ({ label: u.k, a: u.funding, b: u.paid }));
  const mopts = Object.entries(METRICS).map(([k, v]) => `<option value="${k}">${v}</option>`).join('');
  const projChecks = state.projectsList.map((p, i) => `<label class="ps-item"><input type="checkbox" value="${p.id}" ${i < 5 ? 'checked' : ''} onchange="renderCustomChart()"><span title="${esc(p.k)}">${esc(p.k)}</span></label>`).join('');
  const fixedHTML = `<div class="stats-summary">中央指标合计 <b>${fmt(t.funding)}</b>万元 ｜ 已支付 <b>${fmt(t.paid)}</b>万元 ｜ 待支付 <b>${fmt(t.pending)}</b>万元 ｜ 整体支付率 <b>${t.funding > 0 ? (t.paid / t.funding * 100).toFixed(1) : '—'}%</b></div>
    <div class="charts-grid">
      ${groupBar('各年度 中央指标 vs 已支付（万元）', yearItems)}
      ${groupBar('各文物单位 中央指标 vs 已支付（万元）', unitItems)}
    </div>`;
  const customHTML = `<div class="cc-controls">
        <span><label>类型</label><select id="ccType"><option value="bar">柱状</option><option value="cmp">对比柱状</option><option value="pie">饼状</option></select></span>
        <span><label>分组(X轴)</label><select id="ccGroup" onchange="toggleProjSelect()"><option value="by_unit">文保单位</option><option value="by_type">工程类型</option><option value="by_year">年份</option><option value="proj">工程名称</option></select></span>
        <span><label>指标(Y轴)</label><select id="ccMetric">${mopts}</select></span>
        <span><label>显示</label><select id="ccDisp"><option value="num">数额</option><option value="pct">百分比</option></select></span>
        <button class="btn btn-sm" onclick="renderCustomChart()">生成图表</button>
      </div>
      <div id="projSelect" class="proj-select hidden">
        <div class="ps-head">选择工程（可多选，默认前5个）<span class="ps-act"><a onclick="selectAllProj(true)">全选</a> · <a onclick="selectAllProj(false)">清空</a></span></div>
        <div class="ps-list">${projChecks}</div>
      </div>
      <div id="ccResult" class="cc-result-full"></div>`;

  $('#content').innerHTML = `<div class="stats-view">
    <h2>统计分析</h2>
    <div class="stats-tabs">
      <button class="stats-tab active" data-stab="fixed" onclick="switchStatsTab('fixed')">固定图表</button>
      <button class="stats-tab" data-stab="custom" onclick="switchStatsTab('custom')">自定义图表</button>
    </div>
    <div id="statsFixed" class="stats-pane">${fixedHTML}</div>
    <div id="statsCustom" class="stats-pane hidden">${customHTML}</div>
  </div>`;
  renderCustomChart();
}
export function switchStatsTab(tab) {
  document.querySelectorAll('.stats-tab').forEach(b => b.classList.toggle('active', b.dataset.stab === tab));
  $('#statsFixed').classList.toggle('hidden', tab !== 'fixed');
  $('#statsCustom').classList.toggle('hidden', tab !== 'custom');
}
export function toggleProjSelect() {
  const isProj = $('#ccGroup').value === 'proj';
  document.getElementById('projSelect').classList.toggle('hidden', !isProj);
  renderCustomChart();
}
export function selectAllProj(on) { document.querySelectorAll('#projSelect input[type=checkbox]').forEach(c => c.checked = on); renderCustomChart(); }
export function renderCustomChart() {
  const d = state.statsData; if (!d) return;
  const type = $('#ccType').value, grp = $('#ccGroup').value, mkey = $('#ccMetric').value, disp = $('#ccDisp').value;
  const grpName = { by_unit: '文保单位', by_type: '工程类型', by_year: '年份', proj: '工程' }[grp];
  let rows;
  if (grp === 'proj') {
    const ids = [...document.querySelectorAll('#projSelect input:checked')].map(c => c.value);
    rows = (state.projectsList || []).filter(p => ids.includes(String(p.id)));
  } else {
    rows = d[grp] || [];
  }
  let html = '';
  const growOpts = { grow: true, growMin: 900 };
  if (type === 'cmp') {
    const pair = CMP[mkey] || CMP.funding;
    const items = rows.map(r => ({ label: r.k, a: r[pair[0]] || 0, b: r[pair[1]] || 0 }));
    html = groupBar(`${pair[2]} vs ${pair[3]} · 按${grpName}（万元）`, items, pair[2], pair[3], growOpts);
  } else {
    const items = rows.map(r => ({ label: r.k, k: r.k, v: r[mkey] || 0 }));
    const filt = grp === 'proj' ? items : items.filter(r => r.v > 0);
    const title = `${METRICS[mkey]} · 按${grpName}（万元）`;
    html = type === 'pie' ? pieChart(title, filt, CHART_COLORS, '万') : barChart(title, filt, CHART_COLORS[0], { asPct: disp === 'pct', unit: '万', ...growOpts });
  }
  $('#ccResult').innerHTML = html || '<div class="hint">请选择工程或该指标暂无数据</div>';
}
