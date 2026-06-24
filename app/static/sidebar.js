async function init(){
  bindEvents();
  state.viewMode = localStorage.getItem('viewMode') || 'stack';
  state.config = await fetch(`${API}/config`).then(r=>r.json());
  await loadSidebar();
  // hash 路由: #project/<id> 直达工程，便于分享/截图
  window.addEventListener('hashchange', routeFromHash);
  routeFromHash();
}
function routeFromHash(){
  const h=location.hash||'';
  const m=/^#?\/?project\/(\d+)/.exec(h);
  if(m){ selectProject(parseInt(m[1])); return; }
  const e=/^#?\/?edit\/(\d+)/.exec(h);
  if(e){ selectProject(parseInt(e[1])); setTimeout(()=>openEdit(parseInt(e[1])),400); return; }
  if(/stats/.test(h)){ showStats(); return; }
  if(/logs/.test(h)){ showLogs(); return; }
  if(!state.currentProjectId) showDashboard();
}

function bindEvents(){
  $('#btnDashboard').onclick = showDashboard;
  $('#btnStats').onclick = showStats;
  $('#btnLogs').onclick = showLogs;
  $('#btnExport').onclick = exportLedger;
  $('#btnBackup').onclick = showBackup;
  $('#btnRecycle').onclick = showRecycle;
  $('#btnImport').onclick = reImport;
  $('#btnAddProject').onclick = openWizard;
  $('#searchInput').oninput = e => filterSidebar(e.target.value);
  $('#uploadCancel').onclick = closeUpload;
  $('#uploadSubmit').onclick = doUpload;
  $('#previewClose').onclick = ()=>$('#previewModal').classList.add('hidden');
  $('#editCancel').onclick = ()=>$('#editModal').classList.add('hidden');
  $('#editSave').onclick = saveEdit;
  $('#treeClose').onclick = ()=>$('#treeModal').classList.add('hidden');
  $('#wizardClose').onclick = ()=>$('#wizardModal').classList.add('hidden');
  $('#wizardNext').onclick = wizardNext;
}

// ---------- 侧栏 ----------
async function loadSidebar(filter=''){
  state.units = await fetch(`${API}/units`).then(r=>r.json());
  const projects = await fetch(`${API}/projects`).then(r=>r.json());
  const list = $('#unitList'); list.innerHTML='';
  for(const u of state.units){
    const ps = projects.filter(p=>p.unit_id===u.id);
    if(filter){ const f=filter.toLowerCase(); if(!u.name.includes(filter) && !ps.some(p=>p.name.toLowerCase().includes(f)|| (p.approval_no||'').toLowerCase().includes(f))) continue; }
    const g = el('div','unit-group');
    // 默认收起; 搜索时 或 含当前选中工程 时展开
    const expanded = (filter!=='') || ps.some(p=>p.id===state.currentProjectId);
    const head = el('div','unit-head',
      `<span class="caret">${expanded?'-':'+'}</span><span class="uname">${esc(u.name)}</span>${u.level?`<span class="lvl">${esc(u.level)}</span>`:''}<span class="badge">${ps.length}</span><span class="unit-del" title="删除单位" onclick="event.stopPropagation();deleteUnit(${u.id},'${esc(u.name).replace(/'/g,"\\'")}',${ps.length})">×</span>`);
    const body = el('div', expanded?'':'hidden');
    head.onclick = ()=>{
      const hidden = body.classList.toggle('hidden');
      head.querySelector('.caret').textContent = hidden ? '+' : '-';
    };
    for(const p of ps){
      const dotCls = p.status==='已竣工'?'done':(p.status==='在建'?'doing':'early');
      const miss = p.missing_required && p.missing_required.length ? `<span class="miss">缺${p.missing_required.length}</span>` : '';
      const it = el('div','proj-item'+(p.id===state.currentProjectId?' active':''));
      it.innerHTML = `<span class="dot ${dotCls}"></span><span class="proj-name" title="${esc(p.name)}">${esc(p.name)}</span>${miss}`;
      it.onclick = ()=> selectProject(p.id);
      body.appendChild(it);
    }
    g.appendChild(head); g.appendChild(body); list.appendChild(g);
  }
}
function filterSidebar(q){ loadSidebar(q.trim()); }

