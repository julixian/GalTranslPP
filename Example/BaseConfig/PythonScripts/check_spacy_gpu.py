import spacy

# require_gpu() 会在没有 GPU 或 cupy 不可用时直接抛出错误。
spacy.require_gpu()
print("spaCy GPU support is available and enabled.")

