#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import numpy as np
from config import *

# =========================
# NTN sustained logic
# =========================
def ntn_state_series(scores):
    """
    Recovery-aware NTN state machine
    States:
    0 = No NTN
    1 = NTN Start
    2 = NTN Cross
    3 = Full Fallback
    """
    states = []
    critical_count = 0
    recovery_count = 0
    state = 0

    for s in scores:
        # --- Escalation logic ---
        if state < 3:
            if s >= CRITICAL_THRESHOLD:
                critical_count += 1
            else:
                critical_count = 0

            if critical_count >= CRITICAL_SUSTAIN_STEPS:
                state = 3
                recovery_count = 0
            elif s >= NTN_CROSS:
                state = 2
            elif s >= NTN_START:
                state = 1
            else:
                state = 0
        # --- Recovery logic ---
        else: # state == 3 (Full Fallback)
            if s < NTN_CROSS:
                recovery_count += 1
            else:
                recovery_count = 0

            if recovery_count >= 2: # Sustained recovery
                state = 1 if s >= NTN_START else 0
                critical_count = 0

        states.append(state)

    return np.array(states)
