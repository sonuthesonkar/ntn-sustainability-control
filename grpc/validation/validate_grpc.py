#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import numpy as np
import matplotlib.pyplot as plt

from config import *
from data import CrisisDataset
from ntn_state import ntn_state_series

import grpc
import crisis_pb2
import crisis_pb2_grpc

# =========================
# gRPC client
# =========================
channel = grpc.insecure_channel("localhost:50051")
stub = crisis_pb2_grpc.CrisisServiceStub(channel)

# =========================
# Validation data
# =========================
dataset = CrisisDataset(1, force_collapse=True)
X, y = dataset[0]

X_np = X.unsqueeze(0).numpy().astype(np.float32)

# =========================
# gRPC inference (per timestep)
# =========================
ks = X_np.reshape(1,1,-1).flatten().tolist()    # (1, T=1, F)
resp = stub.Evaluate(
    crisis_pb2.CrisisRequest(
        kpi_sequence = ks,
        seq_len = SEQ_LEN,
        feature_dim = INPUT_DIM
    )
)

grpc_out = resp.crisis_scores

# =========================
# NTN logic
# =========================
ntn_states = ntn_state_series(grpc_out)
t = np.arange(len(y))

# =========================
# Plot
# =========================
fig, (ax1, ax2) = plt.subplots(
    2, 1, figsize=(11, 7), sharex=True,
    gridspec_kw={"height_ratios": [3, 1]},
    num="NTN Sustainability Control - Model Validation"
)

ax1.plot(t, y, label="True Crisis", linewidth=2)
ax1.plot(t, grpc_out, "--", label="grpc Predicted Crisis")
ax1.axhline(NTN_START, linestyle="-.", label="NTN Start")
ax1.axhline(NTN_CROSS, linestyle=":", label="NTN Cross")

ax1.legend()
ax1.grid(True)

ax2.step(t, ntn_states, where="post")
ax2.set_yticks([0, 1, 2, 3])
ax2.set_yticklabels([
    "No NTN",
    "NTN Start",
    "NTN Cross",
    "Full Fallback"
])
ax2.set_xlabel("Time")
ax2.grid(True)
plt.tight_layout()
plt.savefig("grpc_model_validation.png")
plt.show()

print("✅ gRPC validation complete")
