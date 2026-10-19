# Проверяем выбор адреса без настоящих SSH, Multipass и Kubernetes.
$ErrorActionPreference = 'Stop'
$tokens = $null
$errors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    "$PSScriptRoot/../scripts/staging-connect.ps1", [ref]$tokens, [ref]$errors
)
if ($errors.Count) { throw ($errors | Out-String) }
$function = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Find-SshAddress'
}, $true)
Invoke-Expression $function.Extent.Text

$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$listener.Start()
try {
    $port = $listener.LocalEndpoint.Port
    $selected = Find-SshAddress -Candidates @('invalid', '127.0.0.2', '127.0.0.1') -Port $port
    if ($selected -ne '127.0.0.1') { throw "Выбран неверный адрес: $selected" }
    $missing = Find-SshAddress -Candidates @('127.0.0.2') -Port $port
    if ($null -ne $missing) { throw 'Недоступный адрес не должен выбираться' }
}
finally {
    $listener.Stop()
}
Write-Host 'Проверки выбора SSH-адреса прошли.'
