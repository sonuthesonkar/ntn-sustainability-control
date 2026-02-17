#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#

# =========================
# Reproducibility
# =========================
SEED = 42

# =========================
# Model & data config
# =========================
SEQ_LEN = 60
INPUT_DIM = 8

# =========================
# NTN policy config
# =========================
NTN_START = 0.6
NTN_CROSS = 0.8
CRITICAL_THRESHOLD = 0.9
CRITICAL_SUSTAIN_STEPS = 3
