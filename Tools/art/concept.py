# Text -> concept image for image-to-3D (SDXL base 1.0, CreativeML Open RAIL++-M: commercial use allowed).
# Images are framed the way Hunyuan3D likes them: whole body, plain white background, soft light.
#   python concept.py "<creature description>" <out.png> [--seed 1] [--count 4]
import argparse, torch
from diffusers import StableDiffusionXLPipeline

p = argparse.ArgumentParser()
p.add_argument("subject"); p.add_argument("out")
p.add_argument("--seed", type=int, default=1); p.add_argument("--count", type=int, default=4)
a = p.parse_args()

pipe = StableDiffusionXLPipeline.from_pretrained("stabilityai/stable-diffusion-xl-base-1.0", torch_dtype=torch.float16,
                                                 variant="fp16", use_safetensors=True)
pipe.enable_model_cpu_offload()
prompt = (f"{a.subject}, full body, three-quarter side view, standing, entire creature visible, centered, "
          "realistic creature concept art, detailed anatomy, natural textures, plain white background, soft studio lighting")
negative = "cropped, cut off, multiple creatures, text, watermark, frame, cartoon, blurry, deformed, extra limbs, busy background, shadow"
for i in range(a.count):
    img = pipe(prompt=prompt, negative_prompt=negative, num_inference_steps=30, guidance_scale=7.0,
               width=1024, height=1024, generator=torch.manual_seed(a.seed + i)).images[0]
    path = a.out.replace(".png", f"_{i}.png")
    img.save(path)
    print("saved", path)
