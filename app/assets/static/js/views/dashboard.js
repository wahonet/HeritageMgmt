// 侧栏（单位/工程列表）与看板视图。
import { state } from '../core/state.js';
import { $, el, esc, wan, jsArg } from '../core/dom.js';
import { get, post, del, API } from '../core/api.js';
import { selectProject } from './project.js';

// ---------- 侧栏 ----------
export async function loadSidebar(filter = '') {
  state.units = await get('/units');
  const projects = await get('/projects');
  const list = $('#unitList'); list.innerHTML = '';
  for (const u of state.units) {
    const ps = projects.filter(p => p.unit_id === u.id);
    if (filter) { const f = filter.toLowerCase(); if (!u.name.includes(filter) && !ps.some(p => p.name.toLowerCase().includes(f) || (p.approval_no || '').toLowerCase().includes(f))) continue; }
    const g = el('div', 'unit-group');
    // 默认收起; 搜索时 或 含当前选中工程 时展开
    const expanded = (filter !== '') || ps.some(p => p.id === state.currentProjectId);
    const head = el('div', 'unit-head',
      `<span class="caret">${expanded ? '-' : '+'}</span><span class="uname">${esc(u.name)}</span>${u.level ? `<span class="lvl">${esc(u.level)}</span>` : ''}<span class="badge">${ps.length}</span><span class="unit-del" title="删除单位" onclick="event.stopPropagation();deleteUnit(${u.id},${jsArg(u.name)},${ps.length})">×</span>`);
    const body = el('div', expanded ? '' : 'hidden');
    head.onclick = () => {
      const hidden = body.classList.toggle('hidden');
      head.querySelector('.caret').textContent = hidden ? '+' : '-';
    };
    for (const p of ps) {
      const dotCls = p.status === '已竣工' ? 'done' : (p.status === '在建' ? 'doing' : 'early');
      const miss = p.missing_required && p.missing_required.length ? `<span class="miss">缺${p.missing_required.length}</span>` : '';
      const it = el('div', 'proj-item' + (p.id === state.currentProjectId ? ' active' : ''));
      it.innerHTML = `<span class="dot ${dotCls}"></span><span class="proj-name" title="${esc(p.name)}">${esc(p.name)}</span>${miss}`;
      it.onclick = () => selectProject(p.id);
      body.appendChild(it);
    }
    g.appendChild(head); g.appendChild(body); list.appendChild(g);
  }
}
export function filterSidebar(q) { loadSidebar(q.trim()); }

// ---------- 看板 ----------
export async function showDashboard() {
  state.currentProjectId = null;
  history.replaceState(null, '', '#');
  document.querySelectorAll('.proj-item').forEach(e => e.classList.remove('active'));
  const data = await get('/dashboard');
  const t = data.totals;
  const c = $('#content');
  const cards = `
    <div class="stat-card"><div class="num">${t.projects}</div><div class="lbl">工程总数</div></div>
    <div class="stat-card"><div class="num" style="color:var(--green)">${t.done}</div><div class="lbl">资料齐全</div></div>
    <div class="stat-card warn"><div class="num">${t.with_missing}</div><div class="lbl">存在缺项</div></div>
    <div class="stat-card"><div class="num">${wan(t.central_funding)}<small> 万元</small></div><div class="lbl">中央资金指标合计</div></div>
    <div class="stat-card"><div class="num">${wan(t.total_paid)}<small> 万元</small></div><div class="lbl">已支付合计</div></div>`;
  let rows = '';
  for (const p of data.projects) {
    const dotCls = p.status === '已竣工' ? 'done' : (p.status === '在建' ? 'doing' : 'early');
    const miss = p.missing.length ? p.missing.map(m => `<span class="tag-miss">${esc(m)}</span>`).join('') : '<span class="tag-ok">齐全</span>';
    rows += `<tr style="cursor:pointer" onclick="selectProject(${p.id})">
      <td><span class="dot ${dotCls}" style="display:inline-block;vertical-align:middle"></span> ${esc(p.unit_name)}</td>
      <td><b>${esc(p.name)}</b></td><td>${esc(p.ptype || '')}</td>
      <td>${esc(p.status || '—')}</td><td>${p.doc_count}</td><td>${miss}</td></tr>`;
  }
  c.innerHTML = `<div class="dashboard">
    <h2>整体情况</h2>
    <div class="stat-cards">${cards}</div>
    <div class="missing-table"><table>
      <thead><tr><th>文物单位</th><th>工程名称</th><th>类型</th><th>状态</th><th>文档数</th><th>必备材料情况</th></tr></thead>
      <tbody>${rows || '<tr><td colspan=6 style="text-align:center;color:#999;padding:20px">暂无工程</td></tr>'}</tbody>
    </table></div></div>`;
}

// ---------- 重新导入 ----------
export async function reImport() {
  if (!confirm('将清空并重新扫描 Basicdata 导入，确认继续？')) return;
  $('#content').innerHTML = '<div class="loading">重新导入中…</div>';
  const r = await post('/import');
  if (r.ok) { alert(`导入完成：单位${r.stats.units} / 工程${r.stats.projects} / 文档${r.stats.docs}`); await loadSidebar(); showDashboard(); }
  else alert('导入失败：' + (r.error || ''));
}

// ---------- 删除文物单位 ----------
export async function deleteUnit(uid, name, projCount) {
  const msg = projCount > 0
    ? `确认删除文物单位「${name}」？\n\n该单位下有 ${projCount} 个工程，将一并放入回收站（可恢复）。`
    : `确认删除文物单位「${name}」？`;
  if (!confirm(msg)) return;
  try {
    const j = await del(`/unit/${uid}`);
    if (j.ok) {
      if (j.projects_deleted > 0) alert(`已删除「${name}」及其 ${j.projects_deleted} 个工程（已放入回收站）。`);
      loadSidebar('');
      showDashboard();
    } else { alert('删除失败：' + (j.error || '')); }
  } catch (e) { alert('请求失败：' + e); }
}
