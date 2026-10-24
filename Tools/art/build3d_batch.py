# Image -> textured 3D for a list of picked concepts: all shapes first (Hunyuan3D-2 DiT), then all textures (Paint),
# so each model is loaded once.   python build3d_batch.py id:index [id:index ...]
import sys, time, gc, torch, trimesh
from PIL import Image
from hy3dgen.rembg import BackgroundRemover
from hy3dgen.shapegen import Hunyuan3DDiTFlowMatchingPipeline, FloaterRemover, DegenerateFaceRemover, FaceReducer

picks = [a.split(":") for a in sys.argv[1:]]
rembg = BackgroundRemover()
images = {}
for cid, idx in picks:
    img = Image.open(f"out/{cid}_{idx}.png")
    images[cid] = img if img.mode == "RGBA" else rembg(img.convert("RGB"))

shape = Hunyuan3DDiTFlowMatchingPipeline.from_pretrained("tencent/Hunyuan3D-2", subfolder="hunyuan3d-dit-v2-0", use_safetensors=True)
for cid, _ in picks:
    t = time.time()
    mesh = shape(image=images[cid], num_inference_steps=50, octree_resolution=384, generator=torch.manual_seed(7))[0]
    mesh = FaceReducer()(DegenerateFaceRemover()(FloaterRemover()(mesh)), max_facenum=40000)
    mesh.export(f"out/{cid}_shape.glb")
    print(f"shape {cid} {time.time() - t:.0f}s", flush=True)
del shape
gc.collect()
torch.cuda.empty_cache()

from hy3dgen.texgen import Hunyuan3DPaintPipeline
paint = Hunyuan3DPaintPipeline.from_pretrained("tencent/Hunyuan3D-2", subfolder="hunyuan3d-paint-v2-0-turbo")
if hasattr(paint, "enable_model_cpu_offload"):
    paint.enable_model_cpu_offload()
for cid, _ in picks:
    t = time.time()
    textured = paint(trimesh.load(f"out/{cid}_shape.glb", force="mesh"), image=images[cid])
    textured.export(f"out/{cid}_textured.glb")
    print(f"texture {cid} {time.time() - t:.0f}s", flush=True)
