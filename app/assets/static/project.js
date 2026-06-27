async function selectProject(pid){
  state.currentProjectId = pid;
  history.replaceState(null,'',`#project/${pid}`);   // 更新地址栏(不触发hashchange)
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  $('#content').innerHTML = '<div class="loading">加载中…</div>';
  const data = await fetch(`${API}/project/${pid}`).then(r=>r.json());
  renderProject(data);
  // 同步侧栏高亮
  loadSidebar($('#searchInput').value);
}

function renderProject(data){
  state.lastData = data;
  const p = data.project;
  const c = $('#content');
  const statusCls = (p.status||'前期');
  const toggle = `<select class="view-select" onchange="switchView(this.value)">
      <option value="stack"${state.viewMode==='stack'?' selected':''}>分段视图</option>
      <option value="tab"${state.viewMode==='tab'?' selected':''}>标签视图</option>
    </select>`;
  const header = `<div class="detail-header">
      <div class="dh-left">
        <h2 class="dh-title">${esc(p.name)}</h2>
        <span class="status-pill ${statusCls}">${esc(p.status||'—')}</span>
        ${p.approval_no?`<span class="dh-approval">批复文号：${esc(p.approval_no)}</span>`:''}
      </div>
      <div class="dh-right">
        ${toggle}
        <button class="btn btn-sm" onclick="showFileTree(${p.id})">文件结构</button>
        <button class="btn btn-sm" onclick="openEdit(${p.id})">编辑台账</button>
        <button class="btn btn-sm" onclick="ocrScan(${p.id},this)">OCR识别合同</button>
        <button class="btn btn-sm" onclick="exportReport(${p.id})">导出报告</button>
        <button class="btn btn-sm btn-del-project" onclick="deleteProject(${p.id},'${esc(p.name).replace(/'/g,"\\'")}')">删除项目</button>
      </div>
    </div>`;

  if(state.viewMode==='tab'){
    const tabs=[['basic','工程基本信息'],['progress','进展情况'],['finance','资金情况'],['flow','工程流程图']];
    const tabBar=`<div class="tab-bar">${tabs.map(t=>`<button class="tab-btn ${state.activeTab===t[0]?'active':''}" data-tab="${t[0]}" onclick="switchTab('${t[0]}')">${t[1]}</button>`).join('')}</div>`;
    c.innerHTML=header+tabBar+`<div id="tabContent" class="tab-content"></div>`;
    renderTabContent();
    return;
  }

  // ===== 分段视图 =====
  const payRate = (p.central_funding && p.central_funding>0 && p.total_paid!=null) ? (p.total_paid/p.central_funding*100).toFixed(1)+'%' : '—';
  const finCell = (k,v) => `<div class="fin-cell"><div class="fc-val">${wan(v)}</div><div class="fc-key">${k}</div></div>`;
  const finRow1 = finCell('总指标',p.central_funding)+finCell('工程合同价',p.eng_contract)+finCell('监理合同价',p.sup_contract)+finCell('设计合同价',p.des_contract);
  const finRow2 = finCell('总支付',p.total_paid)+finCell('工程已付',p.eng_paid)+finCell('监理已付',p.sup_paid)+finCell('设计已付',p.des_paid);
  const stages = data.stages.map((s,i)=>{
    const missingReq = s.types.filter(t=>t.required && t.count===0);
    const cls = s.doc_count>0 ? 'has' : (missingReq.length ? 'missing' : 'empty');
    const flag = missingReq.length ? `<span class="sn-flag">缺${missingReq.length}</span>` : '';
    return `<div class="stage-node ${cls}" data-stage="${s.code}" onclick="openStage('${s.code}')">
      ${flag}<div class="sn-name">${esc(s.name)}</div>
      <div class="sn-count">${s.doc_count} 份</div></div>
      ${i<data.stages.length-1?'<div class="stage-arrow"></div>':''}`;
  }).join('');

  c.innerHTML = header + `
    <div class="section-row">
      <div class="section">
        <div class="section-h">一、工程基本信息</div>
        <div class="section-body meta-grid">
          <div class="meta-item"><span class="mi-k">文保工程类型</span><span class="mi-v">${esc(p.ptype||'—')}</span></div>
          <div class="meta-item"><span class="mi-k">当前状态</span><span class="mi-v"><span class="status-pill ${statusCls}">${esc(p.status||'—')}</span></span></div>
          <div class="meta-item"><span class="mi-k">设计单位</span><span class="mi-v">${esc(p.design_unit||'—')}</span></div>
          <div class="meta-item"><span class="mi-k">施工单位</span><span class="mi-v">${esc(p.construction_unit||'—')}</span></div>
        </div>
      </div>
      <div class="section">
        <div class="section-h">二、进展情况</div>
        <div class="section-body meta-grid">
          <div class="meta-item"><span class="mi-k">合同工期</span><span class="mi-v">${fmtDate(p.sign_date)} 至 ${fmtDate(p.contract_end)}</span></div>
          <div class="meta-item"><span class="mi-k">实际完工时间</span><span class="mi-v">${fmtDate(p.complete_date)}</span></div>
          <div class="meta-item"><span class="mi-k">支付率</span><span class="mi-v">${payRate}</span></div>
        </div>
      </div>
    </div>

    <div class="section">
      <div class="section-h">三、资金情况（万元）</div>
      <div class="section-body fin-block">
        <div class="fin-grid">${finRow1}</div>
        <div class="fin-grid">${finRow2}</div>
      </div>
    </div>

    <div class="section">
      <div class="section-h">四、工程流程图（点击节点查看 / 上传资料）</div>
      <div class="section-body">
        <div class="flowchart">${stages}</div>
        <div id="stagePanel" style="margin-top:14px"></div>
      </div>
    </div>`;
  openStage(data.stages[0].code);
}

