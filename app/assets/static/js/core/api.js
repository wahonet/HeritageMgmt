// 统一 API 客户端：封装所有 fetch，统一 JSON 解析与错误处理。
// 取代散落在各视图里的 fetch(API+...).then(r=>r.json())。
export const API = '/api';

async function json(r) { return r.json(); }

// GET → 解析 JSON
export async function get(path) {
  return json(await fetch(API + path));
}

// POST；isForm=true 时 body 为 FormData（不设 Content-Type）
export async function post(path, body, isForm) {
  const opts = { method: 'POST' };
  if (isForm) {
    opts.body = body;
  } else {
    opts.headers = { 'Content-Type': 'application/json' };
    opts.body = JSON.stringify(body);
  }
  return json(await fetch(API + path, opts));
}

// PUT JSON
export async function put(path, body) {
  return json(await fetch(API + path, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  }));
}

// DELETE → 解析 JSON
export async function del(path) {
  return json(await fetch(API + path, { method: 'DELETE' }));
}
