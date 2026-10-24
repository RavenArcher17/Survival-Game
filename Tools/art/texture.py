# Paint a texture onto an image-to-3D shape from its concept image (Hunyuan3D-Paint v2 turbo, local GPU).
#   python texture.py <shape.glb> <concept.png> <out.glb>
import argparse, time, torch, trimesh
from PIL import Image
from hy3dgen.rembg import BackgroundRemover
from hy3dgen.texgen import Hunyuan3DPaintPipeline

p = argparse.ArgumentParser()
p.add_argument("shape"); p.add_argument("image"); p.add_argument("out")
a = p.parse_args()

t0 = time.time()
pipe = Hunyuan3DPaintPipeline.from_pretrained("tencent/Hunyuan3D-2", subfolder="hunyuan3d-paint-v2-0-turbo")
if hasattr(pipe, "enable_model_cpu_offload"):
    pipe.enable_model_cpu_offload()  # keeps the 12 GB card comfortable
print(f"loaded in {time.time() - t0:.0f}s")
mesh = trimesh.load(a.shape, force="mesh")
img = Image.open(a.image)
if img.mode != "RGBA":
    img = BackgroundRemover()(img.convert("RGB"))
t1 = time.time()
mesh = pipe(mesh, image=img)
mesh.export(a.out)
print(f"textured in {time.time() - t1:.0f}s -> {a.out}; peak VRAM {torch.cuda.max_memory_allocated() / 2**30:.1f} GB")