// ===== 视图 / 标签切换 =====
function switchView(mode){ state.viewMode=mode; localStorage.setItem('viewMode',mode); selectProject(state.currentProjectId); }
function switchTab(code){ state.activeTab=code; document.querySelectorAll('.tab-btn').forEach(b=>b.classList.toggle('active', b.dataset.tab===code)); renderTabContent(); }
function renderTabContent(){
  const data=state.lastData; if(!data) return;
  const el=$('#tabContent'); if(!el) return;
  let html='';
  if(state.activeTab==='basic') html=tabBasic(data);
  else if(state.activeTab==='progress') html=tabProgress(data.project);
  else if(state.activeTab==='finance') html=tabFinance(data.project);
  else if(state.activeTab==='flow') html=tabFlow(data);
  el.innerHTML=html;
}

// ===== 标签内容：工程基本信息（详细） =====
function tabBasic(data){
  const p=data.project;
  const qual=data.qual_warnings||[];
  const qualHtml=qual.length
    ? `<div class="alert-bar warn"><div class="t">资质校验提示（国保要求：设计甲级 / 施工一级 / 监理甲级）</div><div class="tags">${qual.map(m=>`<span class="tag-miss">${esc(m)}</span>`).join('')}</div></div>`
    : `<div class="alert-bar ok"><div class="t">资质校验通过</div></div>`;
  const unit=(name,ql)=> name ? `${esc(name)}${ql?` <span class="ql-pill">${esc(ql)}</span>`:''}` : '—';
  const row=(k,v)=>`<div class="meta-item"><span class="mi-k">${k}</span><span class="mi-v">${v==null||v===''?'—':v}</span></div>`;
  return `<div class="tab-pane">
    <div class="meta-grid detail-grid">
      ${row('文物保护单位',esc(p.unit_name))}
      ${row('级别',esc(levelName(data.unit_level)))}
      ${row('工程名称',esc(p.name))}
      ${row('文保工程类型',esc(p.ptype))}
      ${row('当前状态',`<span class="status-pill ${p.status||'前期'}">${esc(p.status||'—')}</span>`)}
      ${row('批复文号',esc(p.approval_no))}
      ${row('合同编号',esc(p.contract_no))}
      ${row('合同签订日期',fmtDate(p.contract_sign_date))}
      ${row('序号',p.seq!=null?p.seq:'—')}
      ${row('建设单位',esc(p.owner_unit))}
      ${row('施工单位',unit(p.construction_unit,p.construction_qual))}
      ${row('设计单位',unit(p.design_unit,p.design_qual))}
      ${row('监理单位',unit(p.supervision_unit,p.supervision_qual))}
    </div>
    ${qualHtml}
    ${p.progress_note?`<div class="meta-item full"><span class="mi-k">进展说明</span><span class="mi-v">${esc(p.progress_note)}</span></div>`:''}
  </div>`;
}

