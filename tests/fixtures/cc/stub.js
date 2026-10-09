// stands in for the claude CLI in stream-json mode (persistent process): thinking, text, tools, permission requests,
// questions, plans, control requests (mode, model, interrupt, rewind)
const fs = require('fs');
// account commands: claude auth status / login / logout
if (process.argv[2] === 'auth') {
  const sub = process.argv[3];
  if (sub === 'status') {
    console.log(JSON.stringify({ loggedIn: true, authMethod: 'claude.ai', apiProvider: 'firstParty', email: 'test.user@example.com', orgName: 'Test Org', subscriptionType: 'max' }));
  } else if (sub === 'login') {
    console.log('Opening browser to sign in…');
    setTimeout(() => { console.log('Login successful'); process.exit(0); }, 700);
  } else if (sub === 'logout') {
    console.log('Logged out');
    process.exit(0);
  }
  if (sub === 'status') process.exit(0);
} else {
const readline = require('readline');
const out = (o) => process.stdout.write(JSON.stringify(o) + '\n');
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const sid = '11111111-2222-3333-4444-555555555555';
const log = (o) => fs.appendFileSync(__dirname + '/stub.log', JSON.stringify(o) + '\n');
log({ args: process.argv.slice(2), cwd: process.cwd(), env: process.env.CLAUDE_CODE_ENABLE_SDK_FILE_CHECKPOINTING });
let cost = 0, n = 0, backup = null, backupFile = '', userUuid = '';
const pending = new Map(); // request id -> resolver
let reqSeq = 0;
function ask(request) {
  const id = 'req_' + ++reqSeq;
  out({ type: 'control_request', request_id: id, request });
  return new Promise((res) => pending.set(id, res));
}
out({ type: 'system', subtype: 'init', session_id: sid, model: 'claude-opus-5-5', slash_commands: ['compact', 'cost', 'init', 'review'] });
out({ type: 'rate_limit_event', rate_limit_info: { status: 'allowed', rateLimitType: 'seven_day', utilization: 0.42, unifiedWindows: { five_hour: { utilization: 0.1 }, seven_day: { utilization: 0.42 } } } });

async function turn(text) {
  n++;
  userUuid = 'uuid-' + n;
  out({ type: 'user', uuid: userUuid, isReplay: true, message: { role: 'user', content: [{ type: 'text', text }] } });
  const m = /Open file: (.+?)(?: \(cursor on line \d+\))?\r?\n/.exec(text);
  const file = m ? m[1].trim() : '';
  if (/^\/compact/.test(text)) {
    out({ type: 'assistant', message: { id: 'mc', content: [{ type: 'text', text: 'Conversation compacted.' }] } });
    cost += 0.001;
    out({ type: 'result', subtype: 'success', is_error: false, session_id: sid, total_cost_usd: cost, result: 'Conversation compacted.' });
    return;
  }
  const mid = 'm' + n;
  out({ type: 'stream_event', event: { type: 'message_start', message: { id: mid, model: 'claude-opus-5-5' } } });
  out({ type: 'stream_event', event: { type: 'content_block_start', index: 0, content_block: { type: 'thinking', thinking: '' } } });
  for (const t of ['The user wants a change. ', 'Let me do it.']) {
    out({ type: 'stream_event', event: { type: 'content_block_delta', index: 0, delta: { type: 'thinking_delta', thinking: t } } });
    await sleep(80);
  }
  out({ type: 'stream_event', event: { type: 'content_block_stop', index: 0 } });
  out({ type: 'stream_event', event: { type: 'content_block_start', index: 1, content_block: { type: 'text', text: '' } } });
  out({ type: 'stream_event', event: { type: 'content_block_delta', index: 1, delta: { type: 'text_delta', text: 'On it. ' } } });
  out({ type: 'stream_event', event: { type: 'content_block_delta', index: 1, delta: { type: 'text_delta', text: 'Changing **hello**.' } } });
  out({ type: 'stream_event', event: { type: 'content_block_stop', index: 1 } });
  const blocks = [{ type: 'thinking', thinking: 'The user wants a change. Let me do it.' }, { type: 'text', text: 'On it. Changing **hello**.' }];
  if (file) blocks.push({ type: 'tool_use', id: 'tu_edit' + n, name: 'Edit', input: { file_path: file, old_string: 'hello', new_string: 'world' } });
  out({ type: 'assistant', message: { id: mid, model: 'claude-opus-5-5', usage: { input_tokens: 1200, cache_read_input_tokens: 30000 }, content: blocks } });
  if (file) {
    backup = fs.readFileSync(file, 'utf8');
    backupFile = file;
    fs.writeFileSync(file, backup.replace(/hello/g, 'world'));
    await sleep(100);
    out({ type: 'user', message: { content: [{ type: 'tool_result', tool_use_id: 'tu_edit' + n, content: 'ok' }] } });
  }
  // a command that needs approval
  out({ type: 'assistant', message: { id: mid + 'b', content: [{ type: 'tool_use', id: 'tu_bash' + n, name: 'Bash', input: { command: 'npm test', description: 'Run the tests' } }] } });
  const r = await ask({ subtype: 'can_use_tool', tool_name: 'Bash', input: { command: 'npm test' }, tool_use_id: 'tu_bash' + n, permission_suggestions: [{ type: 'addRules', rules: [{ toolName: 'Bash', ruleContent: 'npm test' }], behavior: 'allow', destination: 'session' }] });
  const allowed = r && r.response && r.response.behavior === 'allow';
  out({ type: 'user', message: { content: [{ type: 'tool_result', tool_use_id: 'tu_bash' + n, is_error: !allowed, content: allowed ? 'tests passed' : 'The user denied this action.' }] } });
  if (/question/i.test(text)) {
    const q = await ask({ subtype: 'can_use_tool', tool_name: 'AskUserQuestion', tool_use_id: 'tu_q', input: { questions: [{ question: 'Which style?', header: 'Style', multiSelect: false, options: [{ label: 'Short', description: 'Terse' }, { label: 'Long', description: 'Verbose' }] }] } });
    log({ questionAnswer: q.response });
  }
  if (/plan/i.test(text)) {
    const p = await ask({ subtype: 'can_use_tool', tool_name: 'ExitPlanMode', tool_use_id: 'tu_p', input: { plan: '## Plan\n1. Edit the file\n2. Run tests' } });
    log({ planAnswer: p.response });
  }
  cost += 0.0123;
  out({ type: 'stream_event', event: { type: 'message_start', message: { id: mid + 'c' } } });
  out({ type: 'stream_event', event: { type: 'content_block_delta', delta: { type: 'text_delta', text: allowed ? 'Done, tests passed.' : 'Done, but I was not allowed to run the tests.' } } });
  out({ type: 'assistant', message: { id: mid + 'c', usage: { input_tokens: 1300, cache_read_input_tokens: 30000 }, content: [{ type: 'text', text: allowed ? 'Done, tests passed.' : 'Done, but I was not allowed to run the tests.' }] } });
  out({ type: 'result', subtype: 'success', is_error: false, session_id: sid, total_cost_usd: cost, result: 'Done.' });
}

const rl = readline.createInterface({ input: process.stdin });
let chain = Promise.resolve();
rl.on('line', (line) => {
  if (!line.trim()) return;
  let j;
  try { j = JSON.parse(line); } catch { return; }
  log({ in: j.type, sub: j.request && j.request.subtype, mode: j.request && j.request.mode, model: j.request && j.request.model });
  if (j.type === 'user') {
    const text = (j.message.content || []).filter((b) => b.type === 'text').map((b) => b.text).join('\n');
    const imgs = (j.message.content || []).filter((b) => b.type === 'image').length;
    log({ text: text.slice(0, 300), images: imgs });
    chain = chain.then(() => turn(text));
  } else if (j.type === 'control_response') {
    const id = j.response && j.response.request_id;
    const res = pending.get(id);
    if (res) { pending.delete(id); res({ response: j.response.response }); }
  } else if (j.type === 'control_request') {
    const r = j.request || {};
    if (r.subtype === 'rewind_files' && backupFile && backup != null) fs.writeFileSync(backupFile, backup);
    out({ type: 'control_response', response: { subtype: 'success', request_id: j.request_id, response: {} } });
  }
});
rl.on('close', () => process.exit(0));

}
