#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
from pathlib import Path
from utils.error_utils import error_context

with error_context(Path(__file__).name):
    import numpy as np
    import torch
    import onnxruntime as ort
    from utils.config import *
    from utils.model import CrisisGRU

    MAX_DIFF_THRESHOLD = 1e-3

    # Initialize pytorch model and load trained weights onto the target device
    pt = CrisisGRU().to(DEVICE)
    pt.load_state_dict(torch.load(MODEL_DIR / "crisis_gru.pt", map_location=DEVICE))
    pt.eval()   # Evaluate mode

    # Load ONNX model
    sess = ort.InferenceSession(
        (MODEL_DIR / "crisis_gru.onnx").as_posix(),
        providers=["CPUExecutionProvider"]
    )

    # Create a random synthetic input matching the expected input shape
    x = np.random.rand(1, SEQ_LEN, INPUT_DIM).astype(np.float32)

    # =========================
    # Run Both Inferences
    # =========================
    # 1. PyTorch Forward Pass
    with torch.no_grad():
        pt_out = pt(torch.from_numpy(x)).cpu().numpy()

    # 2. ONNX Runtime Forward Pass
    # sess.run returns a list of outputs; [0] grabs the primary 'crisis_score'
    onnx_out = sess.run(None, {"kpi_sequence": x})[0]

    # Calculate numerical divergence between the two runtimes
    diff = np.abs(pt_out - onnx_out)
    print("Max diff :", diff.max(), "Mean diff:", diff.mean(), end=" ")

    # =========================
    # Parity Verification
    # =========================
    # Return the index of the first time step that crosses the threshold 
    def crossing_times (y, threshold):
        return np.where(y >= threshold)[0][0]

    # Validation logic
    # 1. Timing of threshold crossing is nearly adjacent (<= 1 step)
    # 2. MAx diff is within the allowed threshold
    assert (
        abs(crossing_times(pt_out, 0.6) - crossing_times(onnx_out, 0.6)) <= 1 and
        diff.max() <= MAX_DIFF_THRESHOLD
    ), "❌ Parity failed."

    print("✅ Parity passed.")
