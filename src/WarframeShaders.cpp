#include "WarframeShaders.h"
#include <windows.h>

#include <bcrypt.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <array>
#include <fstream>
#include <regex>
#include <string_view>

#pragma comment(lib, "bcrypt.lib")

namespace Rendering {
namespace {
using Microsoft::WRL::ComPtr;

std::string Sha256(const void* bytes, size_t size) {
    if (!bytes || size == 0 || size > UINT32_MAX)
        return {};
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return {};
    std::array<unsigned char, 32> digest{};
    const auto result = BCryptHash(algorithm, nullptr, 0, const_cast<PUCHAR>(static_cast<const UCHAR*>(bytes)),
                                   static_cast<ULONG>(size), digest.data(), static_cast<ULONG>(digest.size()));
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (result < 0)
        return {};
    std::string hex;
    constexpr char digits[] = "0123456789abcdef";
    for (auto byte : digest) {
        hex += digits[byte >> 4];
        hex += digits[byte & 15];
    }
    return hex;
}

bool CheckDll(const std::filesystem::path& path, std::string_view expected) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size <= 0 || size > 32 * 1024 * 1024)
        return false;
    std::vector<char> bytes(static_cast<size_t>(size));
    file.seekg(0);
    return file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())) &&
           Sha256(bytes.data(), bytes.size()) == expected;
}

struct LoadedModule {
    HMODULE handle;
    ~LoadedModule() { if (handle) FreeLibrary(handle); }
};

bool ReplaceOnce(std::string& text, std::string_view from, std::string_view to) {
    const size_t position = text.find(from);
    if (position == std::string::npos || text.find(from, position + from.size()) != std::string::npos)
        return false;
    text.replace(position, from.size(), to);
    return true;
}

bool SplitShader(const std::string& ir, WarframeShader shader, std::string& scene, std::string& ui) {
    const bool taa = shader == WarframeShader::taa;
    const char* branch = taa ? "br i1 %454," : "br i1 %330,";
    const char* alpha = taa ? "%374" : "%250";
    const std::array<const char*, 3> graded = taa ? std::array{"449", "450", "451"} : std::array{"325", "326", "327"};
    const std::string alphaStore = std::string("call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float ") + alpha + ")";
    scene = ir;
    // Preserve original alpha in a separate R32_FLOAT RTV alongside the
    // graded colour. ReShade can then write every channel of the colour RTV.
    if (!ReplaceOnce(scene, branch, "br i1 true,") ||
        !ReplaceOnce(scene, alphaStore, alphaStore + "\n  call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float " + alpha + ")") ||
        !ReplaceOnce(scene, "!35 = !{!36}", "!35 = !{!36, !40}") ||
        scene.find("\n!40 = ") != std::string::npos)
        return false;
    scene += "\n!40 = !{i32 1, !\"SV_Target\", i8 9, i8 16, !41, i8 0, i32 1, i8 1, i32 1, i8 0, !42}\n"
             "!41 = !{i32 1}\n!42 = !{i32 3, i32 1}\n";

    const std::string entry = "define void @main() {\n";
    const size_t entryStart = ir.find(entry);
    const size_t uiStart = ir.find(taa ? "  %452 = call %dx.types.ResRet.f32 @dx.op.sample.f32" :
                                       "  %328 = call %dx.types.ResRet.f32 @dx.op.sample.f32");
    const size_t end = ir.find("\n}\n", uiStart);
    if (entryStart == std::string::npos || uiStart == std::string::npos || end == std::string::npos)
        return false;
    const std::string prefix = ir.substr(entryStart + entry.size(), uiStart - entryStart - entry.size());
    std::string tail = ir.substr(uiStart, end - uiStart);
    for (size_t channel = 0; channel < graded.size(); ++channel)
        tail = std::regex_replace(tail, std::regex(std::string("%") + graded[channel] + "\\b"),
                                  std::array{"%restR", "%restG", "%restB"}[channel]);
    if (!ReplaceOnce(tail, alphaStore, "call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %restAlpha)"))
        return false;

    std::string body;
    for (const char* id : {"1", "6", "7", "8", "15", "17", "18"}) {
        const size_t start = prefix.find(std::string("  %") + id + " = ");
        const size_t newline = prefix.find('\n', start);
        if (start == std::string::npos || newline == std::string::npos)
            return false;
        body += prefix.substr(start, newline - start + 1);
    }
    // Load by destination pixel, preserving the original UI UV sampling.
    body += R"(  %restX = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %restY = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %restPixelX = fptosi float %restX to i32
  %restPixelY = fptosi float %restY to i32
  %restScene = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %7, i32 0, i32 %restPixelX, i32 %restPixelY, i32 undef, i32 0, i32 0, i32 undef)
  %restCoverage = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %6, i32 0, i32 %restPixelX, i32 %restPixelY, i32 undef, i32 0, i32 0, i32 undef)
  %restR = extractvalue %dx.types.ResRet.f32 %restScene, 0
  %restG = extractvalue %dx.types.ResRet.f32 %restScene, 1
  %restB = extractvalue %dx.types.ResRet.f32 %restScene, 2
  %restAlpha = extractvalue %dx.types.ResRet.f32 %restCoverage, 0
)";
    body += tail;
    body = std::regex_replace(body, std::regex("; <label>:([0-9]+)[^\\n]*"), "restV$1:");
    body = std::regex_replace(body, std::regex("%([0-9]+)\\b"), "%restV$1");
    std::string declarations = ir.substr(end + 3);
    const std::regex unused("declare [^\\n]*(@dx\\.op\\.[a-zA-Z0-9_.]+)\\([^\\n]+\\n");
    std::string kept;
    size_t previous = 0;
    for (std::sregex_iterator it(declarations.begin(), declarations.end(), unused), last; it != last; ++it) {
        const size_t position = it->position();
        kept += declarations.substr(previous, position - previous);
        if (body.find((*it)[1].str() + "(") != std::string::npos)
            kept += it->str();
        previous = position + it->length();
    }
    kept += declarations.substr(previous);
    ui = ir.substr(0, entryStart + entry.size()) + "restV0:\n" + body + "\n}\n" + kept;
    if (!ReplaceOnce(ui, "\nattributes #0", "\ndeclare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2\n\nattributes #0"))
        return false;
    const std::regex dependencies("!dx\\.viewIdState = [^\\n]*\\n");
    scene = std::regex_replace(scene, dependencies, "");
    ui = std::regex_replace(ui, dependencies, "");
    return true;
}
}

