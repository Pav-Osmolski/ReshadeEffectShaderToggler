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
    variants.update({'main-input-tint':main_tint,'secondary-only':force(ir),'secondary-only-main-tint':force(main_tint),'secondary-only-post-tint':force(variants['post-grade-tint'])})
    for name,text in variants.items():
        (out/f'{name}.ll').write_text(text)
        subprocess.run([str(tool),str(compiler),'assemble',str(out/f'{name}.ll'),str(out/f'{name}.dxil')],check=True)
    (out/'manifest.json').write_text(json.dumps({'source_sha256':digest,'pre_ui_values':phis,'variants':list(variants)},indent=2)+'\n')
    print(mode,list(variants))
