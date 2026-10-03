const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync('include/web_ui.h', 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes = new Map();
function element() {
  return {
    textContent: '', value: '', style: {}, dataset: {}, children: [],
    classList: { toggle() {} },
    elements: { trails: { checked: true }, namedItem() { return null; } },
    querySelectorAll() { return []; },
    append(child) { this.children.push(child); },
    replaceChildren() { this.children = []; },
    width: 760, height: 460,
    getContext() { return new Proxy({}, { get: () => () => {} }); }
  };
}
let offline = false;
const state = {
  name: '<script>example</script>', network: 'AP', ip: '192.168.4.1',
  version: '1.0.0', frames: 10, fresh: true, range: 6000,
  attempts: 5, uptime: 90, heap: 100000, bytes: 300, age: 100,
  rssi: null, update: '대기 중임',
  targets: [{ present: true, x: -782, y: 1713, fx: -782, fy: 1713, speed: -16, resolution: 320 },
    { present: false }, { present: false }]
};
const context = vm.createContext({
  document: {
    querySelector(selector) { if (!nodes.has(selector)) nodes.set(selector, element()); return nodes.get(selector); },
    querySelectorAll() { return []; }, createElement: element
  },
  fetch: async url => {
    if (offline) throw Error('offline');
    if (url === '/error') return { ok: false, json: async () => ({ message: '실패함' }) };
    return { ok: true, json: async () => url === '/api/status' ? state : { device: {}, auto: {}, ssid: 'test' } };
  },
  performance: { now: () => 1000 }, setTimeout() {}, setInterval() {},
  URLSearchParams, FormData: class {}, confirm: () => false
});
(async () => {
  vm.runInContext(source, context);
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(nodes.get('#count').textContent, 1);
  assert.equal(nodes.get('#title').textContent, state.name);
  assert.equal(nodes.get('#targetRows').children.length, 3);
  assert.equal(nodes.get('#targetRows').children[0].children[1].textContent, '-782 / 1713');
  await assert.rejects(vm.runInContext("request('/error')", context), /실패함/);
  state.fresh = false;
  state.targets[0].present = false;
  await vm.runInContext('poll()', context);
  assert.equal(nodes.get('#health').textContent, '수신 없음');
  assert.equal(nodes.get('#count').textContent, 0);
  offline = true;
  await vm.runInContext('poll()', context);
  assert.equal(nodes.get('#connection').textContent, '장치 연결 끊김');
  assert.equal(nodes.get('#targetRows').children.length, 0);
  console.log('웹UI 데이터 표시·오류 응답·만료·연결 끊김 검증 통과함');
})().catch(error => { console.error(error); process.exitCode = 1; });