// ===== 标签内容：进展情况（详细，含时间轴） =====
function tabProgress(p){
  const days=dateDiff(p.sign_date,p.contract_end);
  const late=dateDiff(p.contract_end,p.complete_date);
  const payRate=(p.central_funding&&p.central_funding>0&&p.total_paid!=null)?(p.total_paid/p.central_funding*100):null;
  const row=(k,v)=>`<div class="meta-item"><span class="mi-k">${k}</span><span class="mi-v">${v==null||v===''?'—':v}</span></div>`;
  let overdueTxt='—';
  if(late!=null) overdueTxt = late>0 ? `<span style="color:var(--red)">逾期 ${late} 天</span>` : `<span style="color:var(--green)">按期完工（提前 ${-late} 天）</span>`;
  else if(p.sign_date) overdueTxt = `<span style="color:var(--amber)">在建中</span>`;
  const bar = payRate!=null ? `<div class="payrate"><span style="font-size:12px;color:var(--muted)">资金支付率</span><div class="bar"><i style="width:${Math.min(100,payRate).toFixed(1)}%"></i></div><b style="color:var(--primary-dark)">${payRate.toFixed(1)}%</b></div>` : '';
  return `<div class="tab-pane">
    <div class="meta-grid">
      ${row('开工日期',fmtDate(p.sign_date))}
      ${row('合同约定竣工',fmtDate(p.contract_end))}
      ${row('实际完工时间',fmtDate(p.complete_date))}
      ${row('合同工期',days!=null?days+' 天':'—')}
      ${row('完工情况',overdueTxt)}
    </div>
    ${bar}
  </div>`;
}

// ===== 标签内容：资金情况（详细，含分项表与柱状图） =====
function tabFinance(p){
  const totContract=(p.eng_contract||0)+(p.sup_contract||0)+(p.des_contract||0);
  const totPaid=p.total_paid||0;
  const balance=totContract-totPaid;
  const rate=totContract>0?(totPaid/totContract*100).toFixed(1):null;
  const big=(k,v,cls='')=>`<div class="fin-cell ${cls}"><div class="fc-val">${wan(v)}</div><div class="fc-key">${k}</div></div>`;
  const overview=`<div class="fin-grid">${big('总指标',p.central_funding)}${big('总合同价',totContract)}${big('总已付',totPaid)}${big('总结余',balance)}</div>`;
  const catRow=(name,contract,paid)=>{
    const r=(contract&&contract>0)?(paid/contract*100).toFixed(1)+'%':'—';
    const b=(contract!=null&&paid!=null)?(contract-paid):null;
    return `<tr><td>${name}</td><td>${wan(contract)}</td><td>${wan(paid)}</td><td>${r}</td><td>${wan(b)}</td></tr>`;
  };
  const table=`<table class="fin-table"><thead><tr><th>分项</th><th>合同价(万)</th><th>已付(万)</th><th>支付率</th><th>结余(万)</th></tr></thead><tbody>
    ${catRow('工程',p.eng_contract,p.eng_paid)}${catRow('监理',p.sup_contract,p.sup_paid)}${catRow('设计',p.des_contract,p.des_paid)}
    <tr><td>专家费</td><td>—</td><td>${wan(p.expert_fee)}</td><td>—</td><td>—</td></tr>
  </tbody></table>`;
  return `<div class="tab-pane">
    <div style="font-size:12px;color:var(--muted);margin-bottom:8px">整体支付率：<b style="color:var(--primary-dark)">${rate!=null?rate+'%':'—'}</b></div>
    ${overview}
    <div style="margin-top:16px">${table}</div>
  </div>`;
}

