// 回收站视图（列表/恢复/彻底删除）。
import { state } from '../core/state.js';
import { $, esc, jsArg } from '../core/dom.js';
import { get, post, del } from '../core/api.js';
import { loadSidebar } from './dashboard.js';

export async function showRecycle() {
  state.currentProjectId = null;
  document.querySelectorAll('.proj-item').forEach(e => e.classList.remove('active'));
  history.replaceState(null, '', '#recycle');
  const items = await get('/recycle');
  if (!items || !items.length) {
    $('#content').innerHTML = `<div class="dashboard"><h2>回收站</h2><div class="hint" style="padding:40px">回收站为空</div></div>`;
    return;
  }
  const rows = items.map(it => `<tr>
    <td>${esc(it.unit_name)}</td>
    <td><b>${esc(it.name)}</b></td>
    <td>${esc(it.ptype || '—')}</td>
    <td>${esc(it.status || '—')}</td>
    <td>
      <button class="btn btn-sm" style="background:var(--green)" onclick="restoreProject(${it.id},${jsArg(it.name)})">恢复</button>
      <button class="btn btn-sm btn-del-project" onclick="purgeProject(${it.id},${jsArg(it.name)})">彻底删除</button>
    </td></tr>`).join('');
  $('#content').innerHTML = `<div class="dashboard">
    <h2>回收站（${items.length}个已删除工程）</h2>
    <div class="alert-bar warn"><div class="t">恢复：将工程还原到列表（文件从回收站搬回）。彻底删除：永久删除，不可恢复。</div></div>
    <div class="missing-table"><table>
    <thead><tr><th>文物单位</th><th>工程名称</th><th>类型</th><th>状态</th><th>操作</th></tr></thead>
    <tbody>${rows}</tbody></table></div></div>`;
}
export async function restoreProject(pid, name) {
  if (!confirm(`确认恢复「${name}」？\n\n工程将重新出现在列表中。`)) return;
  const r = await post(`/project/${pid}/restore`);
  if (r.ok) { alert('已恢复。'); showRecycle(); loadSidebar(''); }
  else alert('恢复失败：' + (r.error || ''));
}
export async function purgeProject(pid, name) {
  if (!confirm(`确认彻底删除「${name}」？\n\n此操作不可恢复，工程数据和文件将被永久删除。`)) return;
  const r = await del(`/project/${pid}/purge`);
  if (r.ok) { showRecycle(); }
  else alert('删除失败：' + (r.error || ''));
}