// ---------- 看板 ----------
async function showDashboard(){
  state.currentProjectId = null;
  history.replaceState(null,'','#');
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  const data = await fetch(`${API}/dashboard`).then(r=>r.json());
  const t = data.totals;
  const c = $('#content');
  const cards = `
    <div class="stat-card"><div class="num">${t.projects}</div><div class="lbl">工程总数</div></div>
    <div class="stat-card"><div class="num" style="color:var(--green)">${t.done}</div><div class="lbl">资料齐全</div></div>
    <div class="stat-card warn"><div class="num">${t.with_missing}</div><div class="lbl">存在缺项</div></div>
    <div class="stat-card"><div class="num">${wan(t.central_funding)}<small> 万元</small></div><div class="lbl">中央资金指标合计</div></div>
    <div class="stat-card"><div class="num">${wan(t.total_paid)}<small> 万元</small></div><div class="lbl">已支付合计</div></div>`;
  let rows='';
  for(const p of data.projects){
    const dotCls = p.status==='已竣工'?'done':(p.status==='在建'?'doing':'early');
    const miss = p.missing.length ? p.missing.map(m=>`<span class="tag-miss">${esc(m)}</span>`).join('') : '<span class="tag-ok">齐全</span>';
    rows += `<tr style="cursor:pointer" onclick="selectProject(${p.id})">
      <td><span class="dot ${dotCls}" style="display:inline-block;vertical-align:middle"></span> ${esc(p.unit_name)}</td>
      <td><b>${esc(p.name)}</b></td><td>${esc(p.ptype||'')}</td>
      <td>${esc(p.status||'—')}</td><td>${p.doc_count}</td><td>${miss}</td></tr>`;
  }
  c.innerHTML = `<div class="dashboard">
    <h2>整体情况</h2>
    <div class="stat-cards">${cards}</div>
    <div class="missing-table"><table>
      <thead><tr><th>文物单位</th><th>工程名称</th><th>类型</th><th>状态</th><th>文档数</th><th>必备材料情况</th></tr></thead>
      <tbody>${rows||'<tr><td colspan=6 style="text-align:center;color:#999;padding:20px">暂无工程</td></tr>'}</tbody>
    </table></div></div>`;
}

// ---------- 工程详情 ----------

async function reImport(){
  if(!confirm('将清空并重新扫描 Basicdata 导入，确认继续？')) return;
  $('#content').innerHTML='<div class="loading">重新导入中…</div>';
  const r = await fetch(`${API}/import`,{method:'POST'}).then(r=>r.json());
  if(r.ok){ alert(`导入完成：单位${r.stats.units} / 工程${r.stats.projects} / 文档${r.stats.docs}`); await loadSidebar(); showDashboard(); }
  else alert('导入失败：'+(r.error||''));
}


function actionColor(a){ if(a.includes('上传'))return'green'; if(a.includes('删除'))return'red'; if(a.includes('编辑'))return'amber'; if(a.includes('导入'))return'blue'; return'gray'; }
async function showLogs(){
  state.currentProjectId=null;
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  history.replaceState(null,'','#logs');
  const logs=await fetch(API+'/logs').then(r=>r.json());
  const rows=logs.map(l=>`<tr><td style="white-space:nowrap">${esc(l.ts)}</td>
    <td><span class="log-tag log-${actionColor(l.action)}">${esc(l.action)}</span></td>
    <td>${esc(l.target)}</td><td style="color:#777;font-size:12px">${esc(l.detail)}</td></tr>`).join('');
  $('#content').innerHTML=`<div class="logs-view"><h2>操作日志</h2>
    <div class="missing-table"><table>
    <thead><tr><th style="width:160px">时间</th><th style="width:90px">操作</th><th>对象</th><th>详情</th></tr></thead>
    <tbody>${rows||'<tr><td colspan=4 style="text-align:center;color:#999;padding:24px">暂无操作记录</td></tr>'}</tbody></table></div></div>`;
}

// ---------- 导出台账 ----------
function exportLedger(){ window.location.href=API+'/export/ledger'; }

