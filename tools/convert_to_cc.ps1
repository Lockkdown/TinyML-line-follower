# Convert TFLite model to C array with proper 4-byte alignment
# Usage: .\convert_to_cc.ps1 <input.tflite> <output.cc>

param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile,
    
    [Parameter(Mandatory=$true)]
    [string]$OutputFile
)

# Get the directory of this script
$ScriptDir = Split-Path -Parent $PSCommandPath
$ProjectRoot = Split-Path -Parent $ScriptDir

Write-Host "Converting $InputFile to $OutputFile..."

# Check if xxd is available (usually via Git Bash or WSL)
$xxdAvailable = Get-Command xxd -ErrorAction SilentlyContinue

if (-not $xxdAvailable) {
    Write-Error "xxd command not found. Please install Git Bash or WSL to use xxd."
    exit 1
}

# Create the header with proper alignment
$header = @(
    "// file: firmware/model/model_data.cc",
    "// purpose: TFLite model converted to C array with 4-byte alignment", 
    "// dependencies: generated from .tflite model",
    "",
    "// Ensure 4-byte alignment for TFLite flatbuffer",
    "alignas(4) const unsigned char MODEL_DATA[] = {"
)

Set-Content -Path $OutputFile -Value $header

# Convert the binary to hex array
$xxdOutput = xxd -i $InputFile | ForEach-Object { 
    $_ -replace "unsigned char ", "const unsigned char "
}
Add-Content -Path $OutputFile -Value $xxdOutput

# Add the length constant
Add-Content -Path $OutputFile -Value ""
Add-Content -Path $OutputFile -Value "const unsigned int MODEL_DATA_LEN = sizeof(MODEL_DATA);"
Add-Content -Path $OutputFile -Value ";"

Write-Host "Conversion complete!"
$fileSize = (Get-Item $InputFile).Length
Write-Host "Model size: $fileSize bytes"

# Count the hex bytes in the output file
$byteCount = (Select-String -Path $OutputFile -Pattern "0x").Count
Write-Host "Array length: $byteCount bytes"
