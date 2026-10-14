function Test-DNS {
    param (
        [string]$DNSServer = "8.8.8.8",
        [string]$DomainToQuery = "github.com",
        [int] $Port = 53
        )

# Build Request Packet (12-byte header + question)
$DnsPacket = [System.Collections.Generic.List[byte]]::new()
$DnsPacket.AddRange([byte[]](0x1A, 0x2B)) # Transaction ID
$DnsPacket.AddRange([byte[]](0x01, 0x00)) # Flags (Standard Query)
$DnsPacket.AddRange([byte[]](0x00, 0x01)) # Questions: 1
$DnsPacket.AddRange([byte[]](0x00, 0x00)) # Answer RRs: 0
$DnsPacket.AddRange([byte[]](0x00, 0x00)) # Authority RRs: 0
$DnsPacket.AddRange([byte[]](0x00, 0x00)) # Additional RRs: 0

# Encode Domain Name
foreach ($Label in $DomainToQuery.Split('.')) {
    $Bytes = [System.Text.Encoding]::ASCII.GetBytes($Label)
    $DnsPacket.Add([byte]$Bytes.Length)
    $DnsPacket.AddRange($Bytes)
}
$DnsPacket.Add(0x00) # Domain Terminating Null Byte

$DnsPacket.AddRange([byte[]](0x00, 0x01)) # Type: A Record
$DnsPacket.AddRange([byte[]](0x00, 0x01)) # Class: IN
$Payload = $DnsPacket.ToArray()

# 2. Network I/O
try {
    $UdpClient = [System.Net.Sockets.UdpClient]::new()
    $UdpClient.Client.ReceiveTimeout = 5000
    $SentBytes = $UdpClient.Send($Payload, $Payload.Length, $DnsServer, $Port)
    
    $RemoteEndpoint = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
    $Response = $UdpClient.Receive([ref]$RemoteEndpoint)
}
catch {
    Write-Error "Network communication failed: $_"
    if ($UdpClient) { $UdpClient.Close() }; exit
}
finally {
    if ($UdpClient) { $UdpClient.Close() }
}

# 3. Decoding the Response Bytes
Write-Host "`n--- Parsing DNS Response ($($Response.Length) bytes) ---" -ForegroundColor Cyan

# Parse Header Details
$ResponseFlags  = [BitConverter]::ToUInt16($Response[3..2], 0) # Flip bytes for Big Endian
$AnswerCount    = [BitConverter]::ToUInt16($Response[7..6], 0)

# Check Response Flags for Errors (last 4 bits of flag block)
$RCode = $ResponseFlags -band 0x000F
if ($RCode -ne 0) {
    Write-Error "DNS Server returned error code (RCODE): $RCode"; exit
}

Write-Host "Found $AnswerCount answer(s) in the response payload." -ForegroundColor Gray

# Step past the 12-byte header
$Pointer = 12

# Skip the echoed Question section to reach the Answer section
# We iterate until we find the domain null byte (0x00), then skip Type (2 bytes) and Class (2 bytes)
while ($Response[$Pointer] -ne 0x00) {
    $Pointer += $Response[$Pointer] + 1
}
$Pointer += 5 # Move past Null byte, Type, and Class

# Process Answer Records
for ($i = 0; $i -lt $AnswerCount; $i++) {
    # 1. Handle Name Field (Usually a 2-byte compression pointer starting with 0xC0)
    if (($Response[$Pointer] -band 0xC0) -eq 0xC0) {
        $Pointer += 2 # Compression pointer; skip past it
    } else {
        while ($Response[$Pointer] -ne 0x00) { $Pointer += $Response[$Pointer] + 1 }
        $Pointer += 1
    }

    # 2. Extract Type and Class
    $Type  = [BitConverter]::ToUInt16($Response[($Pointer+1)..$Pointer], 0)
    $Class = [BitConverter]::ToUInt16($Response[($Pointer+3)..($Pointer+2)], 0)
    $Pointer += 4

    # 3. Extract TTL (Time to Live) - 4 bytes Big Endian
    $TtlBytes = $Response[$Pointer..($Pointer+3)]
    [Array]::Reverse($TtlBytes) # Flip network byte order to host order
    $TTL = [BitConverter]::ToUInt32($TtlBytes, 0)
    $Pointer += 4

    # 4. Extract Data Length (RDLENGTH) - 2 bytes
    $RdLength = [BitConverter]::ToUInt16($Response[($Pointer+1)..$Pointer], 0)
    $Pointer += 2

    # 5. Extract and Decode Content (If Type 1 = A Record / IPv4)
    if ($Type -eq 1 -and $RdLength -eq 4) {
        $IpBytes = $Response[$Pointer..($Pointer + 3)]
        $IpAddress = $IpBytes -join '.'
        
        # Display nicely formatted object
        [PSCustomObject]@{
            RecordType = "A (IPv4)"
            IPAddress  = $IpAddress
            TTL_Sec    = $TTL
        } | Format-List
    } else {
        Write-Host "Skipped Non-A Record format (Type: $Type, Data Length: $RdLength bytes)" -ForegroundColor DarkGray
    }
    
    $Pointer += $RdLength # Push cursor to the next record block
    }
}

$DNServers= "192.168.5.37"; 
Test-DNS $DNServers "github.com"


for ($n = 0; $n -lt 7000 ; $n++) {
    Test-DNS $DNServers "doubleclick.net"
    Write-Host "Iteration $n completed." -ForegroundColor Green
}

