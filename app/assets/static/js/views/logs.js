// 操作日志视图。
import { state } from '../core/state.js';
import { $, esc } from '../core/dom.js';
import { get } from '../core/api.js';

function actionColor(a) { if (a.includes('上传')) return 'green'; if (a.includes('删除')) return 'red'; if (a.includes('编辑')) return 'amber'; if (a.includes('导入')) return 'blue'; return 'gray'; }

export async function showLogs() {
  state.currentProjectId = null;
  document.querySelectorAll('.proj-item').forEach(e => e.classList.remove('active'));
  history.replaceState(null, '', '#logs');
  const logs = await get('/logs');
  const rows = logs.map(l => `<tr><td style="white-space:nowrap">${esc(l.ts)}</td>
    <td><span class="log-tag log-${actionColor(l.action)}">${esc(l.action)}</span></td>
    <td>${esc(l.target)}</td><td style="color:#777;font-size:12px">${esc(l.detail)}</td></tr>`).join('');
  $('#content').innerHTML = `<div class="logs-view"><h2>操作日志</h2>
    <div class="missing-table"><table>
    <thead><tr><th style="width:160px">时间</th><th style="width:90px">操作</th><th>对象</th><th>详情</th></tr></thead>
    <tbody>${rows || '<tr><td colspan=4 style="text-align:center;color:#999;padding:24px">暂无操作记录</td></tr>'}</tbody></table></div></div>`;
}