// ===== 标签内容：工程流程图（全部展开） =====
function tabFlow(data){
  const p=data.project;
  const blocks=data.stages.map(s=>{
    const hasMissing=s.types.some(t=>t.required&&t.count===0);
    const statusTxt=hasMissing?'待补全':(s.doc_count>0?'已完成':'待办');
    const statusCls=hasMissing?'miss':(s.doc_count>0?'done':'todo');
    const typeRows=s.types.map(t=>{
      const docs=data.documents.filter(d=>d.doc_type===t.code);
      const reqTag=t.required?'<span class="dtr-required">必备</span>':'<span class="dtr-optional">可选</span>';
      const short=s=>s.length>16?s.slice(0,16)+'…':s;
      const chips=docs.length?docs.map(d=>`<span class="doc-chip" onclick="openDoc(${d.id},'${esc(d.orig_name)}')" title="${esc(d.title)}">${extIcon(d.file_ext)} ${esc(short(d.title))}</span>`).join(''):'<span class="no-file">暂无</span>';
      return `<div class="ftype-row"><div class="ftype-label">${reqTag}<span class="ftype-name">${esc(t.name)}</span><span class="ftype-cnt">${t.count}</span></div><div class="ftype-files">${chips}<span class="upload-mini" onclick="openUpload(${p.id},'${t.code}','${esc(t.name)}')">＋上传</span></div></div>`;
    }).join('');
    return `<div class="stage-block"><div class="sb-head"><span class="sb-name">${esc(s.name)}</span><span class="sb-status ${statusCls}">${statusTxt}</span><span class="sb-cnt">${s.doc_count} 份</span></div>${typeRows}</div>`;
  }).join('');
  return `<div class="tab-pane flow-detail">${blocks}</div>`;
}

// 阶段文档面板
function openStage(code){
  state.activeStage = code;
  document.querySelectorAll('.stage-node').forEach(n=>n.classList.toggle('active', n.dataset.stage===code));
  // 重新拉取最新工程数据
  fetch(`${API}/project/${state.currentProjectId}`).then(r=>r.json()).then(data=>{
    const s = data.stages.find(x=>x.code===code);
    if(!s) return;
    const panel = $('#stagePanel');
    let rows='';
    for(const t of s.types){
      const docs = data.documents.filter(d=>d.doc_type===t.code);
      const reqTag = t.required ? '<span class="dtr-required">必备</span>' : '<span class="dtr-optional">可选</span>';
      const missTag = (!docs.length && t.required) ? '<span class="dtr-miss">缺失</span>' : '';
      let cards='';
      for(const d of docs){
        cards += `<div class="doc-card" onclick="openDoc(${d.id},'${esc(d.orig_name)}')">
          <span class="ic">${extIcon(d.file_ext)}</span>
          <div class="dc-body"><div class="dc-title" title="${esc(d.title)}">${esc(d.title)}</div>
          <div class="dc-meta">${esc(d.file_ext||'').toUpperCase()} · ${(d.file_size/1024).toFixed(0)}KB</div></div>
          <span class="dc-del" title="删除" onclick="event.stopPropagation();deleteDoc(${d.id},'${esc(d.title)}')">删除</span>
        </div>`;
      }
      const addBtn = `<span class="upload-mini" onclick="openUpload(${data.project.id},'${t.code}','${esc(t.name)}')">＋ 上传${esc(t.name)}</span>`;
      rows += `<div class="doc-type-row">
        <div class="dtr-head"><span class="dtr-name">${esc(t.name)}</span>${reqTag}${missTag}${addBtn}</div>
        <div class="doc-grid">${cards||'<div style="color:var(--muted);font-size:12px;padding:4px">暂无文件</div>'}</div>
      </div>`;
    }
    panel.innerHTML = `<div class="stage-panel">
      <h3>${esc(s.name)} <span style="font-size:12px;color:var(--muted);font-weight:400">（共 ${s.doc_count} 份）</span></h3>
      ${rows}</div>`;
  });
}

