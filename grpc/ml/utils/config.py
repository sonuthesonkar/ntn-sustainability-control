#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import torch
from pathlib import Path

# =========================
# Reproducibility
# =========================
SEED = 42

# =========================
# Device resolution
# =========================
def get_device():
    if torch.cuda.is_available():
        return torch.device("cuda")
    if hasattr(torch, "xpu") and torch.xpu.is_available():
        return torch.device("xpu")
    return torch.device("cpu")

DEVICE = get_device()

# =========================
# Model & data config
# =========================
SEQ_LEN = 60	# Time steps
INPUT_DIM = 8	# 7 KPIs + 1 crisis score
HIDDEN_DIM = 32	# Giving best accuracy for 500 samples training dataset

BATCH_SIZE = 16	# Giving best accuracy for 500 samples training dataset
EPOCHS = 25		# Default starting point
LR = 1e-3		# Default starting point

# =========================
# NTN policy config
# =========================
NTN_START = 0.6	# Warning
NTN_CROSS = 0.8	# Critical
CRITICAL_THRESHOLD = 0.9	# Full fallback
CRITICAL_SUSTAIN_STEPS = 3	# Times state sustained consecutively

# =========================
# Paths
# =========================
MODEL_DIR = Path("../models")
MODEL_DIR.mkdir(exist_ok=True)
