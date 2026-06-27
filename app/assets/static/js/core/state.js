// 单一状态容器（取代散落的全局 state / editCtx / wizardState）。
// 各视图 import 同一对象，按引用共享。
export const state = {
  config: null,        // /api/config 结果（doc_types / workflow / project_types）
  units: [],
  currentProjectId: null,
  uploadCtx: null,     // 当前上传上下文 {pid, docType, typeName}
  activeStage: null,   // 工程流程图当前选中阶段
  viewMode: 'stack',   // 工程详情视图模式 stack|tab
  activeTab: 'basic',  // 标签视图当前页
  lastData: null,      // 最近一次工程详情数据（供 openStage 复用，避免冗余 refetch）
  statsData: null,     // 统计聚合数据
  projectsList: null,  // 工程精简列表（统计自定义图表用）
};
