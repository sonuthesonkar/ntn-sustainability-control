#-------------------------------------------------------------------#
# Copyright (c) 2026 Sonu Sonkar.                                   #
# This source code is licensed under the MIT license found in the   #
# LICENSE file in the root directory of this source tree.           #
#-------------------------------------------------------------------#
import numpy as np
import torch
from torch.utils.data import Dataset
from config import SEQ_LEN

# =========================
# Synthetic data generation
# =========================
class CrisisTrajectoryGenerator:
    """
    Generates temporal KPI trajectories with a latent crisis score.
    """

    def __init__(self, seq_len=SEQ_LEN, force_collapse=False):
        self.seq_len = seq_len
        self.force_collapse = force_collapse

    def _evolve_crisis(self, c_prev, t):
        drift = np.random.uniform(-0.02, 0.05)
        shock = np.random.normal(0, 0.03)
        c = np.clip(c_prev + drift + shock, 0.0, 1.0)
    
        if self.force_collapse: # Simulate crisis and then recovery
            # Phase 1: Escalation
            if int(0.4 * self.seq_len) < t <= int(0.6 * self.seq_len):
                c = min(1.0, c + np.random.uniform(0.08, 0.12))
    
            # Phase 2: Sustained collapse
            elif int(0.6 * self.seq_len) < t <= int(0.75 * self.seq_len):
                c = max(c, 0.95)
    
            # Phase 3: Recovery
            elif t > int(0.75 * self.seq_len):
                c = max(0.0, c - np.random.uniform(0.05, 0.1))
    
        return c

    def _kpis_from_crisis(self, c):
        # Canonical KPI set (normalized to [0,1]):
        # [congestion, prb_util, traffic_load, ran_energy,
        # carbon_intensity, isac_quality, mobility_rate, emergency<0 or 1> ]
        
        congestion = np.clip(c + np.random.normal(0, 0.05), 0, 1)
        prb_util = np.clip(0.4 + 0.6 * c + np.random.normal(0, 0.05), 0, 1)
        traffic = np.clip(0.3 + 0.7 * c + np.random.normal(0, 0.05), 0, 1)
        ran_energy = np.clip(0.2 + 0.8 * c + np.random.normal(0, 0.05), 0, 1)
        carbon = np.clip(0.5 * c + np.random.normal(0, 0.05), 0, 1)
        isac_quality = np.clip(1.0 - c + np.random.normal(0, 0.05), 0, 1)
        mobility = np.clip(1.0 - 0.8 * c + np.random.normal(0, 0.05), 0, 1)
        emergency = 1.0 if c > 0.9 else 0.0

        return np.array([
            congestion,
            prb_util,
            traffic,
            ran_energy,
            carbon,
            isac_quality,
            mobility,
            emergency
        ], dtype=np.float32)

    def generate(self):
        X, y = [], []
        c = np.random.uniform(0.0, 0.3)

        for t in range(self.seq_len):
            c = self._evolve_crisis(c, t)
            X.append(self._kpis_from_crisis(c))
            y.append(c)

        return np.stack(X), np.array(y, dtype=np.float32)

# =========================
# Dataset
# =========================
class CrisisDataset(Dataset):
    """
    Generates sample dataset.
    """

    # Create data samples
    def __init__(self, n_samples, force_collapse=False):
        gen = CrisisTrajectoryGenerator(force_collapse=force_collapse)
        self.samples = [gen.generate() for _ in range(n_samples)]

    # Get sample count
    def __len__(self):
        return len(self.samples)

    # get sample at idx
    def __getitem__(self, idx):
        X, y = self.samples[idx]
        return torch.tensor(X), torch.tensor(y)
