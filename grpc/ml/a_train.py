#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import json
import torch
import torch.nn as nn
from torch.utils.data import DataLoader
from utils.config import *
from utils.data import CrisisDataset
from utils.error_utils import handle_class_errors, handle_func_errors
from utils.model import CrisisGRU, crisis_loss

torch.manual_seed(SEED) # Reproducibility

# =========================
# Training
# =========================
@handle_func_errors
def train():
    """
    The training routine.
    """
    
    # Initialize data pipeline
    dataset = CrisisDataset(500, force_collapse=False)
    loader = DataLoader(
        dataset,
        batch_size=BATCH_SIZE,
        shuffle=True,
        pin_memory=(DEVICE.type != "cpu")   # Maximum efficiency on GPU, else CPU compatible
    )

    # Initialize model and optimizer
    model = CrisisGRU().to(DEVICE)
    opt = torch.optim.Adam(model.parameters(), lr=LR)

    # Main training loop
    for e in range(EPOCHS):
        total = 0.0
        for _, (X, y) in enumerate(loader):
            X, y = X.to(DEVICE), y.to(DEVICE)
            
            preds = model(X)
            loss = crisis_loss(preds, y) # Forward pass

            # Backward pass and optimization
            opt.zero_grad() # Wipe the gradients from previous batch
            loss.backward() # Calculate new gradients from the current batch
            opt.step()  # Update the model weights based on the fresh gradients
            total += loss.item() # Total loss for the epoch

        print(f"Epoch {e+1:02d} | Loss: {total / len(loader):.4f}") # Average epoch loss

    # Save learned parameters (weights and biases)
    torch.save(model.state_dict(), MODEL_DIR / "crisis_gru.pt")

    # Save hyperparameters
    with open(MODEL_DIR / "metadata.json", "w") as f:
        json.dump({
            "seq_len": SEQ_LEN,
            "input_dim": INPUT_DIM,
            "hidden_dim": HIDDEN_DIM
        }, f, indent=2)

    print("Model saved.")

if __name__ == "__main__":
    print(f"Training on device: {DEVICE}")
    train()
