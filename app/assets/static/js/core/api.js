// 统一 API 客户端：封装所有 fetch，统一 JSON 解析（含非 JSON 响应兜底）与 CSRF 头。
// 取代散落在各视图里的 fetch(API+...).then(r=>r.json())。
export const API = '/api';

let csrfToken = '';
export function setCSRFToken(t) { csrfToken = t || ''; }

// 解析响应：JSON 优先；非 JSON（如错误页/纯文本）兜底为 {ok:false,error}，避免 r.json() 抛错卡住 UI。
async function json(r) {
  const ct = r.headers.get('content-type') || '';
  if (ct.includes('application/json')) return r.json();
  const txt = await r.text();
  return { ok: false, error: (txt || '').slice(0, 200) || `HTTP ${r.status}` };
}

// 非 GET 请求附加 CSRF 头（token 由 /api/config 下发，恶意网页跨域拿不到）。
function withCSRF(headers) {
  return Object.assign({}, headers, { 'X-Heritage-CSRF': csrfToken });
}

// GET → 解析 JSON（GET 免 CSRF）
export async function get(path) {
  return json(await fetch(API + path));
}

// POST；isForm=true 时 body 为 FormData（不设 Content-Type）
export async function post(path, body, isForm) {
  const opts = { method: 'POST', headers: withCSRF() };
  if (isForm) {
    opts.body = body;
  } else {
    opts.headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(body);
  }
  return json(await fetch(API + path, opts));
}

// PUT JSON
export async function put(path, body) {
  return json(await fetch(API + path, {
    method: 'PUT',
    headers: withCSRF({ 'Content-Type': 'application/json' }),
    body: JSON.stringify(body),
  }));
}

// DELETE → 解析 JSON
export async function del(path) {
  return json(await fetch(API + path, { method: 'DELETE', headers: withCSRF() }));
}
