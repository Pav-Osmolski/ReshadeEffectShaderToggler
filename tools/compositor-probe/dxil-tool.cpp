#include <windows.h> // dxcapi.h requires Windows COM declarations first.

#include <cstdio>
#include <dxcapi.h>
#include <fstream>
#include <vector>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
int wmain(int argc, wchar_t **argv) {
  if (argc != 5 || (wcscmp(argv[2], L"assemble") != 0 &&
                    wcscmp(argv[2], L"disassemble") != 0)) {
    fputs("Usage: dxil-tool dxcompiler.dll assemble|disassemble input output\n",
          stderr);
    return 2;
  }
  wchar_t compilerPath[32768];
  auto pathLength = GetFullPathNameW(argv[1], 32768, compilerPath, nullptr);
  if (pathLength == 0 || pathLength >= 32768)
    return 3;
  auto module = LoadLibraryExW(compilerPath, nullptr,
                               LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                   LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!module)
    return 3;
  auto create = reinterpret_cast<DxcCreateInstanceProc>(
      GetProcAddress(module, "DxcCreateInstance"));
  if (!create)
    return 4;
  std::ifstream input(argv[3], std::ios::binary);
  std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
  if (bytes.empty() || bytes.size() > 64u * 1024u * 1024u)
    return 5;
  ComPtr<IDxcLibrary> lib;
  if (FAILED(create(CLSID_DxcLibrary, IID_PPV_ARGS(&lib))))
    return 6;
  ComPtr<IDxcBlobEncoding> blob;
  if (FAILED(lib->CreateBlobWithEncodingFromPinned(
          bytes.data(), static_cast<UINT32>(bytes.size()), CP_UTF8, &blob)))
    return 7;
  ComPtr<IDxcBlob> output;
  if (wcscmp(argv[2], L"disassemble") == 0) {
    ComPtr<IDxcCompiler> compiler;
    if (FAILED(create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))
      return 8;
    ComPtr<IDxcBlobEncoding> text;
    if (FAILED(compiler->Disassemble(blob.Get(), &text)))
      return 9;
    text.As(&output);
  } else {
    ComPtr<IDxcAssembler> assembler;
    if (FAILED(create(CLSID_DxcAssembler, IID_PPV_ARGS(&assembler))))
      return 10;
    ComPtr<IDxcOperationResult> result;
    if (FAILED(assembler->AssembleToContainer(blob.Get(), &result)))
      return 11;
    HRESULT status;
    result->GetStatus(&status);
    ComPtr<IDxcBlobEncoding> errors;
    result->GetErrorBuffer(&errors);
    if (errors && errors->GetBufferSize())
      fwrite(errors->GetBufferPointer(), 1, errors->GetBufferSize(), stderr);
    if (FAILED(status))
      return 12;
    result->GetResult(&output);
    ComPtr<IDxcValidator> validator;
    if (FAILED(create(CLSID_DxcValidator, IID_PPV_ARGS(&validator))))
      return 14;
    ComPtr<IDxcOperationResult> validation;
    if (FAILED(validator->Validate(output.Get(), DxcValidatorFlags_InPlaceEdit,
                                   &validation)))
      return 15;
    validation->GetStatus(&status);
    errors.Reset();
    validation->GetErrorBuffer(&errors);
    if (errors && errors->GetBufferSize())
      fwrite(errors->GetBufferPointer(), 1, errors->GetBufferSize(), stderr);
    if (FAILED(status))
      return 16;
    output.Reset();
    validation->GetResult(&output);
  }
  std::ofstream file(argv[4], std::ios::binary);
  file.write(static_cast<const char *>(output->GetBufferPointer()),
             output->GetBufferSize());
  return file ? 0 : 13;
}
