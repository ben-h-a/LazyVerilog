// SHA-256 checksums keyed by version then platform.
// Updated by CI alongside lua/lazyverilog/checksums.lua.
export const RELEASE_CHECKSUMS: Record<string, Record<string, string>> = {
  "v1.3.3": {
    "linux-x64": "59aa0060ea92a89a7f3e072e037dc3d2ef8d62ca08ac2f38c5d0bd7b1f77bf1c",
    "linux-arm64": "584fd07bae69088bb23ada8453bfed31bdc515402996dad0dbc9bf85902c72c4",
    "linux-x64-static": "21c67369ccc77a1ad7427c14bc4cdb2076733f5901b17b77f7e417ac08eeb5ab",
    "linux-arm64-static": "d344025442b7f708fce95108a1c523e3fa6d72d7bdb900d750dce40254499d95",
    "darwin-x64": "63e04afb3bf4d0b19510f3571194b51048ac83f1c0fddca350cf23d768972040",
    "darwin-arm64": "b4327379795cfd57f8fd953fdfba0457988dced712cbd19c7ed278a606b1ac7d",
    "windows-x64": "c29b3337f4d5c13ae9f2d2199f6ccc006472b51250159a2d51fd6c099e6fc44c",
  },
};
