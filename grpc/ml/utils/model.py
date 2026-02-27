#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import torch
import torch.nn as nn
from .config import *
from .error_utils import handle_class_errors, handle_func_errors

# =========================
# Model
# =========================
@handle_class_errors
class CrisisGRU(nn.Module):
    """
    GRU-based architecture designed to process temporal sequences.
    """
    def __init__(self):
        super().__init__()

        # Buffer for initial hidden state; ensure to move it with the model to GPU
        self.register_buffer(
            "h0",
            torch.zeros(1, 1, HIDDEN_DIM)
        )
        self.gru = nn.GRU(INPUT_DIM, HIDDEN_DIM, batch_first=True)
        
        # Prediction head: Map hidden state to a single value
        self.head = nn.Sequential(
            nn.Linear(HIDDEN_DIM, 1)
        )

    def forward(self, x):
        B = x.size(0) # Batch size
        
        # Expand h0 to match current batch size (1, B, HIDDEN_DIM)
        h0 = self.h0.expand(1, B, HIDDEN_DIM)

        # out shape: [Batch, Seq_Len, Hidden_Dim]
        out, _ = self.gru(x, h0)

        # Return sequence of predictions [Batch, Seq_Len]
        raw_out = self.head(out).squeeze(-1)
        
        if self.training:
            return raw_out # No clamp during training
        else:
            return torch.clamp(raw_out, 0.0, 1.0) # Clamp values during prediction 

# =========================
# Loss 
# =========================
@handle_func_errors
def crisis_loss(pred, target):
    """
    Composite loss function with standard error only.
    With MSE only, along with no clamp in training, model shows best accuracy. 
    """

    # Mean Squared Error for base accuracy
    mse = nn.functional.mse_loss(pred, target)
    
    return mse  # return loss
