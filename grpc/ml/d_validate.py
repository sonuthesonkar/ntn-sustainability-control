#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import json
import numpy as np
from utils.config import *
from utils.data import CrisisDataset
from utils.ntn_states import ntn_state_series
from utils.plot_results import plot_results
from utils.error_utils import handle_func_errors
import argparse

# =========================
# Validation
# =========================
@handle_func_errors
def validate(model_type="gru"):
    """
    Loads the trained model, performs inference on the test data sample, 
    and visualizes the predicted vs original crisis scores and the 
    discrete NTN state transitions.
    """
    
    # Generate test data, ensuring full cycle: normal > collapse > recovery 
    dataset = CrisisDataset(1, force_collapse=True)
    X, y = dataset[0]
    
    # Load model and perform inference, based on model_type
    if (model_type.lower() == "gru"):
        # Import pre-reqs
        import torch
        from a_train import CrisisGRU

        # Load model configuration/metadata
        with open(MODEL_DIR / "metadata.json") as f:
            _ = json.load(f)

        # Initialize model and load trained weights onto the target device
        model = CrisisGRU().to(DEVICE)
        model.load_state_dict(
            torch.load(MODEL_DIR / "crisis_gru.pt", map_location=DEVICE)
        )
        model.eval()    # No longer in training mode

        # Perform inference (No gradients: needed for validation, despite in training mode)
        X = X.unsqueeze(0).to(DEVICE)   # Add batch dimension
        with torch.no_grad():
            pred = model(X).cpu().numpy().squeeze() # Remove batch dimension
    
    elif (model_type.lower() == "onnx"):
        # Import pre-reqs
        import onnxruntime as ort

        # Load onnx model
        sess = ort.InferenceSession(
            (MODEL_DIR / "crisis_gru.onnx").as_posix(),
                providers={
                    "cuda": ["CUDAExecutionProvider", "CPUExecutionProvider"],
                    "xpu":  ["XPUExecutionProvider", "CPUExecutionProvider"],
                }.get(DEVICE.type, ["CPUExecutionProvider"])
        )

        # Execute the model prediction, None: return all available outputs as a list
        X = X.unsqueeze(0).numpy().astype(np.float32)    # Add batch dimension
        resp = sess.run(
            None,
            {"kpi_sequence": X}
        )
        pred = resp[0].squeeze(0)   # Remove batch dimension
    
    elif (model_type.lower() == "grpc"):
        # Import pre-reqs
        import grpc
        import crisis_pb2 as pb
        import crisis_pb2_grpc as pb_grpc

        # gRPC client
        channel = grpc.insecure_channel("localhost:50051")  # Initiate gRPC connection
        stub = pb_grpc.CrisisServiceStub(channel)   # Create client-side proxy

        # gRPC inference (per timestep)
        X = X.unsqueeze(0).numpy().astype(np.float32)    # Add batch dimension
        ks = X.reshape(1,1,-1).flatten().tolist()    # (1, T=1, F) to standard python list

        # Initiate the RPC call, with KPI sequence, and meta-data
        resp = stub.Evaluate(
            pb.CrisisRequest(
                kpi_sequence = ks,
                seq_len = SEQ_LEN,
                feature_dim = INPUT_DIM
            )
        )
        pred = resp.crisis_scores
    
    else:
        raise ValueError(f"Model type {model_type} not supported.")

    t = np.arange(len(y))   # Create time series for plots
    ntn_states = ntn_state_series(pred) # Compute NTN states for predicted crisis scores

    # Plot results
    plot_results(t, y, pred, model_type.upper(), ntn_states)

    print("✅ " + model_type.upper() + " validation completed.")
    
@handle_func_errors
def main():
    parser = argparse.ArgumentParser()
    # Define valid input model types
    parser.add_argument("model_type", choices=["gru", "onnx", "grpc"])
    
    args = parser.parse_args() # Parse and check input argument 
    
    print(f"Validating on device: {DEVICE}")
    
    # Call validate
    validate(args.model_type.lower())

if __name__ == "__main__":
    main()
