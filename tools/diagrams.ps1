#Requires -Version 7
<#
.SYNOPSIS
    Схемы монорепы: JupyterLab с schemdraw + WireViz в Docker и пересборка SVG из ноутбуков.

.DESCRIPTION
    Ноутбуки-исходники: projects/<project>/docs/diagrams/*.ipynb.
    Каждая ячейка сохраняет схему в SVG рядом с ноутбуком; SVG вставляются в документацию.
    Python на ПК не нужен — всё работает в образе esp32-diagrams (tools/diagrams/Dockerfile).

.EXAMPLE
    ./tools/diagrams.ps1 lab                    # JupyterLab на http://127.0.0.1:8888
    ./tools/diagrams.ps1 stop                   # остановить JupyterLab
    ./tools/diagrams.ps1 render                 # выполнить все ноутбуки, обновить SVG
    ./tools/diagrams.ps1 render -Project led_clock
    ./tools/diagrams.ps1 build                  # пересобрать образ (после правки requirements.txt)
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('lab', 'stop', 'render', 'build')]
    [string]$Command = 'render',

    # Только ноутбуки этого проекта (для render)
    [string]$Project,

    # Порт JupyterLab на хосте (реестр портов — docs/conventions.md)
    [int]$Port = 8888
)

$ErrorActionPreference = 'Stop'

$root      = Split-Path -Parent $PSScriptRoot
$image     = 'esp32-diagrams'
$container = 'esp32-diagrams-lab'

function Build-Image {
    docker build -t $image (Join-Path $PSScriptRoot 'diagrams')
    if ($LASTEXITCODE) { throw 'Не удалось собрать образ' }
}

function Assert-Image {
    docker image inspect $image *> $null
    if ($LASTEXITCODE) {
        Write-Host "Образа $image нет — собираю (один раз, ~400 МБ)..." -ForegroundColor Yellow
        Build-Image
    }
}

switch ($Command) {
    'build' {
        Build-Image
    }

    'lab' {
        Assert-Image
        docker rm -f $container *> $null
        docker run -d --name $container -p "127.0.0.1:${Port}:8888" -v "${root}:/repo" $image | Out-Null
        if ($LASTEXITCODE) { throw 'Не удалось запустить JupyterLab' }
        Write-Host "JupyterLab: http://127.0.0.1:$Port/lab" -ForegroundColor Green
        Write-Host "VS Code: открыть .ipynb → Select Kernel → Existing Jupyter Server → http://127.0.0.1:$Port"
        Write-Host 'Остановить: ./tools/diagrams.ps1 stop'
    }

    'stop' {
        docker rm -f $container *> $null
        Write-Host 'JupyterLab остановлен'
    }

    'render' {
        Assert-Image
        $pattern = if ($Project) { "projects/$Project/docs/diagrams/*.ipynb" } else { 'projects/*/docs/diagrams/*.ipynb' }
        $notebooks = @(Get-ChildItem -Path (Join-Path $root $pattern) -ErrorAction SilentlyContinue |
            ForEach-Object { [System.IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/') })
        if (-not $notebooks) { Write-Host "Ноутбуков не найдено: $pattern"; return }

        foreach ($nb in $notebooks) {
            Write-Host "-> $nb" -ForegroundColor Cyan
            docker run --rm -v "${root}:/repo" -w /repo $image `
                jupyter nbconvert --to notebook --execute --inplace $nb
            if ($LASTEXITCODE) { throw "Ошибка при выполнении $nb" }
        }
        Write-Host 'SVG обновлены. Проверьте git status.' -ForegroundColor Green
    }
}
