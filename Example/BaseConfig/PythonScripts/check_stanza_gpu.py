import sys
from pathlib import Path

import torch
import stanza

model_dir = Path(sys.executable).resolve().parent / "stanza_resources"

if torch.cuda.is_available():
    print(f"PyTorch GPU support is available!")
    print(f"CUDA devices found: {torch.cuda.device_count()}")
    print(f"Current device: {torch.cuda.get_device_name(0)}")
    
    print("\nAttempting to create a Stanza pipeline on GPU...")
    try:
        # 尝试加载一个小模型到GPU上
        nlp = stanza.Pipeline(
            lang='ja',
            processors='tokenize',
            dir=str(model_dir),
            use_gpu=True,
            verbose=False,
        )
        print("Stanza pipeline successfully created on GPU!")
    except Exception as e:
        print(f"Stanza failed to use GPU: {e}")
else:
    print("PyTorch GPU support is NOT available. Stanza will run on CPU.")

