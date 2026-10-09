// a tiny language server for testing the LSP client
const fs = require('fs');
const log = (m) => { try { fs.appendFileSync(__dirname + '\\fakelsp.log', m + '\n'); } catch {} };
let buf = Buffer.alloc(0);
const docs = {};
const send = (m) => { const body = JSON.stringify({ jsonrpc: '2.0', ...m }); process.stdout.write('Content-Length: ' + Buffer.byteLength(body) + '\r\n\r\n' + body); };
function diagnose(uri) {
  const lines = (docs[uri] || '').split('\n');
  const diagnostics = [];
  lines.forEach((l, i) => {
    let k = l.indexOf('TODO');
    if (k >= 0) diagnostics.push({ range: { start: { line: i, character: k }, end: { line: i, character: k + 4 } }, severity: 2, message: 'Unfinished work here', source: 'fake' });
    k = l.indexOf('BAD');
    if (k >= 0) diagnostics.push({ range: { start: { line: i, character: k }, end: { line: i, character: k + 3 } }, severity: 1, message: 'Something is bad', source: 'fake' });
  });
  send({ method: 'textDocument/publishDiagnostics', params: { uri, diagnostics } });
}
const wordAt = (uri, pos) => {
  const l = (docs[uri] || '').split('\n')[pos.line] || '';
  let a = pos.character, b = a;
  while (a > 0 && /\w/.test(l[a - 1])) a--;
  while (b < l.length && /\w/.test(l[b])) b++;
  return l.slice(a, b);
};
function handle(m) {
  log(JSON.stringify(m).slice(0, 200));
  if (m.method === 'initialize') {
    send({ id: m.id, result: { capabilities: { textDocumentSync: 1, completionProvider: { triggerCharacters: ['.'] }, hoverProvider: true, definitionProvider: true, referencesProvider: true, renameProvider: true, documentFormattingProvider: true } } });
  } else if (m.method === 'textDocument/didOpen') { docs[m.params.textDocument.uri] = m.params.textDocument.text; diagnose(m.params.textDocument.uri); }
  else if (m.method === 'textDocument/didChange') { docs[m.params.textDocument.uri] = m.params.contentChanges[0].text; diagnose(m.params.textDocument.uri); }
  else if (m.method === 'textDocument/hover') send({ id: m.id, result: { contents: { kind: 'markdown', value: '**' + wordAt(m.params.textDocument.uri, m.params.position) + '**\n\nA fake hover from the test server.' } } });
  else if (m.method === 'textDocument/definition') send({ id: m.id, result: { uri: m.params.textDocument.uri, range: { start: { line: 0, character: 0 }, end: { line: 0, character: 3 } } } });
  else if (m.method === 'textDocument/references') send({ id: m.id, result: [{ uri: m.params.textDocument.uri, range: { start: { line: 0, character: 0 }, end: { line: 0, character: 3 } } }, { uri: m.params.textDocument.uri, range: { start: { line: 2, character: 0 }, end: { line: 2, character: 3 } } }] });
  else if (m.method === 'textDocument/completion') send({ id: m.id, result: { isIncomplete: false, items: [
    { label: 'alphaOne', kind: 3, detail: 'function alphaOne(x)', documentation: { kind: 'markdown', value: 'The first **alpha**.' }, insertText: 'alphaOne', sortText: '1' },
    { label: 'alphaLoop', kind: 15, detail: 'snippet', insertTextFormat: 2, insertText: 'for (let ${1:i} = 0; ${1:i} < ${2:n}; ${1:i}++) {\n\t$0\n}', sortText: '2' },
  ] } });
  else if (m.method === 'textDocument/rename') { const uri = m.params.textDocument.uri; send({ id: m.id, result: { changes: { [uri]: [{ range: { start: { line: 0, character: 0 }, end: { line: 0, character: 3 } }, newText: m.params.newName }] } } }); }
  else if (m.method === 'textDocument/formatting') send({ id: m.id, result: [{ range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } }, newText: '// formatted\n' }] });
  else if (m.method === 'shutdown') send({ id: m.id, result: null });
  else if (m.method === 'exit') process.exit(0);
  else if (m.id != null) send({ id: m.id, result: null });
}
process.stdin.on('data', (d) => {
  buf = Buffer.concat([buf, d]);
  for (;;) {
    const sep = buf.indexOf('\r\n\r\n');
    if (sep < 0) break;
    const mm = /Content-Length:\s*(\d+)/i.exec(buf.slice(0, sep).toString());
    if (!mm) { buf = buf.slice(sep + 4); continue; }
    const len = +mm[1];
    if (buf.length < sep + 4 + len) break;
    const body = buf.slice(sep + 4, sep + 4 + len).toString();
    buf = buf.slice(sep + 4 + len);
    try { handle(JSON.parse(body)); } catch (e) { log('ERR ' + e); }
  }
});