// ---------- 数据备份/恢复 ----------
async function showBackup(){
  state.currentProjectId=null;
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  history.replaceState(null,'','#backup');
  $('#content').innerHTML=`<div class="dashboard">
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

function doBackup(){
  $('#backupResult').innerHTML='<div class="alert-bar warn"><div class="t">正在备份，请稍候（文件较多时可能需要几十秒）…</div></div>';
  // 浏览器直接下载zip
  const a=document.createElement('a');
  a.href=API+'/backup';
  a.download='';
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(()=>{
    $('#backupResult').innerHTML='<div class="alert-bar ok"><div class="t">备份文件已开始下载，请检查浏览器下载列表。</div></div>';
  }, 2000);
}

async function doRestore(input){
  const file=input.files[0];
  if(!file) return;
  if(!confirm('确认从备份文件恢复？\n\n当前所有数据将被覆盖（恢复前会自动备份当前数据）。\n恢复后需要重新打开系统。')){ input.value=''; return; }
  $('#backupResult').innerHTML='<div class="alert-bar warn"><div class="t">正在恢复，请勿关闭浏览器…</div></div>';
  const fd=new FormData();
  fd.append('file',file);
  try{
    const r=await fetch(API+'/restore',{method:'POST',body:fd});
    const j=await r.json();
    if(j.ok){
      $('#backupResult').innerHTML=`<div class="alert-bar ok"><div class="t">恢复成功！解压了 ${j.files} 个文件。</div><div style="margin-top:6px;font-size:12px">请刷新页面以加载恢复后的数据。</div></div>`;
      setTimeout(()=>location.reload(), 3000);
    } else {
      $('#backupResult').innerHTML='<div class="alert-bar"><div class="t">恢复失败：'+(j.error||'')+'</div></div>';
    }
  }catch(e){
    $('#backupResult').innerHTML='<div class="alert-bar"><div class="t">请求失败：'+e+'</div></div>';
  }
  input.value='';
}

// ---------- 回收站 ----------
async function showRecycle(){
  state.currentProjectId=null;
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  history.replaceState(null,'','#recycle');
  const items=await fetch(API+'/recycle').then(r=>r.json());
  if(!items||!items.length){
    $('#content').innerHTML=`<div class="dashboard"><h2>回收站</h2><div class="hint" style="padding:40px">回收站为空</div></div>`;
    return;
  }
  const rows=items.map(it=>`<tr>
    <td>${esc(it.unit_name)}</td>
    <td><b>${esc(it.name)}</b></td>
    <td>${esc(it.ptype||'—')}</td>
    <td>${esc(it.status||'—')}</td>
    <td>
      <button class="btn btn-sm" style="background:var(--green)" onclick="restoreProject(${it.id},'${esc(it.name).replace(/'/g,"\\'")}')">恢复</button>
      <button class="btn btn-sm btn-del-project" onclick="purgeProject(${it.id},'${esc(it.name).replace(/'/g,"\\'")}')">彻底删除</button>
    </td></tr>`).join('');
  $('#content').innerHTML=`<div class="dashboard">
    <h2>回收站（${items.length}个已删除工程）</h2>
    <div class="alert-bar warn"><div class="t">恢复：将工程还原到列表（文件从回收站搬回）。彻底删除：永久删除，不可恢复。</div></div>
    <div class="missing-table"><table>
    <thead><tr><th>文物单位</th><th>工程名称</th><th>类型</th><th>状态</th><th>操作</th></tr></thead>
    <tbody>${rows}</tbody></table></div></div>`;
}
async function restoreProject(pid, name){
  if(!confirm(`确认恢复「${name}」？\n\n工程将重新出现在列表中。`)) return;
  const r=await fetch(`${API}/project/${pid}/restore`,{method:'POST'}).then(r=>r.json());
  if(r.ok){ alert('已恢复。'); showRecycle(); loadSidebar(''); }
  else alert('恢复失败：'+(r.error||''));
}
async function purgeProject(pid, name){
  if(!confirm(`确认彻底删除「${name}」？\n\n此操作不可恢复，工程数据和文件将被永久删除。`)) return;
  const r=await fetch(`${API}/project/${pid}/purge`,{method:'DELETE'}).then(r=>r.json());
  if(r.ok){ showRecycle(); }
  else alert('删除失败：'+(r.error||''));
}

// ---------- 编辑台账 ----------

window.selectProject=selectProject; window.openStage=openStage;
window.openDoc=openDoc; window.openUpload=openUpload; window.deleteDoc=deleteDoc; window.openEdit=openEdit; window.ocrScan=ocrScan; window.switchView=switchView; window.switchTab=switchTab; window.renderCustomChart=renderCustomChart; window.toggleProjSelect=toggleProjSelect; window.selectAllProj=selectAllProj; window.showFileTree=showFileTree; window.openDocByName=openDocByName; window.deleteProject=deleteProject; window.deleteDocType=deleteDocType; window.wzUpload=wzUpload; window.deleteUnit=deleteUnit; window.restoreProject=restoreProject; window.purgeProject=purgeProject; window.switchStatsTab=switchStatsTab;

// ---------- 添加项目向导 ----------
let wizardState = { step:1, newPid:null };

async function openWizard(){
  wizardState = { step:1, newPid:null };
  const units = await fetch(`${API}/units`).then(r=>r.json());
  const unitOpts = units.map(u=>`<option value="${u.id}">${esc(u.name)}（${esc(u.level||'未定级')}）</option>`).join('');
  const typeOpts = ['修缮工程','安防工程','消防工程','抢险加固工程','本体保护工程','保护性设施建设工程','环境整治工程','数字化保护','预防性保护','前期勘察研究','院落整修工程','其他工程'].map(t=>`<option value="${t}">${t}</option>`).join('');
  $('#wizardHint').textContent='第 1 步：填写工程基本信息';
  $('#wizardNext').textContent='创建并进入上传';
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
  $('#wzUnit').onchange = ()=>{ $('#wzNewUnitWrap').style.display = $('#wzUnit').value==='-1'?'flex':'none'; };
  $('#wizardModal').classList.remove('hidden');
}

async function wizardNext(){
  if(wizardState.step===1){
    const name=$('#wzName').value.trim();
    if(!name){ alert('请填写工程名称'); return; }
    const unitVal=$('#wzUnit').value;
    const body={ name, ptype:$('#wzType').value, status:$('#wzStatus').value };
    if(unitVal==='-1'){
      body.new_unit=$('#wzNewUnit').value.trim();
      if(!body.new_unit){ alert('请填写新单位名称'); return; }
    } else {
      body.unit_id=parseInt(unitVal);
    }
    const r=await fetch(`${API}/project/create`,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    const j=await r.json();
    if(!j.ok){ alert('创建失败：'+(j.error||'')); return; }
    wizardState.newPid=j.id;
    wizardState.step=2;
    renderWizardUpload(j.id);
  } else if(wizardState.step===2){
    $('#wizardModal').classList.add('hidden');
    await loadSidebar('');
    selectProject(wizardState.newPid);
  }
}

function renderWizardUpload(pid){
  const types=state.config.doc_types||[];
  $('#wizardHint').textContent='第 2 步：按类别上传文件（没有的可跳过，系统会标记缺项）';
  $('#wizardNext').textContent='完成';
  $('#wizardBody').innerHTML = `
    <div class="wizard-step">
      <div style="font-size:12px;color:var(--muted);margin-bottom:10px">工程已创建（ID: ${pid}）。请将各类文件上传到对应类别，没有的跳过即可。</div>
      <div class="wz-upload-grid">
        ${types.map(t=>`<div class="wz-upload-zone" data-type="${t.code}" data-name="${esc(t.name)}">
          <div class="wz-zone-head"><span class="wz-zone-name">${esc(t.name)}</span>${t.required?'<span class="dtr-required">必备</span>':'<span class="dtr-optional">可选</span>'}</div>
          <div class="wz-zone-files" id="wzFiles_${t.code}"></div>
          <input type="file" multiple style="display:none" id="wzInput_${t.code}" onchange="wzUpload(${pid},'${t.code}','${esc(t.name)}',this)">
          <button class="upload-mini" onclick="document.getElementById('wzInput_${t.code}').click()">＋ 添加文件</button>
        </div>`).join('')}
      </div>
    </div>`;
}

async function wzUpload(pid, docType, typeName, input){
  const files=input.files;
  if(!files.length) return;
  const listEl=document.getElementById('wzFiles_'+docType);
  for(const f of files){
    const fd=new FormData();
    fd.append('project_id',pid);
    fd.append('doc_type',docType);
    fd.append('file',f);
    try{
      const r=await fetch(`${API}/upload`,{method:'POST',body:fd});
      const j=await r.json();
      if(j.ok){
        listEl.innerHTML+=`<div class="wz-file-item">${extIcon(f.name.split('.').pop())} ${esc(f.name)} <span class="wz-ok">✓</span></div>`;
      }
    }catch(e){}
  }
  input.value='';
}

init();

// ---------- 删除文物单位 ----------
async function deleteUnit(uid, name, projCount){
  const msg = projCount>0
    ? `确认删除文物单位「${name}」？\n\n该单位下有 ${projCount} 个工程，将一并放入回收站（可恢复）。`
    : `确认删除文物单位「${name}」？`;
  if(!confirm(msg)) return;
  try{
    const r=await fetch(`${API}/unit/${uid}`,{method:'DELETE'});
    const j=await r.json();
    if(j.ok){
      if(j.projects_deleted>0) alert(`已删除「${name}」及其 ${j.projects_deleted} 个工程（已放入回收站）。`);
      loadSidebar('');
      showDashboard();
    } else { alert('删除失败：'+(j.error||'')); }
  }catch(e){ alert('请求失败：'+e); }
}
