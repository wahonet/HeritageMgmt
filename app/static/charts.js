function fmt(v){ v=Number(v||0); return Math.round(v).toLocaleString('zh-CN'); }
const METRICS={'funding':'中央指标','paid':'总已支付','pending':'总待支付','eng_contract':'工程合同价','eng_paid':'工程已支付','sup_contract':'监理合同价','sup_paid':'监理已支付','des_contract':'设计合同价','des_paid':'设计已支付'};
// 对比图: 每个指标对应的(总额key, 已付key, 总额名, 已付名)
const CMP={'funding':['funding','paid','中央指标','已支付'],'paid':['funding','paid','中央指标','已支付'],'pending':['funding','paid','中央指标','已支付'],'eng_contract':['eng_contract','eng_paid','工程合同价','工程已支付'],'eng_paid':['eng_contract','eng_paid','工程合同价','工程已支付'],'sup_contract':['sup_contract','sup_paid','监理合同价','监理已支付'],'sup_paid':['sup_contract','sup_paid','监理合同价','监理已支付'],'des_contract':['des_contract','des_paid','设计合同价','设计已支付'],'des_paid':['des_contract','des_paid','设计合同价','设计已支付']};
const CHART_COLORS=['#8B5E34','#C9A063','#2E8B57','#D68910','#6C8EAD','#9B59B6','#BDB5A6','#4A90A4'];
// 柱状图(单系列,柱顶标数值/百分比)
function barChart(title, items, color, opts){
  opts=opts||{};
  const short=s=>{s=s||'';return s.length>15?s.slice(0,15)+'…':s;};
  const rot=items.some(it=>(it.label||'').length>5)||items.length>7;
  const W=opts.grow?Math.min(1100,Math.max(560,items.length*58)):440,H=200,padL=46,padB=rot?60:38,padT=16,padR=10;
  const vals=items.map(i=>i.v||0);
  const total=vals.reduce((s,v)=>s+v,0)||1;
  const max=opts.asPct?100:Math.max(1,...vals);
  const slot=(W-padL-padR)/Math.max(1,items.length), bw=Math.min(slot*0.62,42);
  const lblV=v=>opts.asPct?(v/total*100).toFixed(1)+'%':fmt(v);
  const bars=items.map((it,i)=>{
    const val=opts.asPct?(it.v/total*100):(it.v||0);
    const h=(val/max)*(H-padT-padB), x=padL+i*slot+(slot-bw)/2, y=H-padB-h;
    return `<rect x="${x.toFixed(1)}" y="${y.toFixed(1)}" width="${bw.toFixed(1)}" height="${h.toFixed(1)}" fill="${color}" rx="3"><title>${esc(it.label)}: ${fmt(it.v||0)}万</title></rect>
      <text x="${(x+bw/2).toFixed(1)}" y="${(y-3).toFixed(1)}" text-anchor="middle" font-size="9" fill="#555">${lblV(it.v||0)}</text>`;
  }).join('');
  const labels=items.map((it,i)=>{
    const x=(padL+i*slot+slot/2).toFixed(1), y=H-padB+14, t=esc(short(it.label));
    return rot ? `<text x="${x}" y="${y}" text-anchor="end" font-size="10" fill="#666" transform="rotate(-30 ${x} ${y})">${t}</text>`
               : `<text x="${x}" y="${y}" text-anchor="middle" font-size="10" fill="#666">${t}</text>`;
  }).join('');
  const grid=[0,.5,1].map(g=>{const y=padT+(1-g)*(H-padT-padB);return `<line x1="${padL}" y1="${y.toFixed(1)}" x2="${W-padR}" y2="${y.toFixed(1)}" stroke="#eee"/><text x="${padL-5}" y="${(y+3).toFixed(1)}" text-anchor="end" font-size="8" fill="#999">${opts.asPct?(max*g).toFixed(0)+'%':fmt(max*g)}</text>`;}).join('');
  return `<div class="chart-card"${opts.grow?` style="max-width:${W}px"`:''}><h4>${esc(title)}</h4><svg viewBox="0 0 ${W} ${H}" class="chart">${grid}${bars}${labels}<line x1="${padL}" y1="${H-padB}" x2="${W-padR}" y2="${H-padB}" stroke="#ccc"/></svg></div>`;
}
// 分组柱状图: a vs b,每柱标数值,b柱内标占比
function groupBar(title, items, legendA, legendB, opts){
  opts=opts||{};
  legendA=legendA||'中央指标'; legendB=legendB||'已支付';
  const short=s=>{s=s||'';return s.length>15?s.slice(0,15)+'…':s;};
  const rot=items.some(it=>(it.label||'').length>5)||items.length>6;
  const W=opts.grow?Math.min(1100,Math.max(560,items.length*72)):440,H=200,padL=46,padB=rot?60:38,padT=22,padR=10;
  const max=Math.max(1,...items.flatMap(i=>[i.a||0,i.b||0]));
  const slot=(W-padL-padR)/Math.max(1,items.length), bw=Math.min(slot*0.36,22);
  const bars=items.map((it,i)=>{
    const gx=padL+i*slot+(slot-bw*2-3)/2;
    const ha=((it.a||0)/max)*(H-padT-padB), hb=((it.b||0)/max)*(H-padT-padB);
    const rate=(it.a>0)?(it.b/it.a*100).toFixed(0):0;
    return `<rect x="${gx.toFixed(1)}" y="${(H-padB-ha).toFixed(1)}" width="${bw.toFixed(1)}" height="${ha.toFixed(1)}" fill="#8B5E34" rx="2"><title>${esc(legendA)}:${fmt(it.a)}万</title></rect>
      <text x="${(gx+bw/2).toFixed(1)}" y="${(H-padB-ha-3).toFixed(1)}" text-anchor="middle" font-size="8" fill="#8B5E34">${fmt(it.a||0)}</text>
      <rect x="${(gx+bw+3).toFixed(1)}" y="${(H-padB-hb).toFixed(1)}" width="${bw.toFixed(1)}" height="${hb.toFixed(1)}" fill="#C9A063" rx="2"><title>${esc(legendB)}:${fmt(it.b)}万 占比${rate}%</title></rect>
      <text x="${(gx+bw+3+bw/2).toFixed(1)}" y="${(H-padB-hb-3).toFixed(1)}" text-anchor="middle" font-size="8" fill="#B8862A">${fmt(it.b||0)}</text>
      ${hb>16?`<text x="${(gx+bw+3+bw/2).toFixed(1)}" y="${(H-padB-hb/2+3).toFixed(1)}" text-anchor="middle" font-size="8" font-weight="700" fill="#fff">${rate}%</text>`:''}`;
  }).join('');
  const labels=items.map((it,i)=>{
    const x=(padL+i*slot+slot/2).toFixed(1), y=H-padB+14, t=esc(short(it.label));
    return rot?`<text x="${x}" y="${y}" text-anchor="end" font-size="10" fill="#666" transform="rotate(-30 ${x} ${y})">${t}</text>`:`<text x="${x}" y="${y}" text-anchor="middle" font-size="10" fill="#666">${t}</text>`;
  }).join('');
  const grid=[0,.5,1].map(g=>{const y=padT+(1-g)*(H-padT-padB);return `<line x1="${padL}" y1="${y.toFixed(1)}" x2="${W-padR}" y2="${y.toFixed(1)}" stroke="#eee"/><text x="${padL-5}" y="${(y+3).toFixed(1)}" text-anchor="end" font-size="8" fill="#999">${fmt(max*g)}</text>`;}).join('');
  return `<div class="chart-card"${opts.grow?` style="max-width:${W}px"`:''}><h4>${esc(title)}</h4><svg viewBox="0 0 ${W} ${H}" class="chart">${grid}${bars}${labels}<line x1="${padL}" y1="${H-padB}" x2="${W-padR}" y2="${H-padB}" stroke="#ccc"/></svg><div class="legend" style="margin-top:4px"><span class="lg"><i style="background:#8B5E34"></i>${esc(legendA)}</span><span class="lg"><i style="background:#C9A063"></i>${esc(legendB)}(柱内为占比)</span></div></div>`;
}
// 饼图(环形,图例显示 数值+占比)
function pieChart(title, items, colors, unit){
  unit=unit||'';
  const cx=80,cy=80,r=66,r2=40,total=items.reduce((s,i)=>s+(i.v||0),0)||1;
  let a=-Math.PI/2, paths='';
  items.forEach((it,i)=>{
    if(!it.v) return;
    const frac=it.v/total, a2=a+frac*Math.PI*2, lg=frac>0.5?1:0;
    const p=(rr,ang)=>[cx+rr*Math.cos(ang),cy+rr*Math.sin(ang)];
    const [x1,y1]=p(r,a),[x2,y2]=p(r,a2),[x3,y3]=p(r2,a2),[x4,y4]=p(r2,a);
    paths+=`<path d="M${x1.toFixed(1)} ${y1.toFixed(1)} A${r} ${r} 0 ${lg} 1 ${x2.toFixed(1)} ${y2.toFixed(1)} L${x3.toFixed(1)} ${y3.toFixed(1)} A${r2} ${r2} 0 ${lg} 0 ${x4.toFixed(1)} ${y4.toFixed(1)} Z" fill="${colors[i%colors.length]}"><title>${esc(it.k||it.label)}: ${fmt(it.v)}${unit} (${(it.v/total*100).toFixed(1)}%)</title></path>`;
    a=a2;
  });
  const legend=items.filter(i=>i.v).map((it,i)=>`<span class="lg"><i style="background:${colors[i%colors.length]}"></i>${esc(it.k||it.label)} ${fmt(it.v)}${unit} (${(it.v/total*100).toFixed(1)}%)</span>`).join('');
  return `<div class="chart-card"><h4>${esc(title)}</h4><div class="donut-wrap"><svg viewBox="0 0 160 160" class="donut">${paths}<text x="80" y="78" text-anchor="middle" font-size="16" font-weight="700" fill="#6B4423">${fmt(total)}</text><text x="80" y="92" text-anchor="middle" font-size="9" fill="#888">合计${unit}</text></svg><div class="legend">${legend}</div></div></div>`;
}
async function showStats(){
  state.currentProjectId=null;
  document.querySelectorAll('.proj-item').forEach(e=>e.classList.remove('active'));
  history.replaceState(null,'','#stats');
  $('#content').innerHTML='<div class="loading">加载统计中…</div>';
  const d=await fetch(API+'/stats').then(r=>r.json());
  state.statsData=d;
  const projs=await fetch(API+'/projects').then(r=>r.json());
  state.projectsList=projs.map(p=>({id:p.id, k:p.name, funding:p.central_funding||0, paid:p.total_paid||0,
    pending:(p.central_funding||0)-(p.total_paid||0), eng_contract:p.eng_contract||0, eng_paid:p.eng_paid||0,
    sup_contract:p.sup_contract||0, sup_paid:p.sup_paid||0, des_contract:p.des_contract||0, des_paid:p.des_paid||0}));
  const t=d.total||{};
  const yearItems=d.by_year.map(y=>({label:y.k,a:y.funding,b:y.paid}));
  const unitItems=d.by_unit.map(u=>({label:u.k,a:u.funding,b:u.paid}));
  const mopts=Object.entries(METRICS).map(([k,v])=>`<option value="${k}">${v}</option>`).join('');
  const projChecks=state.projectsList.map((p,i)=>`<label class="ps-item"><input type="checkbox" value="${p.id}" ${i<5?'checked':''} onchange="renderCustomChart()"><span title="${esc(p.k)}">${esc(p.k)}</span></label>`).join('');
  const fixedHTML=`<div class="stats-summary">中央指标合计 <b>${fmt(t.funding)}</b>万元 ｜ 已支付 <b>${fmt(t.paid)}</b>万元 ｜ 待支付 <b>${fmt(t.pending)}</b>万元 ｜ 整体支付率 <b>${t.funding>0?(t.paid/t.funding*100).toFixed(1):'—'}%</b></div>
    <div class="charts-grid">
      ${groupBar('各年度 中央指标 vs 已支付（万元）',yearItems)}
      ${groupBar('各文物单位 中央指标 vs 已支付（万元）',unitItems)}
    </div>`;
  const customHTML=`<div class="cc-controls">
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

  $('#content').innerHTML=`<div class="stats-view">
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
function switchStatsTab(tab){
  document.querySelectorAll('.stats-tab').forEach(b=>b.classList.toggle('active', b.dataset.stab===tab));
  $('#statsFixed').classList.toggle('hidden', tab!=='fixed');
  $('#statsCustom').classList.toggle('hidden', tab!=='custom');
}
function toggleProjSelect(){
  const isProj=$('#ccGroup').value==='proj';
  document.getElementById('projSelect').classList.toggle('hidden', !isProj);
  renderCustomChart();
}
function selectAllProj(on){ document.querySelectorAll('#projSelect input[type=checkbox]').forEach(c=>c.checked=on); renderCustomChart(); }
function renderCustomChart(){
  const d=state.statsData; if(!d) return;
  const type=$('#ccType').value, grp=$('#ccGroup').value, mkey=$('#ccMetric').value, disp=$('#ccDisp').value;
  const grpName={by_unit:'文保单位',by_type:'工程类型',by_year:'年份',proj:'工程'}[grp];
  let rows;
  if(grp==='proj'){
    const ids=[...document.querySelectorAll('#projSelect input:checked')].map(c=>c.value);
    rows=(state.projectsList||[]).filter(p=>ids.includes(String(p.id)));
  } else {
    rows=d[grp]||[];
  }
  let html='';
  if(type==='cmp'){
    const pair=CMP[mkey]||CMP.funding;
    const items=rows.map(r=>({label:r.k, a:r[pair[0]]||0, b:r[pair[1]]||0}));
    html=groupBar(`${pair[2]} vs ${pair[3]} · 按${grpName}（万元）`, items, pair[2], pair[3], {grow:true});
  } else {
    const items=rows.map(r=>({label:r.k, k:r.k, v:r[mkey]||0}));
    const filt = grp==='proj' ? items : items.filter(r=>r.v>0);
    const title=`${METRICS[mkey]} · 按${grpName}（万元）`;
    html = type==='pie' ? pieChart(title, filt, CHART_COLORS, '万') : barChart(title, filt, CHART_COLORS[0], {asPct:disp==='pct', unit:'万', grow:true});
  }
  $('#ccResult').innerHTML=html||'<div class="hint">请选择工程或该指标暂无数据</div>';
}

