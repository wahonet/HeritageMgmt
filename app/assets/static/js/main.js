// 前端入口：初始化、事件绑定、hash 路由装配。
// 模块间通信一律走 import/export；仅 HTML 内联 onclick 所需的处理器集中注册到 window
// （取代原 sidebar.js 末尾散落的 window.x=x 巨型行）。后续可演进为事件委托以彻底去除。
import { state } from './core/state.js';
import { $ } from './core/dom.js';
import { API, get, setCSRFToken } from './core/api.js';
import { routeFromHash } from './core/router.js';
import { loadSidebar, filterSidebar, showDashboard, reImport, deleteUnit } from './views/dashboard.js';
import {
  selectProject, switchView, switchTab, openStage, showFileTree, openEdit,
  ocrScan, exportReport, deleteProject, openDoc, openUpload, closeUpload,
  doUpload, deleteDoc, deleteDocType, openDocByName, saveEdit,
} from './views/project.js';
import { showStats, switchStatsTab, toggleProjSelect, selectAllProj, renderCustomChart } from './views/stats.js';
import { showLogs } from './views/logs.js';
import { showBackup, doBackup, doRestore } from './views/backup.js';
import { showRecycle, restoreProject, purgeProject } from './views/recycle.js';
import { openWizard, wizardNext, wzUpload } from './views/wizard.js';

// 导出台账（顶栏动作，一行）
function exportLedger() { window.location.href = API + '/export/ledger'; }

// 事件绑定（原 sidebar.js bindEvents）
function bindEvents() {
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
  $('#previewClose').onclick = () => $('#previewModal').classList.add('hidden');
  $('#editCancel').onclick = () => $('#editModal').classList.add('hidden');
  $('#editSave').onclick = saveEdit;
  $('#treeClose').onclick = () => $('#treeModal').classList.add('hidden');
  $('#wizardClose').onclick = () => $('#wizardModal').classList.add('hidden');
  $('#wizardNext').onclick = wizardNext;
}

// 集中注册内联 onclick 所需的处理器（取代原散落的 window.x=x 巨型行）
function registerHandlers() {
  Object.assign(window, {
    selectProject, switchView, switchTab, openStage, showFileTree, openEdit,
    ocrScan, exportReport, deleteProject, openDoc, openUpload, deleteDoc, deleteDocType, openDocByName, saveEdit,
    renderCustomChart, toggleProjSelect, selectAllProj, switchStatsTab,
    doBackup, doRestore, restoreProject, purgeProject, wzUpload, deleteUnit,
  });
}

// 初始化
async function init() {
  bindEvents();
  registerHandlers();
  state.viewMode = localStorage.getItem('viewMode') || 'stack';
  state.config = await get('/config');
  setCSRFToken(state.config.csrf_token);
  await loadSidebar();
  // hash 路由: #project/<id> 直达工程，便于分享/截图
  window.addEventListener('hashchange', routeFromHash);
  routeFromHash();
}

init();
