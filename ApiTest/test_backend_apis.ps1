# TrainClient API Backend Verification Script
# Using PowerShell to test backend APIs

$BASE_URL = "http://127.0.0.1:8080"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "TrainClient API Backend Verification (PowerShell)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Backend: $BASE_URL"
Write-Host ""

# 1. Test Login
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host "1. POST /api/auth/login" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

$loginResp = Invoke-WebRequest -Uri "$BASE_URL/api/auth/login" -Method Post -ContentType "application/json" -Body '{"username":"admin","password":"admin"}' -UseBasicParsing
Write-Host "Status: $($loginResp.StatusCode)"
$loginData = $loginResp.Content | ConvertFrom-Json
Write-Host "Response: $($loginResp.Content)"

if ($loginData.success) {
    $token = $loginData.token
    Write-Host "[OK] Login success" -ForegroundColor Green
    Write-Host "  Token: $($token.Substring(0, [Math]::Min(20, $token.Length)))..."
} else {
    Write-Host "[FAIL] Login failed" -ForegroundColor Red
    exit 1
}

$headers = @{"Authorization" = "Bearer $token"}

# 2. Test Get Trains
Write-Host ""
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host "2. GET /api/trains" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

$trainsResp = Invoke-WebRequest -Uri "$BASE_URL/api/trains?date=2026-04-23" -Method Get -Headers $headers -UseBasicParsing
Write-Host "Status: $($trainsResp.StatusCode)"
Write-Host "Response: $($trainsResp.Content.Substring(0, [Math]::Min(500, $trainsResp.Content.Length)))"

$trainsData = $trainsResp.Content | ConvertFrom-Json
if ($trainsData.success) {
    $trainCount = $trainsData.data.trains.Count
    Write-Host "[OK] Success, $trainCount trains found" -ForegroundColor Green
} else {
    Write-Host "[FAIL]" -ForegroundColor Red
}

# 3. Test Get Carriages
Write-Host ""
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host "3. GET /api/carriages" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

$carriagesResp = Invoke-WebRequest -Uri "$BASE_URL/api/carriages?trainNumber=G4457&reachDatetime=1776905572" -Method Get -Headers $headers -UseBasicParsing
Write-Host "Status: $($carriagesResp.StatusCode)"
Write-Host "Response: $($carriagesResp.Content.Substring(0, [Math]::Min(500, $carriagesResp.Content.Length)))"

$carriagesData = $carriagesResp.Content | ConvertFrom-Json
if ($carriagesData.success) {
    # 尝试两种格式
    if ($carriagesData.carriages) {
        $carriageCount = $carriagesData.carriages.Count
        Write-Host "[OK] Success, $carriageCount carriages found (format: root.carriages)" -ForegroundColor Green
    } elseif ($carriagesData.data.carriages) {
        $carriageCount = $carriagesData.data.carriages.Count
        Write-Host "[OK] Success, $carriageCount carriages found (format: data.carriages)" -ForegroundColor Green
    }
} else {
    Write-Host "[FAIL]" -ForegroundColor Red
}

# 4. Test Submit Inspection
Write-Host ""
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host "4. PUT /api/carriages/1/inspection" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

$submitResp = Invoke-WebRequest -Uri "$BASE_URL/api/carriages/1/inspection" -Method Put -Headers $headers -ContentType "application/json" -Body '{"status":"examined","remark":"test"}' -UseBasicParsing
Write-Host "Status: $($submitResp.StatusCode)"
Write-Host "Response: $($submitResp.Content)"

if ($submitResp.StatusCode -eq 200 -or $submitResp.StatusCode -eq 201) {
    Write-Host "[OK] Submit success" -ForegroundColor Green
} else {
    Write-Host "[FAIL]" -ForegroundColor Red
}

# 5. Test Unauthorized Access
Write-Host ""
Write-Host "============================================================" -ForegroundColor Yellow
Write-Host "5. GET /api/trains (no token)" -ForegroundColor Yellow
Write-Host "============================================================" -ForegroundColor Yellow

$unauthResp = Invoke-WebRequest -Uri "$BASE_URL/api/trains" -Method Get -UseBasicParsing -ErrorAction SilentlyContinue
if ($unauthResp) {
    Write-Host "Status: $($unauthResp.StatusCode)"
    if ($unauthResp.StatusCode -eq 401) {
        Write-Host "[OK] Correctly rejected unauthorized" -ForegroundColor Green
    } else {
        Write-Host "[WARN] Should return 401, got $($unauthResp.StatusCode)" -ForegroundColor Yellow
    }
} else {
    Write-Host "[OK] Request blocked (no response)" -ForegroundColor Green
}

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Verification Complete" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
