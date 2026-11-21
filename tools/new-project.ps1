#Requires -Version 7
<#
.SYNOPSIS
    Создаёт новый проект в projects/ из шаблона templates/project.

.EXAMPLE
    ./tools/new-project.ps1 -Name weather_station -Sketch weather_node -Description "Метеостанция на BME280"
#>
[CmdletBinding()]
param(
    # Имя папки проекта: snake_case, латиница
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z][a-z0-9_]*$')]
    [string]$Name,

    # Имя скетча (папка и .ino). По умолчанию совпадает с именем проекта.
    [ValidatePattern('^[a-z][a-z0-9_]*$')]
    [string]$Sketch = $Name,

    # Короткое описание — попадёт в README и шапку скетча
    [string]$Description = 'TODO: описание проекта'
)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$template = Join-Path $root 'templates/project'
$dest     = Join-Path $root "projects/$Name"

if (Test-Path $dest) {
    throw "Проект уже существует: $dest"
}

Copy-Item -Path $template -Destination $dest -Recurse

# Переименовать файлы и папки с __SKETCH__ (сначала самые глубокие)
Get-ChildItem -Path $dest -Recurse |
    Where-Object { $_.Name -like '*__SKETCH__*' } |
    Sort-Object { $_.FullName.Length } -Descending |
    ForEach-Object { Rename-Item -LiteralPath $_.FullName -NewName ($_.Name -replace '__SKETCH__', $Sketch) }

# Подставить значения в текстовые файлы (UTF-8 без BOM)
$utf8 = [System.Text.UTF8Encoding]::new($false)
Get-ChildItem -Path $dest -Recurse -File | ForEach-Object {
    $text = [System.IO.File]::ReadAllText($_.FullName, $utf8)
    $new  = $text.Replace('__PROJECT__', $Name).Replace('__SKETCH__', $Sketch).Replace('__DESCRIPTION__', $Description)
    if ($new -ne $text) { [System.IO.File]::WriteAllText($_.FullName, $new, $utf8) }
}

Write-Host "Создан проект: projects/$Name" -ForegroundColor Green
Get-ChildItem -Path $dest -Recurse -File | ForEach-Object {
    Write-Host ('  ' + [System.IO.Path]::GetRelativePath($root, $_.FullName).Replace('\', '/'))
}
Write-Host ''
Write-Host 'Дальше:' -ForegroundColor Yellow
Write-Host "  1. Добавить строку о проекте в таблицы README.md и CLAUDE.md (корень монорепы)."
Write-Host "  2. Если будет сервер — занять порт в реестре docs/conventions.md."
Write-Host "  3. Copy-Item projects/$Name/firmware/$Sketch/config.example.h projects/$Name/firmware/$Sketch/config.h"
