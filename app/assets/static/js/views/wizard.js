// 添加项目向导视图。工程类型清单从 /api/config 读取（取代前端硬编码副本）。
import { state } from '../core/state.js';
import { $, esc, extIcon } from '../core/dom.js';
import { get, post } from '../core/api.js';
import { loadSidebar } from './dashboard.js';
import { selectProject } from './project.js';

let wizardState = { step: 1, newPid: null };

export async function openWizard() {
  wizardState = { step: 1, newPid: null };
  const units = await get('/units');
  const unitOpts = units.map(u => `<option value="${u.id}">${esc(u.name)}（${esc(u.level || '未定级')}）</option>`).join('');
  // 工程类型来自配置（state.config.project_types），追加"其他工程"作为兜底
  const types = (state.config?.project_types || []).slice();
  if (!types.includes('其他工程')) types.push('其他工程');
  const typeOpts = types.map(t => `<option value="${t}">${t}</option>`).join('');
  $('#wizardHint').textContent = '第 1 步：填写工程基本信息';
  $('#wizardNext').textContent = '创建并进入上传';
  $('#wizardBody').innerHTML = `
    <div class="wizard-step">
      <div class="ef-grp"><div class="ef-grp-t">工程基本信息</div><div class="ef-grp-b">
        <div class="ef"><label>工程名称</label><input id="wzName" type="text" placeholder="必填"></div>
        <div class="ef"><label>文物单位</label><select id="wzUnit"><option value="0">— 选择现有单位 —</option>${unitOpts}<option value="-1">— 新建单位 —</option></select></div>
        <div class="ef" id="wzNewUnitWrap" style="display:none"><label>新单位名称</label><input id="wzNewUnit" type="text"></div>
        <div class="ef"><label>工程类型</label><select id="wzType">${typeOpts}</select></div>
        <div class="ef"><label>初始状态</label><select id="wzStatus"><option value="前期">前期</option><option value="在建">在建</option><option value="已竣工">已竣工</option></select></div>
      </div></div>
    </div>`;
  $('#wzUnit').onchange = () => { $('#wzNewUnitWrap').style.display = $('#wzUnit').value === '-1' ? 'flex' : 'none'; };
  $('#wizardModal').classList.remove('hidden');
}

export async function wizardNext() {
  if (wizardState.step === 1) {
    const name = $('#wzName').value.trim();
    if (!name) { alert('请填写工程名称'); return; }
    const unitVal = $('#wzUnit').value;
    const body = { name, ptype: $('#wzType').value, status: $('#wzStatus').value };
    if (unitVal === '-1') {
      body.new_unit = $('#wzNewUnit').value.trim();
      if (!body.new_unit) { alert('请填写新单位名称'); return; }
    } else {
      body.unit_id = parseInt(unitVal);
    }
    const j = await post('/project/create', body);
    if (!j.ok) { alert('创建失败：' + (j.error || '')); return; }
    wizardState.newPid = j.id;
    wizardState.step = 2;
    renderWizardUpload(j.id);
  } else if (wizardState.step === 2) {
    $('#wizardModal').classList.add('hidden');
    await loadSidebar('');
    selectProject(wizardState.newPid);
  }
}

export function renderWizardUpload(pid) {
  const types = state.config.doc_types || [];
  $('#wizardHint').textContent = '第 2 步：按类别上传文件（没有的可跳过，系统会标记缺项）';
  $('#wizardNext').textContent = '完成';
  $('#wizardBody').innerHTML = `
    <div class="wizard-step">
      <div style="font-size:12px;color:var(--muted);margin-bottom:10px">工程已创建（ID: ${pid}）。请将各类文件上传到对应类别，没有的跳过即可。</div>
      <div class="wz-upload-grid">
        ${types.map(t => `<div class="wz-upload-zone" data-type="${t.code}" data-name="${esc(t.name)}">
          <div class="wz-zone-head"><span class="wz-zone-name">${esc(t.name)}</span>${t.required ? '<span class="dtr-required">必备</span>' : '<span class="dtr-optional">可选</span>'}</div>
          <div class="wz-zone-files" id="wzFiles_${t.code}"></div>
          <input type="file" multiple style="display:none" id="wzInput_${t.code}" onchange="wzUpload(${pid},'${t.code}','${esc(t.name)}',this)">
          <button class="upload-mini" onclick="document.getElementById('wzInput_${t.code}').click()">＋ 添加文件</button>
        </div>`).join('')}
      </div>
    </div>`;
}

export async function wzUpload(pid, docType, typeName, input) {
  const files = input.files;
  if (!files.length) return;
  const listEl = document.getElementById('wzFiles_' + docType);
  for (const f of files) {
    const fd = new FormData();
    fd.append('project_id', pid);
    fd.append('doc_type', docType);
    fd.append('file', f);
    try {
      const j = await post('/upload', fd, true);
      if (j.ok) {
        listEl.innerHTML += `<div class="wz-file-item">${extIcon(f.name.split('.').pop())} ${esc(f.name)} <span class="wz-ok">✓</span></div>`;
      }
    } catch (e) { }
  }
  input.value = '';
}