// 打开文件
function openDoc(id, name){
  $('#previewTitle').textContent = name;
  $('#previewFrame').src = `${API}/file/${id}`;
  $('#previewModal').classList.remove('hidden');
}

// 上传
function openUpload(pid, docType, typeName){
  state.uploadCtx = {pid, docType, typeName};
  $('#uploadTitle').textContent = '上传文件';
  $('#uploadTypeLabel').textContent = `${typeName}（将归档到该工程专属文件夹）`;
  $('#uploadFile').value='';
  $('#uploadResult').innerHTML='';
  $('#uploadModal').classList.remove('hidden');
}
function closeUpload(){ $('#uploadModal').classList.add('hidden'); state.uploadCtx=null; }

async function doUpload(){
  const ctx = state.uploadCtx; if(!ctx) return;
  const files = $('#uploadFile').files;
  if(!files.length){ $('#uploadResult').innerHTML='<div class="err">请选择文件</div>'; return; }
  const res = $('#uploadResult'); res.innerHTML='<div>上传中…</div>';
  let ok=0, fail=0;
  for(const f of files){
    const fd = new FormData();
    fd.append('project_id', ctx.pid);
    fd.append('doc_type', ctx.docType);
    fd.append('file', f);
    try{
      const r = await fetch(`${API}/upload`,{method:'POST',body:fd});
      const j = await r.json();
      if(j.ok) ok++; else fail++;
    }catch(e){ fail++; }
  }
  res.innerHTML = `<div class="${ok?'ok':'err'}">成功 ${ok} 个${fail?`，失败 ${fail} 个`:''}</div>`;
  // 刷新(两种视图都覆盖)
  if(state.currentProjectId) selectProject(state.currentProjectId);
  if(ok && !fail) setTimeout(closeUpload, 800);
}

async function deleteDoc(id, name){
  if(!confirm(`确认删除「${name}」？\n（同时删除归档文件夹中的文件，不可恢复）`)) return;
  const r = await fetch(`${API}/document/${id}`,{method:'DELETE'}).then(r=>r.json());
  if(r.ok){ selectProject(state.currentProjectId); }
  else alert('删除失败：'+(r.error||''));
}


