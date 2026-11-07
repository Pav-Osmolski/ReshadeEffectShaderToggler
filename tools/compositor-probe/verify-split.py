"""Compare the native two-pass compositor with the original replay."""
import argparse
import json
from pathlib import Path

import numpy as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, required=True)
args = parser.parse_args()


def pixels(folder, name, count):
    data = np.fromfile(folder / f'{name}.raw', dtype='<u4')
    if len(data) != count:
        raise ValueError(f'Wrong pixel count for {folder.name}/{name}')
    return data


def difference(reference, candidate):
    channels = [np.abs(((reference >> shift) & 1023).astype(np.int16)
                       - ((candidate >> shift) & 1023).astype(np.int16))
                for shift in (0, 10, 20)]
    maximum = np.maximum.reduce(channels)
    return {
        'changed_pixels': int(np.count_nonzero(reference != candidate)),
        'max_10bit_rgb_delta': int(maximum.max()),
        'rgb_pixels_over_one_code': int(np.count_nonzero(maximum > 1)),
        'mean_10bit_rgb_delta': sum(float(channel.mean()) for channel in channels) / 3,
        'changed_alpha_pixels': int(np.count_nonzero((reference >> 30) != (candidate >> 30))),
    }


def ui_masks(folder, width, height):
    raw = np.fromfile(folder / 't8-0.raw', dtype=np.uint8)
    if raw.size != width * height * 4:
        raise ValueError('Invalid UI texture dimensions')
    alpha = raw.reshape(height, width, 4)[:, :, 3]
    # Classification by captured texels. The neighbourhood excludes pixels
    # whose bilinear UI sample could straddle an opacity boundary.
    opaque = np.pad(alpha == 255, 2, constant_values=False)
    interior = np.logical_and.reduce(
        [opaque[y:y + height, x:x + width] for x in range(5) for y in range(5)])
    return {'opaque_ui_interior': interior.ravel(),
            'translucent_ui_texels': ((alpha > 0) & (alpha < 255)).ravel()}


report = {}
failures = []
for mode in ('taa', 'no-taa', 'fsr'):
    folder = args.root / mode
    viewport = next(line.split() for line in (folder / 'split-inputs.tsv').read_text().splitlines()
                    if line.startswith('viewport '))
    width, height = map(int, viewport[3:5])
    count = width * height
    original = pixels(folder, 'original', count)
    native = pixels(folder, 'native-original', count)
    inline_tint = pixels(folder, 'post-grade-tint', count)
    masks = ui_masks(folder, width, height)
    measurements = {'native_original_vs_capture': difference(original, native)}
    for precision in ('fp32', 'fp16', 'rgb10'):
        identity = pixels(folder, f'split-{precision}-identity', count)
        tint = pixels(folder, f'split-{precision}-tint', count)
        measurements[precision] = {
            'identity_vs_native_original': difference(native, identity),
            'tint_vs_inline_tint': difference(inline_tint, tint),
            'opaque_ui_interior_tint_vs_identity': difference(
                identity[masks['opaque_ui_interior']], tint[masks['opaque_ui_interior']]),
            'translucent_ui_identity_vs_original': difference(
                original[masks['translucent_ui_texels']], identity[masks['translucent_ui_texels']]),
        }
    secondary = pixels(folder, 'secondary-only', count)
    secondary_tint = pixels(folder, 'secondary-only-post-tint', count)
    split_secondary = pixels(folder, 'split-secondary-identity', count)
    split_secondary_tint = pixels(folder, 'split-secondary-tint', count)
    measurements['full_secondary_weight'] = {
        'identity_vs_inline': difference(secondary, split_secondary),
        'tint_vs_inline': difference(secondary_tint, split_secondary_tint),
        'changed_pixels_with_tint': int(np.count_nonzero(split_secondary != split_secondary_tint)),
    }
    exact_checks = {
        'native original': measurements['native_original_vs_capture'],
        **measurements['fp32'],
        'full secondary identity': measurements['full_secondary_weight']['identity_vs_inline'],
        'full secondary tint': measurements['full_secondary_weight']['tint_vs_inline'],
    }
    for check, metrics in exact_checks.items():
        if metrics['changed_pixels']:
            failures.append(f'{mode}: {check} differs by {metrics["changed_pixels"]} pixels')
    if not measurements['full_secondary_weight']['changed_pixels_with_tint']:
        failures.append(f'{mode}: tint had no effect at full secondary weight')
    report[mode] = measurements

output = args.root / 'split-verification.json'
output.write_text(json.dumps(report, indent=2) + '\n')
print(output)
for mode, measurements in report.items():
    print(mode, 'original:', measurements['native_original_vs_capture'])
    for precision in ('fp32', 'fp16', 'rgb10'):
        print(precision, measurements[precision])
    print('full secondary:', measurements['full_secondary_weight'])
if failures:
    raise SystemExit('\n'.join(failures))
