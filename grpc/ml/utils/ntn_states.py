#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import numpy as np
from .config import *
from .error_utils import handle_func_errors

# =========================
# Logic for NTN state change
# =========================
@handle_func_errors
def ntn_state_series(scores: list[float]):
    """
    Computes a NTN state based on crisis score and sustained counts.
    Assumption: scores in the input list follow the pattern, 
                normal > full collapse > recovery
    
    States:
    0: No NTN (Normal)
    1: NTN Start (Initial Warning)
    2: NTN Cross (Critical Warning)
    3: Full Fallback (Full failure - requires crisis for sustained count)
    """

    states = [] # Return ntn state for each score in scores[]
    critical_count = 0  # Counter for critical state
    recovery_count = 0  # Counter for recovery state
    state = 0   # Initial state: No NTN

    for s in scores:
        if state < 3:
            if s >= CRITICAL_THRESHOLD: # Start critical count
                critical_count += 1
            else:
                critical_count = 0

            if critical_count >= CRITICAL_SUSTAIN_STEPS: # Full fallback
                state = 3
                recovery_count = 0
            elif s >= NTN_CROSS:
                state = 2
            elif s >= NTN_START:
                state = 1
            else:
                state = 0
        else:
            if s < NTN_CROSS:   # Start recovery count
                recovery_count += 1
            else:
                recovery_count = 0

            if recovery_count >= 2: # Start recovery
                state = 1 if s >= NTN_START else 0
                critical_count = 0

        states.append(state)

    return np.array(states)
