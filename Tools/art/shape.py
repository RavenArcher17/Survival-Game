# Image -> untextured 3D shape with Hunyuan3D-2 (runs on the local GPU).
#   python shape.py <image.png> <out.glb> [--steps 30] [--octree 256]
import argparse, time, torch
from PIL import Image
from hy3dgen.rembg import BackgroundRemover
from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline, FloaterRemover, DegenerateFaceRemover, FaceReducer

p = argparse.ArgumentParser()
p.add_argument("image"); p.add_argument("out")
p.add_argument("--steps", type=int, default=30); p.add_argument("--octree", type=int, default=256)
p.add_argument("--faces", type=int, default=40000)
a = p.parse_args()

t0 = time.time()
pipe = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained("tencent/Hunyuan3D-2", subfolder="hunyuan3d-dit-v2-0", use_safetensors=True)
print(f"loaded in {time.time() - t0:.0f}s")
img = Image.open(a.image)
if img.mode != "RGBA":
    img = BackgroundRemover()(img.convert("RGB"))
t1 = time.time()
mesh = pipe(image=img, num_inference_steps=a.steps, octree_resolution=a.octree, generator=torch.manual_seed(7))[0]
mesh = FloaterRemover()(mesh)
mesh = DegenerateFaceRemover()(mesh)
mesh = FaceReducer()(mesh, max_facenum=a.faces)
mesh.export(a.out)
print(f"shape in {time.time() - t1:.0f}s: {len(mesh.faces)} faces -> {a.out}; peak VRAM {torch.cuda.max_memory_allocated() / 2**30:.1f} GB")
