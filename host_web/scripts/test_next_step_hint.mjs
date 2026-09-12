// 验证下一步提示的可操作性、单目标和限时清理，不触发业务动作。
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import vm from 'node:vm';

const source = await readFile(new URL('../app.js', import.meta.url), 'utf8');
const start = source.indexOf('let nextStepElement;');
const end = source.indexOf('\nfunction hex(', start);
assert(start >= 0 && end > start);
const elements = new Map();
function element(id, disabled = false, visible = true) {
  const classes = new Set();
  const value = { disabled, classes, classList: {
    add: name => classes.add(name), remove: name => classes.delete(name)
  }, getClientRects: () => visible ? [{}] : [] };
  elements.set(id, value);
  return value;
}
const first = element('#first');
const second = element('#second');
const disabled = element('#disabled', true);
const hidden = element('#hidden', false, false);
const timers = new Map();
let serial = 0;
const context = vm.createContext({
  $: id => elements.get(id),
  setTimeout(callback, duration) { assert.equal(duration, 2400); timers.set(++serial, callback); return serial; },
  clearTimeout(id) { timers.delete(id); }
});
vm.runInContext(source.slice(start, end), context);
context.hintNextStep('#first');
assert(first.classes.has('is-next-step'));
context.hintNextStep('#second');
assert.equal(first.classes.size, 0);
assert(second.classes.has('is-next-step'));
assert.equal(timers.size, 1);
[...timers.values()][0]();
assert.equal(second.classes.size, 0);
assert.equal(timers.size, 0);
for (const id of ['#disabled', '#hidden', '#missing']) context.hintNextStep(id);
assert.equal(disabled.classes.size + hidden.classes.size + timers.size, 0);
context.hintNextStep('#first');
context.clearNextStepHint();
assert.equal(first.classes.size + timers.size, 0);
console.log('Next-step hint eligibility, replacement and timeout tests passed');
