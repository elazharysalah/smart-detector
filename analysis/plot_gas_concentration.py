"""Plot the MQ-5 readings recorded from the Smart Detector serial port.

Reproduces the "Avant / Apres" figures of the presentation:
  * the gas level inside the model house without any extraction;
  * the same scenario where the fan is switched on once the warning
    threshold (250) is crossed.

Usage:
    python plot_gas_concentration.py                 # open the figures
    python plot_gas_concentration.py --output out/   # save them as PNG
"""

import argparse
from datetime import datetime
from pathlib import Path

import matplotlib.pyplot as plt

WARNING_THRESHOLD = 250

# Arduino Serial Monitor timestamps, newest first (as copied from the IDE).
TIMESTAMPS = [
    '18:29:39.06', '18:29:38.92', '18:29:38.79', '18:29:38.65', '18:29:38.52',
    '18:29:38.39', '18:29:38.25', '18:29:38.12', '18:29:37.98', '18:29:37.85',
    '18:29:37.72', '18:29:37.58', '18:29:37.45', '18:29:37.31', '18:29:37.18',
    '18:29:37.05', '18:29:36.90', '18:29:36.78', '18:29:36.64', '18:29:36.51',
    '18:29:36.38', '18:29:36.25', '18:29:36.10', '18:29:35.98', '18:29:35.84',
    '18:29:35.71', '18:29:35.58', '18:29:35.44', '18:29:35.31', '18:29:35.17',
    '18:29:35.04', '18:29:34.90', '18:29:34.77', '18:29:34.64', '18:29:34.50',
    '18:29:34.37', '18:29:34.23', '18:29:34.10', '18:29:33.97', '18:29:33.83',
    '18:29:33.70', '18:29:33.57', '18:29:33.42', '18:29:33.30', '18:29:33.17',
    '18:29:33.02', '18:29:32.90', '18:29:32.76', '18:29:32.63', '18:29:32.50',
    '18:29:32.36', '18:29:32.22', '18:29:32.09', '18:29:31.96', '18:29:31.83',
    '18:29:31.69', '18:29:31.56', '18:29:31.43', '18:29:31.29', '18:29:31.16',
    '18:29:31.03', '18:29:30.89', '18:29:30.75', '18:29:30.62', '18:29:30.48',
    '18:29:30.35', '18:29:30.22', '18:29:30.09', '18:29:29.94', '18:29:29.81',
    '18:29:29.68', '18:29:29.54', '18:29:29.41', '18:29:29.28', '18:29:29.15',
    '18:29:29.01', '18:29:28.88', '18:29:28.74', '18:29:28.61', '18:29:28.48',
    '18:29:28.35', '18:29:28.21', '18:29:28.08', '18:29:27.95', '18:29:27.82',
    '18:29:27.69', '18:29:27.56', '18:29:27.43', '18:29:27.29', '18:29:27.16',
    '18:29:27.03', '18:29:26.90', '18:29:26.78', '18:29:26.65', '18:29:26.51',
    '18:29:26.38', '18:29:26.26', '18:29:26.12', '18:29:26.00', '18:29:25.87',
    '18:29:25.74', '18:29:25.61', '18:29:25.48', '18:29:25.34', '18:29:25.22',
    '18:29:25.09', '18:29:24.95', '18:29:24.82', '18:29:24.70', '18:29:24.57',
    '18:29:24.44', '18:29:24.31', '18:29:24.18', '18:29:24.04',
]

# Raw analogRead() values (0-1023) without the fan, newest first.
WITHOUT_FAN = [
    510, 516, 512, 508, 510, 507, 508, 505, 499, 500, 501, 502, 484, 503, 497, 498,
    494, 489, 497, 495, 488, 495, 493, 490, 493, 487, 490, 488, 482, 489, 485, 480,
    482, 480, 484, 478, 477, 473, 474, 477, 478, 472, 461, 467, 462, 457, 461, 451,
    446, 444, 441, 440, 437, 439, 439, 432, 431, 430, 425, 418, 414, 409, 408, 411,
    402, 393, 393, 382, 376, 372, 362, 352, 345, 326, 317, 298, 280, 262, 253, 226,
    218, 196, 177, 163, 148, 137, 129, 118, 119, 107, 104, 93, 88, 87, 84, 78, 76,
    72, 69, 70, 72, 71, 71, 64, 65, 66, 71, 63, 63, 67, 71, 61, 60, 71,
]

