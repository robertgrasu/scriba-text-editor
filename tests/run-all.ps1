# Runs every scripted scenario (s*.txt) against build\Scriba.exe and reports crashes, time-outs and failed steps.
# Screenshots land in <out>\<script name>\ for a look by hand.   Usage: .\tests\run-all.ps1 [-Exe path] [-Only s5,s14] [-Out folder]
param([string]$Exe = "$PSScriptRoot\..\build\Scriba.exe", [string[]]$Only = @(), [string]$Out = "$env:TEMP\scriba-test-out", [int]$Timeout = 60)
$tests = "$env:TEMP\scriba-tests"
Remove-Item $tests -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item $PSScriptRoot $tests -Recurse
Remove-Item $Out -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $Out | Out-Null
function Reset-Fixtures([string]$name) {
  $cc = "$tests\fixtures\cc\target.txt"
  Set-Content $cc "say hello to hello" -Encoding ascii
  if ($name -eq 's41') {  # an earlier Claude conversation with edits and writes (opening it from the history list crashed)
    $proj = "$env:USERPROFILE\.claude\projects\" + (("$tests\fixtures\cc") -replace '[^A-Za-z0-9]', '-')
    New-Item -ItemType Directory $proj -Force | Out-Null
    $l1 = '{"type":"user","uuid":"u1","message":{"role":"user","content":"change the file please"}}'
    $l2 = '{"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"Done, here are the edits."},{"type":"tool_use","id":"t1","name":"Edit","input":{"file_path":"a.txt","old_string":"a\nb","new_string":"c\nd\ne"}},{"type":"tool_use","id":"t2","name":"Write","input":{"file_path":"b.txt","content":"x\ny"}},{"type":"tool_use","id":"t3","name":"MultiEdit","input":{"file_path":"c.txt","edits":[{"old_string":"1","new_string":"2\n3"}]}}]}}'
    [IO.File]::WriteAllText("$proj\0a0a0a0a-1111-2222-3333-444444444444.jsonl", "$l1`n$l2`n", (New-Object Text.UTF8Encoding $false))
  }
  if ($name -in @('s15', 's16')) {
    $p = "$tests\fixtures\ctxproj"
    Remove-Item $p -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory "$p\sub" -Force | Out-Null
    if ($name -eq 's15') { 'one' | Set-Content "$p\a.txt"; 'two' | Set-Content "$p\b.txt" }
    else { 'two' | Set-Content "$p\b.txt"; '' | Set-Content "$p\hello.txt"; 'one' | Set-Content "$p\renamed.txt" }
    'three' | Set-Content "$p\sub\c.txt"
  }
}
$failed = 0
$scripts = Get-ChildItem "$tests\s*.txt" | Sort-Object { [int]($_.BaseName -replace '\D', '') }
foreach ($s in $scripts) {
  if ($Only.Count -and $s.BaseName -notin $Only) { continue }
  Reset-Fixtures $s.BaseName
  $script = "$Out\$($s.BaseName).script"
  $text = [IO.File]::ReadAllText($s.FullName, [Text.Encoding]::UTF8).Replace('%TESTS%', $tests)
  [IO.File]::WriteAllText($script, $text, (New-Object Text.UTF8Encoding $false))
  $data = "$Out\$($s.BaseName)\data"
  $shots = "$Out\$($s.BaseName)"
  New-Item -ItemType Directory $data -Force | Out-Null
  $env:SCRIBA_DATA = $data; $env:SCRIBA_TEST = $script; $env:SCRIBA_TEST_OUT = $shots
  $p = Start-Process $Exe -ArgumentList '--fresh' -PassThru
  $timedOut = -not $p.WaitForExit($Timeout * 1000)
  if ($timedOut) { Stop-Process $p -Force }
  $log = if (Test-Path "$shots\log.txt") { Get-Content "$shots\log.txt" } else { @() }
  $crash = Test-Path "$data\crash.txt"
  $bad = @($log | Where-Object { $_ -match 'unknown step|failed|no item' })
  $finished = $log -contains 'done'
  $ok = -not $timedOut -and -not $crash -and $bad.Count -eq 0 -and $finished
  if (-not $ok) { $failed++ }
  '{0,-5} {1}{2}{3}{4}' -f $s.BaseName, $(if ($ok) { 'ok' } else { 'FAIL' }), $(if ($timedOut) { ' timeout' }), $(if ($crash) { ' crash' }), $(if ($bad.Count) { ' ' + ($bad -join '; ') })
}
Remove-Item Env:SCRIBA_TEST, Env:SCRIBA_TEST_OUT, Env:SCRIBA_DATA -ErrorAction SilentlyContinue
if ($failed) { "$failed scenario(s) failed"; exit 1 } else { 'all scenarios passed' }
