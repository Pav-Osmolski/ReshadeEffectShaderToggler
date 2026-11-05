from pathlib import Path
import hashlib,json,argparse
import numpy as np
parser=argparse.ArgumentParser(description='Verify local compositor replay experiments; no live HDR claim.')
parser.add_argument('--root',type=Path,required=True)
root=parser.parse_args().root.resolve()
results={}
for mode in ('taa','no-taa','fsr'):
    folder=root/mode
    def pixels(name):
        value=np.fromfile(folder/f'{name}.raw',dtype='<u4')
        assert value.size==3824*1600
        return value
    original=pixels('original')
    identity=pixels('identity');hook=pixels('identity-hook')
    assert np.array_equal(original,identity)
    assert np.array_equal(original,hook)
    baseline=pixels('secondary-only');main=pixels('secondary-only-main-tint');post=pixels('secondary-only-post-tint')
    def rgb(value):return np.stack([(value>>shift)&1023 for shift in (0,10,20)],axis=-1).astype(np.int32)
    main_delta=np.abs(rgb(baseline)-rgb(main))
    assert main_delta.max() <= 1, 'Main tint should vanish, allowing 10-bit rounding'
    assert not np.array_equal(baseline,post), 'Late tint should survive full secondary weight'
    result={'identity_exact':True,'identity_hook_exact':True,
      'full_secondary_main_tint_changed_pixels':int(np.count_nonzero(baseline!=main)),
      'full_secondary_main_tint_max_10bit_error':int(main_delta.max()),
      'full_secondary_main_tint_mean_10bit_error':float(main_delta.mean()),
      'full_secondary_post_tint_changed_pixels':int(np.count_nonzero(baseline!=post)),
      'post_grade_tint_changed_pixels':int(np.count_nonzero(original!=pixels('post-grade-tint'))),
      'sha256':{p.stem:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.glob('*.raw')}}
    ui=folder/'ui.raw'
    if ui.exists():
        alpha=np.fromfile(ui,dtype=np.uint8).reshape(-1,4)[:,3]
        assert alpha.size==original.size
        opaque=alpha==255;empty=alpha==0
        tint=pixels('post-grade-tint')
        result['opaque_ui_pixels']=int(opaque.sum())
        result['opaque_ui_changed_pixels']=int(np.count_nonzero(original[opaque]!=tint[opaque]))
        # Bilinear UV sampling can straddle alpha edges; evaluate only the
        # interior where a 5x5 neighbourhood is fully opaque.
        mask=opaque.reshape(1600,3824);interior=mask[2:-2,2:-2].copy()
        for y in range(5):
            for x in range(5):interior &= mask[y:y+1596,x:x+3820]
        o=original.reshape(1600,3824)[2:-2,2:-2];t=tint.reshape(1600,3824)[2:-2,2:-2]
        result['opaque_ui_interior_pixels']=int(interior.sum())
        result['opaque_ui_interior_changed_pixels']=int(np.count_nonzero(o[interior]!=t[interior]))
        assert result['opaque_ui_interior_changed_pixels']==0
        assert result['opaque_ui_interior_pixels']>=10000
        result['empty_ui_scene_changed_pixels']=int(np.count_nonzero(original[empty]!=tint[empty]))
    results[mode]=result
(root/'replay-verification.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