# Raw analogRead() values with the fan triggered at the warning threshold, newest first.
WITH_FAN = [
    53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65, 53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65,
    53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65, 53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65,
    53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65, 53, 65, 62, 53, 56, 60, 63, 59, 64, 58, 53, 65,
    59, 64, 58, 53, 53, 63, 250, 226,
    218, 196, 177, 163, 148, 137, 129, 118, 119, 107, 104, 93, 88, 87, 84, 78, 76,
    72, 69, 70, 72, 71, 71, 64, 65, 66, 71, 63, 63, 67, 71, 61, 60, 71,
]


def elapsed_seconds(timestamps):
    """Convert 'HH:MM:SS.ff' strings (chronological order) to seconds since the first one."""
    times = [datetime.strptime(t, '%H:%M:%S.%f') for t in timestamps]
    return [(t - times[0]).total_seconds() for t in times]


def summarize(name, t, values):
    peak = max(values)
    peak_time = t[values.index(peak)]
    print(f'{name:<12} peak = {peak:4d} at t = {peak_time:5.2f} s, '
          f'final = {values[-1]:4d}')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--output', type=Path,
                        help='directory where the figures are saved instead of being shown')
    args = parser.parse_args()

    assert len(TIMESTAMPS) == len(WITHOUT_FAN) == len(WITH_FAN), 'datasets must have the same length'

    # The Serial Monitor log was copied newest-first: put everything back in chronological order.
    t = elapsed_seconds(TIMESTAMPS[::-1])
    without_fan = WITHOUT_FAN[::-1]
    with_fan = WITH_FAN[::-1]

    duration = t[-1]
    print(f'{len(t)} samples over {duration:.2f} s '
          f'(~{duration / (len(t) - 1) * 1000:.0f} ms per sample)')
    summarize('without fan', t, without_fan)
    summarize('with fan', t, with_fan)

    fig_before, ax = plt.subplots(figsize=(8, 5))
    ax.plot(t, without_fan, color='tab:blue', label='without fan')
    ax.axhline(WARNING_THRESHOLD, color='grey', linestyle='--', linewidth=1,
               label=f'warning threshold ({WARNING_THRESHOLD})')
    ax.set_title('Gas level over time - without fan')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('MQ-5 reading (raw ADC, 0-1023)')
    ax.grid(alpha=0.3)
    ax.legend()
    fig_before.tight_layout()

    fig_after, ax = plt.subplots(figsize=(8, 5))
    ax.plot(t, without_fan, color='tab:blue', label='without fan')
    ax.plot(t, with_fan, color='tab:orange', label='with fan')
    ax.axhline(WARNING_THRESHOLD, color='grey', linestyle='--', linewidth=1,
               label=f'warning threshold ({WARNING_THRESHOLD})')
    trigger = with_fan.index(max(with_fan))
    ax.annotate(f'fan ON ({with_fan[trigger]})', xy=(t[trigger], with_fan[trigger]),
                xytext=(t[trigger] + 1.5, with_fan[trigger] + 120),
                arrowprops=dict(arrowstyle='->', color='black'))
    ax.set_title('Gas level over time - with vs without fan')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('MQ-5 reading (raw ADC, 0-1023)')
    ax.grid(alpha=0.3)
    ax.legend()
    fig_after.tight_layout()

    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        fig_before.savefig(args.output / 'gas_without_fan.png', dpi=120)
        fig_after.savefig(args.output / 'gas_with_vs_without_fan.png', dpi=120)
        print(f'figures saved to {args.output.resolve()}')
    else:
        plt.show()


if __name__ == '__main__':
    main()
