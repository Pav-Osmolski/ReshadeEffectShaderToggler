"""Capture experiment only: exact input digests; generated game data stays local."""
from pathlib import Path
import re, subprocess, hashlib, json, argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root',type=Path,required=True,help='External output root containing taa/no-taa/fsr/original.shader')
parser.add_argument('--dxcompiler',type=Path,required=True,help='Absolute path to pinned DXC 1.9.2609 dxcompiler.dll; dxil.dll must be beside it')
parser.add_argument('--dxil-tool',type=Path,required=True)
args=parser.parse_args()
root=args.root.resolve();compiler=args.dxcompiler.resolve();tool=args.dxil_tool.resolve()
if root.is_relative_to(Path(__file__).resolve().parents[2]):
    parser.error('Use an output directory outside the repository; generated game data must stay local')
sources={
    'taa':'decf4a475a432bea96bc55741cb32f33c0b6559c79f2c3439642cc53d7ed28de',
    'no-taa':'2836a2f676ec06304901a777363458b90295af78eb0de9753b7e00810efc1083',
    'fsr':'2836a2f676ec06304901a777363458b90295af78eb0de9753b7e00810efc1083'}
# Verify all source identities before writing any generated variants. Unknown
# shaders need a fresh analysis; no broad pattern-based runtime rewriting.
for mode,digest in sources.items():
    if hashlib.sha256((root/mode/'original.shader').read_bytes()).hexdigest()!=digest:
        raise ValueError(f'Unrecognized shader for {mode}; refusing to transform it')
for mode,digest in sources.items():
    out=root/mode;out.mkdir(exist_ok=True)
    source=out/'original.shader'
    subprocess.run([str(tool),str(compiler),'disassemble',str(source),str(out/'original.ll')],check=True)
    ir=(out/'original.ll').read_text().rstrip('\0')
    phis=re.findall(r'^  %\d+ = phi float \[ %\d+, %\d+ \], \[ (%\d+), %0 \]',ir,re.M)
    assert len(phis)==3
    branch=re.findall(r'br i1 %\d+, label %\d+, label %\d+[^\n]*',ir)
    assert len(branch)==1
    marker=re.findall(r'^  %\d+ = call %dx.types.ResRet.f32 @dx.op.sample.f32\(i32 60, %dx.types.Handle %1,.*',ir,re.M)
    assert len(marker)==1
    prefix,suffix=ir.split(marker[0]);modified=suffix
    for old,new in zip(phis,['%restR','%restG','%restB']):
        modified=re.sub(re.escape(old)+r'(?!\d)',new,modified)
    def hook(factors):
        lines=''.join(f'  {new} = fmul float {old}, {factor}\n' for old,new,factor in zip(phis,['%restR','%restG','%restB'],factors))
        return prefix+lines+marker[0]+modified
    variants={'identity':ir,
      'scene-only':ir.replace(branch[0],re.sub(r'br i1 %\d+', 'br i1 true',branch[0])),
      'identity-hook':hook(['1.000000e+00']*3),
      'post-grade-tint':hook(['1.000000e+00','2.500000e-01','2.500000e-01'])}
    # Read the complete graded scene at the destination pixel. Reuse t0 for the
    # intermediate and retain the original t8 sample, branch and UI arithmetic.
    load = """  %restX = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %restY = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %restPixelX = fptosi float %restX to i32
  %restPixelY = fptosi float %restY to i32
  %restScene = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %7, i32 0, i32 %restPixelX, i32 %restPixelY, i32 undef, i32 0, i32 0, i32 undef)
  %restR = extractvalue %dx.types.ResRet.f32 %restScene, 0
  %restG = extractvalue %dx.types.ResRet.f32 %restScene, 1
  %restB = extractvalue %dx.types.ResRet.f32 %restScene, 2
  %restAlpha = extractvalue %dx.types.ResRet.f32 %restScene, 3
"""
    ui = prefix + load + marker[0] + modified
    ui, count = re.subn(r'(call void @dx.op.storeOutput.f32\(i32 5, i32 0, i32 0, i8 3, float )%\d+', r'\g<1>%restAlpha', ui)
    assert count == 1
    # Keep only the original root handles and UI UVs at the entry point. Rename
    # numeric SSA IDs and blocks so removing the original scene does not leave
    # gaps in LLVM's unnamed-value numbering.
    setup = ''.join(line + '\n' for line in prefix.splitlines()
                    if re.match(r'^  %(1|7|8|15|17|18) = ', line))
    entry = 'define void @main() {\n'
    assert ui.count(entry) == 1
    body, declarations = (load + marker[0] + ui.split(marker[0], 1)[1]).split('\n}\n', 1)
    body = re.sub(r'^; <label>:(\d+).*$', r'restV\1:', setup + body, flags=re.M)
    body = re.sub(r'%(\d+)\b', r'%restV\1', body)
    ui = ir.split(entry, 1)[0] + entry + 'restV0:\n' + body + '\n}\n' + declarations
    ui = ui.replace('\nattributes #0', '\ndeclare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2\n\nattributes #0')
    ui = re.sub(r'^declare [^\n]*(@dx\.op\.[\w.]+)\([^\n]+\n',
                lambda m: m[0] if m[1] + '(' in body else '', ui, flags=re.M)
    # The assembler derives dependency information for the modified inputs.
    ui = re.sub(r'^!dx.viewIdState = .*\n', '', ui, flags=re.M)
    variants['split-ui'] = ui
    # Controlled condition: the secondary colour fully replaces the main scene.
    secondary=re.findall(r'^  (%\d+) = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32\(i32 62, %dx.types.Handle %5,.*',ir,re.M)
    assert secondary
    alpha=re.findall(r'^  %\d+ = extractvalue %dx.types.ResRet.f32 '+re.escape(secondary[0])+r', 3.*',ir,re.M)
    assert len(alpha)==1
    force=lambda text:text.replace(alpha[0],alpha[0].split(' = ')[0]+' = fadd float 1.000000e+00, 0.000000e+00')
    # Equivalent sample math to multiplying t0 before the compositor. Keep all
    # source textures untouched; multiply each main green/blue sample instead.
    main_tint=ir
    samples=re.findall(r'^  (%\d+) = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32\(i32 62, %dx.types.Handle %7,.*',ir,re.M)
    assert samples
    for sample in samples:
        for channel in (1,2):
            for match in re.finditer(r'^  (%\d+) = extractvalue %dx.types.ResRet.f32 '+re.escape(sample)+rf', {channel}.*',ir,re.M):
                variable=match.group(1);temp='%restSample'+variable[1:]
                main_tint=main_tint.replace(match.group(0),match.group(0).replace(variable+' =',temp+' =')+f'\n  {variable} = fmul float {temp}, 2.500000e-01')
    variants.update({'main-input-tint':main_tint,'secondary-only':force(ir),'secondary-only-main-tint':force(main_tint),'secondary-only-post-tint':force(variants['post-grade-tint']),
                     'secondary-scene-only':force(variants['scene-only'])})
    for name,text in variants.items():
        (out/f'{name}.ll').write_text(text)
        subprocess.run([str(tool),str(compiler),'assemble',str(out/f'{name}.ll'),str(out/f'{name}.dxil')],check=True)
    (out/'manifest.json').write_text(json.dumps({'source_sha256':digest,'pre_ui_values':phis,'variants':list(variants)},indent=2)+'\n')
    print(mode,list(variants))
