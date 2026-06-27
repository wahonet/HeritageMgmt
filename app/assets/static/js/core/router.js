// Hash 路由（原 sidebar.js routeFromHash）。
// #project/<id> 直达工程、#edit/<id> 进工程并打开编辑、#stats/#logs 等。
import { state } from './state.js';
import { selectProject, openEdit } from '../views/project.js';
import { showDashboard } from '../views/dashboard.js';
import { showStats } from '../views/stats.js';
import { showLogs } from '../views/logs.js';

export function routeFromHash() {
  const h = location.hash || '';
  const m = /^#?\/?project\/(\d+)/.exec(h);
  if (m) { selectProject(parseInt(m[1])); return; }
  const e = /^#?\/?edit\/(\d+)/.exec(h);
  if (e) { selectProject(parseInt(e[1])); setTimeout(() => openEdit(parseInt(e[1])), 400); return; }
  if (/stats/.test(h)) { showStats(); return; }
  if (/logs/.test(h)) { showLogs(); return; }
  if (!state.currentProjectId) showDashboard();
}