WarframeShader IdentifyWarframePixelShader(const void* code, size_t size) {
    if (size == 7992 && Sha256(code, size) == "decf4a475a432bea96bc55741cb32f33c0b6559c79f2c3439642cc53d7ed28de")
        return WarframeShader::taa;
    if (size == 7072 && Sha256(code, size) == "2836a2f676ec06304901a777363458b90295af78eb0de9753b7e00810efc1083")
        return WarframeShader::normal;
    return WarframeShader::unknown;
}

bool IsWarframeVertexShader(const void* code, size_t size) {
    return size == 3244 && Sha256(code, size) == "0bbef015db6861ed94bd0abaad8df4e67a11fe823d22495789910fc5a25d9209";
}

bool BuildWarframeShaderPair(const std::filesystem::path& directory, const void* code, size_t size,
                            WarframeShaderPair& pair, const char*& status) {
    const auto profile = IdentifyWarframePixelShader(code, size);
    status = "Warframe split: compositor shader revision is unsupported";
    if (profile == WarframeShader::unknown)
        return false;
    status = "Warframe split: install the pinned x64 DXC 1.9.2609 pair in REST-DXC";
    const auto compilerPath = directory / L"dxcompiler.dll";
    const auto validatorPath = directory / L"dxil.dll";
    if (!directory.is_absolute() ||
        !CheckDll(compilerPath, "0689e2fee0a1c9df8662a59680469f24e2a3ad7ce6bbbcc9284f7a83172bfc59") ||
        !CheckDll(validatorPath, "64be2368211d257a038a7d4ea6bb3e24c878eb8d588c7b8414fccf759b69c2ac"))
        return false;
    constexpr auto flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32;
    const LoadedModule validationModule{LoadLibraryExW(validatorPath.c_str(), nullptr, flags)};
    const LoadedModule compilerModule{LoadLibraryExW(compilerPath.c_str(), nullptr, flags)};
    if (!validationModule.handle || !compilerModule.handle)
        return false;
    const auto create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(compilerModule.handle, "DxcCreateInstance"));
    status = "Warframe split: DXC initialization or disassembly failed";
    ComPtr<IDxcLibrary> library;
    ComPtr<IDxcCompiler> compiler;
    ComPtr<IDxcAssembler> assembler;
    ComPtr<IDxcValidator> validator;
    if (!create || FAILED(create(CLSID_DxcLibrary, IID_PPV_ARGS(&library))) ||
        FAILED(create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))) ||
        FAILED(create(CLSID_DxcAssembler, IID_PPV_ARGS(&assembler))) ||
        FAILED(create(CLSID_DxcValidator, IID_PPV_ARGS(&validator))))
        return false;
    ComPtr<IDxcBlobEncoding> input, disassembly;
    if (FAILED(library->CreateBlobWithEncodingFromPinned(code, static_cast<UINT32>(size), CP_UTF8, &input)) ||
        FAILED(compiler->Disassemble(input.Get(), &disassembly)))
        return false;
    std::string ir(static_cast<const char*>(disassembly->GetBufferPointer()), disassembly->GetBufferSize());
    if (!ir.empty() && ir.back() == '\0')
        ir.pop_back();
    std::string scene, ui;
    status = "Warframe split: compositor transformation did not match";
    if (!SplitShader(ir, profile, scene, ui))
        return false;
    auto assemble = [&](const std::string& text, std::vector<char>& bytes) {
        ComPtr<IDxcBlobEncoding> source;
        ComPtr<IDxcOperationResult> result;
        HRESULT resultCode;
        ComPtr<IDxcBlob> assembled, validated;
        if (FAILED(library->CreateBlobWithEncodingFromPinned(text.data(), static_cast<UINT32>(text.size()), CP_UTF8, &source)) ||
            FAILED(assembler->AssembleToContainer(source.Get(), &result)) ||
            FAILED(result->GetStatus(&resultCode)) || FAILED(resultCode) || FAILED(result->GetResult(&assembled)) || !assembled)
            return false;
        result.Reset();
        if (FAILED(validator->Validate(assembled.Get(), DxcValidatorFlags_InPlaceEdit, &result)) ||
            FAILED(result->GetStatus(&resultCode)) || FAILED(resultCode) || FAILED(result->GetResult(&validated)) || !validated)
            return false;
        const auto* first = static_cast<const char*>(validated->GetBufferPointer());
        bytes.assign(first, first + validated->GetBufferSize());
        return !bytes.empty();
    };
    status = "Warframe split: generated shader failed DXIL validation";
    WarframeShaderPair validated;
    if (!assemble(scene, validated.scene) || !assemble(ui, validated.ui))
        return false;
    pair = std::move(validated);
    status = "Warframe split: shaders validated";
    return true;
}
}
