# Media Foundation Encoder

MFEncode is a simple audio encoder using Advanced Audio Coding (AAC). It uses the codec provided
with the Microsoft Media Foundation, a built-in component of Windows.

Basically, this is a simple command-line wrapper around the Media Foundation codec, allowing you to
convert any file type that Media Foundation can use as input (including WAV and MP3) to AAC.

Using it is as simple as running the following:

```text
mfencode file.wav
```

This will create a file called file.m4a in the same folder as the input file, and encode the output
at 128Kbps. You can also customize the output path and quality if desired. MFEncode will not
overwrite the output file if it exists unless you specify the `-Force` argument. Run `mfencode
-Help` for full usage help.

MFEncode has only been tested on Windows 10 and 11; support for older Windows versions is not
guaranteed.
