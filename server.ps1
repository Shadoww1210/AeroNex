param([switch]$NoBrowser)

$ErrorActionPreference = 'Stop'

$bindAddress = [System.Net.IPAddress]::Loopback
$port = 8787
$webRoot = $PSScriptRoot
$pageFiles = @{
    '/' = @{ File = 'index.html'; Type = 'text/html; charset=utf-8' }
    '/index.html' = @{ File = 'index.html'; Type = 'text/html; charset=utf-8' }
    '/style.css' = @{ File = 'style.css'; Type = 'text/css; charset=utf-8' }
    '/script.js' = @{ File = 'script.js'; Type = 'text/javascript; charset=utf-8' }
    '/team.jpeg' = @{ File = 'team.jpeg'; Type = 'image/jpeg' }
    '/robotics-club-logo.jpeg' = @{ File = 'robotics-club-logo.jpeg'; Type = 'image/jpeg' }
}
$deviceRoutes = @{
    '/data' = 'GET'
    '/calibrate' = 'POST'
    '/reset' = 'POST'
}

function Send-HttpResponse($stream, [int]$status, [string]$contentType, [byte[]]$body) {
    $statusText = switch ($status) {
        200 { 'OK' }
        400 { 'Bad Request' }
        404 { 'Not Found' }
        405 { 'Method Not Allowed' }
        500 { 'Internal Server Error' }
        502 { 'Bad Gateway' }
        default { 'OK' }
    }
    $header = "HTTP/1.1 $status $statusText`r`nContent-Type: $contentType`r`nContent-Length: $($body.Length)`r`nConnection: close`r`nCache-Control: no-store`r`nX-Content-Type-Options: nosniff`r`nReferrer-Policy: no-referrer`r`n`r`n"
    $headerBytes = [System.Text.Encoding]::ASCII.GetBytes($header)
    $stream.Write($headerBytes, 0, $headerBytes.Length)
    if ($body.Length -gt 0) { $stream.Write($body, 0, $body.Length) }
    $stream.Flush()
}

function Get-TextBytes([string]$text) {
    return ,([System.Text.Encoding]::UTF8.GetBytes($text))
}

function Invoke-DeviceRoute([string]$method, [string]$path) {
    $request = [System.Net.HttpWebRequest]::Create("http://192.168.4.1$path")
    $request.Method = $method
    # Keep the original five-second device timeout to tolerate brief Wi-Fi delays.
    $requestTimeout = 5000
    $request.Timeout = $requestTimeout
    $request.ReadWriteTimeout = $requestTimeout
    $request.KeepAlive = $false
    if ($method -eq 'POST') { $request.ContentLength = 0 }
    try {
        $upstream = [System.Net.HttpWebResponse]$request.GetResponse()
    } catch [System.Net.WebException] {
        if ($_.Exception.Response) {
            $upstream = [System.Net.HttpWebResponse]$_.Exception.Response
        } else {
            throw
        }
    }
    try {
        $memory = [System.IO.MemoryStream]::new()
        $upstream.GetResponseStream().CopyTo($memory)
        return [pscustomobject]@{
            Status = [int]$upstream.StatusCode
            ContentType = $upstream.ContentType
            Body = $memory.ToArray()
        }
    } finally {
        $upstream.Close()
    }
}

$listener = [System.Net.Sockets.TcpListener]::new($bindAddress, $port)
try {
    $listener.Start()
} catch {
    Write-Host "Could not start the local dashboard on port $port. Close another PosturePal window and try again."
    Write-Host $_.Exception.Message
    exit 1
}

$dashboardUrl = "http://127.0.0.1:$port/"
Write-Host "PosturePal laptop dashboard is ready at $dashboardUrl"
Write-Host 'Connect this laptop to PosturePal Wi-Fi if it is not connected already.'
Write-Host 'Keep this window open while using the dashboard. Press Ctrl+C to stop it.'
if (-not $NoBrowser) {
    try {
        Start-Process -FilePath $dashboardUrl -ErrorAction Stop | Out-Null
    } catch {
        Write-Host "The browser did not open automatically. Open $dashboardUrl manually."
        Write-Host $_.Exception.Message
    }
}

try {
    while ($true) {
        $client = $listener.AcceptTcpClient()
        $client.ReceiveTimeout = 6000
        $client.SendTimeout = 6000
        $stream = $client.GetStream()
        $reader = [System.IO.StreamReader]::new($stream, [System.Text.Encoding]::ASCII, $false, 1024, $true)

        try {
            $requestLine = $reader.ReadLine()
            if (-not $requestLine) { $client.Close(); continue }
            $parts = $requestLine.Split(' ')
            if ($parts.Length -lt 2) {
                Send-HttpResponse $stream 400 'text/plain; charset=utf-8' (Get-TextBytes 'Bad request')
                $client.Close()
                continue
            }

            $method = $parts[0].ToUpperInvariant()
            $requestUri = [System.Uri]::new("http://127.0.0.1:$port$($parts[1])")
            $path = $requestUri.AbsolutePath
            while ($true) {
                $headerLine = $reader.ReadLine()
                if (-not $headerLine -or $headerLine.Length -eq 0) { break }
            }

            if ($method -eq 'GET' -and $pageFiles.ContainsKey($path)) {
                $filePath = Join-Path $webRoot $pageFiles[$path].File
                $body = [System.IO.File]::ReadAllBytes($filePath)
                Send-HttpResponse $stream 200 $pageFiles[$path].Type $body
            } elseif ($deviceRoutes.ContainsKey($path)) {
                if ($method -ne $deviceRoutes[$path]) {
                    Send-HttpResponse $stream 405 'text/plain; charset=utf-8' (Get-TextBytes 'Method not allowed')
                } else {
                    try {
                        $result = Invoke-DeviceRoute $method $path
                        Send-HttpResponse $stream $result.Status $result.ContentType $result.Body
                    } catch {
                        Send-HttpResponse $stream 502 'text/plain; charset=utf-8' (Get-TextBytes 'Cannot reach PosturePal. Join its Wi-Fi network and try again.')
                    }
                }
            } else {
                Send-HttpResponse $stream 404 'text/plain; charset=utf-8' (Get-TextBytes 'Not found')
            }
        } catch {
            try { Send-HttpResponse $stream 500 'text/plain; charset=utf-8' (Get-TextBytes 'The local dashboard helper hit an error.') } catch {}
        } finally {
            $reader.Dispose()
            $stream.Dispose()
            $client.Close()
        }
    }
} finally {
    $listener.Stop()
}
