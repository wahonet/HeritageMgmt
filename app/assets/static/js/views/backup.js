// 数据备份/恢复视图。
import { state } from '../core/state.js';
import { $ } from '../core/dom.js';
import { API, post } from '../core/api.js';

export async function showBackup() {
  state.currentProjectId = null;
  document.querySelectorAll('.proj-item').forEach(e => e.classList.remove('active'));
  history.replaceState(null, '', '#backup');
  $('#content').innerHTML = `<div class="dashboard">
    <h2>数据备份与恢复</h2>
    <div style="display:flex;gap:20px;flex-wrap:wrap;margin-top:20px">
      <div class="backup-card">
        <div class="bc-icon" style="background:var(--green-bg);color:var(--green)">↓</div>
        <h3>一键备份</h3>
        <p>将数据库和全部工程文件打包为zip，下载到本地保存。建议定期备份。</p>
        <button class="btn btn-primary" onclick="doBackup()">立即备份</button>
      </div>
      <div class="backup-card">
        <div class="bc-icon" style="background:var(--amber-bg);color:var(--amber)">↑</div>
        <h3>数据恢复</h3>
        <p>从之前备份的zip文件恢复数据。<b style="color:var(--red)">将覆盖当前所有数据</b>，恢复前会自动备份当前数据。</p>
        <input type="file" id="restoreFile" accept=".zip" style="display:none" onchange="doRestore(this)">
        <button class="btn" onclick="document.getElementById('restoreFile').click()">选择备份文件</button>
      </div>
    </div>
    <div id="backupResult" style="margin-top:20px"></div>
  </div>`;
}

export function doBackup() {
  $('#backupResult').innerHTML = '<div class="alert-bar warn"><div class="t">正在备份，请稍候（文件较多时可能需要几十秒）…</div></div>';
  // 浏览器直接下载zip
  const a = document.createElement('a');
  a.href = API + '/backup';
  a.download = '';
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => {
    $('#backupResult').innerHTML = '<div class="alert-bar ok"><div class="t">备份文件已开始下载，请检查浏览器下载列表。</div></div>';
  }, 2000);
}

export async function doRestore(input) {
  const file = input.files[0];
  if (!file) return;
  if (!confirm('确认从备份文件恢复？\n\n当前所有数据将被覆盖（恢复前会自动备份当前数据）。\n恢复后需要重新打开系统。')) { input.value = ''; return; }
  $('#backupResult').innerHTML = '<div class="alert-bar warn"><div class="t">正在恢复，请勿关闭浏览器…</div></div>';
  const fd = new FormData();
  fd.append('file', file);
  try {
    const j = await post('/restore', fd, true);
    if (j.ok) {
      $('#backupResult').innerHTML = `<div class="alert-bar ok"><div class="t">恢复成功！解压了 ${j.files} 个文件。</div><div style="margin-top:6px;font-size:12px">请刷新页面以加载恢复后的数据。</div></div>`;
      setTimeout(() => location.reload(), 3000);
    } else {
      $('#backupResult').innerHTML = '<div class="alert-bar"><div class="t">恢复失败：' + (j.error || '') + '</div></div>';
    }
  } catch (e) {
    $('#backupResult').innerHTML = '<div class="alert-bar"><div class="t">请求失败：' + e + '</div></div>';
  }
  input.value = '';
}