function openEdit(pid){
  fetch(API+'/project/'+pid).then(r=>r.json()).then(d=>{
    const p=d.project; editCtx={pid};
    const F=(label,field,type,val)=>`<div class="ef"><label>${label}</label><input data-f="${field}" type="${type||'text'}" value="${esc(val==null?'':val)}"></div>`;
    const grp=(t,html)=>`<div class="ef-grp"><div class="ef-grp-t">${t}</div><div class="ef-grp-b">${html}</div></div>`;
    $('#editBody').innerHTML=`
      ${grp('基本信息', F('工程名称','name','text',p.name)+F('工程类型','ptype','text',p.ptype)+F('状态','status','text',p.status)+F('批复文号','approval_no','text',p.approval_no)+F('合同编号','contract_no','text',p.contract_no)+F('合同签订日期','contract_sign_date','date',p.contract_sign_date)+F('开工日期','sign_date','date',p.sign_date)+F('合同约定竣工日期','contract_end','date',p.contract_end)+F('实际完工日期','complete_date','date',p.complete_date))}
      ${grp('参建单位与资质 <span style="font-weight:400;font-size:11px;color:var(--muted)">（国保：设计甲级/施工一级/监理甲级）</span>', F('建设单位','owner_unit','text',p.owner_unit)+F('施工单位','construction_unit','text',p.construction_unit)+F('施工资质','construction_qual','text',p.construction_qual)+F('设计单位','design_unit','text',p.design_unit)+F('设计资质','design_qual','text',p.design_qual)+F('监理单位','supervision_unit','text',p.supervision_unit)+F('监理资质','supervision_qual','text',p.supervision_qual))}
      ${grp('财务（万元）', F('中央指标','central_funding','number',p.central_funding)+F('工程合同','eng_contract','number',p.eng_contract)+F('工程已付','eng_paid','number',p.eng_paid)+F('监理合同','sup_contract','number',p.sup_contract)+F('监理已付','sup_paid','number',p.sup_paid)+F('设计合同','des_contract','number',p.des_contract)+F('设计已付','des_paid','number',p.des_paid)+F('专家费','expert_fee','number',p.expert_fee)+F('已付合计','total_paid','number',p.total_paid))}
      ${grp('其他', `<div class="ef" style="width:100%"><label>项目进展情况</label><textarea data-f="progress_note" rows="2">${esc(p.progress_note||'')}</textarea></div>`)}
    `;
    $('#editModal').classList.remove('hidden');
  });
}
async function saveEdit(){
  const data={};
  document.querySelectorAll('#editBody [data-f]').forEach(inp=>{ data[inp.dataset.f]=inp.value; });
  const r=await fetch(API+'/project/'+editCtx.pid,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
  const j=await r.json();
  if(j.ok){ $('#editModal').classList.add('hidden'); await loadSidebar($('#searchInput').value); if(state.currentProjectId) selectProject(state.currentProjectId); }
  else alert('保存失败：'+(j.error||''));
}

// ---------- OCR识别合同 ----------
const FIELD_LABELS={construction_unit:'施工单位',construction_qual:'施工资质',design_unit:'设计单位',design_qual:'设计资质',supervision_unit:'监理单位',supervision_qual:'监理资质',contract_end:'合同约定竣工'};
async function ocrScan(pid,btn){
  if(!confirm('将对本工程的项目/设计/监理合同PDF进行OCR识别,并调用DeepSeek自动提取参建单位、资质、合同约定竣工日期。\n\n前提:本机已装 Tesseract+chi_sim 与 PDF渲染工具,且联网。\n继续?')) return;
  const old=btn.textContent; btn.disabled=true; btn.textContent='识别中(约30-60秒)...';
  try{
    const r=await fetch(API+'/ocr/scan?project_id='+pid,{method:'POST'});
    const j=await r.json();
    if(j.ok){
      const f=j.fields||{};
      const lines=Object.keys(FIELD_LABELS).map(k=>`${FIELD_LABELS[k]}：${f[k]||'(空)'}`).join('\n');
      alert('OCR识别完成，回填 '+j.applied+' 个字段：\n\n'+lines);
      selectProject(pid);
    }else{
      alert('OCR失败：\n'+(j.error||'未知错误'));
    }
  }catch(e){ alert('请求失败：'+e); }
  btn.disabled=false; btn.textContent=old;
}


// ---------- 文件结构树 ----------
const STAGE_NAMES={approve:'方案审批',funding:'资金下达',design:'勘察设计',bidding:'招标与合同',supervision:'监理委托',construction:'施工开工',review:'过程评审',acceptance:'竣工验收',archive:'归档'};
async function showFileTree(pid){
  $('#treeTitle').textContent='文件结构';
  $('#treeBody').innerHTML='<div class="loading">加载中…</div>';
  $('#treeModal').classList.remove('hidden');
  const data=await fetch(`${API}/project/${pid}/tree`).then(r=>r.json());
  if(!data||!data.length){ $('#treeBody').innerHTML='<div class="hint">暂无归档文件</div>'; return; }
  const sizeStr=s=> s>1048576?(s/1048576).toFixed(1)+'MB':Math.max(1,Math.round(s/1024))+'KB';
  const extTag=e=>{e=(e||'').toUpperCase();return e?`<span class="ext-tag">${e}</span>`:'';};
  let html='<div class="file-tree">';
  for(const d of data){
    const stageName=STAGE_NAMES[d.stage]||d.stage||'';
    const reqTag=d.required?'<span class="dtr-required">必备</span>':'<span class="dtr-optional">可选</span>';
    const fileCount=d.files?d.files.length:0;
    const hasFiles=fileCount>0;
    html+=`<div class="ft-folder ${hasFiles?'':'ft-empty'}" id="ftf_${d.code}">
      <div class="ft-head"><span class="ft-icon">${hasFiles?'▾':'▸'}</span>
        <span class="ft-label">${esc(d.label)}</span>${reqTag}
        <span class="ft-stage">${esc(stageName)}</span>
        <span class="ft-count">${fileCount}个文件</span>
        <span class="ft-del" title="删除此分类全部文件" onclick="deleteDocType(${pid},'${d.code}','${esc(d.label).replace(/'/g,"\\'")}')">删除分类</span></div>`;
    if(hasFiles){
      html+='<div class="ft-files">';
      for(const f of d.files){
        html+=`<div class="ft-file" onclick="openDocByName(${pid},'${esc(f.name).replace(/'/g,"\'")}')">
          ${extTag(f.ext)}<span class="ft-fname" title="${esc(f.name)}">${esc(f.name)}</span>
          <span class="ft-fsize">${sizeStr(f.size)}</span></div>`;
      }
      html+='</div>';
    }
    html+='</div>';
  }
  html+='</div>';
  $('#treeBody').innerHTML=html;
}
async function openDocByName(pid, name){
  // 先查文档ID
  const d=await fetch(`${API}/project/${pid}`).then(r=>r.json());
  const doc=(d.documents||[]).find(x=>x.orig_name===name);
  if(doc){ openDoc(doc.id, name); }
}

// ---------- 删除项目 ----------
async function deleteProject(pid, name){
  if(!confirm(`确认删除「${name}」？\n\n删除后将放入回收站（data/recycle/），数据库记录保留可恢复，但该工程将从列表中移除。`)) return;
  try{
    const r=await fetch(`${API}/project/${pid}`,{method:'DELETE'});
    const j=await r.json();
    if(j.ok){
      alert('已删除，工程已放入回收站。');
      state.currentProjectId=null;
      history.replaceState(null,'','#');
      await loadSidebar('');
      showDashboard();
    } else {
      alert('删除失败：'+(j.error||''));
    }
  }catch(e){ alert('请求失败：'+e); }
}

// ---------- 删除分类 ----------
async function deleteDocType(pid, docType, label){
  if(!confirm(`确认删除「${label}」分类下的全部文件？\n\n将同时删除数据库记录和磁盘文件，不可恢复。`)) return;
  try{
    const r=await fetch(`${API}/project/${pid}/doctype/${docType}`,{method:'DELETE'});
    const j=await r.json();
    if(j.ok){
      const el=document.getElementById(`ftf_${docType}`);
      if(el) el.remove();
      alert(`已删除${j.deleted}个文件。`);
    } else { alert('删除失败：'+(j.error||'')); }
  }catch(e){ alert('请求失败：'+e); }
}

// ---------- 导出报告 ----------
function exportReport(pid){
  if(!confirm('将生成工程报告PDF（含大模型智能分析），需要联网调用API，约30-60秒。继续？')) return;
  window.location.href=API+'/project/'+pid+'/report';
}
