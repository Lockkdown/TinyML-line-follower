# Simple TFLite to C array converter with proper 4-byte alignment

param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile,
    
    [Parameter(Mandatory=$true)]
    [string]$OutputFile
)

Write-Host "Converting $InputFile to $OutputFile..."

# Read the binary file
$bytes = [System.IO.File]::ReadAllBytes($InputFile)

# Create the output file with proper alignment
$writer = [System.IO.StreamWriter]::new($OutputFile)

$writer.WriteLine("// file: firmware/model/model_data.cc")
$writer.WriteLine("// purpose: TFLite model converted to C array with 4-byte alignment")
$writer.WriteLine("// dependencies: generated from .tflite model")
$writer.WriteLine("")
$writer.WriteLine("// Ensure 4-byte alignment for TFLite flatbuffer")
$writer.Write("alignas(4) const unsigned char MODEL_DATA[] = {")

# Write bytes as hex values
for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($i % 16 -eq 0) {
        $writer.WriteLine("")
        $writer.Write("  ")
    }
    $writer.Write("0x{0:X2}" -f $bytes[$i])
    if ($i -lt $bytes.Length - 1) {
        $writer.Write(", ")
    }
}

$writer.WriteLine("")
$writer.WriteLine("};")
$writer.WriteLine("")
$writer.WriteLine("const unsigned int MODEL_DATA_LEN = {0};" -f $bytes.Length)
$writer.WriteLine(";")

$writer.Close()

Write-Host "Conversion complete!"
Write-Host "Model size: $($bytes.Length) bytes"
