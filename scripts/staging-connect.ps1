# Подключение к личному стенду: два port-forward внутри одного SSH-сеанса.
[CmdletBinding()]
param(
    [string]$Instance = 'broker-staging',
    [string]$VmAddress = '',
    [string]$IdentityFile = "$env:USERPROFILE\.ssh\broker-staging"
)

$ErrorActionPreference = 'Stop'

foreach ($tool in @('multipass', 'ssh')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Не найдена команда $tool. Установи её и открой PowerShell заново."
    }
}
if (-not (Test-Path -LiteralPath $IdentityFile -PathType Leaf)) {
    throw "Не найден SSH-ключ: $IdentityFile. Выполни подготовку из docs/staging.md."
}

$infoText = & multipass info $Instance --format json
if ($LASTEXITCODE -ne 0) {
    throw 'Multipass не смог прочитать состояние VM. Проверь доступ через multipass info.'
}
$info = ($infoText -join "`n") | ConvertFrom-Json
$vm = $info.info.PSObject.Properties[$Instance].Value
if (-not $vm -or $vm.state -ne 'Running') {
    throw "VM $Instance не запущена. Выполни: multipass start $Instance"
}
function Find-SshAddress {
    param(
        [string[]]$Candidates,
        [int]$Port = 22,
        [int]$TimeoutMs = 1500
    )

    foreach ($candidate in ($Candidates | Select-Object -Unique)) {
        if ($candidate -notmatch '^\d+\.\d+\.\d+\.\d+$') { continue }
        $client = New-Object System.Net.Sockets.TcpClient
        try {
            $task = $client.ConnectAsync($candidate, $Port)
            if ($task.Wait($TimeoutMs) -and $client.Connected) {
                return $candidate
            }
        }
        catch {
            # Недоступный или устаревший адрес не мешает проверить остальные.
        }
        finally {
            $client.Dispose()
        }
    }
    return $null
}

$candidates = @($vm.ipv4)
if ($VmAddress) { $candidates = @($VmAddress) }
$address = Find-SshAddress -Candidates $candidates
if (-not $address) {
    throw "SSH недоступен по адресам: $($candidates -join ', '). Проверь сеть VM."
}
Write-Host "Подключение к ubuntu@$address"

# Дочерние процессы завершаются при выходе и Ctrl+C. Завершение любого
# port-forward закрывает весь сеанс: повторный запуск выбирает новые поды.
$remoteScript = @'
set -eu
export KUBECONFIG="$HOME/.kube/config"
pids=""
cleanup() {
    trap - EXIT HUP INT TERM
    if [ -n "$pids" ]; then
        kill $pids 2>/dev/null || true
        wait $pids 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap "exit 130" INT
trap "exit 143" HUP TERM
kubectl -n broker-staging port-forward --address 127.0.0.1 service/admin-bff 13080:8180 &
pids="$!"
kubectl -n broker-staging port-forward --address 127.0.0.1 service/keycloak 18888:8080 &
pids="$pids $!"
wait -n $pids
'@
$remoteScript = $remoteScript.Replace("`r", '')
# Base64 сохраняет кавычки и переводы строк при передаче через Windows OpenSSH.
$encodedScript = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($remoteScript))
$remoteCommand = "echo $encodedScript | base64 --decode | bash"

Write-Host 'После появления двух сообщений Forwarding from открой http://localhost:13080'
Write-Host 'Keycloak: http://localhost:18888. Завершить подключение: Ctrl+C.'
Write-Host 'После обновления подов запусти эту команду повторно.'

& ssh -tt -i $IdentityFile `
    -o IdentitiesOnly=yes `
    -o ExitOnForwardFailure=yes `
    -o ServerAliveInterval=15 `
    -o ServerAliveCountMax=3 `
    -L '127.0.0.1:13080:127.0.0.1:13080' `
    -L '127.0.0.1:18888:127.0.0.1:18888' `
    "ubuntu@$address" $remoteCommand
if ($LASTEXITCODE -ne 0) {
    throw 'Подключение завершилось. Проверь сообщения SSH/kubectl выше; затем повтори запуск.'
}
