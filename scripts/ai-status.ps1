$root = Split-Path -Parent $PSScriptRoot
$state=Join-Path $root 'ai\state\state.json'
if (Test-Path $state) { Get-Content $state -Raw | ConvertFrom-Json | Format-List }
$q=Join-Path $root 'ai\tasks\queue.json'; if(Test-Path $q){ $items=(Get-Content $q -Raw|ConvertFrom-Json).tasks; "QUEUE SIZE: $($items.Count)" }
