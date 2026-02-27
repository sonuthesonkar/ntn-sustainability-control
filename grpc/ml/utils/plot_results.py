#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import matplotlib.pyplot as plt
from .config import *
from .error_utils import handle_func_errors

# =========================
# Plot
# =========================
@handle_func_errors
def plot_results(t_series: list[int], original: list[float], 
                 prediction: list[float], model: str, ntn_states: list[int]):
    """
    Setup visualization: 
        top plot for scores (original vs prediction), 
        bottom plot, as step chart, for ntn_states,
        both with common time series (t_series) as x-axis,
        and model being used in label and file name for saving the plot
    """
    fig, (ax1, ax2) = plt.subplots(
        2, 1, figsize=(11, 7), sharex=True,
        gridspec_kw={"height_ratios": [3, 1]}
    )

    # Plot preditcted and original crisis scores and threshold lines
    ax1.plot(t_series, original, label="True Crisis", linewidth=2)
    ax1.plot(t_series, prediction, "--", label=model + " Predicted Crisis")
    ax1.axhline(NTN_START, linestyle="-.", label="NTN Start")
    ax1.axhline(NTN_CROSS, linestyle=":", label="NTN Cross")
    ax1.legend()
    ax1.grid(True)
    ax1.minorticks_on()
    ax1.grid(visible=True, which='minor', color='#999999', linestyle='-', alpha=0.2)
    ax1.grid(visible=True, which='major', color='#666666', linestyle='-', alpha=0.5)

    # Plot step-function for state transitions
    ax2.step(t_series, ntn_states, where="post")
    ax2.set_yticks([0, 1, 2, 3])
    ax2.set_yticklabels([
        "No NTN",
        "NTN Start",
        "NTN Cross",
        "Full Fallback"
    ])
    ax2.set_xlabel("Time")
    ax2.grid(True)
    ax2.minorticks_on()
    ax2.grid(visible=True, which='minor', color='#999999', linestyle='-', alpha=0.2)
    ax2.grid(visible=True, which='major', color='#666666', linestyle='-', alpha=0.5)

    plt.tight_layout()
    plt.savefig("validation_" + model + ".webp")
    plt.show()
