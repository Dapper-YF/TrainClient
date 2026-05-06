$token = "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"
$repo = "Dapper-YF/TrainClient"
$base = "E:\Study\AI_Workspace\TrainClient"
$headers = @{ Authorization = "Bearer $token"; Accept = "application/vnd.github+json" }

$exclude = @("\.git","build","release","debug","MVSimages","MVSvideos","资料","\.copilot_bridge","object_script","moc_","ui_","qrc_","\.o$","\.obj$","\.exe$","\.dll$","Makefile","\.pro\.user","\.autosave","\.log$","\.tmp$","\.bak$","\.qmake","\.qtc_clangd","CMakeCache","CMakeFiles","compile_commands","push-to-github","\.git-credentials","release_backup","Web\\dist")

$files = Get-ChildItem $base -Recurse -File | Where-Object {
    $rel = $_.FullName.Substring($base.Length + 1)
    $excluded = $false
    foreach ($pat in $exclude) { if ($rel -match $pat) { $excluded = $true; break } }
    -not $excluded
}

Write-Host "Uploading $($files.Count) files..."

# Step 1: Create all blobs
$blobMap = @{}
$count = 0
foreach ($file in $files) {
    $count++
    $rel = $file.FullName.Substring($base.Length + 1).Replace("\", "/")
    $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
    $b64 = [Convert]::ToBase64String($bytes)

    $body = @{ content = $b64; encoding = "base64" } | ConvertTo-Json -Compress
    try {
        $resp = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/git/blobs" -Headers $headers -Method Post -Body $body -ContentType "application/json"
        $blobMap[$rel] = $resp.sha
        Write-Host "  [$count/$($files.Count)] $rel"
    } catch {
        Write-Host "  [$count/$($files.Count)] ERROR: $rel - $($_.Exception.Message)"
    }
}

Write-Host "`nBlobs created: $($blobMap.Count)"

# Step 2: Create tree
$treeItems = @()
foreach ($kv in $blobMap.GetEnumerator()) {
    $treeItems += @{ path = $kv.Key; mode = "100644"; type = "blob"; sha = $kv.Value }
}
$treeBody = @{ tree = $treeItems } | ConvertTo-Json -Depth 5 -Compress
$treeResp = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/git/trees" -Headers $headers -Method Post -Body $treeBody -ContentType "application/json"
Write-Host "Tree SHA: $($treeResp.sha)"

# Step 3: Create commit
$commitBody = @{ message = "feat: initial project upload"; tree = $treeResp.sha } | ConvertTo-Json -Compress
$commitResp = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/git/commits" -Headers $headers -Method Post -Body $commitBody -ContentType "application/json"
Write-Host "Commit SHA: $($commitResp.sha)"

# Step 4: Update ref
$refBody = @{ sha = $commitResp.sha; force = $true } | ConvertTo-Json -Compress
Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/git/refs/heads/main" -Headers $headers -Method Patch -Body $refBody -ContentType "application/json" | Out-Null

Write-Host "`n✅ Done! Repo: https://github.com/$repo"
