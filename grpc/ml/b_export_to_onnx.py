#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
from pathlib import Path
from utils.error_utils import error_context

with error_context(Path(__file__).name):
    import torch
    from utils.config import *
    from utils.model import CrisisGRU

    # =========================
    # Model Initialization
    # =========================
    # Initialize model and load trained weights onto the target device
    model = CrisisGRU().to(DEVICE)
    model.load_state_dict(torch.load(MODEL_DIR / "crisis_gru.pt", map_location=DEVICE))
    model.eval()    # No longer in training mode

    # Create a dummy input tensor matching the expected model input shape:
    # (Batch Size, Sequence Length, Feature Dimensions)
    dummy = torch.zeros(1, SEQ_LEN, INPUT_DIM, device=DEVICE)

    # Convert the PyTorch graph into a static, cross-platform ONNX format
    with torch.no_grad():
        torch.onnx.export(
            model,                          # The model to be exported
            dummy,                          # A sample input for the tracer to map the data flow
            MODEL_DIR / "crisis_gru.onnx",  # The destination file path
            opset_version=17,               # Ver 17 supports newer GRU optimizations
            input_names=["kpi_sequence"],   # Input node name
            output_names=["crisis_score"],  # Output node name
            dynamo=False,                   # Force the legacy exporter. MUST for accuracy
            dynamic_axes={                  # Enable flexible batch sizes
                "kpi_sequence": {0: "batch_size"}, 
                "crisis_score": {0: "batch_size"}
            } 
        )

    print("✅ ONNX export completed.")
